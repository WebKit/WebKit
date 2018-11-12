/*
 * Copyright (C) 2017 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted, provided that the following conditions
 * are required to be met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. AND ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeOutgoingAudioSource.h"

#if USE(LIBWEBRTC)

#include "LibWebRTCAudioFormat.h"
#include "LibWebRTCProvider.h"

namespace WebCore {

RealtimeOutgoingAudioSource::RealtimeOutgoingAudioSource(Ref<MediaStreamTrackPrivate>&& source)
    : m_audioSource(WTFMove(source))
{
}

RealtimeOutgoingAudioSource::~RealtimeOutgoingAudioSource()
{
    ASSERT(m_sinks.isEmpty());
    stop();
}

void RealtimeOutgoingAudioSource::observeSource()
{
    m_audioSource->addObserver(*this);
    initializeConverter();
}

void RealtimeOutgoingAudioSource::unobserveSource()
{
    m_audioSource->removeObserver(*this);
}

bool RealtimeOutgoingAudioSource::setSource(Ref<MediaStreamTrackPrivate>&& newSource)
{
    auto locker = holdLock(m_sinksLock);
    bool hasSinks = !m_sinks.isEmpty();

    if (hasSinks)
        unobserveSource();
    m_audioSource = WTFMove(newSource);
    if (hasSinks)
        observeSource();

    return true;
}

void RealtimeOutgoingAudioSource::initializeConverter()
{
    m_muted = m_audioSource->muted();
    m_enabled = m_audioSource->enabled();
}

void RealtimeOutgoingAudioSource::sourceMutedChanged()
{
    m_muted = m_audioSource->muted();
}

void RealtimeOutgoingAudioSource::sourceEnabledChanged()
{
    m_enabled = m_audioSource->enabled();
}

void RealtimeOutgoingAudioSource::AddSink(webrtc::AudioTrackSinkInterface* sink)
{
    {
    auto locker = holdLock(m_sinksLock);
    if (!m_sinks.add(sink) || m_sinks.size() != 1)
        return;
    }

    callOnMainThread([protectedThis = makeRef(*this)]() {
        protectedThis->observeSource();
    });
}

void RealtimeOutgoingAudioSource::RemoveSink(webrtc::AudioTrackSinkInterface* sink)
{
    {
    auto locker = holdLock(m_sinksLock);
    if (!m_sinks.remove(sink) || !m_sinks.isEmpty())
        return;
    }

    unobserveSource();
}

void RealtimeOutgoingAudioSource::sendAudioFrames(const void* audioData, int bitsPerSample, int sampleRate, size_t numberOfChannels, size_t numberOfFrames)
{
    auto locker = holdLock(m_sinksLock);
    for (auto sink : m_sinks)
        sink->OnData(audioData, bitsPerSample, sampleRate, numberOfChannels, numberOfFrames);
}


} // namespace WebCore

#endif // USE(LIBWEBRTC)
