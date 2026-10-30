// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file client.cpp
 * Hyperion Client
 *
 * Sends the displayed video to the flatbuffers server of Hyperion
 * (https://github.com/hyperion-project/hyperion.ng), default port 19400.
 *
 * Protocol:
 * - every message is a flatbuffer, prefixed by its size (4 bytes, big endian)
 * - Register(origin, priority) once after connecting, the priority has to be
 *   in the range 100 - 199 (lower values win in Hyperion)
 * - Image(RawImage(rgb, width, height), duration) for every image
 * - Clear(priority) to give the leds back to lower priorities
 * - the server answers every request with a Reply(error, video, registered)
 * - the server drops a client, which doesn't send anything for some seconds
 *   (default 5s), so the last image is repeated while the video is paused.
 *   A dropped or closed connection clears the priority in Hyperion as well.
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstring>

#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <vdr/plugin.h>
#include <vdr/tools.h>

#include "ambientimage_service.h"
#include "client.h"
#include "protocol.h"

namespace {

uint64_t NowMs(void)
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

} // namespace

/**
 * Create the client
 *
 * The thread has to be started with Start()
 */
cHyperionClient::cHyperionClient(void)
	: cThread("hyperion")
{
}

cHyperionClient::~cHyperionClient(void)
{
	Stop();
}

/**
 * Stop the thread
 *
 * Clears the priority in Hyperion and closes the connection.
 */
void cHyperionClient::Stop(void)
{
	if (!Active())
		return;

	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_changed = true;
	}
	m_cond.notify_one();
	Cancel(3);
}

/**
 * Set the configuration
 *
 * A changed host, port or priority opens a new connection.
 */
void cHyperionClient::Configure(const sHyperionConfig &config)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_config = config;
		m_config.priority = std::clamp(m_config.priority, cHyperionProtocol::PRIORITY_MIN, cHyperionProtocol::PRIORITY_MAX);
		m_config.fps = std::clamp(m_config.fps, 1, 60);
		m_config.width = std::clamp(m_config.width, 8, 640);
		m_config.jsonPort = std::clamp(m_config.jsonPort, 1, 65535);
		m_changed = true;
	}
	m_cond.notify_one();
}

/**
 * Switch the ambient light on or off (hotkey, menu, SVDRP)
 *
 * Depending on the setup, the led device of Hyperion is switched off
 * or only the video isn't sent anymore. The state isn't stored.
 */
void cHyperionClient::SetOn(bool on)
{
	{
		std::lock_guard<std::mutex> lock(m_mutex);
		m_on = on;
		m_changed = true;
	}
	m_cond.notify_one();
}

/**
 * Wait, until the time is over or the configuration has changed
 */
void cHyperionClient::Wait(int ms)
{
	std::unique_lock<std::mutex> lock(m_mutex);
	m_cond.wait_for(lock, std::chrono::milliseconds(ms), [this] { return m_changed; });
	m_changed = false;
}

/**
 * Thread loop
 */
