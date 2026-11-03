// SPDX-License-Identifier: AGPL-3.0-or-later

/**
 * @file testpattern.h
 * Test Pattern Header File
 *
 * @copyright 2026 by Andreas Baierl. All Rights Reserved.
 *
 * @license{AGPL-3.0-or-later}
 */

#ifndef __HYPERION_TESTPATTERN_H
#define __HYPERION_TESTPATTERN_H

#include <cstdint>
#include <vector>

/**
 * Test patterns for the leds
 *
 * The same pattern is shown on the screen (OSD) and sent to Hyperion, so
 * the leds can be compared with the picture of the TV.
 */
class cTestPattern {
public:
	/// the patterns
	enum eMode {
		MODE_COLORS,                    ///< red, green, blue and white (one after the other)
		MODE_EDGES,                     ///< top red, right green, bottom blue, left yellow
		MODE_DELAY,                     ///< black and white (one after the other, to find the delay of the leds)
		MODES,
	};

	/// colors of a pattern (0xRRGGBB)
	struct sColors {
		uint32_t top, right, bottom, left, center;
	};

	static constexpr int WIDTH = 64;        ///< width of the image for Hyperion
	static constexpr int HEIGHT = 36;       ///< height of the image for Hyperion
	static constexpr int BORDER = 5;        ///< a border is 1/BORDER of the image

	static int Steps(int mode);
	static int StepMs(int mode);
	static sColors Colors(int mode, int step);
	static std::vector<uint8_t> Image(const sColors &, int width = WIDTH, int height = HEIGHT);
};

#endif
