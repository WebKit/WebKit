/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AudioMediaStreamTrackRendererUnit.h"

#if ENABLE(MEDIA_STREAM)

#include "AudioSampleDataSource.h"
#include "Logging.h"

namespace WebCore {

static AudioMediaStreamTrackRendererUnit::CreateInternalUnitFunction& getCreateInternalUnitFunction()
{
    static NeverDestroyed<AudioMediaStreamTrackRendererUnit::CreateInternalUnitFunction> function;
    return function;
}

void AudioMediaStreamTrackRendererUnit::setCreateInternalUnitFunction(CreateInternalUnitFunction&& function)
{
    getCreateInternalUnitFunction() = WTFMove(function);
}

static UniqueRef<AudioMediaStreamTrackRendererInternalUnit> createInternalUnit(AudioMediaStreamTrackRendererUnit& unit)
{
    AudioMediaStreamTrackRendererInternalUnit::RenderCallback callback = [&unit](auto sampleCount, auto& list, auto sampleTime, auto hostTime, auto& flags) {
        unit.render(sampleCount, list, sampleTime, hostTime, flags);
        return 0;
    };

    auto& function = getCreateInternalUnitFunction();
    if (function)
        return function(WTFMove(callback));

    return AudioMediaStreamTrackRendererInternalUnit::createLocalInternalUnit(WTFMove(callback));
}

AudioMediaStreamTrackRendererUnit& AudioMediaStreamTrackRendererUnit::singleton()
{
    static NeverDestroyed<AudioMediaStreamTrackRendererUnit> registry;
    return registry;
}

AudioMediaStreamTrackRendererUnit::AudioMediaStreamTrackRendererUnit()
    : m_internalUnit(createInternalUnit(*this))
{
}

AudioMediaStreamTrackRendererUnit::~AudioMediaStreamTrackRendererUnit()
{
    stop();
}

void AudioMediaStreamTrackRendererUnit::setAudioOutputDevice(const String& deviceID)
{
    m_internalUnit->setAudioOutputDevice(deviceID);
}

void AudioMediaStreamTrackRendererUnit::addSource(Ref<AudioSampleDataSource>&& source)
{
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::addSource");

    {
        auto locker = holdLock(m_sourcesLock);
        ASSERT(!m_sources.contains(source.get()));
        m_sources.add(WTFMove(source));
        m_sourcesCopy = copyToVector(m_sources);
        m_shouldUpdateRenderSources = true;
    }
    start();
}

void AudioMediaStreamTrackRendererUnit::removeSource(AudioSampleDataSource& source)
{
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::removeSource");

    bool shouldStop = false;
    {
        auto locker = holdLock(m_sourcesLock);
        m_sources.remove(source);
        shouldStop = m_sources.isEmpty();
        m_sourcesCopy = copyToVector(m_sources);
        m_shouldUpdateRenderSources = true;
    }
    if (shouldStop)
        stop();
}

void AudioMediaStreamTrackRendererUnit::start()
{
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::start");
    m_internalUnit->start();
}

void AudioMediaStreamTrackRendererUnit::stop()
{
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::stop");
    m_internalUnit->stop();
}

void AudioMediaStreamTrackRendererUnit::retrieveFormatDescription(CompletionHandler<void(const CAAudioStreamDescription*)>&& callback)
{
    m_internalUnit->retrieveFormatDescription(WTFMove(callback));
}

void AudioMediaStreamTrackRendererUnit::render(size_t sampleCount, AudioBufferList& ioData, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags& actionFlags)
{
    // For performance reasons, we forbid heap allocations while doing rendering on the audio thread.
    ForbidMallocUseForCurrentThreadScope forbidMallocUse;

    ASSERT(!isMainThread());
    if (m_shouldUpdateRenderSources) {
        auto locker = tryHoldLock(m_sourcesLock);
        if (!locker)
            return;

        m_renderSources = WTFMove(m_sourcesCopy);
        m_shouldUpdateRenderSources = false;
    }

    if (m_renderSources.isEmpty()) {
        actionFlags = kAudioUnitRenderAction_OutputIsSilence;
        return;
    }

    // Mix all sources.
    bool isFirstSource = true;
    for (auto& source : m_renderSources) {
        source->pullSamples(ioData, sampleCount, sampleTime, hostTime, isFirstSource ? AudioSampleDataSource::Copy : AudioSampleDataSource::Mix);
        isFirstSource = false;
    }
    return;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