void cHyperionClient::Action(void)
{
	// the ambient light must never steal time from decoding and displaying
	SetPriority(10);

	dsyslog("hyperion: thread started");

	while (Running()) {
		UpdateStats(NowMs());

		sHyperionConfig config;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			config = m_config;
			m_changed = false;
		}

		// reconnect with the new settings
		if (config != m_activeConfig) {
			// clean up on the old server with the old settings
			if (IsConnected())
				Disconnect(true);
			StopEffect(true);

			// the led device of the old server isn't controlled anymore: switch it on again
			bool sameServer = config.host == m_activeConfig.host && config.jsonPort == m_activeConfig.jsonPort && config.token == m_activeConfig.token;
			if (!sameServer || !ControlsLedDevice(config)) {
				if (m_ledDeviceState == 0)
					JsonRequest({ cHyperionJson::ComponentState("LEDDEVICE", true, ++m_jsonTan) }, true);
				m_ledDeviceState = -1;
			}

			m_activeConfig = config;
			m_reconnectAtMs = 0;
			m_reconnectDelayMs = RECONNECT_MIN_MS;
			m_connectErrorLogged = false;
			m_jsonRetryAtMs = 0;
			m_lastJsonError.clear();
		}

		UpdateLedDevice();

		uint64_t now = NowMs();

		if (!config.enable || !FindProvider(now)) {
			ReleaseLeds();
			Wait(250);
			continue;
		}

		// switched off: only follow the attach state of the output device
		if (!m_on) {
			ReleaseLeds();
			m_pausedSinceMs = 0;
			m_brightness = 100;
			AmbientImage_v1_0_t state = {};
			state.structSize = sizeof(state);
			state.width = config.width;
			state.rgb = m_rgb.data();
			state.rgbSize = m_rgb.size();
			if (m_pProvider->Service(AMBIENTIMAGE_SERVICE, &state))
				m_attached = !(state.flags & AMBIENT_DETACHED);
			Wait(250);
			continue;
		}

		// Limit the images per second: wait a bit less than the interval, the service
		// waits for the next frame. So with 50 Hz and 25 images/s, every second frame is sent.
		int intervalMs = 1000 / config.fps;
		int waitMs = intervalMs - std::min(INTERVAL_SLACK_MS, intervalMs / 2);
		if (m_lastImageMs && now - m_lastImageMs < (uint64_t)waitMs) {
			Wait(waitMs - (int)(now - m_lastImageMs));
			continue;
		}

		// wait for the next frame of the video and read its image (in this thread)
		AmbientImage_v1_0_t req = {};
		req.structSize = sizeof(req);
		req.width = config.width;
		// wake up more often, while the brightness fades
		req.timeoutMs = m_pausedSinceMs && m_brightness != config.pauseBrightness ? FADE_STEP_MS : SERVICE_WAIT_MS;
		req.lastFrame = m_lastFrame;
		req.rgb = m_rgb.data();
		req.rgbSize = m_rgb.size();

		if (!m_pProvider->Service(AMBIENTIMAGE_SERVICE, &req)) {
			esyslog("hyperion: the output plugin rejected the request");
			SetError("the output plugin rejected the request");
			m_pProvider = nullptr;
			m_providerRetryAtMs = NowMs() + PROVIDER_RETRY_MS;
			continue;
		}
		now = NowMs();

		if (req.flags & AMBIENT_DETACHED) {
			// no video can be displayed: give the leds back at once
			m_attached = false;
			ReleaseLeds();
			m_pausedSinceMs = 0;
			m_brightness = 100;
			m_lastFrame = 0;
			Wait(250);
			continue;
		}
		m_attached = true;

		// pause: dim the image after a moment (setup), restore it at once with the playback
		bool paused = (req.flags & AMBIENT_PAUSED) && (req.flags & AMBIENT_VIDEO);
		if (paused && !m_pausedSinceMs)
			m_pausedSinceMs = now;
		else if (!paused)
			m_pausedSinceMs = 0;

		int brightness = PauseBrightness(now);
		if (brightness != m_brightness) {
			bool fadeStep = brightness < m_brightness && brightness != config.pauseBrightness && now - m_lastSendMs < FADE_STEP_MS;
			if (!fadeStep) {
				m_brightness = brightness;
				if (IsConnected() && m_imageSent && !m_image.empty() && !m_idleColorShown && SendImage())
					m_lastSendMs = now;
			}
		}

		if (req.flags & AMBIENT_NEW_IMAGE) {
			m_idleSinceMs = 0;
			m_idleShown = false;
			m_lastReadError.clear();
			m_statsSampleUsSum += req.sampleUs;
			m_statsSampleUsMax = std::max(m_statsSampleUsMax, req.sampleUs);
			m_statsSamples++;
			{
				std::lock_guard<std::mutex> lock(m_statsMutex);
				m_stats.format = req.format;
				m_stats.width = req.imageWidth;
				m_stats.height = req.imageHeight;
			}

			// leave the idle state
			if (m_effectActive)
				StopEffect(false);
			if (m_idleColorShown) {
				m_idleColorShown = false;
				m_imageSent = false;
			}

			m_lastFrame = req.frame;
			m_lastImageMs = now;
			m_imageWidth = req.imageWidth;
			m_imageHeight = req.imageHeight;
			m_image.assign(req.rgb, req.rgb + (size_t)req.imageWidth * req.imageHeight * 3);

			if (!IsConnected() && now >= m_reconnectAtMs)
				Connect();
			if (IsConnected() && SendImage()) {
				m_imageSent = true;
				m_lastSendMs = now;
				m_statsImages++;
			}
		} else if ((req.flags & AMBIENT_VIDEO) && req.format[0]) {
			// a frame was taken, but the output plugin can't read it
			std::string error = std::string("the output plugin can't read the frame (") + req.format + ")";
			if (error != m_lastReadError) {
				esyslog("hyperion: %s", error.c_str());
				SetError(error);
			}
			m_lastReadError = error;
		} else if (!(req.flags & AMBIENT_VIDEO) && !m_idleSinceMs && !m_idleShown) {
			m_idleSinceMs = now;
		}

		// No video is displayed anymore (e.g. radio). A channel switch shows no video for
		// a moment, too: meanwhile the last image is kept.
		if (m_idleSinceMs && now - m_idleSinceMs >= IDLE_RELEASE_MS)
			ShowIdle();

		if (IsConnected()) {
			ReceiveReplies();

			// keep the connection and the image or the dark leds alive (paused video, radio)
			if (IsConnected() && now - m_lastSendMs >= KEEPALIVE_MS) {
				if (m_idleColorShown) {
					if (Send(cHyperionProtocol::EncodeColor(0x000000)))
						m_lastSendMs = now;
				} else if (m_imageSent && !m_image.empty()) {
					if (SendImage())
						m_lastSendMs = now;
				}
			}
		}
	}

	// VDR ends: give the leds back and switch the led device off, if it follows
	// the attach state. Only one request: the time to stop the thread is limited.
	if (IsConnected())
		Disconnect(true);

	std::vector<std::string> requests;
	if (m_effectActive)
		requests.push_back(cHyperionJson::Clear(m_activeConfig.priority, ++m_jsonTan));
	if (ControlsLedDevice(m_activeConfig) && m_activeConfig.ledsFollowAttach && m_ledDeviceState != 0)
		requests.push_back(cHyperionJson::ComponentState("LEDDEVICE", false, ++m_jsonTan));
	if (!requests.empty() && JsonRequest(requests, true)) {
		m_effectActive = false;
		if (m_activeConfig.ledsFollowAttach)
			m_ledDeviceState = 0;
	}

	dsyslog("hyperion: thread stopped");
}

