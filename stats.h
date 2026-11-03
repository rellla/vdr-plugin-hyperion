// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file stats.h
 * Statistics Header File
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __HYPERION_STATS_H
#define __HYPERION_STATS_H

#include <ctime>
#include <string>

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
		STATE_PAUSED,                   ///< the video is paused (and dimmed)
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

	static const char *StateName(int);
	std::string Format(bool on) const;
};

#endif
