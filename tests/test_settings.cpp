/**
 * @file test_settings.cpp
 * Unit tests for cHyperionSettingsJson
 *
 * The replies follow the replies of Hyperion 2.1 (JsonAPI::handleConfigGetCommand,
 * DBConfigManager::getConfiguration).
 */

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "hyperionsettings.h"

namespace {

// reply of getconfig with two instances, the first one is disabled
const char *kReply =
	"{\"command\":\"config-getconfig\",\"info\":{\"instanceIds\":[0,1],\"instances\":["
	"{\"enabled\":false,\"id\":0,\"name\":\"First LED Hardware instance\",\"settings\":{"
	"\"blackborderdetector\":{\"blurRemoveCnt\":1,\"borderFrameCnt\":50,\"enable\":true,\"maxInconsistentCnt\":10,"
	"\"mode\":\"default\",\"threshold\":5,\"unknownFrameCnt\":600},"
	"\"color\":{\"channelAdjustment\":[{\"brightness\":100,\"id\":\"default\",\"leds\":\"*\"}],\"imageToLedMappingType\":\"multicolor_mean\"},"
	"\"smoothing\":{\"decay\":1,\"dithering\":false,\"enable\":true,\"interpolationRate\":25,\"time_ms\":150,"
	"\"type\":\"linear\",\"updateDelay\":0,\"updateFrequency\":25}}},"
	"{\"enabled\":true,\"id\":1,\"name\":\"Second instance\",\"settings\":{"
	"\"blackborderdetector\":{\"enable\":false,\"mode\":\"letterbox\",\"threshold\":12},"
	"\"color\":{\"channelAdjustment\":[{\"brightness\":80,\"id\":\"a\"},{\"brightness\":60,\"id\":\"b\"}]},"
	"\"smoothing\":{\"enable\":false,\"time_ms\":300,\"updateDelay\":4,\"updateFrequency\":40.5}}}"
	"]},\"instance\":0,\"success\":true,\"tan\":7}";

} // namespace

TEST_CASE("HyperionSettings: read the configuration", "[hyperion]")
{
	std::string config;
	sHyperionSettings settings;
	std::string error;

	SECTION("the first enabled instance is shown")
	{
		REQUIRE(cHyperionSettingsJson::Read(kReply, &config, &settings, &error));
		CHECK_FALSE(settings.smoothing);
		CHECK(settings.smoothingTimeMs == 300);
		CHECK(settings.smoothingDelay == 4);
		CHECK(settings.smoothingFrequency == 41);
		CHECK_FALSE(settings.blackBorder);
		CHECK(settings.blackBorderThreshold == 12);
		CHECK(settings.blackBorderMode == sHyperionSettings::BORDER_LETTERBOX);
		CHECK(settings.brightness == 80);
	}

	SECTION("no authorization")
	{
		CHECK_FALSE(cHyperionSettingsJson::Read("{\"command\":\"config-getconfig\",\"error\":\"No Authorization\",\"success\":false,\"tan\":3}",
		                                        &config, &settings, &error));
		CHECK(error == "no authorization, a token is needed");
	}

	SECTION("old configuration format")
	{
		CHECK_FALSE(cHyperionSettingsJson::Read("{\"command\":\"config-getconfig\",\"info\":{\"smoothing\":{}},\"success\":true,\"tan\":3}",
		                                        &config, &settings, &error));
		CHECK(error.find("2.1") != std::string::npos);
	}

	SECTION("invalid reply")
	{
		CHECK_FALSE(cHyperionSettingsJson::Read("{\"success\":true", &config, &settings, &error));
		CHECK(error.find("invalid reply") == 0);
	}
}

