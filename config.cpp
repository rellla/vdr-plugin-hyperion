// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file config.cpp
 * Plugin Configuration
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <algorithm>
#include <cstdlib>
#include <strings.h>

#include "config.h"

/**
 * Parse a setup.conf parameter
 *
 * @return  true, if the parameter is known
 */
bool sHyperionConfig::Parse(const char *name, const char *value)
{
	if      (!strcasecmp(name, "Enable"))   enable = atoi(value);
	else if (!strcasecmp(name, "Host"))     host = *value ? value : "127.0.0.1";
	else if (!strcasecmp(name, "Port"))     port = std::clamp(atoi(value), 1, 65535);
	else if (!strcasecmp(name, "Priority")) priority = std::clamp(atoi(value), 100, 199);
	else if (!strcasecmp(name, "Fps"))      fps = std::clamp(atoi(value), 1, 50);
	else if (!strcasecmp(name, "Width"))    width = std::clamp(atoi(value), 16, 320);
	else if (!strcasecmp(name, "JsonPort")) jsonPort = std::clamp(atoi(value), 1, 65535);
	else if (!strcasecmp(name, "Token"))    token = value;
	else if (!strcasecmp(name, "OffAction")) offAction = std::clamp(atoi(value), 0, 1);
	else if (!strcasecmp(name, "LedsFollowAttach")) ledsFollowAttach = atoi(value);
	else if (!strcasecmp(name, "IdleMode")) idleMode = std::clamp(atoi(value), 0, 2);
	else if (!strcasecmp(name, "IdleEffect")) idleEffect = *value ? value : "Rainbow swirl";
	else if (!strcasecmp(name, "HideMainMenuEntry")) hideMainMenuEntry = atoi(value);
	else if (!strcasecmp(name, "PauseBrightness")) pauseBrightness = std::clamp(atoi(value), 0, 100);
	else
		return false;

	return true;
}
