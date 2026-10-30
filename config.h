// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file config.h
 * Plugin Configuration Header File
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __HYPERION_CONFIG_H
#define __HYPERION_CONFIG_H

#include <string>

/**
 * Settings of the plugin (setup.conf)
 */
struct sHyperionConfig {
	/// what the switch off (hotkey, menu, SVDRP) does
	enum eOffAction {
		OFF_LEDDEVICE = 0,              ///< switch the led device of Hyperion off
		OFF_VIDEO = 1,                  ///< only stop sending the video (Hyperion shows the lower priorities)
	};
	/// what is shown, if no video is displayed (e.g. radio)
	enum eIdleMode {
		IDLE_RELEASE = 0,               ///< give the leds back (Hyperion shows the lower priorities)
		IDLE_EFFECT = 1,                ///< start an effect
		IDLE_BLACK = 2,                 ///< dark leds
	};

	bool enable = true;                 ///< send the video to Hyperion
	std::string host = "127.0.0.1";     ///< host of the Hyperion server
	int port = 19400;                   ///< port of the flatbuffers server
	int priority = 150;                 ///< priority of the input (100 - 199, lower values win)
	int fps = 25;                       ///< max. images per second
	int width = 80;                     ///< width of the sent image (the height follows the screen aspect)
	int jsonPort = 19444;               ///< port of the JSON server (control)
	std::string token;                  ///< authorization token of the JSON server (empty: none)
	int offAction = OFF_LEDDEVICE;      ///< see eOffAction
	bool ledsFollowAttach = false;      ///< switch the led device on with an attach, off with a detach and at the end
	int idleMode = IDLE_RELEASE;        ///< see eIdleMode
	std::string idleEffect = "Rainbow swirl"; ///< effect for IDLE_EFFECT
	bool hideMainMenuEntry = false;     ///< hide the main menu entry (the switch on/off)

	bool Parse(const char *name, const char *value);

	bool operator==(const sHyperionConfig &o) const {
		return enable == o.enable && host == o.host && port == o.port &&
		       priority == o.priority && fps == o.fps && width == o.width &&
		       jsonPort == o.jsonPort && token == o.token && offAction == o.offAction &&
		       ledsFollowAttach == o.ledsFollowAttach && idleMode == o.idleMode && idleEffect == o.idleEffect;
	}
	bool operator!=(const sHyperionConfig &o) const { return !(*this == o); }
};

#endif
