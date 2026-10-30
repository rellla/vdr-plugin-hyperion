// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file client.h
 * Hyperion Client Header File
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __HYPERION_CLIENT_H
#define __HYPERION_CLIENT_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <ctime>
#include <mutex>
#include <string>
#include <vector>

#include <vdr/thread.h>

#include "config.h"

class cPlugin;

/**
 * Statistics of the client (shown in the setup)
 */
struct sHyperionStats {
	/// what the leds show
	enum eState {
		STATE_DISABLED,                 ///< disabled in the setup
		STATE_NO_OUTPUT,                ///< no output plugin provides the image
		STATE_DETACHED,                 ///< the output device is detached
		STATE_OFF,                      ///< switched off by the user
		STATE_VIDEO,                    ///< the video is sent
		STATE_KEPT,                     ///< no video for a moment (e.g. channel switch), the last image is kept
		STATE_RELEASED,                 ///< no video, the leds are given back to Hyperion
		STATE_EFFECT,                   ///< no video, the idle effect is shown
		STATE_DARK,                     ///< no video, the leds are dark
	};

	int state = STATE_DISABLED;         ///< see eState
	std::string output;                 ///< name of the output plugin (empty: none)
	bool connected = false;             ///< connected to the flatbuffers server
	std::string server;                 ///< host:port of the flatbuffers server
	int priority = 0;                   ///< priority of the input
	int ledDevice = -1;                 ///< state of the led device set by the plugin (-1: not switched)
	int imagesPerSecond = 0;            ///< sent video images per second
	std::string format;                 ///< format of the frames (empty: none read yet)
	int width = 0;                      ///< width of the sent image
	int height = 0;                     ///< height of the sent image
	int sampleUsAvg = 0;                ///< average time to read an image from a frame (microseconds)
	int sampleUsMax = 0;                ///< max. time to read an image from a frame (microseconds)
	std::string lastError;              ///< last error (empty: none)
	time_t lastErrorTime = 0;           ///< time of the last error
};

/**
 * Hyperion Client
 *
 * Gets a small image of the displayed video from the output plugin
 * (service AmbientImage-v1.0) and sends it to the flatbuffers server of
 * Hyperion (ambient lighting). Everything runs in the own (low priority)
 * thread: the service call waits for the next frame of the video and reads
 * the image from it in this thread.
 *
 * Hyperion is controlled with its JSON server (short connections, only when
 * something changes): the led device is switched on and off (switch off by
 * the user, attach/detach of the output device) and an effect is started,
 * while no video is shown.
 */
class cHyperionClient : public cThread {
public:
	cHyperionClient(void);
	virtual ~cHyperionClient(void);

	void Configure(const sHyperionConfig &);
	void SetOn(bool);
	bool IsOn(void) const { return m_on; }      ///< false, if the user switched the ambient light off
	void GetStats(sHyperionStats *);
	void Stop(void);

protected:
	virtual void Action(void);

private:
	static constexpr int KEEPALIVE_MS = 1000;          ///< resend the last image, if the video doesn't change (pause)
	static constexpr int CONNECT_TIMEOUT_MS = 1000;    ///< timeout of a connection attempt
	static constexpr int SEND_TIMEOUT_MS = 500;        ///< max. time to send a message
	static constexpr int RECONNECT_MIN_MS = 1000;      ///< first retry after a failed connection
	static constexpr int RECONNECT_MAX_MS = 30000;     ///< max. time between connection attempts
	static constexpr int IDLE_RELEASE_MS = 2000;       ///< give the leds back, if no video is shown for this time (not with every channel switch)
	static constexpr int SERVICE_WAIT_MS = 100;        ///< max. time to wait in the service for a new frame
	static constexpr int INTERVAL_SLACK_MS = 5;        ///< the next frame is requested this time before the interval is over
	static constexpr int PROVIDER_RETRY_MS = 5000;     ///< look for the output plugin again after this time
	static constexpr int JSON_TIMEOUT_MS = 1500;       ///< max. time for a JSON request (connect, send, replies)
	static constexpr int JSON_RETRY_MS = 5000;         ///< retry a failed JSON request after this time
	static constexpr int STATS_INTERVAL_MS = 1000;     ///< interval of the images/s and sampling time statistics
	static constexpr const char *ORIGIN = "VDR";       ///< name of the input in Hyperion