/**
 * Look for the output plugin, which provides the image of the video
 */
bool cHyperionClient::FindProvider(uint64_t now)
{
	if (m_pProvider)
		return true;
	if (now < m_providerRetryAtMs)
		return false;

	m_pProvider = cPluginManager::CallFirstService(AMBIENTIMAGE_SERVICE);
	if (!m_pProvider) {
		if (!m_providerErrorLogged) {
			esyslog("hyperion: no output plugin provides the service %s (e.g. drmdevice)", AMBIENTIMAGE_SERVICE);
			SetError(*cString::sprintf("no output plugin provides the service %s", AMBIENTIMAGE_SERVICE));
		}
		m_providerErrorLogged = true;
		m_providerRetryAtMs = now + PROVIDER_RETRY_MS;
		return false;
	}

	isyslog("hyperion: the plugin %s provides the image of the video", m_pProvider->Name() ? m_pProvider->Name() : "?");
	m_providerErrorLogged = false;
	m_rgb.resize(640 * 640 * 3);
	return true;
}

/**
 * Show, that no video is displayed (setup: HyperionIdleMode)
 *
 * Called, when no video was displayed for IDLE_RELEASE_MS. If it fails (e.g.
 * Hyperion isn't reachable), it is retried with the next loop.
 */
void cHyperionClient::ShowIdle(void)
{
	switch (m_activeConfig.idleMode) {
		case sHyperionConfig::IDLE_EFFECT:
			// the video of this priority is replaced by the effect
			if (IsConnected())
				Disconnect(true);
			m_image.clear();

			if (JsonRequest({ cHyperionJson::Effect(m_activeConfig.idleEffect, m_activeConfig.priority, ORIGIN, ++m_jsonTan) }, false)) {
				dsyslog("hyperion: no video, started the effect \"%s\"", m_activeConfig.idleEffect.c_str());
				m_effectActive = true;
				m_idleSinceMs = 0;
				m_idleShown = true;
			}
			break;
		case sHyperionConfig::IDLE_BLACK:
			if (!IsConnected() && NowMs() >= m_reconnectAtMs)
				Connect();
			if (IsConnected() && Send(cHyperionProtocol::EncodeColor(0x000000))) {
				dsyslog("hyperion: no video, the leds are dark");
				m_idleColorShown = true;
				m_imageSent = true;     // the priority has to be cleared
				m_lastSendMs = NowMs();
				m_image.clear();
				m_idleSinceMs = 0;
				m_idleShown = true;
			}
			break;
		default:
			ReleaseLeds();
			m_idleShown = true;
			break;
	}
}

