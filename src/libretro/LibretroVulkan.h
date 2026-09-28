#pragma once

#include "libretro.h"
#include "libretro_vulkan.h"
#include <vulkan/vulkan.h>

namespace Libretro
{
	extern const struct retro_hw_render_interface_vulkan* vulkan_iface;

	void SetVideoRefreshCallback(retro_video_refresh_t cb);
	bool SetupVulkanContext();
	void OnVulkanContextReset();
	void OnVulkanContextDestroy();
	void SetVulkanImage(VkImageView view, VkImage image, VkFormat format, uint32_t width, uint32_t height, VkImageLayout layout, VkSemaphore semaphore);
	void MarkVulkanFrameSubmitted();
	void PresentVulkanFrame();
} // namespace Libretro
