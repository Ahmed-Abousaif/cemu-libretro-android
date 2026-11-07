#include "LibretroInput.h"
#include "Cafe/OS/libs/swkbd/swkbd.h"
#include "Environment.h"
#include "input/InputManager.h"
#include "Common/precompiled.h"
#include <vector>
#include <memory>
#include <algorithm>
#include <mutex>

namespace Libretro
{
	namespace
	{
		static retro_input_poll_t s_input_poll_cb = nullptr;
		static retro_input_state_t s_input_state_cb = nullptr;
		static std::vector<std::shared_ptr<LibretroController>> s_controllers;
	} // namespace

	void SetInputPollCallback(retro_input_poll_t cb)
	{
		s_input_poll_cb = cb;
	}

	void SetInputStateCallback(retro_input_state_t cb)
	{
		s_input_state_cb = cb;
	}

	LibretroController::LibretroController(unsigned port)
		: ControllerBase(fmt::format("libretro_{}", port), fmt::format("Libretro Pad {}", port + 1)),
		  m_port(port)
	{
	}

	ControllerState LibretroController::raw_state()
	{
		std::lock_guard lock(m_stateMutex);
		return m_state;
	}

	glm::vec2 LibretroController::get_position()
	{
		std::lock_guard lock(m_stateMutex);
		return m_touch_pos;
	}

	PositionVisibility LibretroController::GetPositionVisibility()
	{
		std::lock_guard lock(m_stateMutex);
		return m_touch_active ? PositionVisibility::FULL : PositionVisibility::NONE;
	}