/**
 * Stop the idle effect
 *
 * @param force     send the request, even if the last request failed a moment ago
 * @return          true, if no effect is active anymore
 */
bool cHyperionClient::StopEffect(bool force)
{
	if (!m_effectActive)
		return true;

	if (JsonRequest({ cHyperionJson::Clear(m_activeConfig.priority, ++m_jsonTan) }, force))
		m_effectActive = false;

	return !m_effectActive;
}

/**
 * True, if the client switches the led device of Hyperion with the given settings
 */
bool cHyperionClient::ControlsLedDevice(const sHyperionConfig &config) const
{
	return config.enable && (config.ledsFollowAttach || config.offAction == sHyperionConfig::OFF_LEDDEVICE);
}

/**
 * Switch the led device of Hyperion according to the state
 *
 * The led device is off, if the user switched the ambient light off (with
 * OFF_LEDDEVICE) or if it follows the attach state and the device is detached.
 * A request is only sent, if the state changes. Without ledsFollowAttach, the
 * led device is assumed to be on at the start, so the client doesn't switch
 * on a led device, which was switched off elsewhere.
 */
void cHyperionClient::UpdateLedDevice(void)
{
	if (!ControlsLedDevice(m_activeConfig))
		return;

	bool on = true;
	if (m_activeConfig.ledsFollowAttach && !m_attached)
		on = false;
	if (m_activeConfig.offAction == sHyperionConfig::OFF_LEDDEVICE && !m_on)
		on = false;

	if (m_ledDeviceState == -1 && on && !m_activeConfig.ledsFollowAttach)
		m_ledDeviceState = 1;
	if (m_ledDeviceState == (on ? 1 : 0))
		return;

	if (JsonRequest({ cHyperionJson::ComponentState("LEDDEVICE", on, ++m_jsonTan) }, false)) {
		isyslog("hyperion: switched the led device %s", on ? "on" : "off");
		m_ledDeviceState = on ? 1 : 0;
	}
}

/**
 * Send requests to the JSON server and wait for the replies
 *
 * A short connection is used for every call: the requests are rare (a
 * switch of the led device, the start of an effect). With a token, the
 * client logs in first. The whole call takes at most JSON_TIMEOUT_MS.
 * After a failure, further requests are skipped for JSON_RETRY_MS, so an
 * unreachable server doesn't block the thread again and again.
 *
 * @param requests  the requests (lines)
 * @param force     send, even if the last request failed a moment ago
 * @return          true, if all requests succeeded
 */
bool cHyperionClient::JsonRequest(const std::vector<std::string> &requests, bool force)
{
	uint64_t start = NowMs();
	if (!force && start < m_jsonRetryAtMs)
		return false;

	std::string error;
	std::string data;
	std::vector<int> tans;

	if (!m_activeConfig.token.empty()) {
		int tan = ++m_jsonTan;
		data += cHyperionJson::Login(m_activeConfig.token, tan);
		tans.push_back(tan);
	}
	for (const auto &r : requests) {
		data += r;
		size_t p = r.rfind("\"tan\":");
		tans.push_back(p == std::string::npos ? -1 : atoi(r.c_str() + p + 6));
	}

	uint64_t deadline = start + JSON_TIMEOUT_MS;
	int fd = OpenSocket(m_activeConfig.host, m_activeConfig.jsonPort, JSON_TIMEOUT_MS);
	if (fd < 0) {
		error = cString::sprintf("cannot connect (%s)", strerror(errno));
	} else {
		// send
		size_t sent = 0;
		while (sent < data.size() && error.empty()) {
			ssize_t n = send(fd, data.data() + sent, data.size() - sent, MSG_NOSIGNAL | MSG_DONTWAIT);
			if (n > 0) {
				sent += n;
				continue;
			}
			if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
				error = cString::sprintf("send failed (%s)", strerror(errno));
				break;
			}
			int64_t left = (int64_t)(deadline - NowMs());
			struct pollfd pfd = { fd, POLLOUT, 0 };
			if (left <= 0 || poll(&pfd, 1, left) <= 0)
				error = "send timed out";
		}

		// Read the replies, one line each. Hyperion sends one reply per instance,
		// so a tan may be answered several times.
		std::string rx;
		while (error.empty() && !tans.empty()) {
			size_t eol;
			while (error.empty() && (eol = rx.find('\n')) != std::string::npos) {
				std::string line = rx.substr(0, eol);
				rx.erase(0, eol + 1);

				int tan;
				bool success;
				std::string replyError;
				if (!cHyperionJson::ParseReply(line, &tan, &success, &replyError))
					continue;   // no reply (e.g. a notification)
				if (!success)
					error = replyError.empty() ? std::string("request failed") : replyError;
				tans.erase(std::remove(tans.begin(), tans.end(), tan), tans.end());
			}
			if (!error.empty() || tans.empty())
				break;

			int64_t left = (int64_t)(deadline - NowMs());
			struct pollfd pfd = { fd, POLLIN, 0 };
			if (left <= 0 || poll(&pfd, 1, left) <= 0) {
				error = "no reply";
				break;
			}

			char buf[4096];
			ssize_t n = recv(fd, buf, sizeof(buf), MSG_DONTWAIT);
			if (n > 0)
				rx.append(buf, n);
			else if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR))
				error = "connection closed by the server";
		}
		close(fd);
	}

	if (!error.empty()) {
		if (error != m_lastJsonError)
			esyslog("hyperion: request to the JSON server %s:%d failed: %s", m_activeConfig.host.c_str(), m_activeConfig.jsonPort, error.c_str());
		m_lastJsonError = error;
		SetError("JSON server: " + error);
		m_jsonRetryAtMs = NowMs() + JSON_RETRY_MS;
		return false;
	}

	m_lastJsonError.clear();
	m_jsonRetryAtMs = 0;
	return true;
}

