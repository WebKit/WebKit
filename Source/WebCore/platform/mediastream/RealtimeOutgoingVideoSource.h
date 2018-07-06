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

#pragma once

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include "MediaStreamTrackPrivate.h"
#include <Timer.h>
#include <webrtc/api/mediastreaminterface.h>
#include <webrtc/api/optional.h>
#include <webrtc/common_video/include/i420_buffer_pool.h>
#include <webrtc/api/videosinkinterface.h>
#include <wtf/Optional.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace WebCore {

class RealtimeOutgoingVideoSource : public ThreadSafeRefCounted<RealtimeOutgoingVideoSource, WTF::DestructionThread::Main>, public webrtc::VideoTrackSourceInterface, private MediaStreamTrackPrivate::Observer {
public:
    static Ref<RealtimeOutgoingVideoSource> create(Ref<MediaStreamTrackPrivate>&& videoSource);
    ~RealtimeOutgoingVideoSource() { stop(); }

    void stop();
    bool setSource(Ref<MediaStreamTrackPrivate>&&);
    MediaStreamTrackPrivate& source() const { return m_videoSource.get(); }

    void AddRef() const final { ref(); }
    rtc::RefCountReleaseStatus Release() const final
    {
        auto result = hasOneRef() ? rtc::RefCountReleaseStatus::kDroppedLastRef : rtc::RefCountReleaseStatus::kOtherRefsRemained;
        deref();
        return result;
    }

    void setApplyRotation(bool shouldApplyRotation) { m_shouldApplyRotation = shouldApplyRotation; }

protected:
    explicit RealtimeOutgoingVideoSource(Ref<MediaStreamTrackPrivate>&&);

    void sendFrame(rtc::scoped_refptr<webrtc::VideoFrameBuffer>&&);

    Vector<rtc::VideoSinkInterface<webrtc::VideoFrame>*> m_sinks;
    webrtc::I420BufferPool m_bufferPool;

    bool m_enabled { true };
    bool m_muted { false };
    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    bool m_shouldApplyRotation { false };
    webrtc::VideoRotation m_currentRotation { webrtc::kVideoRotation_0 };

private:
    void sendBlackFramesIfNeeded();
    void sendOneBlackFrame();
    void initializeFromSource();
    void updateBlackFramesSending();

    // Notifier API
    void RegisterObserver(webrtc::ObserverInterface*) final { }
    void UnregisterObserver(webrtc::ObserverInterface*) final { }

    // VideoTrackSourceInterface API
    bool is_screencast() const final { return false; }
    rtc::Optional<bool> needs_denoising() const final { return rtc::Optional<bool>(); }
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
    std::optional<RealtimeMediaSourceSettings> m_initialSettings;
    bool m_isStopped { false };
    Timer m_blackFrameTimer;
    rtc::scoped_refptr<webrtc::VideoFrameBuffer> m_blackFrame;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
