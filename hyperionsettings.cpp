// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file hyperionsettings.cpp
 * Hyperion Settings
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include <algorithm>
#include <cmath>

#include <nlohmann/json.hpp>

#include "hyperionsettings.h"

using json = nlohmann::json;

namespace {

/// names of the black border modes in the configuration of Hyperion
const char *const BorderModes[sHyperionSettings::BORDER_MODES] = { "default", "classic", "osd", "letterbox" };

// The configuration comes from another program: nothing may throw, a missing
// or wrong value gives the default.

int GetInt(const json &o, const char *key, int def)
{
	auto it = o.find(key);
	if (it == o.end() || !it->is_number())
		return def;
	double d = it->get<double>();
	return d < -1e9 || d > 1e9 ? def : (int)std::lround(d);
}

bool GetBool(const json &o, const char *key, bool def)
{
	auto it = o.find(key);
	return it != o.end() && it->is_boolean() ? it->get<bool>() : def;
}

std::string GetString(const json &o, const char *key, const std::string &def)
{
	auto it = o.find(key);
	return it != o.end() && it->is_string() ? it->get<std::string>() : def;
}

const json *GetObject(const json &o, const char *key)
{
	auto it = o.find(key);
	return it != o.end() && it->is_object() ? &*it : nullptr;
}

const json *GetArray(const json &o, const char *key)
{
	auto it = o.find(key);
	return it != o.end() && it->is_array() ? &*it : nullptr;
}

/// the instance, whose settings are shown: the first enabled one
const json *ShownInstance(const json &instances)
{
	for (const auto &instance : instances)
		if (instance.is_object() && GetBool(instance, "enabled", true))
			return &instance;
	return !instances.empty() && instances[0].is_object() ? &instances[0] : nullptr;
}

std::string Line(const json &request)
{
	return request.dump(-1, ' ', false, json::error_handler_t::replace) + "\n";
}

} // namespace

/**
 * Name of a black border mode (in the configuration of Hyperion)
 */
const char *sHyperionSettings::BorderModeName(int mode)
{
	return mode >= 0 && mode < BORDER_MODES ? BorderModes[mode] : BorderModes[BORDER_DEFAULT];
}

/**
 * Request of the configuration of all instances
 */
std::string cHyperionSettingsJson::GetConfig(int tan)
{
	return Line({
		{ "command", "config" },
		{ "subcommand", "getconfig" },
		{ "configFilter", {
			{ "global", nullptr },
			{ "instances", { { "ids", json::array() }, { "types", { "smoothing", "blackborderdetector", "color" } } } },
		} },
		{ "tan", tan },
	});
}

/**
 * Read the settings from the reply of GetConfig()
 *
 * @param reply         the reply (one line)
 * @param[out] config   the configuration of the instances (for SetConfig())
 * @param[out] settings the settings of the first enabled instance
 * @param[out] error    the error
 * @return              true on success
 */
