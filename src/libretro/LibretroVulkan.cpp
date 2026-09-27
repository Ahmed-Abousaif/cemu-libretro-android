#include "LibretroVulkan.h"
#include "Environment.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanAPI.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "Common/precompiled.h"

namespace Libretro
{
	namespace
	{
		static retro_video_refresh_t s_video_cb = nullptr;
		static uint32_t s_frame_width = 1280;
		static uint32_t s_frame_height = 720;
		static struct retro_vulkan_image s_current_image{};
		static bool s_has_image = false;
	} // namespace

	void SetVideoRefreshCallback(retro_video_refresh_t cb)
	{
		s_video_cb = cb;
	}

	void SetVulkanImage(VkImageView view, VkImage image, VkFormat format, uint32_t width, uint32_t height)
	{
		static uint32_t s_setCount = 0;
		if (s_setCount++ % 60 == 0 || !s_has_image)
		{
			LogInfo("SetVulkanImage: view=%p, img=%p, format=%d, %ux%u (count=%u)",
				(void*)view, (void*)image, (int)format, width, height, s_setCount);
		}
		s_current_image.image_view = view;
		s_current_image.image_layout = VK_IMAGE_LAYOUT_GENERAL;
		s_current_image.create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		s_current_image.create_info.image = image;
		s_current_image.create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
		s_current_image.create_info.format = format;
		if (width > 0) s_frame_width = width;
		if (height > 0) s_frame_height = height;
		s_has_image = (view != VK_NULL_HANDLE);
	}

	void PresentVulkanFrame()
	{
		static uint32_t s_presentCount = 0;
		if (vulkan_iface && vulkan_iface->set_image && s_has_image)
		{
			if (s_presentCount++ % 60 == 0)
			{
				LogInfo("PresentVulkanFrame: calling set_image with view=%p, %ux%u",
					(void*)s_current_image.image_view, s_frame_width, s_frame_height);
			}
			vulkan_iface->set_image(vulkan_iface->handle, &s_current_image, 0, nullptr, VK_QUEUE_FAMILY_IGNORED);
		}
		if (s_video_cb)
		{
			s_video_cb(s_has_image ? RETRO_HW_FRAME_BUFFER_VALID : nullptr, s_frame_width, s_frame_height, 0);
		}
	}

	bool SetupVulkanContext()
	{
		hw_render.context_type = RETRO_HW_CONTEXT_VULKAN;
		hw_render.version_major = 1;
		hw_render.version_minor = 1;
		hw_render.context_reset = OnVulkanContextReset;
		hw_render.context_destroy = OnVulkanContextDestroy;
		hw_render.cache_context = true;
		hw_render.bottom_left_origin = false;

		if (!EnvCb(RETRO_ENVIRONMENT_SET_HW_RENDER, &hw_render))
		{
			LogError("Failed to negotiate RETRO_HW_CONTEXT_VULKAN with frontend.");
			return false;
		}

		if (EnvCb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, (void**)&vulkan_iface) && vulkan_iface)
		{
			LogInfo("Immediately obtained Vulkan HW render interface: instance=%p, gpu=%p, device=%p, queue=%p, queue_idx=%u",
				vulkan_iface->instance, vulkan_iface->gpu, vulkan_iface->device, vulkan_iface->queue, vulkan_iface->queue_index);
			InitializeGlobalVulkan();
			if (vulkan_iface->get_instance_proc_addr)
				vkGetInstanceProcAddr = vulkan_iface->get_instance_proc_addr;
			if (vulkan_iface->get_device_proc_addr)
				vkGetDeviceProcAddr = vulkan_iface->get_device_proc_addr;
			if (vulkan_iface->instance)
				InitializeInstanceVulkan(vulkan_iface->instance);
			if (vulkan_iface->device)
				InitializeDeviceVulkan(vulkan_iface->device);
		}
		else
		{
			LogWarn("RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE returned null in SetupVulkanContext.");
		}

		return true;
	}

	void OnVulkanContextReset()
	{
		LogInfo("Vulkan context reset triggered.");

		if (!EnvCb(RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE, (void**)&vulkan_iface) || !vulkan_iface)
		{
			LogWarn("RETRO_ENVIRONMENT_GET_HW_RENDER_INTERFACE returned null.");
		}
		else
		{
			LogInfo("Obtained Vulkan HW render interface (Vulkan %u.%u)",
				vulkan_iface->interface_version, 0);
		}

		InitializeGlobalVulkan();

		if (vulkan_iface && vulkan_iface->get_instance_proc_addr)
		{
			vkGetInstanceProcAddr = vulkan_iface->get_instance_proc_addr;
		}

		if (vulkan_iface && vulkan_iface->get_device_proc_addr)
		{
			vkGetDeviceProcAddr = vulkan_iface->get_device_proc_addr;
		}

		if (vulkan_iface && vulkan_iface->instance)
		{
			InitializeInstanceVulkan(vulkan_iface->instance);
		}
		if (vulkan_iface && vulkan_iface->device)
		{
			InitializeDeviceVulkan(vulkan_iface->device);

			// Inject the libretro-provided device into VulkanRenderer so
			// Vulkan resources can be initialized with a valid VkDevice.
			if (g_renderer)
			{
				auto* vkRenderer = static_cast<VulkanRenderer*>(g_renderer.get());
				vkRenderer->SetLibretroVulkanContext(
					vulkan_iface->instance,
					vulkan_iface->gpu,
					vulkan_iface->device,
					vulkan_iface->queue,
					vulkan_iface->queue_index
				);
				LogInfo("OnVulkanContextReset: VulkanRenderer updated with libretro device=%p", (void*)vulkan_iface->device);
			}
		}
	}

	void OnVulkanContextDestroy()
	{
		LogInfo("Vulkan context destroy triggered.");
		vulkan_iface = nullptr;
	}
} // namespace Libretro
