#include "Environment.h"
#include <cstdarg>
#include <cstdio>
#include <string>
#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"

namespace Libretro
{
	struct retro_hw_render_callback hw_render{};
	const struct retro_hw_render_interface_vulkan* vulkan_iface = nullptr;

	namespace
	{
		static retro_environment_t environ_cb = nullptr;
		static retro_log_printf_t log_cb = nullptr;
	} // namespace

	bool EnvCb(unsigned cmd, void* data)
	{
		if (!environ_cb)
			return false;
		return environ_cb(cmd, data);
	}

	void LogStart()
	{
		static struct retro_log_callback logging;
		if (EnvCb(RETRO_ENVIRONMENT_GET_LOG_INTERFACE, &logging))
		{
			log_cb = logging.log;
		}
	}

	bool LogEnabled()
	{
		return log_cb != nullptr;
	}

	void LogStop()
	{
		log_cb = nullptr;
	}

#include <android/log.h>

	void Log(retro_log_level level, const char* fmt, ...)
	{
		va_list va;
		va_start(va, fmt);
		int androidLevel = ANDROID_LOG_INFO;
		if (level == RETRO_LOG_WARN) androidLevel = ANDROID_LOG_WARN;
		else if (level == RETRO_LOG_ERROR) androidLevel = ANDROID_LOG_ERROR;
		__android_log_vprint(androidLevel, "CemuLibretro", fmt, va);
		va_end(va);

		if (LogEnabled())
		{
			va_list va2;
			va_start(va2, fmt);
			log_cb(level, fmt, va2);
			va_end(va2);
		}
	}

	void LogInfo(const char* fmt, ...)
	{
		va_list va;
		va_start(va, fmt);
		char buf[1024];
		vsnprintf(buf, sizeof(buf), fmt, va);
		va_end(va);
		Log(RETRO_LOG_INFO, "[Cemu-ARM] %s\n", buf);
	}

	void LogError(const char* fmt, ...)
	{
		va_list va;
		va_start(va, fmt);
		char buf[1024];
		vsnprintf(buf, sizeof(buf), fmt, va);
		va_end(va);
		Log(RETRO_LOG_ERROR, "[Cemu-ARM ERROR] %s\n", buf);
	}

	void LogWarn(const char* fmt, ...)
	{
		va_list va;
		va_start(va, fmt);
		char buf[1024];
		vsnprintf(buf, sizeof(buf), fmt, va);
		va_end(va);
		Log(RETRO_LOG_WARN, "[Cemu-ARM WARN] %s\n", buf);
	}

	std::string GetEnvStr(unsigned cmd)
	{
		if (!environ_cb)
			return {};
		const char* var = nullptr;
		if (!environ_cb(cmd, &var) || var == nullptr)
		{
			return {};
		}
		return std::string(var);
	}

	std::string GetSaveDir()
	{
		std::string dir = GetEnvStr(RETRO_ENVIRONMENT_GET_SAVE_DIRECTORY);
		if (dir.empty())
			dir = ".";
		return dir;
	}

	std::string GetSystemDir()
	{
		std::string dir = GetEnvStr(RETRO_ENVIRONMENT_GET_SYSTEM_DIRECTORY);
		if (dir.empty())
			dir = ".";
		return dir;
	}

	bool DisplayMessage(const char* msg, unsigned frames)
	{
		if (!environ_cb)
			return false;
		retro_message m;
		m.msg = msg;
		m.frames = frames;
		return environ_cb(RETRO_ENVIRONMENT_SET_MESSAGE, &m);
	}

	void InitCoreOptions()
	{
		static const struct retro_core_option_definition options[] = {
			{
				"cemu_cpu_mode",
				"CPU Recompiler Mode",
				"Select the CPU emulation mode. Multi-Core Recompiler is recommended for best performance on ARM64.",
				{
					{"recompiler_multi", "Multi-Core Recompiler (Recommended)"},
					{"recompiler_dual", "Dual-Core Recompiler"},
					{"recompiler_single", "Single-Core Recompiler"},
					{"interpreter", "Interpreter (Slow)"},
					{nullptr, nullptr},
				},
				"recompiler_multi"
			},
			{
				"cemu_screen_view",
				"Screen View",
				"Display TV screen, GamePad screen, or both.",
				{
					{"tv", "TV View"},
					{"pad", "GamePad View"},
					{"side_by_side", "Side by Side"},
					{nullptr, nullptr},
				},
				"tv"
			},
			{
				"cemu_audio_volume",
				"Audio Volume",
				"Master audio output volume percentage.",
				{
					{"100", "100%"},
					{"80", "80%"},
					{"60", "60%"},
					{"40", "40%"},
					{"20", "20%"},
					{"0", "Mute"},
					{nullptr, nullptr},
				},
				"100"
			},
			{nullptr, nullptr, nullptr, {{nullptr, nullptr}}, nullptr}
		};

		static const struct retro_core_options_intl options_intl = {
			(struct retro_core_option_definition*)options,
			nullptr
		};

		bool ok = EnvCb(RETRO_ENVIRONMENT_SET_CORE_OPTIONS_INTL, (void*)&options_intl);
		if (!ok)
		{
			// Fallback to legacy v1 options if needed
			static const struct retro_variable legacy_vars[] = {
				{"cemu_cpu_mode", "CPU Mode; recompiler_multi|recompiler_dual|recompiler_single|interpreter"},
				{"cemu_screen_view", "Screen View; tv|pad|side_by_side"},
				{"cemu_audio_volume", "Volume; 100|80|60|40|20|0"},
				{nullptr, nullptr}
			};
			EnvCb(RETRO_ENVIRONMENT_SET_VARIABLES, (void*)legacy_vars);
		}
	}

	void UpdateCoreOptions()
	{
		struct retro_variable var;

		var.key = "cemu_cpu_mode";
		if (EnvCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			auto& cfg = GetConfig();
			if (std::string(var.value) == "recompiler_multi")
				cfg.cpu_mode = CPUMode::MulticoreRecompiler;
			else if (std::string(var.value) == "recompiler_dual")
				cfg.cpu_mode = CPUMode::DualcoreRecompiler;
			else if (std::string(var.value) == "recompiler_single")
				cfg.cpu_mode = CPUMode::SinglecoreRecompiler;
			else
				cfg.cpu_mode = CPUMode::SinglecoreInterpreter;
		}

		var.key = "cemu_audio_volume";
		if (EnvCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			int vol = std::atoi(var.value);
			GetConfig().tv_volume = vol;
			GetConfig().pad_volume = vol;
		}
	}
} // namespace Libretro

void retro_set_environment(retro_environment_t cb)
{
	Libretro::environ_cb = cb;

	static const struct retro_controller_description controllers[] = {
		{"Wii U GamePad", RETRO_DEVICE_JOYPAD},
		{"Wii U Pro Controller", RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)},
	};

	static const struct retro_controller_info ports[] = {
		{controllers, 2},
		{controllers, 2},
		{controllers, 2},
		{controllers, 2},
		{nullptr, 0},
	};

	cb(RETRO_ENVIRONMENT_SET_CONTROLLER_INFO, (void*)ports);
	Libretro::InitCoreOptions();
}