/**
 * Get the statistics (for the setup)
 */
void cHyperionClient::GetStats(sHyperionStats *stats)
{
	std::lock_guard<std::mutex> lock(m_statsMutex);
	*stats = m_stats;
}

/**
 * Remember an error for the statistics
 */
void cHyperionClient::SetError(const std::string &error)
{
	std::lock_guard<std::mutex> lock(m_statsMutex);
	m_stats.lastError = error;
	m_stats.lastErrorTime = time(nullptr);
}

/**
 * Update the statistics (once per loop of the client thread)
 */
void cHyperionClient::UpdateStats(uint64_t now)
{
	int state;
	if (!m_activeConfig.enable)
		state = sHyperionStats::STATE_DISABLED;
	else if (!m_pProvider)
		state = sHyperionStats::STATE_NO_OUTPUT;
	else if (!m_attached)
		state = sHyperionStats::STATE_DETACHED;
	else if (!m_on)
		state = sHyperionStats::STATE_OFF;
	else if (m_effectActive)
		state = sHyperionStats::STATE_EFFECT;
	else if (m_idleColorShown)
		state = sHyperionStats::STATE_DARK;
	else if (m_idleSinceMs)
		state = m_imageSent ? sHyperionStats::STATE_KEPT : sHyperionStats::STATE_RELEASED;
	else if (m_idleShown)
		state = sHyperionStats::STATE_RELEASED;
	else if (m_pausedSinceMs)
		state = sHyperionStats::STATE_PAUSED;
	else
		state = sHyperionStats::STATE_VIDEO;    // also, if Hyperion isn't reachable (see connected)

	std::lock_guard<std::mutex> lock(m_statsMutex);
	m_stats.state = state;
	const char *output = m_pProvider ? m_pProvider->Name() : nullptr;
	m_stats.output = output ? output : "";
	m_stats.connected = IsConnected();
	m_stats.server = *cString::sprintf("%s:%d", m_activeConfig.host.c_str(), m_activeConfig.port);
	m_stats.priority = m_activeConfig.priority;
	m_stats.ledDevice = ControlsLedDevice(m_activeConfig) ? m_ledDeviceState : -1;

	uint64_t elapsed = now - m_statsWindowStartMs;
	if (elapsed >= STATS_INTERVAL_MS) {
		m_stats.imagesPerSecond = (int)((m_statsImages * 1000 + elapsed / 2) / elapsed);
		if (m_statsSamples) {
			m_stats.sampleUsAvg = (int)(m_statsSampleUsSum / m_statsSamples);
			m_stats.sampleUsMax = m_statsSampleUsMax;
		}
		m_statsWindowStartMs = now;
		m_statsImages = 0;
		m_statsSampleUsSum = 0;
		m_statsSampleUsMax = 0;
		m_statsSamples = 0;
	}
}

