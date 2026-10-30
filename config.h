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
	bool enable = true;                 ///< send the video to Hyperion
	std::string host = "127.0.0.1";     ///< host of the Hyperion server
	int port = 19400;                   ///< port of the flatbuffers server
	int priority = 150;                 ///< priority of the input (100 - 199, lower values win)
	int fps = 25;                       ///< max. images per second
	int width = 80;                     ///< width of the sent image (the height follows the screen aspect)

	bool Parse(const char *name, const char *value);

	bool operator==(const sHyperionConfig &o) const {
		return enable == o.enable && host == o.host && port == o.port &&
		       priority == o.priority && fps == o.fps && width == o.width;
	}
	bool operator!=(const sHyperionConfig &o) const { return !(*this == o); }
};

#endif
