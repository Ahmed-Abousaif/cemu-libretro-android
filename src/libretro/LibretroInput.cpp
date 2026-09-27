#include "LibretroInput.h"
#include "input/InputManager.h"
#include "Common/precompiled.h"
#include <vector>
#include <memory>

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
		return m_state;
	}

	void LibretroController::UpdateState()
	{
		if (!s_input_state_cb)
			return;

		auto check_btn = [&](unsigned id) -> bool {
			return s_input_state_cb(m_port, RETRO_DEVICE_JOYPAD, 0, id) != 0;
		};

		// Buttons mapping
		m_state.buttons.SetButtonState(kButton0, check_btn(RETRO_DEVICE_ID_JOYPAD_A));
		m_state.buttons.SetButtonState(kButton1, check_btn(RETRO_DEVICE_ID_JOYPAD_B));
		m_state.buttons.SetButtonState(kButton2, check_btn(RETRO_DEVICE_ID_JOYPAD_X));
		m_state.buttons.SetButtonState(kButton3, check_btn(RETRO_DEVICE_ID_JOYPAD_Y));

		m_state.buttons.SetButtonState(kButton4, check_btn(RETRO_DEVICE_ID_JOYPAD_L));
		m_state.buttons.SetButtonState(kButton5, check_btn(RETRO_DEVICE_ID_JOYPAD_R));
		m_state.buttons.SetButtonState(kButtonZL, check_btn(RETRO_DEVICE_ID_JOYPAD_L2));
		m_state.buttons.SetButtonState(kButtonZR, check_btn(RETRO_DEVICE_ID_JOYPAD_R2));

		m_state.buttons.SetButtonState(kButtonUp, check_btn(RETRO_DEVICE_ID_JOYPAD_UP));
		m_state.buttons.SetButtonState(kButtonDown, check_btn(RETRO_DEVICE_ID_JOYPAD_DOWN));
		m_state.buttons.SetButtonState(kButtonLeft, check_btn(RETRO_DEVICE_ID_JOYPAD_LEFT));
		m_state.buttons.SetButtonState(kButtonRight, check_btn(RETRO_DEVICE_ID_JOYPAD_RIGHT));

		m_state.buttons.SetButtonState(kButton6, check_btn(RETRO_DEVICE_ID_JOYPAD_L3));
		m_state.buttons.SetButtonState(kButton7, check_btn(RETRO_DEVICE_ID_JOYPAD_R3));
		m_state.buttons.SetButtonState(kButton8, check_btn(RETRO_DEVICE_ID_JOYPAD_START));
		m_state.buttons.SetButtonState(kButton9, check_btn(RETRO_DEVICE_ID_JOYPAD_SELECT));

		// Analog Left Stick
		int16_t lx = s_input_state_cb(m_port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
		int16_t ly = s_input_state_cb(m_port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
		m_state.axis.x = static_cast<float>(lx) / 32767.0f;
		m_state.axis.y = static_cast<float>(-ly) / 32767.0f; // Invert Y for Cemu coordinate system

		// Analog Right Stick
		int16_t rx = s_input_state_cb(m_port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_X);
		int16_t ry = s_input_state_cb(m_port, RETRO_DEVICE_ANALOG, RETRO_DEVICE_INDEX_ANALOG_RIGHT, RETRO_DEVICE_ID_ANALOG_Y);
		m_state.rotation.x = static_cast<float>(rx) / 32767.0f;
		m_state.rotation.y = static_cast<float>(-ry) / 32767.0f;

		// Pointer/Touchscreen for Port 0 (Wii U GamePad)
		if (m_port == 0)
		{
			int16_t touch_pressed = s_input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
			m_touch_active = (touch_pressed != 0);
			if (m_touch_active)
			{
				int16_t px = s_input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_X);
				int16_t py = s_input_state_cb(0, RETRO_DEVICE_POINTER, 0, RETRO_DEVICE_ID_POINTER_Y);
				// Normalized from [-0x7fff, 0x7fff] to [0.0, 1.0]
				float norm_x = (static_cast<float>(px) + 32767.0f) / 65534.0f;
				float norm_y = (static_cast<float>(py) + 32767.0f) / 65534.0f;
				m_touch_pos = glm::vec2(norm_x * 1280.0f, norm_y * 720.0f);
			}
		}
	}

	void InitializeInput()
	{
		s_controllers.clear();
		for (unsigned i = 0; i < 4; ++i)
		{
			auto ctrl = std::make_shared<LibretroController>(i);
			s_controllers.push_back(ctrl);

			if (i == 0)
			{
				auto emulated = InputManager::instance().set_controller(0, EmulatedController::VPAD);
				if (emulated)
				{
					emulated->add_controller(ctrl);
					emulated->set_default_mapping(ctrl);
				}
			}
			else
			{
				auto emulated = InputManager::instance().set_controller(i, EmulatedController::Pro);
				if (emulated)
				{
					emulated->add_controller(ctrl);
					emulated->set_default_mapping(ctrl);
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
