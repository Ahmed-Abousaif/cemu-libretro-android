#include "../../src/libretro/ScreenLayout.h"
#include <cassert>
#include <cmath>
#include <iostream>

int main()
{
	const auto stacked = Libretro::ComputeDualScreenLayout(720, true);
	assert(stacked.width == 1280 && stacked.height == 1080);
	assert(stacked.tv.x == 0 && stacked.tv.y == 0 && stacked.tv.width == 1280 && stacked.tv.height == 720);
	assert(stacked.pad.x == 320 && stacked.pad.y == 720 && stacked.pad.width == 640 && stacked.pad.height == 360);
	float x = -1, y = -1;
	assert(Libretro::MapGamePadTouch(stacked, .5f, 5.f / 6.f, x, y));
	assert(std::abs(x - .5f) < .0001f && std::abs(y - .5f) < .0001f);
	assert(Libretro::MapGamePadTouch(stacked, .25f, 2.f / 3.f, x, y));
	assert(std::abs(x) < .0001f && std::abs(y) < .0001f);
	assert(Libretro::MapGamePadTouch(stacked, .75f, 1.f, x, y));
	assert(std::abs(x - 1.f) < .0001f && std::abs(y - 1.f) < .0001f);
	assert(!Libretro::MapGamePadTouch(stacked, .5f, .3f, x, y)); // TV
	assert(!Libretro::MapGamePadTouch(stacked, .2f, .8f, x, y)); // left margin
	assert(!Libretro::MapGamePadTouch(stacked, .8f, .8f, x, y)); // right margin
	assert(!Libretro::MapGamePadTouch(stacked, .5f, 1.1f, x, y)); // outside image
	const auto portrait = Libretro::ComputeDualScreenLayout(720, false);
	assert(portrait.width == 2560 && portrait.height == 720);
	assert(Libretro::MapGamePadTouch(portrait, .75f, .5f, x, y));
	assert(std::abs(x - .5f) < .0001f && std::abs(y - .5f) < .0001f);
	assert(!Libretro::MapGamePadTouch(portrait, .25f, .5f, x, y));
	std::cout << "PASS: stacked dimensions, centered half-size DRC, touch corners/center, rejected TV/margins, portrait\n";
}
