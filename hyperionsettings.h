// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file hyperionsettings.h
 * Hyperion Settings Header File
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __HYPERION_SETTINGS_H
#define __HYPERION_SETTINGS_H

#include <string>

/**
 * Some settings of Hyperion, which can be changed in the setup of the plugin
 *
 * They are stored in the configuration of Hyperion, not in VDR's setup.conf.
 */
struct sHyperionSettings {
	/// mode of the black border detection
	enum eBorderMode {
		BORDER_DEFAULT,
		BORDER_CLASSIC,
		BORDER_OSD,
		BORDER_LETTERBOX,
		BORDER_MODES,
	};

	bool smoothing = true;              ///< smooth the colors of the leds
	int smoothingTimeMs = 150;          ///< time to reach a new color (1 - 5000 ms)
	int smoothingDelay = 0;             ///< delay of the leds (0 - 2048 frames of the smoothing)
	int smoothingFrequency = 25;        ///< update frequency of the smoothing (Hz, only shown)
	bool blackBorder = true;            ///< detect black borders
	int blackBorderThreshold = 5;       ///< max. brightness of a black border (0 - 100 %)
	int blackBorderMode = BORDER_DEFAULT; ///< see eBorderMode
	int brightness = 100;               ///< brightness of the leds (0 - 100 %)

	static const char *BorderModeName(int);

	bool operator==(const sHyperionSettings &o) const {
		return smoothing == o.smoothing && smoothingTimeMs == o.smoothingTimeMs && smoothingDelay == o.smoothingDelay &&
		       blackBorder == o.blackBorder && blackBorderThreshold == o.blackBorderThreshold &&
		       blackBorderMode == o.blackBorderMode && brightness == o.brightness;
	}
	bool operator!=(const sHyperionSettings &o) const { return !(*this == o); }
};

/**
 * Reads and writes the settings with the JSON server of Hyperion
 *
 * The configuration is read with "config getconfig" and written with
 * "config setconfig" (Hyperion 2.1 or newer, both need a login with a token).
 * Hyperion replaces a whole section (e.g. "smoothing") with setconfig, so the
 * changed values are put into the sections, as they were read, and only the
 * changed sections are sent. They are written into every instance of Hyperion,
 * all of them get the video.
 */
class cHyperionSettingsJson {
public:
	static std::string GetConfig(int tan);
	static bool Read(const std::string &reply, std::string *config, sHyperionSettings *settings, std::string *error);
	static std::string SetConfig(const std::string &config, const sHyperionSettings &from, const sHyperionSettings &to, int tan);
};

#endif
