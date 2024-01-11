/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "LibWebRTCAudioModule.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCAudioFormat.h"
#include "Logging.h"
#include <wtf/FastMalloc.h>

#if PLATFORM(COCOA)
#include "IncomingAudioMediaStreamTrackRendererUnit.h"
#endif

namespace WebCore {

LibWebRTCAudioModule::LibWebRTCAudioModule()
    : m_queue(WorkQueue::create("WebKitWebRTCAudioModule", WorkQueue::QOS::UserInteractive))
    , m_logTimer(makeUnique<Timer>(*this, &LibWebRTCAudioModule::logTimerFired))
{
    ASSERT(isMainThread());
}

LibWebRTCAudioModule::~LibWebRTCAudioModule()
{
    ASSERT(!m_logTimer);
}

int32_t LibWebRTCAudioModule::Terminate()
{
    callOnMainThread([this, protectedThis = rtc::scoped_refptr<webrtc::AudioDeviceModule>(this)] {
        m_logTimer = nullptr;
    });
    return 0;
}

int32_t LibWebRTCAudioModule::RegisterAudioCallback(webrtc::AudioTransport* audioTransport)
{
    RELEASE_LOG(WebRTC, "LibWebRTCAudioModule::RegisterAudioCallback %d", !!audioTransport);

    m_audioTransport = audioTransport;
    return 0;
}

int32_t LibWebRTCAudioModule::StartPlayout()
{
    RELEASE_LOG(WebRTC, "LibWebRTCAudioModule::StartPlayout %d", m_isPlaying);

    if (m_isPlaying)
        return 0;

    m_isPlaying = true;
    callOnMainThread([this, protectedThis = rtc::scoped_refptr<webrtc::AudioDeviceModule>(this)] {
        m_logTimer->startRepeating(logTimerInterval);
    });

    m_queue->dispatch([this, protectedThis = rtc::scoped_refptr<webrtc::AudioDeviceModule>(this)] {
        m_pollingTime = MonotonicTime::now();
#if PLATFORM(COCOA)
        m_currentAudioSampleCount = 0;
#endif
        pollAudioData();
    });
    return 0;
}

int32_t LibWebRTCAudioModule::StopPlayout()
{
    RELEASE_LOG(WebRTC, "LibWebRTCAudioModule::StopPlayout %d", m_isPlaying);

    m_isPlaying = false;
    callOnMainThread([this, protectedThis = rtc::scoped_refptr<webrtc::AudioDeviceModule>(this)] {
        m_logTimer->stop();
    });
    return 0;
}

void LibWebRTCAudioModule::logTimerFired()
{
    RELEASE_LOG_IF(m_timeSpent, WebRTC, "LibWebRTCAudioModule::pollAudioData, polling took too much time: %d ms", m_timeSpent);
    m_timeSpent = 0;
}

// libwebrtc uses 10ms frames.
const unsigned frameLengthMs = 1000 * LibWebRTCAudioFormat::chunkSampleCount / LibWebRTCAudioFormat::sampleRate;
const unsigned pollInterval = LibWebRTCAudioModule::PollSamplesCount * frameLengthMs;
const unsigned channels = 2;

Seconds LibWebRTCAudioModule::computeDelayUntilNextPolling()
{
    auto now = MonotonicTime::now();
    auto delayUntilNextPolling = m_pollingTime + Seconds::fromMilliseconds(pollInterval) - now;
    if (delayUntilNextPolling.milliseconds() < 0) {
        m_timeSpent = (now - m_pollingTime).milliseconds();
        delayUntilNextPolling = 0_s;
    }
    m_pollingTime = now + delayUntilNextPolling;
    return delayUntilNextPolling;
}

void LibWebRTCAudioModule::pollAudioData()
{
    if (!m_isPlaying)
        return;

    Function<void()> nextPollFunction = [this, protectedThis = rtc::scoped_refptr<webrtc::AudioDeviceModule>(this)] {
        pollAudioData();
    };

    {
        // For performance reasons, we forbid heap allocations while doing rendering on the webrtc audio thread.
        ForbidMallocUseForCurrentThreadScope forbidMallocUse;

        pollFromSource();
    }
    m_queue->dispatchAfter(computeDelayUntilNextPolling(), WTFMove(nextPollFunction));
}

void LibWebRTCAudioModule::pollFromSource()
{
    if (!m_audioTransport)
        return;

    for (unsigned i = 0; i < PollSamplesCount; i++) {
        int64_t elapsedTime = -1;
        int64_t ntpTime = -1;
        char data[LibWebRTCAudioFormat::sampleByteSize * channels * LibWebRTCAudioFormat::chunkSampleCount];
        m_audioTransport->PullRenderData(LibWebRTCAudioFormat::sampleByteSize * 8, LibWebRTCAudioFormat::sampleRate, channels, LibWebRTCAudioFormat::chunkSampleCount, data, &elapsedTime, &ntpTime);
#if PLATFORM(COCOA)
        if (m_isRenderingIncomingAudio)
            m_incomingAudioMediaStreamTrackRendererUnit->newAudioChunkPushed(m_currentAudioSampleCount);
        m_currentAudioSampleCount += LibWebRTCAudioFormat::chunkSampleCount;
#endif
    }
}

#if PLATFORM(COCOA)
BaseAudioMediaStreamTrackRendererUnit& LibWebRTCAudioModule::incomingAudioMediaStreamTrackRendererUnit()
{
    if (!m_incomingAudioMediaStreamTrackRendererUnit)
        m_incomingAudioMediaStreamTrackRendererUnit = makeUnique<IncomingAudioMediaStreamTrackRendererUnit>(*this);
    return *m_incomingAudioMediaStreamTrackRendererUnit;
}
#endif

} // namespace WebCore

#endif // USE(LIBWEBRTC)
