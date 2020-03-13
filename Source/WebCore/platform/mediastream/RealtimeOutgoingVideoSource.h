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
#include <Timer.h>

ALLOW_UNUSED_PARAMETERS_BEGIN

#include <webrtc/api/media_stream_interface.h>
#include <webrtc/common_video/include/i420_buffer_pool.h>

ALLOW_UNUSED_PARAMETERS_END

#include <wtf/LoggerHelper.h>
#include <wtf/Optional.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class RealtimeOutgoingVideoSource
    : public ThreadSafeRefCounted<RealtimeOutgoingVideoSource, WTF::DestructionThread::Main>
    , public webrtc::VideoTrackSourceInterface
    , private MediaStreamTrackPrivate::Observer
#if !RELEASE_LOG_DISABLED
    , private LoggerHelper
#endif
{
public:
    static Ref<RealtimeOutgoingVideoSource> create(Ref<MediaStreamTrackPrivate>&& videoSource);
    ~RealtimeOutgoingVideoSource();

    void start() { observeSource(); }
    void stop();
    void setSource(Ref<MediaStreamTrackPrivate>&&);
    MediaStreamTrackPrivate& source() const { return m_videoSource.get(); }

    void AddRef() const final { ref(); }
    rtc::RefCountReleaseStatus Release() const final
    {
        deref();
        return rtc::RefCountReleaseStatus::kOtherRefsRemained;
    }

    void setApplyRotation(bool shouldApplyRotation) { m_shouldApplyRotation = shouldApplyRotation; }

protected:
    explicit RealtimeOutgoingVideoSource(Ref<MediaStreamTrackPrivate>&&);

    void sendFrame(rtc::scoped_refptr<webrtc::VideoFrameBuffer>&&);
    bool isSilenced() const { return m_muted || !m_enabled; }

    virtual rtc::scoped_refptr<webrtc::VideoFrameBuffer> createBlackFrame(size_t width, size_t height) = 0;

    bool m_shouldApplyRotation { false };
    webrtc::VideoRotation m_currentRotation { webrtc::kVideoRotation_0 };

#if !RELEASE_LOG_DISABLED
    // LoggerHelper API
    const Logger& logger() const final { return m_logger.get(); }
    const void* logIdentifier() const final { return m_logIdentifier; }
    const char* logClassName() const final { return "RealtimeOutgoingVideoSource"; }
    WTFLogChannel& logChannel() const final;
#endif

private:
    void sendBlackFramesIfNeeded();
    void sendOneBlackFrame();
    void initializeFromSource();
    void updateBlackFramesSending();

    void observeSource();
    void unobserveSource();

    // Notifier API
    void RegisterObserver(webrtc::ObserverInterface*) final { }
    void UnregisterObserver(webrtc::ObserverInterface*) final { }

    // VideoTrackSourceInterface API
    bool is_screencast() const final { return false; }
    absl::optional<bool> needs_denoising() const final { return absl::optional<bool>(); }
    bool GetStats(Stats*) final { return false; };

    // MediaSourceInterface API
    SourceState state() const final { return SourceState(); }
    bool remote() const final { return true; }

    // rtc::VideoSourceInterface<webrtc::VideoFrame> API
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>*, const rtc::VideoSinkWants&) final;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>*) final;

    void sourceMutedChanged();
    void sourceEnabledChanged();

    // MediaStreamTrackPrivate::Observer API
    void trackMutedChanged(MediaStreamTrackPrivate&) final { sourceMutedChanged(); }
    void trackEnabledChanged(MediaStreamTrackPrivate&) final { sourceEnabledChanged(); }
    void trackSettingsChanged(MediaStreamTrackPrivate&) final { initializeFromSource(); }
    void sampleBufferUpdated(MediaStreamTrackPrivate&, MediaSample&) override { }
    void trackEnded(MediaStreamTrackPrivate&) final { }

    Ref<MediaStreamTrackPrivate> m_videoSource;
    Timer m_blackFrameTimer;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> m_blackFrame;

    mutable RecursiveLock m_sinksLock;
    HashSet<rtc::VideoSinkInterface<webrtc::VideoFrame>*> m_sinks;

    bool m_enabled { true };
    bool m_muted { false };
    uint32_t m_width { 0 };
    uint32_t m_height { 0 };

#if !RELEASE_LOG_DISABLED
    Ref<const Logger> m_logger;
    const void* m_logIdentifier;
    MonotonicTime m_lastFrameLogTime;
    unsigned m_frameCount { 0 };
#endif
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
