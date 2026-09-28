#include "LibretroAudio.h"
#include "Cafe/OS/libs/snd_core/ax.h"
#include "Common/precompiled.h"
#include <algorithm>
#include <mutex>
#include <shared_mutex>
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
		static LibretroAudioAPI* s_tvAudio = nullptr;
		constexpr uint32 kDrainChunkFrames = 1024;
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
		if (m_isTV)
		{
			const uint32 samples = m_samplesPerBlock * m_channels;
			m_ring.resize(kBlockCount);
			for (auto& block : m_ring)
				block.resize(samples);
		}
	}

	uint32 LibretroAudioAPI::FilledBlocks() const
	{
		const uint32 write = m_write.load(std::memory_order_acquire);
		const uint32 read = m_read.load(std::memory_order_acquire);
		return write - read;
	}

	bool LibretroAudioAPI::NeedAdditionalBlocks() const
	{
		// Pad output is discarded. Returning true here keeps AX on the 2.9 ms fast path.
		if (!m_isTV)
			return false;
		// retro_run drains the ring every frame, so an empty ring is normal and
		// must not put AX back on the 2.9 ms clock. That clock produces audio
		// about 3% fast and the frontend buffer then sits high, which is heard
		// as late sound. Stay on the 3 ms clock once the delay watermark is hit.
		if (m_primed.load(std::memory_order_relaxed))
			return false;
		return FilledBlocks() < GetAudioDelay();
	}

	bool LibretroAudioAPI::FeedBlock(sint16* data)
	{
		if (!m_playing || !data)
			return true;
		if (!m_isTV)
			return true;

		const uint32 write = m_write.load(std::memory_order_relaxed);
		const uint32 read = m_read.load(std::memory_order_acquire);
		if (write - read >= kBlockCount)
			return false;

		auto& block = m_ring[write % kBlockCount];
		const uint32 samples = m_samplesPerBlock * m_channels;
		std::copy_n(data, samples, block.data());
		m_write.store(write + 1, std::memory_order_release);
		if (!m_primed.load(std::memory_order_relaxed) && (write + 1 - read) >= GetAudioDelay())
			m_primed.store(true, std::memory_order_relaxed);
		return true;
	}

	void LibretroAudioAPI::DrainToFrontend()
	{
		if (!m_isTV || !s_audio_batch_cb)
			return;
		uint32 read = m_read.load(std::memory_order_relaxed);
		const uint32 write = m_write.load(std::memory_order_acquire);
		while (read != write)
		{
			const sint16* pcm = m_ring[read % kBlockCount].data();
			uint32 framesLeft = m_samplesPerBlock;
			while (framesLeft > 0)
			{
				const uint32 chunk = std::min(framesLeft, kDrainChunkFrames);
				s_audio_batch_cb(pcm, chunk);
				pcm += chunk * m_channels;
				framesLeft -= chunk;
			}
			++read;
		}
		m_read.store(read, std::memory_order_release);
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
		s_tvAudio = static_cast<LibretroAudioAPI*>(g_tvAudio.get());

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
		s_tvAudio = nullptr;
		g_tvAudio.reset();
		g_padAudio.reset();
	}

	void DrainAudio()
	{
		std::shared_lock lock(g_audioMutex);
		if (s_tvAudio)
			s_tvAudio->DrainToFrontend();
	}
} // namespace Libretro
