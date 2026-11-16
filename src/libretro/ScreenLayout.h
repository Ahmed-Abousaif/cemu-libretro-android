#pragma once

#include <cstdint>

namespace Libretro
{
	struct ScreenRect
	{
		uint32_t x, y, width, height;
	};

	struct DualScreenLayout
	{
		uint32_t width, height;
		ScreenRect tv, pad;
	};

	// Landscape: full-size TV on top, centered DRC at half width AND height below.
	// Portrait retains the existing equal-size side-by-side composition.
	inline DualScreenLayout ComputeDualScreenLayout(uint32_t tvHeight, bool stacked)
	{
		const uint32_t width = tvHeight * 16 / 9;
		if (stacked)
		{
			const uint32_t padWidth = width / 2;
			const uint32_t padHeight = tvHeight / 2;
			return {width, tvHeight + padHeight,
				{0, 0, width, tvHeight}, {(width - padWidth) / 2, tvHeight, padWidth, padHeight}};
		}
		return {width * 2, tvHeight, {0, 0, width, tvHeight}, {width, 0, width, tvHeight}};
	}

	inline bool MapGamePadTouch(const DualScreenLayout& layout, float x, float y, float& padX, float& padY)
	{
		const float imageX = x * layout.width;
		const float imageY = y * layout.height;
		const auto& pad = layout.pad;
		if (!pad.width || !pad.height || imageX < pad.x || imageY < pad.y ||
			imageX > pad.x + pad.width || imageY > pad.y + pad.height)
			return false;
		padX = (imageX - pad.x) / pad.width;
		padY = (imageY - pad.y) / pad.height;
		return true;
	}
}
