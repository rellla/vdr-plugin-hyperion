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

#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <vdr/thread.h>

#include "config.h"

class cPlugin;

/**
 * Hyperion Client
 *
 * Gets a small image of the displayed video from the output plugin
 * (service AmbientImage-v1.0) and sends it to the flatbuffers server of
 * Hyperion (ambient lighting). Everything runs in the own (low priority)
 * thread: the service call waits for the next frame of the video and reads
 * the image from it in this thread.
 */
class cHyperionClient : public cThread {
public:
	cHyperionClient(void);
	virtual ~cHyperionClient(void);

	void Configure(const sHyperionConfig &);
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
	static constexpr const char *ORIGIN = "VDR";       ///< name of the input in Hyperion

	// configuration (written by the setup, read by the thread)
	std::mutex m_mutex;                                ///< protects m_config and m_changed
	std::condition_variable m_cond;                    ///< signals a change
	sHyperionConfig m_config;                          ///< current configuration
	bool m_changed = false;                            ///< the configuration or a state has changed

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

	void Wait(int);
	bool FindProvider(uint64_t);
	bool Connect(void);
	void Disconnect(bool);
	void ReleaseLeds(void);
	static int OpenSocket(const std::string &, int, int);
	bool IsConnected(void) const { return m_socket >= 0; }
	bool Send(const std::vector<uint8_t> &);
	bool SendImage(void);
	void ReceiveReplies(void);
};

#endif
