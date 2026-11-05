#include "CemuLibretro.h"
#include "Environment.h"
#include "LibretroAudio.h"
#include "LibretroInput.h"
#include "LibretroVulkan.h"

#include "Common/precompiled.h"
#include "Cafe/CafeSystem.h"
#include "Cafe/TitleList/TitleList.h"
#include "Cafe/HW/Latte/Renderer/Vulkan/VulkanRenderer.h"
#include "Cafe/HW/Latte/Core/LatteOverlay.h"
#include "util/crypto/aes128.h"
#include "config/ActiveSettings.h"
#include "config/CemuConfig.h"

#include "WindowSystem.h"

// forward declarations
void PPCTimer_init();
void CemuCommonInit();

namespace Libretro::Cemu
{
	static void CreateCemuDirectories(const fs::path& mlc)
	{
		fs::create_directories(mlc / "sys");
		fs::create_directories(mlc / "usr/title/00050000"); // base
		fs::create_directories(mlc / "usr/title/0005000c"); // dlc
		fs::create_directories(mlc / "usr/title/0005000e"); // update
		fs::create_directories(mlc / "usr/save/00050010/1004a100/user/common/db");
		fs::create_directories(mlc / "usr/save/00050010/1004a000/user/common/db");
		fs::create_directories(mlc / "usr/save/00050010/1004a200/user/common/db");

		auto langDir = mlc / "sys/title/0005001b/1005c000/content";
		fs::create_directories(langDir);
		auto langFile = langDir / "language.txt";
		if (!fs::exists(langFile))
		{
			if (std::ofstream file{langFile})
			{
				for (const char* lang : {"ja", "en", "fr", "de", "it", "es", "ko", "nl", "pt", "ru", "zh"})
					fmt::println(file, R"("{}",)", lang);
			}
		}
	}

	bool GameLoad(const fs::path& launchPath)
	{
		LogInfo("Loading game from path: %s", launchPath.string().c_str());

		// 1. Initialize Paths & Settings
		std::string save_dir = Libretro::GetSaveDir() + "/Cemu";
		std::string system_dir = Libretro::GetSystemDir() + "/Cemu";

		fs::create_directories(save_dir + "/config");
		fs::create_directories(save_dir + "/cache");
		fs::create_directories(save_dir + "/mlc01");

		std::set<fs::path> failedWriteAccess;
		ActiveSettings::SetPaths(false, {}, system_dir, save_dir + "/config", save_dir + "/cache", system_dir, failedWriteAccess);
		ActiveSettings::Init();
		GetConfig().mlc_path = save_dir + "/mlc01";

		CreateCemuDirectories(GetConfig().mlc_path.GetValue());

		// Initialize WindowSystem defaults for Libretro
		auto& winInfo = WindowSystem::GetWindowInfo();
		winInfo.width = 1280;
		winInfo.height = 720;
		winInfo.phys_width = 1280;
		winInfo.phys_height = 720;
		winInfo.dpi_scale = 1.0;
		winInfo.window_main.backend = WindowSystem::WindowHandleInfo::Backend::Android;

		// 2. Initialize Crypto & Timers
		AES128_init();
		PPCTimer_init();

		// 3. Initialize Overlay & Subsystems
		LatteOverlay_init();
		CemuCommonInit();

		// 4. Initialize Graphics Renderer
		InitializeGlobalVulkan();
		if (!g_renderer)
		{
			g_renderer = std::make_unique<VulkanRenderer>();
		}

		// 5. Initialize Audio & Input
		Libretro::InitializeAudio();
		Libretro::InitializeInput();

		// 6. Initialize Cafe System
		CafeSystem::Initialize();

		// 7. Mount & Prepare Title
		TitleInfo launchTitle{launchPath};
		if (launchTitle.IsValid())
		{
			CafeTitleList::AddTitleFromPath(launchPath);
			TitleId baseTitleId;
			if (!CafeTitleList::FindBaseTitleId(launchTitle.GetAppTitleId(), baseTitleId))
			{
				DisplayMessage("Unable to launch game: base title files not found.");
				LogError("Base title files not found for %s", launchPath.string().c_str());
				return false;
			}
			CafeSystem::PREPARE_STATUS_CODE r = CafeSystem::PrepareForegroundTitle(baseTitleId);
			if (r != CafeSystem::PREPARE_STATUS_CODE::SUCCESS)
			{
				DisplayMessage("Failed to prepare Wii U title.");
				LogError("PrepareForegroundTitle failed with code %d", static_cast<int>(r));
				return false;
			}
		}
		else
		{
			auto invalidReason = launchTitle.GetInvalidReason();
			if (invalidReason == TitleInfo::InvalidReason::NO_DISC_KEY)
			{
				DisplayMessage("Wii U disc key missing in keys.txt");
				LogError("Disc key missing for %s in keys.txt", launchPath.string().c_str());
				return false;
			}
			else if (invalidReason == TitleInfo::InvalidReason::BAD_PATH_OR_INACCESSIBLE)
			{
				DisplayMessage("Wii U title file inaccessible.");
				LogError("Title file inaccessible: %s", launchPath.string().c_str());
				return false;
			}

			CafeTitleFileType fileType = DetermineCafeSystemFileType(launchPath);
			if (fileType == CafeTitleFileType::RPX || fileType == CafeTitleFileType::ELF)
			{
				CafeSystem::PREPARE_STATUS_CODE r = CafeSystem::PrepareForegroundTitleFromStandaloneRPX(launchPath);
				if (r != CafeSystem::PREPARE_STATUS_CODE::SUCCESS)
				{
					DisplayMessage("Failed to prepare standalone RPX/ELF.");
					LogError("PrepareForegroundTitleFromStandaloneRPX failed with code %d", static_cast<int>(r));
					return false;
				}
			}
			else
			{
				DisplayMessage("Unrecognized or encrypted Wii U title format.");
				LogError("Invalid title structure (reason %d) for %s", static_cast<int>(invalidReason), launchPath.string().c_str());
				return false;
			}
		}

		// 8. Launch Title Emulation
		CafeSystem::LaunchForegroundTitle();
		LogInfo("Wii U title emulation successfully started.");
		return true;
	}

	void GameUnload()
	{
		LogInfo("Unloading game and shutting down CafeSystem.");
		CafeSystem::Shutdown();
		Libretro::ShutdownAudio();
		if (g_renderer)
		{
			g_renderer->Shutdown();
			g_renderer.reset();
		}
	}

	void RunFrame()
	{
		// Poll at the start so Cafe's async VPAD reads see fresh state immediately,
		// then again after audio drain so presses that arrived mid-frame (common when
		// heavy 3D titles keep the frontend busy) still land before we return.
		Libretro::PollInput();
		Libretro::UpdateCoreOptions();
		Libretro::DrainAudio();
		Libretro::PollInput();
		Libretro::PresentVulkanFrame();
	}

	void PauseGame()
	{
		CafeSystem::PauseTitle();
	}

	void ResumeGame()
	{
		CafeSystem::ResumeTitle();
	}
} // namespace Libretro::Cemu
