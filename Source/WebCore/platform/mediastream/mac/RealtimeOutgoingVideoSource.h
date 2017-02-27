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
#include "RealtimeMediaSource.h"
#include <webrtc/api/mediastreaminterface.h>
#include <webrtc/base/optional.h>
#include <webrtc/common_video/include/i420_buffer_pool.h>
#include <webrtc/media/base/videosinkinterface.h>

namespace WebCore {

class RealtimeOutgoingVideoSource final : public RefCounted<RealtimeOutgoingVideoSource>, public webrtc::VideoTrackSourceInterface, private RealtimeMediaSource::Observer {
public:
    static Ref<RealtimeOutgoingVideoSource> create(Ref<RealtimeMediaSource>&& videoSource) { return adoptRef(*new RealtimeOutgoingVideoSource(WTFMove(videoSource))); }

    int AddRef() const final { ref(); return refCount(); }
    int Release() const final { deref(); return refCount(); }

private:
    RealtimeOutgoingVideoSource(Ref<RealtimeMediaSource>&&);

    void sendFrame(rtc::scoped_refptr<webrtc::VideoFrameBuffer>&&);

    // Notifier API
    void RegisterObserver(webrtc::ObserverInterface*) final { }
    void UnregisterObserver(webrtc::ObserverInterface*) final { }

    // VideoTrackSourceInterface API
    bool is_screencast() const final { return false; }
    rtc::Optional<bool> needs_denoising() const final { return rtc::Optional<bool>(); }
    bool GetStats(Stats*) final;

    // MediaSourceInterface API
    SourceState state() const final { return SourceState(); }
    bool remote() const final { return true; }

    // rtc::VideoSourceInterface<webrtc::VideoFrame> API
    void AddOrUpdateSink(rtc::VideoSinkInterface<webrtc::VideoFrame>*, const rtc::VideoSinkWants&) final;
    void RemoveSink(rtc::VideoSinkInterface<webrtc::VideoFrame>*) final;

    // RealtimeMediaSource::Observer API
    void sourceMutedChanged() final;
    void sourceEnabledChanged() final;
    void videoSampleAvailable(MediaSample&) final;

    Vector<rtc::VideoSinkInterface<webrtc::VideoFrame>*> m_sinks;
    webrtc::I420BufferPool m_bufferPool;
    Ref<RealtimeMediaSource> m_videoSource;
    bool m_enabled { true };
    bool m_muted { false };
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)
