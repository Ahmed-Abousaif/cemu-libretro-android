#pragma once

#include <string>
#include <filesystem>

namespace fs = std::filesystem;

namespace Libretro::Cemu
{
	bool GameLoad(const fs::path& launchPath);
	void GameUnload();
	void RunFrame();
	void PauseGame();
	void ResumeGame();
} // namespace Libretro::Cemu
