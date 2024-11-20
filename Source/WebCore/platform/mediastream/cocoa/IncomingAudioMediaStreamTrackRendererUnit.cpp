/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "IncomingAudioMediaStreamTrackRendererUnit.h"

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)

#include "AudioMediaStreamTrackRendererUnit.h"
#include "AudioSampleDataSource.h"
#include "LibWebRTCAudioFormat.h"
#include "LibWebRTCAudioModule.h"
#include "Logging.h"
#include "WebAudioBufferList.h"
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WorkQueue.h>

#include <pal/cf/CoreMediaSoftLink.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(IncomingAudioMediaStreamTrackRendererUnit);

IncomingAudioMediaStreamTrackRendererUnit::IncomingAudioMediaStreamTrackRendererUnit(LibWebRTCAudioModule& audioModule)
    : m_audioModule(audioModule)
    , m_queue(WorkQueue::create("IncomingAudioMediaStreamTrackRendererUnit"_s, WorkQueue::QOS::UserInitiated))
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(LoggerHelper::uniqueLogIdentifier())
#endif
{
}

IncomingAudioMediaStreamTrackRendererUnit::~IncomingAudioMediaStreamTrackRendererUnit()
{
    ASSERT(isMainThread());
}

void IncomingAudioMediaStreamTrackRendererUnit::setAudioOutputDevice(const String& deviceID)
{
    AudioMediaStreamTrackRendererUnit::singleton().setAudioOutputDevice(deviceID);
}

void IncomingAudioMediaStreamTrackRendererUnit::addResetObserver(ResetObserver& observer)
{
    AudioMediaStreamTrackRendererUnit::singleton().addResetObserver(observer);
}

void IncomingAudioMediaStreamTrackRendererUnit::addSource(Ref<AudioSampleDataSource>&& source)
{
#if !RELEASE_LOG_DISABLED
    source->logger().logAlways(LogWebRTC, "IncomingAudioMediaStreamTrackRendererUnit::addSource ", source->logIdentifier());
#endif
    ASSERT(isMainThread());
#if !RELEASE_LOG_DISABLED
        if (!m_logger)
            m_logger = &source->logger();
#endif

    ASSERT(source->outputDescription());
    if (!source->outputDescription())
        return;
    auto outputDescription = *source->outputDescription();

    ASSERT(!m_sources.contains(source.get()));
    bool shouldStart = m_sources.isEmpty();
    m_sources.add(WTFMove(source));

    postTask([this, newSources = copyToVector(m_sources), shouldStart, outputDescription]() mutable {
        m_renderSources = WTFMove(newSources);
        if (!shouldStart)
            return;

        m_outputStreamDescription = outputDescription;
        m_audioBufferList = makeUnique<WebAudioBufferList>(*m_outputStreamDescription);
        m_sampleCount = m_outputStreamDescription->sampleRate() / 100;
        m_audioBufferList->setSampleCount(m_sampleCount);
        m_mixedSource = AudioSampleDataSource::create(m_outputStreamDescription->sampleRate() * 0.5, *this, LibWebRTCAudioModule::PollSamplesCount + 1);
        m_mixedSource->setInputFormat(outputDescription);
        m_mixedSource->setOutputFormat(outputDescription);
        m_writeCount = 0;
        callOnMainThread([protectedThis = Ref { *this }, source = m_mixedSource]() mutable {
            protectedThis->m_registeredMixedSource = WTFMove(source);
            protectedThis->start();
        });
    });
}

void IncomingAudioMediaStreamTrackRendererUnit::removeSource(AudioSampleDataSource& source)
{
#if !RELEASE_LOG_DISABLED
    source.logger().logAlways(LogWebRTC, "IncomingAudioMediaStreamTrackRendererUnit::removeSource ", source.logIdentifier());
#endif
    ASSERT(isMainThread());

    bool shouldStop = !m_sources.isEmpty();
    m_sources.remove(source);
    shouldStop &= m_sources.isEmpty();

    postTask([this, newSources = copyToVector(m_sources), shouldStop]() mutable {
        m_renderSources = WTFMove(newSources);
        if (!shouldStop)
            return;

        callOnMainThread([protectedThis = Ref { *this }] {
            protectedThis->stop();
        });
    });

}

void IncomingAudioMediaStreamTrackRendererUnit::start()
{
    RELEASE_LOG(WebRTC, "IncomingAudioMediaStreamTrackRendererUnit::start");
    ASSERT(isMainThread());

    AudioMediaStreamTrackRendererUnit::singleton().addSource(*m_registeredMixedSource);
    m_audioModule.get()->startIncomingAudioRendering();
}

void IncomingAudioMediaStreamTrackRendererUnit::stop()
{
    RELEASE_LOG(WebRTC, "IncomingAudioMediaStreamTrackRendererUnit::stop");
    ASSERT(isMainThread());

    AudioMediaStreamTrackRendererUnit::singleton().removeSource(*m_registeredMixedSource);
    m_registeredMixedSource = nullptr;
    m_audioModule.get()->stopIncomingAudioRendering();
}

void IncomingAudioMediaStreamTrackRendererUnit::postTask(Function<void()>&& callback)
{
    m_queue->dispatch([protectedThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
        callback();
    });
}

void IncomingAudioMediaStreamTrackRendererUnit::newAudioChunkPushed(uint64_t currentAudioSampleCount)
{
    DisableMallocRestrictionsForCurrentThreadScope disableMallocRestrictions;
    postTask([this, currentAudioSampleCount] {
        renderAudioChunk(currentAudioSampleCount);
    });
}

void IncomingAudioMediaStreamTrackRendererUnit::renderAudioChunk(uint64_t currentAudioSampleCount)
{
    ASSERT(!isMainThread());

    uint64_t timeStamp = currentAudioSampleCount * m_outputStreamDescription->sampleRate() / LibWebRTCAudioFormat::sampleRate;

    // Mix all sources.
    bool hasCopiedData = false;
    for (auto& source : m_renderSources) {
        if (source->pullAvailableSampleChunk(*m_audioBufferList, m_sampleCount, timeStamp, hasCopiedData ? AudioSampleDataSource::Mix : AudioSampleDataSource::Copy))
            hasCopiedData = true;
    }

    CMTime startTime = PAL::CMTimeMake(m_writeCount, m_outputStreamDescription->sampleRate());
    if (hasCopiedData)
        m_mixedSource->pushSamples(PAL::toMediaTime(startTime), *m_audioBufferList, m_sampleCount);
    m_writeCount += m_sampleCount;
}

#if !RELEASE_LOG_DISABLED
const Logger& IncomingAudioMediaStreamTrackRendererUnit::logger() const
{
    return *m_logger;
}

WTFLogChannel& IncomingAudioMediaStreamTrackRendererUnit::logChannel() const
{
    return LogWebRTC;
}

uint64_t IncomingAudioMediaStreamTrackRendererUnit::logIdentifier() const
{
    return m_logIdentifier;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)
