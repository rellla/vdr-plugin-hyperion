// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file stats.cpp
 * Statistics
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include "stats.h"

/**
 * Name of a state (for SVDRP STAT)
 */
const char *sHyperionStats::StateName(int state)
{
	switch (state) {
		case STATE_NO_OUTPUT: return "no_output";
		case STATE_DETACHED:  return "detached";
		case STATE_OFF:       return "off";
		case STATE_VIDEO:     return "video";
		case STATE_PAUSED:    return "paused";
		case STATE_KEPT:      return "kept";
		case STATE_RELEASED:  return "released";
		case STATE_EFFECT:    return "effect";
		case STATE_DARK:      return "dark";
		case STATE_TEST:      return "test";
		default:              return "disabled";
	}
}

/**
 * The statistics as "key: value" lines (SVDRP STAT)
 *
 * The keys don't change, so scripts can read them. A value never contains a
 * line break.
 *
 * @param on    the user didn't switch the ambient light off
 */
std::string sHyperionStats::Format(bool on) const
{
	auto line = [](const char *key, const std::string &value) {
		std::string v = value;
		for (char &c : v)
			if (c == '\n' || c == '\r')
				c = ' ';
		return std::string(key) + ": " + v + "\n";
	};

	std::string out;
	out += line("state", StateName(state));
	out += line("on", on ? "1" : "0");
	out += line("output", output);
	out += line("server", server);
	out += line("connected", connected ? "1" : "0");
	out += line("priority", std::to_string(priority));
	out += line("leds", ledDevice < 0 ? "-" : ledDevice ? "on" : "off");
	out += line("images_per_second", std::to_string(imagesPerSecond));
	out += line("format", format);
	out += line("image", width > 0 ? std::to_string(width) + "x" + std::to_string(height) : "");
	out += line("sample_us", std::to_string(sampleUsAvg));
	out += line("sample_us_max", std::to_string(sampleUsMax));
	out += line("error", lastError);
	out += line("error_time", lastError.empty() ? "" : std::to_string((long long)lastErrorTime));

	out.pop_back();     // VDR adds the last line break
	return out;
}
