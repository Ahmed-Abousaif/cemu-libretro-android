#pragma once

#include "audio/IAudioAPI.h"
#include "libretro.h"

namespace Libretro
{
	void SetAudioSampleBatchCallback(retro_audio_sample_batch_t cb);
	void SetAudioSampleCallback(retro_audio_sample_t cb);

	void InitializeAudio();
	void ShutdownAudio();

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

	private:
		bool m_isTV;
	};
} // namespace Libretro
