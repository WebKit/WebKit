/*
 * Copyright (C) 2017 Igalia S.L. All rights reserved.
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC) && USE(GSTREAMER)

#include "RealtimeOutgoingVideoSource.h"

namespace WebCore {

class RealtimeOutgoingVideoSourceLibWebRTC final : public RealtimeOutgoingVideoSource {
public:
    static Ref<RealtimeOutgoingVideoSourceLibWebRTC> create(Ref<MediaStreamTrackPrivate>&&);

private:
    explicit RealtimeOutgoingVideoSourceLibWebRTC(Ref<MediaStreamTrackPrivate>&&);

    rtc::scoped_refptr<webrtc::VideoFrameBuffer> createBlackFrame(size_t, size_t) final;

    // MediaStreamTrackPrivate::Observer API
    void sampleBufferUpdated(MediaStreamTrackPrivate&, MediaSample&) final;
};

} // namespace WebCore

#endif // USE(LIBWEBRTC)