	// configuration (written by the setup, read by the thread)
	std::mutex m_mutex;                                ///< protects m_config and m_changed
	std::condition_variable m_cond;                    ///< signals a change
	sHyperionConfig m_config;                          ///< current configuration
	bool m_changed = false;                            ///< the configuration or a state has changed
	std::atomic<bool> m_on = true;                     ///< the user didn't switch the ambient light off

	// client thread only
	sHyperionConfig m_activeConfig;                    ///< configuration of the current connection
	cPlugin *m_pProvider = nullptr;                    ///< output plugin, which provides the image
	uint64_t m_providerRetryAtMs = 0;                  ///< earliest time to look for the output plugin again
	bool m_providerErrorLogged = false;                ///< the missing output plugin was logged
	int m_socket = -1;                                 ///< socket to the server
	uint64_t m_reconnectAtMs = 0;                      ///< earliest time of the next connection attempt
	int m_reconnectDelayMs = RECONNECT_MIN_MS;         ///< delay of the next connection attempt
	bool m_connectErrorLogged = false;                 ///< a failed connection was already logged
	std::vector<uint8_t> m_rxBuffer;                   ///< received, not yet decoded data
	std::string m_lastServerError;                     ///< last logged error of the server
	std::vector<uint8_t> m_rgb;                        ///< buffer for the service
	std::vector<uint8_t> m_image;                      ///< current rgb image
	int m_imageWidth = 0;                              ///< width of the current image
	int m_imageHeight = 0;                             ///< height of the current image
	unsigned int m_lastFrame = 0;                      ///< number of the last frame
	bool m_imageSent = false;                          ///< an image was sent on this connection
	uint64_t m_lastImageMs = 0;                        ///< time of the last new image
	uint64_t m_lastSendMs = 0;                         ///< time of the last sent message
	uint64_t m_idleSinceMs = 0;                        ///< no video is shown since this time (0: video is shown)
	bool m_idleShown = false;                          ///< the state without video is shown (until the video returns)
	bool m_attached = true;                            ///< the output device is attached (as the service reported)
	bool m_idleColorShown = false;                     ///< the leds are dark (IDLE_BLACK), the color is repeated
	bool m_effectActive = false;                       ///< the idle effect was started (has to be cleared)
	int m_ledDeviceState = -1;                         ///< last state of the led device set by the client (-1: unknown)
	uint64_t m_jsonRetryAtMs = 0;                      ///< earliest time of the next JSON request after an error
	std::string m_lastJsonError;                       ///< last logged JSON error
	int m_jsonTan = 0;                                 ///< last used tan of a JSON request
	std::string m_lastReadError;                       ///< last logged error of the output plugin

	// statistics (written by the client thread, read by the setup)
	std::mutex m_statsMutex;                           ///< protects m_stats
	sHyperionStats m_stats;                            ///< the statistics
	uint64_t m_statsWindowStartMs = 0;                 ///< start of the current statistics interval
	int m_statsImages = 0;                             ///< sent video images in the current interval
	int64_t m_statsSampleUsSum = 0;                    ///< sum of the sampling times in the current interval
	int m_statsSampleUsMax = 0;                        ///< max. sampling time in the current interval
	int m_statsSamples = 0;                            ///< read images in the current interval

	void Wait(int);
	bool FindProvider(uint64_t);
	bool Connect(void);
	void Disconnect(bool);
	void ReleaseLeds(void);
	void ShowIdle(void);
	bool StopEffect(bool);
	void UpdateLedDevice(void);
	bool ControlsLedDevice(const sHyperionConfig &) const;
	bool JsonRequest(const std::vector<std::string> &, bool);
	static int OpenSocket(const std::string &, int, int);
	bool IsConnected(void) const { return m_socket >= 0; }
	bool Send(const std::vector<uint8_t> &);
	bool SendImage(void);
	void ReceiveReplies(void);
	void SetError(const std::string &);
	void UpdateStats(uint64_t);
};

#endif