/**
 * Give the leds back to the lower priorities in Hyperion
 *
 * Clears the priority (video, dark leds or effect) and closes the connection.
 */
void cHyperionClient::ReleaseLeds(void)
{
	if (IsConnected())
		Disconnect(true);
	m_image.clear();
	m_idleSinceMs = 0;
	m_idleShown = false;
	m_idleColorShown = false;
	StopEffect(false);
}

/**
 * Get the brightness of the image (%)
 *
 * While paused, it fades to the brightness of the setup after a short delay,
 * so stepping single frames doesn't dim the leds.
 */
int cHyperionClient::PauseBrightness(uint64_t now) const
{
	int target = m_activeConfig.pauseBrightness;
	if (!m_pausedSinceMs || target >= 100)
		return 100;

	uint64_t paused = now - m_pausedSinceMs;
	if (paused < PAUSE_DIM_DELAY_MS)
		return 100;

	uint64_t fade = paused - PAUSE_DIM_DELAY_MS;
	if (fade >= PAUSE_DIM_FADE_MS)
		return target;

	return 100 - (int)((100 - target) * fade / PAUSE_DIM_FADE_MS);
}

/**
 * Send the current image with the current brightness
 */
bool cHyperionClient::SendImage(void)
{
	if (m_brightness >= 100)
		return Send(cHyperionProtocol::EncodeImage(m_image.data(), m_imageWidth, m_imageHeight));

	m_dimmed.resize(m_image.size());
	for (size_t i = 0; i < m_image.size(); i++)
		m_dimmed[i] = (uint8_t)((m_image[i] * m_brightness + 50) / 100);

	return Send(cHyperionProtocol::EncodeImage(m_dimmed.data(), m_imageWidth, m_imageHeight));
}

/**
 * Open a TCP connection
 *
 * The connection is non-blocking with a timeout.
 *
 * @param host          host name or address
 * @param port          port
 * @param timeoutMs     max. time to connect
 * @return              the (non-blocking) socket or -1 (errno is set)
 */
int cHyperionClient::OpenSocket(const std::string &host, int port, int timeoutMs)
{
	struct addrinfo hints = {};
	struct addrinfo *result = nullptr;
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	cString portStr = cString::sprintf("%d", port);
	int err = getaddrinfo(host.c_str(), *portStr, &hints, &result);
	if (err) {
		dsyslog("hyperion: cannot resolve %s: %s", host.c_str(), gai_strerror(err));
		errno = EHOSTUNREACH;
		return -1;
	}

	int sock = -1;
	for (struct addrinfo *ai = result; ai && sock < 0; ai = ai->ai_next) {
		int fd = socket(ai->ai_family, ai->ai_socktype | SOCK_NONBLOCK | SOCK_CLOEXEC, ai->ai_protocol);
		if (fd < 0)
			continue;

		if (connect(fd, ai->ai_addr, ai->ai_addrlen) < 0) {
			if (errno != EINPROGRESS) {
				close(fd);
				continue;
			}

			struct pollfd pfd = { fd, POLLOUT, 0 };
			int soError = 0;
			socklen_t len = sizeof(soError);
			if (poll(&pfd, 1, timeoutMs) != 1 ||
			    getsockopt(fd, SOL_SOCKET, SO_ERROR, &soError, &len) < 0 || soError) {
				close(fd);
				errno = soError ? soError : ETIMEDOUT;
				continue;
			}
		}

		int one = 1;
		setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof(one));
		sock = fd;
	}
	freeaddrinfo(result);

	return sock;
}

/**
 * Connect to the flatbuffers server and register the input
 *
 * Failed attempts are repeated with an increasing delay.
 */
bool cHyperionClient::Connect(void)
{
	m_reconnectAtMs = NowMs() + m_reconnectDelayMs;
	m_reconnectDelayMs = std::min(m_reconnectDelayMs * 2, RECONNECT_MAX_MS);

	m_socket = OpenSocket(m_activeConfig.host, m_activeConfig.port, CONNECT_TIMEOUT_MS);
	if (m_socket < 0) {
		int err = errno;
		if (!m_connectErrorLogged)
			esyslog("hyperion: cannot connect to %s:%d (%d): %s, retrying in the background",
				m_activeConfig.host.c_str(), m_activeConfig.port, err, strerror(err));
		m_connectErrorLogged = true;
		SetError(*cString::sprintf("cannot connect to %s:%d (%s)", m_activeConfig.host.c_str(), m_activeConfig.port, strerror(err)));
		return false;
	}

	m_rxBuffer.clear();
	m_lastServerError.clear();
	m_imageSent = false;

	if (!Send(cHyperionProtocol::EncodeRegister(ORIGIN, m_activeConfig.priority)))
		return false;

	isyslog("hyperion: connected to %s:%d with priority %d", m_activeConfig.host.c_str(), m_activeConfig.port, m_activeConfig.priority);
	m_reconnectDelayMs = RECONNECT_MIN_MS;
	m_connectErrorLogged = false;

	return true;
}

