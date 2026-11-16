#include "Common/precompiled.h"
#include "LibretroKeyboard.h"
#include "Environment.h"
#include "omni_text_input.h"
#include "Cafe/OS/libs/swkbd/swkbd.h"
#include <array>
#include <mutex>

namespace Libretro
{
	namespace
	{
		omni_text_input_interface s_interface{};
		std::mutex s_mutex;
		uint64_t s_nextSession = 0;
		uint64_t s_session = 0;

		class NativeKeyboard final : public swkbd::swkbdCallbacks
		{
			void showSoftwareKeyboard(const std::string&, sint32 maxLength) override
			{
				std::lock_guard lock(s_mutex);
				s_session = ++s_nextSession;
				const auto text = swkbd::getInputText();
				const uint32_t limit = std::clamp(maxLength, 0, OMNI_TEXT_INPUT_CAPACITY - 1);
				s_interface.show(s_session, reinterpret_cast<const uint16_t*>(text.data()),
					(uint32_t)text.size(), limit);
				LogInfo("Native software keyboard requested (limit=%u)", limit);
			}

			void hideSoftwareKeyboard() override
			{
				std::lock_guard lock(s_mutex);
				if (s_session) s_interface.hide(s_session);
				s_session = 0;
			}
		};
	}

	void InitializeSoftwareKeyboard()
	{
		s_interface = {};
		s_interface.version = OMNI_TEXT_INPUT_VERSION;
		if (EnvCb(OMNI_ENVIRONMENT_GET_TEXT_INPUT_INTERFACE, &s_interface) &&
			s_interface.show && s_interface.hide && s_interface.poll)
			swkbd::setSwkbdCallbacks(std::make_shared<NativeKeyboard>());
		else
			s_interface = {};
	}

	bool HasNativeSoftwareKeyboard()
	{
		return s_interface.show != nullptr;
	}

	void ShutdownSoftwareKeyboard()
	{
		std::lock_guard lock(s_mutex);
		if (s_session && s_interface.hide) s_interface.hide(s_session);
		s_session = 0;
		swkbd::setSwkbdCallbacks(nullptr);
		s_interface = {};
	}

	void PollSoftwareKeyboard()
	{
		std::lock_guard lock(s_mutex);
		if (!s_session || !s_interface.poll || !swkbd::hasKeyboardInputHook()) return;
		std::array<uint16_t, OMNI_TEXT_INPUT_CAPACITY> text{};
		uint32_t length = 0;
		bool confirmed = false;
		if (!s_interface.poll(s_session, text.data(), (uint32_t)text.size(), &length, &confirmed)) return;
		if (length >= text.size()) return;
		swkbd::setInputText(std::u16string(text.begin(), text.begin() + length));
		if (confirmed)
		{
			swkbd::keyInput(swkbd::RETURN_KEYCODE);
			LogInfo("Native software keyboard confirmed");
		}
	}
}
