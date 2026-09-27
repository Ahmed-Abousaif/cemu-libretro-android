#pragma once

#include <string>
#include <cstdint>
#include "libretro.h"
#include "libretro_vulkan.h"

namespace Libretro
{
	extern struct retro_hw_render_callback hw_render;
	extern const struct retro_hw_render_interface_vulkan* vulkan_iface;

	bool EnvCb(unsigned cmd, void* data);

	void LogStart();
	bool LogEnabled();
	void LogStop();
	void Log(retro_log_level level, const char* fmt, ...);
	void LogInfo(const char* fmt, ...);
	void LogError(const char* fmt, ...);
	void LogWarn(const char* fmt, ...);

	std::string GetSaveDir();
	std::string GetSystemDir();

	bool DisplayMessage(const char* msg, unsigned frames = 180);

	void InitCoreOptions();
	void UpdateCoreOptions();
} // namespace Libretro
