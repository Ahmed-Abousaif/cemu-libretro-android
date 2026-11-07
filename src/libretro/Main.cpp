#include "libretro.h"
#include "Environment.h"
#include "LibretroAudio.h"
#include "LibretroInput.h"
#include "LibretroVulkan.h"
#include "CemuLibretro.h"

#include <cstring>
#include <cstdio>
#include <string>

#define EMULATOR_NAME "Cemu"
#define EMULATOR_VERSION "2.0-ARM64"
#define DEFAULT_WIDTH 1280
#define DEFAULT_HEIGHT 720
#define DEFAULT_FPS 60.0
#define DEFAULT_SAMPLE_RATE 48000.0

void retro_init(void)
{
	Libretro::LogStart();
	Libretro::LogInfo("Initializing Cemu Libretro Core (%s)...", EMULATOR_VERSION);
}

void retro_deinit(void)
{
	Libretro::LogInfo("Deinitializing Cemu Libretro Core...");
	Libretro::LogStop();
}

unsigned retro_api_version(void)
{
	return RETRO_API_VERSION;
}

void retro_set_controller_port_device(unsigned port, unsigned device)
{
	Libretro::SetControllerPortDevice(port, device);
}

void retro_get_system_info(struct retro_system_info* info)
{
	memset(info, 0, sizeof(*info));
	info->library_name = EMULATOR_NAME;
	info->library_version = EMULATOR_VERSION;
	info->need_fullpath = true;
	info->block_extract = false;
	info->valid_extensions = "wud|wux|wua|iso|rpx|elf";
}

void retro_get_system_av_info(struct retro_system_av_info* info)
{
	memset(info, 0, sizeof(*info));
	info->geometry.base_width = DEFAULT_WIDTH;
	info->geometry.base_height = DEFAULT_HEIGHT;
	info->geometry.max_width = 3840;
	info->geometry.max_height = 1080;
	info->geometry.aspect_ratio = 16.0f / 9.0f;

	info->timing.fps = DEFAULT_FPS;
	info->timing.sample_rate = DEFAULT_SAMPLE_RATE;
}

void retro_set_audio_sample(retro_audio_sample_t cb)
{
	Libretro::SetAudioSampleCallback(cb);
}

void retro_set_audio_sample_batch(retro_audio_sample_batch_t cb)
{
	Libretro::SetAudioSampleBatchCallback(cb);
}

void retro_set_input_poll(retro_input_poll_t cb)
{
	Libretro::SetInputPollCallback(cb);
}

void retro_set_input_state(retro_input_state_t cb)
{
	Libretro::SetInputStateCallback(cb);
}

void retro_set_video_refresh(retro_video_refresh_t cb)
{
	Libretro::SetVideoRefreshCallback(cb);
}

void retro_reset(void)
{
}

void retro_run(void)
{
	Libretro::Cemu::RunFrame();
}

bool retro_load_game(const struct retro_game_info* info)
{
	if (!info || !info->path)
	{
		Libretro::LogError("retro_load_game called with invalid info or path.");
		return false;
	}

	enum retro_pixel_format fmt = RETRO_PIXEL_FORMAT_XRGB8888;
	if (!Libretro::EnvCb(RETRO_ENVIRONMENT_SET_PIXEL_FORMAT, &fmt))
	{
		Libretro::LogError("XRGB8888 pixel format is not supported by frontend.");
		return false;
	}

	struct retro_input_descriptor desc[] = {
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_LEFT, "D-Pad Left"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_UP, "D-Pad Up"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_DOWN, "D-Pad Down"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_RIGHT, "D-Pad Right"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B, "B"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A, "A"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_X, "X"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_Y, "Y"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L, "L"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R, "R"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L2, "ZL"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R2, "ZR"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_L3, "Left Stick Click"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_R3, "Right Stick Click"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START, "Plus (+)"},
		{0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_SELECT, "Minus (-)"},
		{0},
	};
	Libretro::EnvCb(RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS, desc);

	Libretro::UpdateCoreOptions();

	// Setup Vulkan hardware rendering
	if (!Libretro::SetupVulkanContext())
	{
		Libretro::LogError("Vulkan HW rendering initialization failed.");
		return false;
	}

	return Libretro::Cemu::GameLoad(info->path);
}

void retro_unload_game(void)
{
	Libretro::Cemu::GameUnload();
}

unsigned retro_get_region(void)
{
	return RETRO_REGION_NTSC;
}

bool retro_load_game_special(unsigned type, const struct retro_game_info* info, size_t num)
{
	(void)type;
	(void)info;
	(void)num;
	return false;
}

size_t retro_serialize_size(void)
{
	return 0;
}

bool retro_serialize(void* data, size_t size)
{
	(void)data;
	(void)size;
	return false;
}

bool retro_unserialize(const void* data, size_t size)
{
	(void)data;
	(void)size;
	return false;
}

void* retro_get_memory_data(unsigned id)
{
	(void)id;
	return nullptr;
}

size_t retro_get_memory_size(unsigned id)
{
	(void)id;
	return 0;
}

void retro_cheat_reset(void)
{
}

void retro_cheat_set(unsigned index, bool enabled, const char* code)
{
	(void)index;
	(void)enabled;
	(void)code;
}
