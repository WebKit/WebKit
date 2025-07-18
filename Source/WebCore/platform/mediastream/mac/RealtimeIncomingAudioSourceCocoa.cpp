/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeIncomingAudioSourceCocoa.h"

#if USE(LIBWEBRTC)

#include "AudioStreamDescription.h"
#include "CAAudioStreamDescription.h"
#include "LibWebRTCAudioFormat.h"
#include "LibWebRTCAudioModule.h"
#include "Logging.h"
#include <pal/avfoundation/MediaTimeAVFoundation.h>

#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

Ref<RealtimeIncomingAudioSource> RealtimeIncomingAudioSource::create(webrtc::scoped_refptr<webrtc::AudioTrackInterface>&& audioTrack, String&& audioTrackId)
{
    auto source = RealtimeIncomingAudioSourceCocoa::create(WTFMove(audioTrack), WTFMove(audioTrackId));
    source->start();
    return WTFMove(source);
}

Ref<RealtimeIncomingAudioSourceCocoa> RealtimeIncomingAudioSourceCocoa::create(webrtc::scoped_refptr<webrtc::AudioTrackInterface>&& audioTrack, String&& audioTrackId)
{
    return adoptRef(*new RealtimeIncomingAudioSourceCocoa(WTFMove(audioTrack), WTFMove(audioTrackId)));
}

static inline AudioStreamBasicDescription streamDescription(size_t sampleRate, size_t channelCount)
{
    AudioStreamBasicDescription streamFormat;
    FillOutASBDForLPCM(streamFormat, sampleRate, channelCount, LibWebRTCAudioFormat::sampleSize, LibWebRTCAudioFormat::sampleSize, LibWebRTCAudioFormat::isFloat, LibWebRTCAudioFormat::isBigEndian, LibWebRTCAudioFormat::isNonInterleaved);
    return streamFormat;
}

RealtimeIncomingAudioSourceCocoa::RealtimeIncomingAudioSourceCocoa(webrtc::scoped_refptr<webrtc::AudioTrackInterface>&& audioTrack, String&& audioTrackId)
    : RealtimeIncomingAudioSource(WTFMove(audioTrack), WTFMove(audioTrackId))
    , m_sampleRate(LibWebRTCAudioFormat::sampleRate)
    , m_numberOfChannels(1)
    , m_streamDescription(streamDescription(m_sampleRate, m_numberOfChannels))
    , m_audioBufferList(makeUnique<WebAudioBufferList>(m_streamDescription))
#if !RELEASE_LOG_DISABLED
    , m_logTimer(*this, &RealtimeIncomingAudioSourceCocoa::logTimerFired)
#endif
{
}

void RealtimeIncomingAudioSourceCocoa::startProducingData()
{
    RealtimeIncomingAudioSource::startProducingData();
#if !RELEASE_LOG_DISABLED
    m_logTimer.startRepeating(LogTimerInterval);
#endif
}

void RealtimeIncomingAudioSourceCocoa::stopProducingData()
{
#if !RELEASE_LOG_DISABLED
    m_logTimer.stop();
#endif
    RealtimeIncomingAudioSource::stopProducingData();
}

#if !RELEASE_LOG_DISABLED
void RealtimeIncomingAudioSourceCocoa::logTimerFired()
{
    if (!m_lastChunksReceived || (m_chunksReceived - m_lastChunksReceived) >= ChunksReceivedCountForLogging) {
        m_lastChunksReceived = m_chunksReceived;
        ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "chunk ", m_chunksReceived);
    }
    if (m_audioFormatChanged) {
        ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "new audio buffer list for sampleRate ", m_sampleRate, " and ", m_numberOfChannels, " channel(s)");
        m_audioFormatChanged = false;
    }
}
#endif

void RealtimeIncomingAudioSourceCocoa::OnData(const void* audioData, int bitsPerSample, int sampleRate, size_t numberOfChannels, size_t numberOfFrames)
{
    ++m_chunksReceived;

    static constexpr size_t initialSampleRate = 16000;
    static constexpr size_t initialChunksReceived = 20;
    // We usually receive some initial callbacks with no data at 16000, then we got real data at the actual sample rate.
    // To limit reallocations, let's skip these initial calls.
    if (m_chunksReceived < initialChunksReceived && sampleRate == initialSampleRate)
        return;

    if (!m_audioBufferList || m_numberOfChannels != numberOfChannels || m_sampleRate != sampleRate) {
#if !RELEASE_LOG_DISABLED
        m_audioFormatChanged = true;
#endif

        m_sampleRate = sampleRate;
        m_numberOfChannels = numberOfChannels;
        m_streamDescription = streamDescription(sampleRate, numberOfChannels);

        {
            DisableMallocRestrictionsForCurrentThreadScope scope;
            m_audioBufferList = makeUnique<WebAudioBufferList>(m_streamDescription);
        }
        if (m_sampleRate && m_numberOfFrames)
            m_numberOfFrames = m_numberOfFrames * sampleRate / m_sampleRate;
        else
            m_numberOfFrames = 0;
    }

    CMTime startTime = PAL::CMTimeMake(audioModule() ? audioModule()->currentAudioSampleCount() : m_numberOfFrames, LibWebRTCAudioFormat::sampleRate);
    auto mediaTime = PAL::toMediaTime(startTime);
    m_numberOfFrames += numberOfFrames;

    auto& bufferList = *m_audioBufferList->buffer(0);
    bufferList.mDataByteSize = numberOfChannels * numberOfFrames * bitsPerSample / 8;
    bufferList.mNumberChannels = numberOfChannels;
    bufferList.mData = const_cast<void*>(audioData);

    audioSamplesAvailable(mediaTime, *m_audioBufferList, m_streamDescription, numberOfFrames);
}

}

#endif // USE(LIBWEBRTC)