	void LibretroController::UpdateState()
	{
		if (!s_input_state_cb)
			return;

		ControllerState next{};
		bool touch_active = false;
		glm::vec2 touch_pos{0.0f, 0.0f};

		auto check_btn = [&](unsigned id) -> bool {
			return s_input_state_cb(m_port, RETRO_DEVICE_JOYPAD, 0, id) != 0;
		};

		// Buttons mapping
		next.buttons.SetButtonState(kButton0, check_btn(RETRO_DEVICE_ID_JOYPAD_A));
		next.buttons.SetButtonState(kButton1, check_btn(RETRO_DEVICE_ID_JOYPAD_B));
		next.buttons.SetButtonState(kButton2, check_btn(RETRO_DEVICE_ID_JOYPAD_X));
		next.buttons.SetButtonState(kButton3, check_btn(RETRO_DEVICE_ID_JOYPAD_Y));

		next.buttons.SetButtonState(kButton4, check_btn(RETRO_DEVICE_ID_JOYPAD_L));
		next.buttons.SetButtonState(kButton5, check_btn(RETRO_DEVICE_ID_JOYPAD_R));
		next.buttons.SetButtonState(kButtonZL, check_btn(RETRO_DEVICE_ID_JOYPAD_L2));
		next.buttons.SetButtonState(kButtonZR, check_btn(RETRO_DEVICE_ID_JOYPAD_R2));

		next.buttons.SetButtonState(kButtonUp, check_btn(RETRO_DEVICE_ID_JOYPAD_UP));
		next.buttons.SetButtonState(kButtonDown, check_btn(RETRO_DEVICE_ID_JOYPAD_DOWN));
		next.buttons.SetButtonState(kButtonLeft, check_btn(RETRO_DEVICE_ID_JOYPAD_LEFT));
		next.buttons.SetButtonState(kButtonRight, check_btn(RETRO_DEVICE_ID_JOYPAD_RIGHT));

		next.buttons.SetButtonState(kButton6, check_btn(RETRO_DEVICE_ID_JOYPAD_L3));
		next.buttons.SetButtonState(kButton7, check_btn(RETRO_DEVICE_ID_JOYPAD_R3));
		next.buttons.SetButtonState(kButton8, check_btn(RETRO_DEVICE_ID_JOYPAD_START));
		next.buttons.SetButtonState(kButton9, check_btn(RETRO_DEVICE_ID_JOYPAD_SELECT));

		// Analog Left Stick
		int16_t lx = s_input_state_cb(m_port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
		int16_t ly = s_input_state_cb(m_port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
		next.axis.x = static_cast<float>(lx) / 32767.0f;
		next.axis.y = static_cast<float>(-ly) / 32767.0f; // Invert Y for Cemu coordinate system

		// Analog Right Stick
		int16_t rx = s_input_state_cb(m_port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
		int16_t ry = s_input_state_cb(m_port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
		next.rotation.x = static_cast<float>(rx) / 32767.0f;
		next.rotation.y = static_cast<float>(-ry) / 32767.0f;

		// Pointer/Touchscreen for Port 0 (Wii U GamePad)
		if (m_port == 0)
		{
			int16_t touch_pressed = s_input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
			if (touch_pressed)
			{
				int16_t px = s_input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
				int16_t py = s_input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
				// Normalized from [-0x7fff, 0x7fff] to [0.0, 1.0]
				float norm_x = (static_cast<float>(px) + 32767.0f) / 65534.0f;
				float norm_y = (static_cast<float>(py) + 32767.0f) / 65534.0f;

				ScreenViewMode screenMode = GetScreenViewMode();
				if (screenMode == ScreenViewMode::SideBySide)
				{
					// Side-by-Side: Left half [0.0..0.5] is TV, Right half [0.5..1.0] is GamePad
					if (norm_x >= 0.5f)
					{
						float pad_x = (norm_x - 0.5f) * 2.0f;
						touch_pos = glm::vec2(std::clamp(pad_x, 0.0f, 1.0f), std::clamp(norm_y, 0.0f, 1.0f));
						touch_active = true;
					}
					else
					{
						touch_active = false;
					}
				}
				else
				{
					// Pad view or TV view fallback: map full viewport
					touch_pos = glm::vec2(std::clamp(norm_x, 0.0f, 1.0f), std::clamp(norm_y, 0.0f, 1.0f));
					touch_active = true;
				}
			}
			else
			{
				touch_active = false;
			}
		}

		if (m_port == 0 && swkbd::hasKeyboardInputHook())
		{
			static bool s_enter_down = false;
			if (s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_START) ||
			    s_input_state_cb(0, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_A))
			{
				if (!s_enter_down)
				{
					s_enter_down = true;
					swkbd::keyInput(swkbd::RETURN_KEYCODE);
				}
			}
			else
			{
				s_enter_down = false;
			}
		}

		if (!next.buttons.IsIdle())
		{
			static uint32 s_log_cooldown = 0;
			if (++s_log_cooldown % 60 == 1)
			{
				cemuLog_log(LogType::Force, "Libretro Pad %u active buttons", m_port);
			}
		}

		std::lock_guard lock(m_stateMutex);
		m_state = std::move(next);
		m_touch_active = touch_active;
		m_touch_pos = touch_pos;
	}

	void InitializeInput()
	{
		s_controllers.clear();
		s_controllers.reserve(4);

		for (unsigned i = 0; i < 4; ++i)
		{
			auto ctrl = std::make_shared<LibretroController>(i);
			s_controllers.push_back(ctrl);
		}

		// Port 0 defaults to GamePad (VPAD channel 0)
		auto vpad = InputManager::instance().set_controller(0, EmulatedController::VPAD);
		if (vpad && !s_controllers.empty())
		{
			vpad->clear_controllers();
			vpad->add_controller(s_controllers[0]);
			vpad->set_default_mapping(s_controllers[0]);
		}

		// Ports 1..3 default to Pro Controllers
		for (unsigned i = 1; i < 4; ++i)
		{
			auto pro = InputManager::instance().set_controller(i, EmulatedController::Pro);
			if (pro && i < s_controllers.size())
			{
				pro->clear_controllers();
				pro->add_controller(s_controllers[i]);
				pro->set_default_mapping(s_controllers[i]);
			}
		}
	}

	void SetControllerPortDevice(unsigned port, unsigned device)
	{
		if (port >= 4)
			return;

		Libretro::LogInfo("SetControllerPortDevice: port %u, device %u", port, device);

		if (port == 0)
		{
			if (device == RETRO_DEVICE_NONE)
			{
				InputManager::instance().delete_controller(0, false, EmulatedController::VPAD);
				InputManager::instance().delete_controller(0, false, EmulatedController::Pro);
			}
			else if (device == RETRO_DEVICE_SUBCLASS(RETRO_DEVICE_JOYPAD, 1)) // 257 = Wii U Pro Controller
			{
				InputManager::instance().delete_controller(0, false, EmulatedController::VPAD);
				auto pro = InputManager::instance().set_controller(0, EmulatedController::Pro);
				if (pro && !s_controllers.empty() && s_controllers[0])
				{
					pro->clear_controllers();
					pro->add_controller(s_controllers[0]);
					pro->set_default_mapping(s_controllers[0]);
				}
			}
			else // RETRO_DEVICE_JOYPAD (1) = Wii U GamePad
			{
				InputManager::instance().delete_controller(0, false, EmulatedController::Pro);
				auto vpad = InputManager::instance().set_controller(0, EmulatedController::VPAD);
				if (vpad && !s_controllers.empty() && s_controllers[0])
				{
					vpad->clear_controllers();
					vpad->add_controller(s_controllers[0]);
					vpad->set_default_mapping(s_controllers[0]);
				}
			}
		}
		else
		{
			if (device == RETRO_DEVICE_NONE)
			{
				InputManager::instance().delete_controller(port, false, EmulatedController::Pro);
			}
			else
			{
				auto pro = InputManager::instance().set_controller(port, EmulatedController::Pro);
				if (pro && port < s_controllers.size() && s_controllers[port])
				{
					pro->clear_controllers();
					pro->add_controller(s_controllers[port]);
					pro->set_default_mapping(s_controllers[port]);
				}
			}
		}
	}

	void PollInput()
	{
		if (s_input_poll_cb)
		{
			s_input_poll_cb();
		}

		for (auto& ctrl : s_controllers)
		{
			if (ctrl)
			{
				ctrl->UpdateState();
			}
		}
	}
} // namespace Libretro
