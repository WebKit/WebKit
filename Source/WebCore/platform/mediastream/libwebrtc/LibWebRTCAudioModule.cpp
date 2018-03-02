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

#include <wtf/MonotonicTime.h>

namespace WebCore {

LibWebRTCAudioModule::LibWebRTCAudioModule()
    : m_audioTaskRunner(rtc::Thread::Create())
{
    m_audioTaskRunner->SetName("WebKitWebRTCAudioModule", nullptr);
    m_audioTaskRunner->Start();
}

int32_t LibWebRTCAudioModule::RegisterAudioCallback(webrtc::AudioTransport* audioTransport)
{
    m_audioTransport = audioTransport;
    return 0;
}

void LibWebRTCAudioModule::OnMessage(rtc::Message* message)
{
    ASSERT_UNUSED(message, message->message_id == 1);
    StartPlayoutOnAudioThread();
}

int32_t LibWebRTCAudioModule::StartPlayout()
{
    if (!m_isPlaying && m_audioTaskRunner) {
        m_audioTaskRunner->Post(RTC_FROM_HERE, this, 1);
        m_isPlaying = true;
    }
    return 0;
}

int32_t LibWebRTCAudioModule::StopPlayout()
{
    if (m_isPlaying)
        m_isPlaying = false;
    return 0;
}

// libwebrtc uses 10ms frames.
const unsigned samplingRate = 48000;
const unsigned frameLengthMs = 10;
const unsigned samplesPerFrame = samplingRate * frameLengthMs / 1000;
const unsigned pollSamples = 5;
const unsigned pollInterval = 5 * frameLengthMs;
const unsigned channels = 2;
const unsigned bytesPerSample = 2;

void LibWebRTCAudioModule::StartPlayoutOnAudioThread()
{
    MonotonicTime startTime = MonotonicTime::now();
    while (true) {
        PollFromSource();

        MonotonicTime now = MonotonicTime::now();
        double sleepFor = pollInterval - remainder((now - startTime).milliseconds(), pollInterval);
        m_audioTaskRunner->SleepMs(sleepFor);
        if (!m_isPlaying)
            return;
    }
}

void LibWebRTCAudioModule::PollFromSource()
{
    if (!m_audioTransport)
        return;

    for (unsigned i = 0; i < pollSamples; i++) {
        int64_t elapsedTime = -1;
        int64_t ntpTime = -1;
        char data[(bytesPerSample * channels * samplesPerFrame)];
        m_audioTransport->PullRenderData(bytesPerSample * 8, samplingRate, channels, samplesPerFrame, data, &elapsedTime, &ntpTime);
    }
}

} // namespace WebCore

#endif // USE(LIBWEBRTC)
