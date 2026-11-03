// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file testpattern.cpp
 * Test Pattern
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#include "testpattern.h"

namespace {

constexpr uint32_t BLACK  = 0x000000;
constexpr uint32_t WHITE  = 0xffffff;
constexpr uint32_t RED    = 0xff0000;
constexpr uint32_t GREEN  = 0x00ff00;
constexpr uint32_t BLUE   = 0x0000ff;
constexpr uint32_t YELLOW = 0xffff00;

} // namespace

/**
 * Number of steps of a pattern (the colors change with every step)
 */
int cTestPattern::Steps(int mode)
{
	switch (mode) {
		case MODE_COLORS: return 4;
		case MODE_DELAY:  return 2;
		default:          return 1;
	}
}

/**
 * Duration of a step (ms)
 */
int cTestPattern::StepMs(int mode)
{
	return mode == MODE_DELAY ? 1000 : 2000;
}

/**
 * Colors of a step of a pattern
 */
cTestPattern::sColors cTestPattern::Colors(int mode, int step)
{
	switch (mode) {
		case MODE_COLORS: {
			static const uint32_t colors[] = { RED, GREEN, BLUE, WHITE };
			uint32_t c = colors[(unsigned)step % 4];
			return { c, c, c, c, c };
		}
		case MODE_EDGES:
			return { RED, GREEN, BLUE, YELLOW, BLACK };
		case MODE_DELAY: {
			uint32_t c = step % 2 ? WHITE : BLACK;
			return { c, c, c, c, c };
		}
		default:
			return { BLACK, BLACK, BLACK, BLACK, BLACK };
	}
}

/**
 * Create the rgb image of a pattern
 *
 * The borders are 1/BORDER of the image. Top and bottom cover the corners.
 */
std::vector<uint8_t> cTestPattern::Image(const sColors &colors, int width, int height)
{
	std::vector<uint8_t> rgb((size_t)width * height * 3);
	int bx = width / BORDER;
	int by = height / BORDER;

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			uint32_t c = colors.center;
			if (y < by)
				c = colors.top;
			else if (y >= height - by)
				c = colors.bottom;
			else if (x < bx)
				c = colors.left;
			else if (x >= width - bx)
				c = colors.right;

			uint8_t *p = &rgb[((size_t)y * width + x) * 3];
			p[0] = (uint8_t)(c >> 16);
			p[1] = (uint8_t)(c >> 8);
			p[2] = (uint8_t)c;
		}
	}

	return rgb;
}
