/*
 * Copyright (C) 2017-2019 Apple Inc.
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

#pragma once


#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "MediaStreamTrackPrivate.h"
#include "Timer.h"

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/media_stream_interface.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/Lock.h>
#include <wtf/LoggerHelper.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace webrtc {
class AudioTrackInterface;
class AudioTrackSinkInterface;
}

namespace WebCore {

class RealtimeOutgoingAudioSource
    : public ThreadSafeRefCounted<RealtimeOutgoingAudioSource, WTF::DestructionThread::Main>
    , public webrtc::AudioSourceInterface
    , private MediaStreamTrackPrivate::Observer
    , private RealtimeMediaSource::AudioSampleObserver
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<RealtimeOutgoingAudioSource> create(Ref<MediaStreamTrackPrivate>&& audioSource);

    ~RealtimeOutgoingAudioSource();

    void start() { observeSource(); }
    void stop() { unobserveSource(); }

    void setSource(Ref<MediaStreamTrackPrivate>&&);
    MediaStreamTrackPrivate& source() const { return m_audioSource.get(); }

protected:
    explicit RealtimeOutgoingAudioSource(Ref<MediaStreamTrackPrivate>&&);

    bool isSilenced() const { return m_muted || !m_enabled; }

    void sendAudioFrames(const void* audioData, int bitsPerSample, int sampleRate, size_t numberOfChannels, size_t numberOfFrames);

#if !RELEASE_LOG_DISABLED
    // LoggerHelper API
    const Logger& logger() const final { return m_audioSource->logger(); }
    const void* logIdentifier() const final { return m_audioSource->logIdentifier(); }
    const char* logClassName() const final { return "RealtimeOutgoingAudioSource"; }
    WTFLogChannel& logChannel() const final;
#endif

private:
    // webrtc::AudioSourceInterface API
    void AddSink(webrtc::AudioTrackSinkInterface*) final;
    void RemoveSink(webrtc::AudioTrackSinkInterface*) final;

    void AddRef() const final { ref(); }
    rtc::RefCountReleaseStatus Release() const final
    {
        auto result = refCount() - 1;
        deref();
        return result ? rtc::RefCountReleaseStatus::kOtherRefsRemained : rtc::RefCountReleaseStatus::kDroppedLastRef;
    }

    SourceState state() const final { return kLive; }
    bool remote() const final { return false; }
    void RegisterObserver(webrtc::ObserverInterface*) final { }
    void UnregisterObserver(webrtc::ObserverInterface*) final { }

    void observeSource();
    void unobserveSource();

    void sourceMutedChanged();
    void sourceEnabledChanged();

    virtual bool isReachingBufferedAudioDataHighLimit() { return false; };
    virtual bool isReachingBufferedAudioDataLowLimit() { return false; };
    virtual bool hasBufferedEnoughData() { return false; };
    virtual void sourceUpdated() { }

    // MediaStreamTrackPrivate::Observer API
    void trackMutedChanged(MediaStreamTrackPrivate&) final { sourceMutedChanged(); }
    void trackEnabledChanged(MediaStreamTrackPrivate&) final { sourceEnabledChanged(); }
    void trackEnded(MediaStreamTrackPrivate&) final { }
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { }

    void initializeConverter();

    Ref<MediaStreamTrackPrivate> m_audioSource;
    bool m_muted { false };
    bool m_enabled { true };

    mutable Lock m_sinksLock;
    HashSet<webrtc::AudioTrackSinkInterface*> m_sinks WTF_GUARDED_BY_LOCK(m_sinksLock);

#if !RELEASE_LOG_DISABLED
    size_t m_chunksSent { 0 };
#endif
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
