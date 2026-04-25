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

#include "AudioMediaStreamTrackRenderer.h"
#include "AudioSampleDataSource.h"
#include "Logging.h"

namespace WebCore {

AudioMediaStreamTrackRendererUnit& AudioMediaStreamTrackRendererUnit::singleton()
{
    static LazyNeverDestroyed<std::unique_ptr<AudioMediaStreamTrackRendererUnit>> sharedUnit;
    static std::once_flag onceKey;
    std::call_once(onceKey, [&] {
        sharedUnit.construct(std::unique_ptr<AudioMediaStreamTrackRendererUnit>(new AudioMediaStreamTrackRendererUnit()));
    });
    return *sharedUnit.get();
}

bool AudioMediaStreamTrackRendererUnit::supportsPerDeviceRendering()
{
#if !PLATFORM(IOS_FAMILY)
    return true;
#else
    return false;
#endif
}

AudioMediaStreamTrackRendererUnit::AudioMediaStreamTrackRendererUnit()
    : m_deleteUnitsTimer([] { AudioMediaStreamTrackRendererUnit::singleton().deleteUnitsIfPossible(); })
{
}

AudioMediaStreamTrackRendererUnit::~AudioMediaStreamTrackRendererUnit() = default;

void AudioMediaStreamTrackRendererUnit::setLastDeviceUsed(const String& deviceID)
{
    if (supportsPerDeviceRendering())
        return;

    UNUSED_PARAM(deviceID);
    Ref unit = ensureDeviceUnit(AudioMediaStreamTrackRenderer::defaultDeviceID());
    unit->setLastDeviceUsed(deviceID);
}

void AudioMediaStreamTrackRendererUnit::deleteUnitsIfPossible()
{
    assertIsMainThread();

    m_units.removeIf([] (auto& keyValue) {
        if (keyValue.value->isDefault() || keyValue.value->hasSources())
            return false;

        Ref unit = keyValue.value;
        unit->close();
        return true;
    });
}

Ref<AudioMediaStreamTrackRendererUnit::Unit> AudioMediaStreamTrackRendererUnit::ensureDeviceUnit(const String& identifier)
{
    String deviceID = supportsPerDeviceRendering() ? identifier : AudioMediaStreamTrackRenderer::defaultDeviceID();

    assertIsMainThread();

    return m_units.ensure(deviceID, [&deviceID] {
        return Unit::create(deviceID);
    }).iterator->value;
}

RefPtr<AudioMediaStreamTrackRendererUnit::Unit> AudioMediaStreamTrackRendererUnit::getDeviceUnit(const String& identifier)
{
    String deviceID = supportsPerDeviceRendering() ? identifier : AudioMediaStreamTrackRenderer::defaultDeviceID();

    assertIsMainThread();

    auto iterator = m_units.find(deviceID);
    if (iterator == m_units.end())
        return { };
    return iterator->value.ptr();
}

void AudioMediaStreamTrackRendererUnit::addSource(const String& deviceID, Ref<AudioSampleDataSource>&& source)
{
    setLastDeviceUsed(deviceID);

    Ref unit = ensureDeviceUnit(deviceID);
    unit->addSource(WTF::move(source));
}

void AudioMediaStreamTrackRendererUnit::removeSource(const String& deviceID, AudioSampleDataSource& source)
{
    assertIsMainThread();

    RefPtr unit = getDeviceUnit(deviceID);
    if (!unit)
        return;

    static constexpr Seconds deleteUnitDelay = 10_s;
    if (unit->removeSource(source) && !unit->isDefault())
        m_deleteUnitsTimer.startOneShot(deleteUnitDelay);
}

void AudioMediaStreamTrackRendererUnit::addResetObserver(const String& deviceID, ResetObserver& observer)
{
    Ref unit = ensureDeviceUnit(deviceID);
    unit->addResetObserver(observer);
}

void AudioMediaStreamTrackRendererUnit::retrieveFormatDescription(CompletionHandler<void(std::optional<CAAudioStreamDescription>)>&& callback)
{
    assertIsMainThread();

    Ref unit = ensureDeviceUnit(AudioMediaStreamTrackRenderer::defaultDeviceID());
    unit->retrieveFormatDescription(WTF::move(callback));
}

AudioMediaStreamTrackRendererUnit::Unit::Unit(const String& deviceID)
    : m_internalUnit(AudioMediaStreamTrackRendererInternalUnit::create(deviceID, *this))
    , m_isDefaultUnit(deviceID == AudioMediaStreamTrackRenderer::defaultDeviceID())
{
}

AudioMediaStreamTrackRendererUnit::Unit::~Unit()
{
    stop();
}

void AudioMediaStreamTrackRendererUnit::Unit::close()
{
    assertIsMainThread();
    m_internalUnit->close();
}

void AudioMediaStreamTrackRendererUnit::Unit::addSource(Ref<AudioSampleDataSource>&& source)
{
#if !RELEASE_LOG_DISABLED
    protect(source->logger())->logAlways(LogWebRTC, "AudioMediaStreamTrackRendererUnit::addSource ", source->logIdentifier());
#endif
    assertIsMainThread();

    ASSERT(!m_sources.contains(source.get()));
    bool shouldStart = m_sources.isEmpty();
    m_sources.add(WTF::move(source));

    {
        Locker locker { m_pendingRenderSourcesLock };
        m_pendingRenderSources = copyToVector(m_sources);
        m_hasPendingRenderSources = true;
    }

    if (shouldStart)
        start();
}

bool AudioMediaStreamTrackRendererUnit::Unit::removeSource(AudioSampleDataSource& source)
{
#if !RELEASE_LOG_DISABLED
    protect(source.logger())->logAlways(LogWebRTC, "AudioMediaStreamTrackRendererUnit::removeSource ", source.logIdentifier());
#endif
    assertIsMainThread();

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
    return shouldStop;
}

void AudioMediaStreamTrackRendererUnit::Unit::addResetObserver(ResetObserver& observer)
{
    assertIsMainThread();
    m_resetObservers.add(observer);
}

void AudioMediaStreamTrackRendererUnit::Unit::setLastDeviceUsed(const String& deviceID)
{
    assertIsMainThread();
    m_internalUnit->setLastDeviceUsed(deviceID);
}

void AudioMediaStreamTrackRendererUnit::Unit::retrieveFormatDescription(CompletionHandler<void(std::optional<CAAudioStreamDescription>)>&& callback)
{
    assertIsMainThread();
    m_internalUnit->retrieveFormatDescription(WTF::move(callback));
}

void AudioMediaStreamTrackRendererUnit::Unit::start()
{
    assertIsMainThread();
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::start");

    m_internalUnit->start();
}

void AudioMediaStreamTrackRendererUnit::Unit::stop()
{
    assertIsMainThread();
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::stop");

    m_internalUnit->stop();
}

void AudioMediaStreamTrackRendererUnit::Unit::reset()
{
    RELEASE_LOG(WebRTC, "AudioMediaStreamTrackRendererUnit::reset");
    if (!isMainThread()) {
        callOnMainThread([weakThis = ThreadSafeWeakPtr { *this }] {
            if (RefPtr strongThis = weakThis.get())
                strongThis->reset();
        });
        return;
    }

    assertIsMainThread();
    m_resetObservers.forEach([](auto& observer) {
        observer();
    });
}

void AudioMediaStreamTrackRendererUnit::Unit::updateRenderSourcesIfNecessary()
{
    if (!m_pendingRenderSourcesLock.tryLock())
        return;

    Locker locker { AdoptLock, m_pendingRenderSourcesLock };
    if (!m_hasPendingRenderSources)
        return;

    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    m_renderSources = WTF::move(m_pendingRenderSources);
    m_hasPendingRenderSources = false;
}

OSStatus AudioMediaStreamTrackRendererUnit::Unit::render(size_t sampleCount, AudioBufferList& ioData, uint64_t sampleTime, double hostTime, AudioUnitRenderActionFlags& actionFlags)
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
