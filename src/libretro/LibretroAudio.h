#pragma once

#include "audio/IAudioAPI.h"
#include "libretro.h"
#include <atomic>
#include <vector>

namespace Libretro
{
	void SetAudioSampleBatchCallback(retro_audio_sample_batch_t cb);
	void SetAudioSampleCallback(retro_audio_sample_t cb);

	void InitializeAudio();
	void ShutdownAudio();
	void DrainAudio();

	class LibretroAudioAPI : public IAudioAPI
	{
	public:
		LibretroAudioAPI(uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample, bool isTV);
		~LibretroAudioAPI() override = default;

		AudioAPI GetType() const override { return AudioAPI::Cubeb; }
		bool NeedAdditionalBlocks() const override;
		bool FeedBlock(sint16* data) override;
		bool Play() override;
		bool Stop() override;
		void DrainToFrontend();

	private:
		uint32 FilledBlocks() const;

		bool m_isTV;
		std::vector<std::vector<sint16>> m_ring;
		std::atomic<uint32> m_read{0};
		std::atomic<uint32> m_write{0};
		std::atomic<bool> m_primed{false};
	};
} // namespace Libretro
