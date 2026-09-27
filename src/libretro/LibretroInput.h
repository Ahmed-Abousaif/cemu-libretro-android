#pragma once

#include "input/api/Controller.h"
#include "input/emulated/VPADController.h"
#include "input/emulated/WPADController.h"
#include "libretro.h"

namespace Libretro
{
	void SetInputPollCallback(retro_input_poll_t cb);
	void SetInputStateCallback(retro_input_state_t cb);

	void InitializeInput();
	void PollInput();

	class LibretroController : public ControllerBase
	{
	public:
		explicit LibretroController(unsigned port);
		~LibretroController() override = default;

		std::string_view api_name() const override { return "libretro"; }
		InputAPI::Type api() const override { return InputAPI::Device; }

		bool is_connected() override { return true; }
		ControllerState raw_state() override;

		bool has_position() override { return m_port == 0; }
		glm::vec2 get_position() override { return m_touch_pos; }
		PositionVisibility GetPositionVisibility() override
		{
			return m_touch_active ? PositionVisibility::FULL : PositionVisibility::NONE;
		}

		void UpdateState();

	private:
		unsigned m_port;
		ControllerState m_state;
		glm::vec2 m_touch_pos{0.0f, 0.0f};
		bool m_touch_active = false;
	};
} // namespace Libretro