TEST_CASE("HyperionSettings: write the changed settings", "[hyperion]")
{
	std::string config;
	sHyperionSettings from;
	std::string error;
	REQUIRE(cHyperionSettingsJson::Read(kReply, &config, &from, &error));

	SECTION("nothing changed")
	{
		CHECK(cHyperionSettingsJson::SetConfig(config, from, from, 1).empty());
	}

	SECTION("only the changed values are written into every instance")
	{
		sHyperionSettings to = from;
		to.smoothingTimeMs = 200;
		to.blackBorderMode = sHyperionSettings::BORDER_OSD;

		std::string request = cHyperionSettingsJson::SetConfig(config, from, to, 9);
		CHECK(request ==
			"{\"command\":\"config\",\"config\":{\"instances\":["
			"{\"id\":0,\"settings\":{"
			"\"blackborderdetector\":{\"blurRemoveCnt\":1,\"borderFrameCnt\":50,\"enable\":true,\"maxInconsistentCnt\":10,"
			"\"mode\":\"osd\",\"threshold\":5,\"unknownFrameCnt\":600},"
			"\"smoothing\":{\"decay\":1,\"dithering\":false,\"enable\":true,\"interpolationRate\":25,\"time_ms\":200,"
			"\"type\":\"linear\",\"updateDelay\":0,\"updateFrequency\":25}}},"
			"{\"id\":1,\"settings\":{"
			"\"blackborderdetector\":{\"enable\":false,\"mode\":\"osd\",\"threshold\":12},"
			"\"smoothing\":{\"enable\":false,\"time_ms\":200,\"updateDelay\":4,\"updateFrequency\":40.5}}}"
			"]},\"subcommand\":\"setconfig\",\"tan\":9}\n");
	}

	SECTION("brightness is written into every adjustment")
	{
		sHyperionSettings to = from;
		to.brightness = 50;

		std::string request = cHyperionSettingsJson::SetConfig(config, from, to, 2);
		CHECK(request ==
			"{\"command\":\"config\",\"config\":{\"instances\":["
			"{\"id\":0,\"settings\":{\"color\":{\"channelAdjustment\":[{\"brightness\":50,\"id\":\"default\",\"leds\":\"*\"}],"
			"\"imageToLedMappingType\":\"multicolor_mean\"}}},"
			"{\"id\":1,\"settings\":{\"color\":{\"channelAdjustment\":[{\"brightness\":50,\"id\":\"a\"},{\"brightness\":50,\"id\":\"b\"}]}}}"
			"]},\"subcommand\":\"setconfig\",\"tan\":2}\n");
	}

	SECTION("an instance, which has the value already, isn't written")
	{
		sHyperionSettings to = from;
		to.smoothing = true;     // instance 0 is on already

		std::string request = cHyperionSettingsJson::SetConfig(config, from, to, 4);
		CHECK(request ==
			"{\"command\":\"config\",\"config\":{\"instances\":["
			"{\"id\":1,\"settings\":{\"smoothing\":{\"enable\":true,\"time_ms\":300,\"updateDelay\":4,\"updateFrequency\":40.5}}}"
			"]},\"subcommand\":\"setconfig\",\"tan\":4}\n");
	}

	SECTION("wrong types in the configuration don't throw")
	{
		std::string reply = "{\"info\":{\"instances\":[{\"id\":0,\"settings\":{\"smoothing\":{\"enable\":\"yes\",\"time_ms\":\"x\"},"
		                    "\"blackborderdetector\":[],\"color\":{\"channelAdjustment\":[1]}}}]},\"success\":true,\"tan\":1}";
		sHyperionSettings s;
		REQUIRE(cHyperionSettingsJson::Read(reply, &config, &s, &error));
		CHECK(s.smoothing);
		CHECK(s.smoothingTimeMs == 150);
		CHECK(s.brightness == 100);
		sHyperionSettings to = s;
		to.brightness = 10;
		to.blackBorder = false;
		CHECK(cHyperionSettingsJson::SetConfig(config, s, to, 1) ==
			"{\"command\":\"config\",\"config\":{\"instances\":[{\"id\":0,\"settings\":{\"color\":{\"channelAdjustment\":[1]}}}]},"
			"\"subcommand\":\"setconfig\",\"tan\":1}\n");
		CHECK(cHyperionSettingsJson::SetConfig("[", s, to, 1).empty());
	}

	SECTION("values are limited")
	{
		sHyperionSettings to = from;
		to.smoothingTimeMs = 99999;
		std::string request = cHyperionSettingsJson::SetConfig(config, from, to, 4);
		CHECK(request.find("\"time_ms\":5000") != std::string::npos);
	}
}
