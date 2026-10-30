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
		sHyperionConfig config;
		{
			std::lock_guard<std::mutex> lock(m_mutex);
			config = m_config;
			m_changed = false;
		}

		// reconnect with the new settings
		if (config != m_activeConfig) {
			if (IsConnected())
				Disconnect(true);
			m_activeConfig = config;
			m_reconnectAtMs = 0;
			m_reconnectDelayMs = RECONNECT_MIN_MS;
			m_connectErrorLogged = false;
		}

		uint64_t now = NowMs();

		if (!config.enable || !FindProvider(now)) {
			ReleaseLeds();
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
		req.timeoutMs = SERVICE_WAIT_MS;
		req.lastFrame = m_lastFrame;
		req.rgb = m_rgb.data();
		req.rgbSize = m_rgb.size();

		if (!m_pProvider->Service(AMBIENTIMAGE_SERVICE, &req)) {
			esyslog("hyperion: the output plugin rejected the request");
			m_pProvider = nullptr;
			m_providerRetryAtMs = NowMs() + PROVIDER_RETRY_MS;
			continue;
		}
		now = NowMs();

		if (req.flags & AMBIENT_DETACHED) {
			// no video can be displayed: give the leds back at once
			ReleaseLeds();
			m_lastFrame = 0;
			Wait(250);
			continue;
		}

		if (req.flags & AMBIENT_NEW_IMAGE) {
			m_idleSinceMs = 0;
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
			}
		} else if (!(req.flags & AMBIENT_VIDEO) && !m_idleSinceMs) {
			m_idleSinceMs = now;
		}

		// No video is displayed anymore (e.g. radio), give the leds back. A channel switch
		// shows no video for a moment, too: meanwhile the last image is kept.
		if (m_idleSinceMs && now - m_idleSinceMs >= IDLE_RELEASE_MS)
			ReleaseLeds();

		if (IsConnected()) {
			ReceiveReplies();

			// keep the connection and the image alive (paused video)
			if (IsConnected() && m_imageSent && !m_image.empty() && now - m_lastSendMs >= KEEPALIVE_MS) {
				if (SendImage())
					m_lastSendMs = now;
			}
		}
	}

	Disconnect(true);
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
		if (!m_providerErrorLogged)
			esyslog("hyperion: no output plugin provides the service %s (e.g. drmdevice)", AMBIENTIMAGE_SERVICE);
		m_providerErrorLogged = true;
		m_providerRetryAtMs = now + PROVIDER_RETRY_MS;
		return false;
	}

	isyslog("hyperion: the plugin %s provides the image of the video", m_pProvider->Name());
	m_providerErrorLogged = false;
	m_rgb.resize(640 * 640 * 3);
	return true;
}

/**
 * Give the leds back to the lower priorities in Hyperion
 *
 * Clears the priority and closes the connection.
 */
void cHyperionClient::ReleaseLeds(void)
{
	if (IsConnected())
		Disconnect(true);
	m_image.clear();
	m_idleSinceMs = 0;
}

/**
 * Send the current image
 */
bool cHyperionClient::SendImage(void)
{
	return Send(cHyperionProtocol::EncodeImage(m_image.data(), m_imageWidth, m_imageHeight));
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
			Disconnect(false);
			return false;
		}

		// the server doesn't read fast enough, wait a bit
		int64_t left = (int64_t)(timeout - NowMs());
		struct pollfd pfd = { m_socket, POLLOUT, 0 };
		if (left <= 0 || poll(&pfd, 1, left) <= 0) {
			esyslog("hyperion: send to %s:%d timed out", m_activeConfig.host.c_str(), m_activeConfig.port);
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
			Disconnect(false);
			return;
		}
		if (m_rxBuffer.size() - pos - 4 < size)
			break;

		std::string error;
		int video, registered;
		if (cHyperionProtocol::DecodeReply(p + 4, size, &error, &video, &registered) && !error.empty()) {
			if (error != m_lastServerError)
				esyslog("hyperion: server error: %s", error.c_str());
			m_lastServerError = error;
		}

		pos += 4 + size;
	}

	m_rxBuffer.erase(m_rxBuffer.begin(), m_rxBuffer.begin() + pos);
}
