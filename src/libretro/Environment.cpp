#include "Environment.h"
#include "Cafe/OS/libs/swkbd/swkbd.h"
#include <cstdarg>
#include <cstdio>
#include <string>
#include <string_view>
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
		#include <atomic>
		static std::atomic<ScreenViewMode> s_screenViewMode{ScreenViewMode::TV};
	} // namespace

	ScreenViewMode GetScreenViewMode()
	{
		return s_screenViewMode;
	}

	void SetScreenViewMode(ScreenViewMode mode)
	{
		if (s_screenViewMode == mode)
			return;

		s_screenViewMode = mode;
		LogInfo("Screen view mode changed to: %s",
			mode == ScreenViewMode::Pad ? "pad" :
			mode == ScreenViewMode::SideBySide ? "side_by_side" : "tv");

		struct retro_game_geometry geom{};
		geom.max_width = 3840;
		geom.max_height = 1080;
		if (mode == ScreenViewMode::SideBySide)
		{
			geom.base_width = 2560;
			geom.base_height = 720;
			geom.aspect_ratio = 32.0f / 9.0f;
		}
		else
		{
			geom.base_width = 1280;
			geom.base_height = 720;
			geom.aspect_ratio = 16.0f / 9.0f;
		}
		EnvCb(RETRO_ENVIRONMENT_SET_GEOMETRY, &geom);
	}

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

		var.key = "cemu_screen_view";
		if (EnvCb(RETRO_ENVIRONMENT_GET_VARIABLE, &var) && var.value)
		{
			std::string_view val(var.value);
			if (val == "pad" || val == "GamePad" || val == "gamepad")
				SetScreenViewMode(ScreenViewMode::Pad);
			else if (val == "side_by_side" || val == "Side by Side" || val == "Side-by-Side")
				SetScreenViewMode(ScreenViewMode::SideBySide);
			else
				SetScreenViewMode(ScreenViewMode::TV);
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

static void KeyboardCallback(bool down, unsigned keycode, uint32_t character, uint16_t key_modifiers)
{
	(void)key_modifiers;
	if (down && swkbd::hasKeyboardInputHook())
	{
		if (keycode == RETROK_RETURN || keycode == RETROK_KP_ENTER)
			swkbd::keyInput(swkbd::RETURN_KEYCODE);
		else if (keycode == RETROK_BACKSPACE)
			swkbd::keyInput(swkbd::BACKSPACE_KEYCODE);
		else if (character >= 32 && character <= 126)
			swkbd::keyInput(character);
	}
}

void retro_set_environment(retro_environment_t cb)
{
	Libretro::environ_cb = cb;

	struct retro_keyboard_callback kbcb{ KeyboardCallback };
	cb(RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK, &kbcb);

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
