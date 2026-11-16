#pragma once

namespace Libretro
{
	void InitializeSoftwareKeyboard();
	void ShutdownSoftwareKeyboard();
	void PollSoftwareKeyboard();
	bool HasNativeSoftwareKeyboard();
}
