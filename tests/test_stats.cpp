/**
 * @file test_stats.cpp
 * Unit tests for sHyperionStats
 */

#include <catch2/catch_test_macros.hpp>

#include <string>

#include "stats.h"

TEST_CASE("HyperionStats: SVDRP STAT", "[hyperion]")
{
	sHyperionStats stats;

	SECTION("video")
	{
		stats.state = sHyperionStats::STATE_VIDEO;
		stats.output = "drmdevice";
		stats.connected = true;
		stats.server = "127.0.0.1:19400";
		stats.priority = 150;
		stats.ledDevice = 1;
		stats.imagesPerSecond = 25;
		stats.format = "NV12/SAND128 1920x1080";
		stats.width = 80;
		stats.height = 45;
		stats.sampleUsAvg = 210;
		stats.sampleUsMax = 480;
		CHECK(stats.Format(true) ==
			"state: video\n"
			"on: 1\n"
			"output: drmdevice\n"
			"server: 127.0.0.1:19400\n"
			"connected: 1\n"
			"priority: 150\n"
			"leds: on\n"
			"images_per_second: 25\n"
			"format: NV12/SAND128 1920x1080\n"
			"image: 80x45\n"
			"sample_us: 210\n"
			"sample_us_max: 480\n"
			"error: \n"
			"error_time: ");
	}

	SECTION("error without line breaks")
	{
		stats.state = sHyperionStats::STATE_OFF;
		stats.lastError = "a\nb";
		stats.lastErrorTime = 1762200000;
		std::string text = stats.Format(false);
		CHECK(text.find("state: off\non: 0\n") == 0);
		CHECK(text.find("leds: -\n") != std::string::npos);
		CHECK(text.find("image: \n") != std::string::npos);
		CHECK(text.find("error: a b\nerror_time: 1762200000") != std::string::npos);
	}

	SECTION("state names")
	{
		CHECK(std::string(sHyperionStats::StateName(sHyperionStats::STATE_DISABLED)) == "disabled");
		CHECK(std::string(sHyperionStats::StateName(sHyperionStats::STATE_NO_OUTPUT)) == "no_output");
		CHECK(std::string(sHyperionStats::StateName(sHyperionStats::STATE_DARK)) == "dark");
		CHECK(std::string(sHyperionStats::StateName(99)) == "disabled");
	}
}