/**
 * Close the connection
 *
 * @param clear    clear the priority in Hyperion before
 */
void cHyperionClient::Disconnect(bool clear)
{
	if (!IsConnected())
		return;

	if (clear && m_imageSent) {
		std::vector<uint8_t> msg = cHyperionProtocol::EncodeClear(m_activeConfig.priority);
		send(m_socket, msg.data(), msg.size(), MSG_NOSIGNAL | MSG_DONTWAIT);
	}

	close(m_socket);
	m_socket = -1;
	m_imageSent = false;
	m_rxBuffer.clear();

	dsyslog("hyperion: disconnected from %s:%d", m_activeConfig.host.c_str(), m_activeConfig.port);
}

/**
 * Send a message
 *
 * Waits at most SEND_TIMEOUT_MS. A message which can't be sent completely
 * would break the stream, so the connection is closed then.
 */
bool cHyperionClient::Send(const std::vector<uint8_t> &msg)
{
	size_t sent = 0;
	uint64_t timeout = NowMs() + SEND_TIMEOUT_MS;

	while (sent < msg.size()) {
		ssize_t n = send(m_socket, msg.data() + sent, msg.size() - sent, MSG_NOSIGNAL | MSG_DONTWAIT);
		if (n > 0) {
			sent += n;
			continue;
		}

		if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
			int err = errno;
			esyslog("hyperion: send to %s:%d failed (%d): %s", m_activeConfig.host.c_str(), m_activeConfig.port, err, strerror(err));
			SetError(*cString::sprintf("send failed (%s)", strerror(err)));
			Disconnect(false);
			return false;
		}

		// the server doesn't read fast enough, wait a bit
		int64_t left = (int64_t)(timeout - NowMs());
		struct pollfd pfd = { m_socket, POLLOUT, 0 };
		if (left <= 0 || poll(&pfd, 1, left) <= 0) {
			esyslog("hyperion: send to %s:%d timed out", m_activeConfig.host.c_str(), m_activeConfig.port);
			SetError("send timed out");
			Disconnect(false);
			return false;
		}
	}

	return true;
}

/**
 * Read and decode the replies of the server
 *
 * The replies have to be read, otherwise the server blocks at some point.
 * Errors are logged once.
 */
void cHyperionClient::ReceiveReplies(void)
{
	uint8_t buf[1024];

	for (;;) {
		ssize_t n = recv(m_socket, buf, sizeof(buf), MSG_DONTWAIT);
		if (n > 0) {
			m_rxBuffer.insert(m_rxBuffer.end(), buf, buf + n);
			continue;
		}

		if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR)) {
			esyslog("hyperion: connection to %s:%d closed by the server", m_activeConfig.host.c_str(), m_activeConfig.port);
			SetError("connection closed by the server");
			Disconnect(false);
			m_reconnectAtMs = NowMs() + m_reconnectDelayMs;
			return;
		}
		break;
	}

	size_t pos = 0;
	while (m_rxBuffer.size() - pos >= 4) {
		const uint8_t *p = m_rxBuffer.data() + pos;
		uint32_t size = ((uint32_t)p[0] << 24) | (p[1] << 16) | (p[2] << 8) | p[3];

		if (size > 64 * 1024) {
			esyslog("hyperion: invalid reply from %s:%d", m_activeConfig.host.c_str(), m_activeConfig.port);
			SetError("invalid reply from the server");
			Disconnect(false);
			return;
		}
		if (m_rxBuffer.size() - pos - 4 < size)
			break;

		std::string error;
		int video, registered;
		if (cHyperionProtocol::DecodeReply(p + 4, size, &error, &video, &registered) && !error.empty()) {
			if (error != m_lastServerError) {
				esyslog("hyperion: server error: %s", error.c_str());
				SetError(error);
			}
			m_lastServerError = error;
		}

		pos += 4 + size;
	}

	m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + pos);
}
