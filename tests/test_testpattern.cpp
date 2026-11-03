/**
 * @file test_testpattern.cpp
 * Unit tests for cTestPattern
 */

#include <catch2/catch_test_macros.hpp>

#include <cstdint>
#include <vector>

#include "testpattern.h"

namespace {

uint32_t Pixel(const std::vector<uint8_t> &rgb, int width, int x, int y)
{
	const uint8_t *p = &rgb[((size_t)y * width + x) * 3];
	return ((uint32_t)p[0] << 16) | (p[1] << 8) | p[2];
}

} // namespace

TEST_CASE("TestPattern: images", "[testpattern]")
{
	const int w = cTestPattern::WIDTH;
	const int h = cTestPattern::HEIGHT;

	SECTION("edges")
	{
		auto rgb = cTestPattern::Image(cTestPattern::Colors(cTestPattern::MODE_EDGES, 0));
		REQUIRE(rgb.size() == (size_t)w * h * 3);
		CHECK(Pixel(rgb, w, w / 2, 0) == 0xff0000);         // top
		CHECK(Pixel(rgb, w, w - 1, h / 2) == 0x00ff00);     // right
		CHECK(Pixel(rgb, w, w / 2, h - 1) == 0x0000ff);     // bottom
		CHECK(Pixel(rgb, w, 0, h / 2) == 0xffff00);         // left
		CHECK(Pixel(rgb, w, w / 2, h / 2) == 0x000000);     // center
		CHECK(Pixel(rgb, w, 0, 0) == 0xff0000);             // corners belong to top and bottom
		CHECK(Pixel(rgb, w, w - 1, h - 1) == 0x0000ff);
	}

	SECTION("colors change with the steps")
	{
		CHECK(cTestPattern::Steps(cTestPattern::MODE_COLORS) == 4);
		CHECK(cTestPattern::Colors(cTestPattern::MODE_COLORS, 0).center == 0xff0000);
		CHECK(cTestPattern::Colors(cTestPattern::MODE_COLORS, 1).center == 0x00ff00);
		CHECK(cTestPattern::Colors(cTestPattern::MODE_COLORS, 2).center == 0x0000ff);
		CHECK(cTestPattern::Colors(cTestPattern::MODE_COLORS, 3).top == 0xffffff);
	}

	SECTION("delay flashes black and white every second")
	{
		CHECK(cTestPattern::Steps(cTestPattern::MODE_DELAY) == 2);
		CHECK(cTestPattern::StepMs(cTestPattern::MODE_DELAY) == 1000);
		auto black = cTestPattern::Image(cTestPattern::Colors(cTestPattern::MODE_DELAY, 0));
		auto white = cTestPattern::Image(cTestPattern::Colors(cTestPattern::MODE_DELAY, 1));
		CHECK(Pixel(black, w, 3, 3) == 0x000000);
		CHECK(Pixel(white, w, w / 2, h / 2) == 0xffffff);
	}
}
