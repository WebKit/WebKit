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
#include "AudioUtilities.h"
#include "CAAudioStreamDescription.h"
#include "Logging.h"
#include "WebAudioBufferList.h"
#include <CoreAudio/CoreAudioTypes.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/cf/CoreAudioExtras.h>
#include <wtf/StdLibExtras.h>

#include <pal/cf/CoreMediaSoftLink.h>
#include "CoreVideoSoftLink.h"

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
    RELEASE_ASSERT(buffer.mDataByteSize <= numberOfFrames * sizeof(float), "copyChannelData() given an AudioBuffer with insufficient size");
    buffer.mNumberChannels = 1;
    if (isMuted) {
        zeroSpan(mutableSpan<uint8_t>(buffer));
        return;
    }

    auto destination = mutableSpan<uint8_t>(buffer);
    auto source = asByteSpan(channel.span());
    ASSERT(destination.size() >= source.size(), "copyChannelData() given a larger source than destination");

    memcpySpan(destination, source.first(buffer.mDataByteSize));
    if (destination.size() > source.size())
        zeroSpan(destination.last(destination.size() - source.size()));
}

void MediaStreamAudioSource::setNumberOfChannels(unsigned numberOfChannels)
{
    if (numberOfChannels != 1 && numberOfChannels != 2) {
        RELEASE_LOG_ERROR(Media, "MediaStreamAudioSource::setNumberOfChannels(%p) trying to configure source with %u channels", this, numberOfChannels);
        m_audioBuffer = nullptr;
        return;
    }

    if (auto* audioBuffer = downcast<WebAudioBufferList>(m_audioBuffer.get()); audioBuffer && audioBuffer->channelCount() == numberOfChannels)
        return;

    auto description = streamDescription(m_currentSettings.sampleRate(), numberOfChannels);
    auto numberOfFrames = AudioUtilities::renderQuantumSize;

    // Heap allocations are forbidden on the audio thread for performance reasons so we need to
    // explicitly allow the following allocation(s).
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    m_audioBuffer = makeUnique<WebAudioBufferList>(description, numberOfFrames);
}

void MediaStreamAudioSource::consumeAudio(AudioBus& bus, size_t numberOfFrames)
{
    CMTime startTime = PAL::CMTimeMake(m_numberOfFrames, m_currentSettings.sampleRate());
    auto mediaTime = PAL::toMediaTime(startTime);
    m_numberOfFrames += numberOfFrames;

    auto* audioBuffer = m_audioBuffer ? &downcast<WebAudioBufferList>(*m_audioBuffer) : nullptr;

    if (!audioBuffer || audioBuffer->bufferCount() != bus.numberOfChannels()) {
        ASSERT_NOT_REACHED("MediaStreamAudioSource::consumeAudio without being initialized with a valid number of channels");
        return;
    }

    audioBuffer->setSampleCount(numberOfFrames);

    for (size_t cptr = 0; cptr < bus.numberOfChannels(); ++cptr)
        copyChannelData(*bus.channel(cptr), *audioBuffer->buffer(cptr), numberOfFrames, muted());

    auto description = streamDescription(m_currentSettings.sampleRate(), bus.numberOfChannels());
    audioSamplesAvailable(mediaTime, *m_audioBuffer, description, numberOfFrames);
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
