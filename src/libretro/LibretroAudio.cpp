#include "LibretroAudio.h"
#include "Cafe/OS/libs/snd_core/ax.h"
#include "Common/precompiled.h"
#include <mutex>
#include <vector>

extern std::shared_mutex g_audioMutex;
extern AudioAPIPtr g_tvAudio;
extern AudioAPIPtr g_padAudio;

namespace Libretro
{
	namespace
	{
		static retro_audio_sample_batch_t s_audio_batch_cb = nullptr;
		static retro_audio_sample_t s_audio_cb = nullptr;
	} // namespace

	void SetAudioSampleBatchCallback(retro_audio_sample_batch_t cb)
	{
		s_audio_batch_cb = cb;
	}

	void SetAudioSampleCallback(retro_audio_sample_t cb)
	{
		s_audio_cb = cb;
	}

	LibretroAudioAPI::LibretroAudioAPI(uint32 samplerate, uint32 channels, uint32 samples_per_block, uint32 bits_per_sample, bool isTV)
		: IAudioAPI(samplerate, channels, samples_per_block, bits_per_sample), m_isTV(isTV)
	{
		m_playing = true;
	}

	bool LibretroAudioAPI::NeedAdditionalBlocks() const
	{
		return true;
	}

	bool LibretroAudioAPI::FeedBlock(sint16* data)
	{
		if (!m_playing || !data)
			return true;

		// If TV audio, send directly to frontend audio callback
		if (m_isTV && s_audio_batch_cb)
		{
			// Cemu feeds stereo PCM 16-bit (2 channels * m_samplesPerBlock)
			s_audio_batch_cb(data, m_samplesPerBlock);
		}
		return true;
	}

	bool LibretroAudioAPI::Play()
	{
		m_playing = true;
		return true;
	}

	bool LibretroAudioAPI::Stop()
	{
		m_playing = false;
		return true;
	}

	void InitializeAudio()
	{
		constexpr int AX_FRAMES_PER_GROUP = 4;
		std::unique_lock lock(g_audioMutex);

		g_tvAudio = std::make_unique<LibretroAudioAPI>(
			48000,
			2,
			snd_core::AX_SAMPLES_PER_3MS_48KHZ * AX_FRAMES_PER_GROUP,
			16,
			true);
		g_tvAudio->SetVolume(100);

		g_padAudio = std::make_unique<LibretroAudioAPI>(
			48000,
			2,
			snd_core::AX_SAMPLES_PER_3MS_48KHZ * AX_FRAMES_PER_GROUP,
			16,
			false);
		g_padAudio->SetVolume(100);
	}

	void ShutdownAudio()
	{
		std::unique_lock lock(g_audioMutex);
		g_tvAudio.reset();
		g_padAudio.reset();
	}
} // namespace Libretro
