/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaStreamAudioSource.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioBus.h"
#include "CAAudioStreamDescription.h"
#include "Logging.h"
#include "WebAudioBufferList.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

using namespace PAL;

namespace WebCore {

static inline CAAudioStreamDescription streamDescription(size_t sampleRate, size_t channelCount)
{
    bool isFloat = true;
    bool isBigEndian = false;
    bool isNonInterleaved = true;
    static const size_t sampleSize = 8 * sizeof(float);

    AudioStreamBasicDescription streamFormat;
    FillOutASBDForLPCM(streamFormat, sampleRate, channelCount, sampleSize, sampleSize, isFloat, isBigEndian, isNonInterleaved);
    return streamFormat;
}

static inline void copyChannelData(AudioChannel& channel, AudioBuffer& buffer, size_t numberOfFrames, bool isMuted)
{
    buffer.mDataByteSize = numberOfFrames * sizeof(float);
    buffer.mNumberChannels = 1;
    if (isMuted) {
        memset(buffer.mData, 0, buffer.mDataByteSize);
        return;
    }
    memcpy(buffer.mData, channel.data(), buffer.mDataByteSize);
}

void MediaStreamAudioSource::consumeAudio(AudioBus& bus, size_t numberOfFrames)
{
    if (bus.numberOfChannels() != 1 && bus.numberOfChannels() != 2) {
        RELEASE_LOG_ERROR(Media, "MediaStreamAudioSource::consumeAudio(%p) trying to consume bus with %u channels", this, bus.numberOfChannels());
        return;
    }

    CMTime startTime = CMTimeMake(m_numberOfFrames, m_currentSettings.sampleRate());
    auto mediaTime = PAL::toMediaTime(startTime);
    m_numberOfFrames += numberOfFrames;

    auto* audioBuffer = m_audioBuffer ? &downcast<WebAudioBufferList>(*m_audioBuffer) : nullptr;

    auto description = streamDescription(m_currentSettings.sampleRate(), bus.numberOfChannels());
    if (!audioBuffer || audioBuffer->channelCount() != bus.numberOfChannels()) {
        m_audioBuffer = makeUnique<WebAudioBufferList>(description, WTF::safeCast<uint32_t>(numberOfFrames));
        audioBuffer = &downcast<WebAudioBufferList>(*m_audioBuffer);
    } else
        audioBuffer->setSampleCount(numberOfFrames);

    for (size_t cptr = 0; cptr < bus.numberOfChannels(); ++cptr)
        copyChannelData(*bus.channel(cptr), *audioBuffer->buffer(cptr), numberOfFrames, muted());

    audioSamplesAvailable(mediaTime, *m_audioBuffer, description, numberOfFrames);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
