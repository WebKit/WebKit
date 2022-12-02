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

AudioMediaStreamTrackRendererUnit& AudioMediaStreamTrackRendererUnit::singleton()
{
    static NeverDestroyed<AudioMediaStreamTrackRendererUnit> registry;
    return registry;
}

AudioMediaStreamTrackRendererUnit::AudioMediaStreamTrackRendererUnit()
    : m_internalUnit(AudioMediaStreamTrackRendererInternalUnit::create(*this))
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
#if !RELEASE_LOG_DISABLED
    source->logger().logAlways(LogWebRTC, "AudioMediaStreamTrackRendererUnit::addSource ", source->logIdentifier());
#endif
    ASSERT(isMainThread());

    ASSERT(!m_sources.contains(source.get()));
    bool shouldStart = m_sources.isEmpty();
    m_sources.add(WTFMove(source));

    {
        Locker locker { m_pendingRenderSourcesLock };
        m_pendingRenderSources = copyToVector(m_sources);
        m_hasPendingRenderSources = true;
    }

    if (shouldStart)
        start();
}

void AudioMediaStreamTrackRendererUnit::removeSource(AudioSampleDataSource& source)
{
#if !RELEASE_LOG_DISABLED
    source.logger().logAlways(LogWebRTC, "AudioMediaStreamTrackRendererUnit::removeSource ", source.logIdentifier());
#endif
    ASSERT(isMainThread());

    bool shouldStop = !m_sources.isEmpty();
    m_sources.remove(source);
    shouldStop &= m_sources.isEmpty();

    {
        Locker locker { m_pendingRenderSourcesLock };
        m_pendingRenderSources = copyToVector(m_sources);
        m_hasPendingRenderSources = true;
    }

    if (shouldStop)
        stop();
}

void AudioMediaStreamTrackRendererUnit::start()
{
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::start");
    ASSERT(isMainThread());

    m_internalUnit->start();
}

void AudioMediaStreamTrackRendererUnit::stop()
{
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::stop");
    ASSERT(isMainThread());

    m_internalUnit->stop();
}

void AudioMediaStreamTrackRendererUnit::reset()
{
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::reset");
    if (!isMainThread()) {
        callOnMainThread([weakThis = WeakPtr { this }] {
            if (weakThis)
                weakThis->reset();
        });
        return;
    }

    m_resetObservers.forEach([](auto& observer) {
        observer();
    });
}

void AudioMediaStreamTrackRendererUnit::retrieveFormatDescription(CompletionHandler<void(std::optional<CAAudioStreamDescription>)>&& callback)
{
    ASSERT(isMainThread());
    m_internalUnit->retrieveFormatDescription(WTFMove(callback));
}

void AudioMediaStreamTrackRendererUnit::updateRenderSourcesIfNecessary()
{
    if (!m_pendingRenderSourcesLock.tryLock())
        return;

    Locker locker { AdoptLock, m_pendingRenderSourcesLock };
    if (!m_hasPendingRenderSources)
        return;

    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    m_renderSources = WTFMove(m_pendingRenderSources);
    m_hasPendingRenderSources = false;
}

OSStatus AudioMediaStreamTrackRendererUnit::render(size_t sampleCount, AudioBufferList& ioData, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags& actionFlags)
{
    // For performance reasons, we forbid heap allocations while doing rendering on the audio thread.
    ForbidMallocUseForCurrentThreadScope forbidMallocUse;

    ASSERT(!isMainThread());

    updateRenderSourcesIfNecessary();

    // Mix all sources.
    bool hasCopiedData = false;
    for (auto& source : m_renderSources) {
        if (source->pullSamples(ioData, sampleCount, sampleTime, hostTime, hasCopiedData ? AudioSampleDataSource::Mix : AudioSampleDataSource::Copy))
            hasCopiedData = true;
    }
    if (!hasCopiedData)
        actionFlags = kAudioUnitRenderAction_OutputIsSilence;
    return 0;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