bool cHyperionSettingsJson::Read(const std::string &reply, std::string *config, sHyperionSettings *settings, std::string *error)
{
	json root = json::parse(reply, nullptr, false);
	if (!root.is_object()) {
		*error = "invalid reply";
		return false;
	}

	if (!GetBool(root, "success", false)) {
		std::string text = GetString(root, "error", "");
		if (text == "No Authorization")
			*error = "no authorization, a token is needed";
		else
			*error = text.empty() ? "reading the configuration failed" : text;
		return false;
	}

	const json *info = GetObject(root, "info");
	const json *instances = info ? GetArray(*info, "instances") : nullptr;
	if (!instances) {
		*error = "unknown configuration format (Hyperion 2.1 or newer is needed)";
		return false;
	}

	const json *instance = ShownInstance(*instances);
	const json *s = instance ? GetObject(*instance, "settings") : nullptr;
	if (!s) {
		*error = "no instance found";
		return false;
	}

	sHyperionSettings result;
	if (const json *o = GetObject(*s, "smoothing")) {
		result.smoothing = GetBool(*o, "enable", result.smoothing);
		result.smoothingTimeMs = GetInt(*o, "time_ms", result.smoothingTimeMs);
		result.smoothingDelay = GetInt(*o, "updateDelay", result.smoothingDelay);
		result.smoothingFrequency = std::max(1, GetInt(*o, "updateFrequency", result.smoothingFrequency));
	}
	if (const json *o = GetObject(*s, "blackborderdetector")) {
		result.blackBorder = GetBool(*o, "enable", result.blackBorder);
		result.blackBorderThreshold = GetInt(*o, "threshold", result.blackBorderThreshold);
		std::string name = GetString(*o, "mode", "default");
		for (int i = 0; i < sHyperionSettings::BORDER_MODES; i++)
			if (name == BorderModes[i])
				result.blackBorderMode = i;
	}
	if (const json *o = GetObject(*s, "color")) {
		const json *adjustments = GetArray(*o, "channelAdjustment");
		if (adjustments && !adjustments->empty() && (*adjustments)[0].is_object())
			result.brightness = GetInt((*adjustments)[0], "brightness", result.brightness);
	}

	*config = info->dump(-1, ' ', false, json::error_handler_t::replace);
	*settings = result;
	return true;
}

/**
 * Request to write the changed settings
 *
 * Only the changed values are written: an instance keeps its own values of
 * the other settings.
 *
 * @param config    the configuration, as Read() returned it
 * @param from      the settings, as Read() returned them
 * @param to        the changed settings
 * @param tan       the tan of the request
 * @return          the request (empty, if nothing has changed)
 */
std::string cHyperionSettingsJson::SetConfig(const std::string &config, const sHyperionSettings &from, const sHyperionSettings &to, int tan)
{
	json info = json::parse(config, nullptr, false);
	const json *instances = info.is_object() ? GetArray(info, "instances") : nullptr;
	if (!instances)
		return "";

	json changed = json::array();

	for (const auto &instance : *instances) {
		if (!instance.is_object() || !instance.contains("id"))
			continue;
		const json *s = GetObject(instance, "settings");
		if (!s)
			continue;

		json settings = json::object();

		if (const json *o = GetObject(*s, "smoothing")) {
			json smoothing = *o;
			if (from.smoothing != to.smoothing)
				smoothing["enable"] = to.smoothing;
			if (from.smoothingTimeMs != to.smoothingTimeMs)
				smoothing["time_ms"] = std::clamp(to.smoothingTimeMs, 1, 5000);
			if (from.smoothingDelay != to.smoothingDelay)
				smoothing["updateDelay"] = std::clamp(to.smoothingDelay, 0, 2048);
			if (smoothing != *o)
				settings["smoothing"] = smoothing;
		}

		if (const json *o = GetObject(*s, "blackborderdetector")) {
			json border = *o;
			if (from.blackBorder != to.blackBorder)
				border["enable"] = to.blackBorder;
			if (from.blackBorderThreshold != to.blackBorderThreshold)
				border["threshold"] = std::clamp(to.blackBorderThreshold, 0, 100);
			if (from.blackBorderMode != to.blackBorderMode)
				border["mode"] = sHyperionSettings::BorderModeName(to.blackBorderMode);
			if (border != *o)
				settings["blackborderdetector"] = border;
		}

		const json *o = GetObject(*s, "color");
		const json *adjustments = o ? GetArray(*o, "channelAdjustment") : nullptr;
		if (adjustments && !adjustments->empty() && from.brightness != to.brightness) {
			// every adjustment gets the brightness (usually there is only one)
			json color = *o;
			for (auto &a : color["channelAdjustment"])
				if (a.is_object())
					a["brightness"] = std::clamp(to.brightness, 0, 100);
			settings["color"] = color;
		}

		if (settings.empty())
			continue;

		changed.push_back({ { "id", instance["id"] }, { "settings", settings } });
	}

	if (changed.empty())
		return "";

	return Line({ { "command", "config" }, { "subcommand", "setconfig" }, { "config", { { "instances", changed } } }, { "tan", tan } });
}
