/*
 * Copyright (C) 2020 Apple Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_RTC)

#include <wtf/RefCounted.h>
#include <wtf/Span.h>
#include <wtf/Vector.h>

namespace WebCore {

struct RTCEncodedAudioFrameMetadata {
    uint32_t synchronizationSource;
    Vector<uint32_t> contributingSources;
};

struct RTCEncodedVideoFrameMetadata {
    std::optional<int64_t> frameId;
    Vector<int64_t> dependencies;
    uint16_t width;
    uint16_t height;
    std::optional<int32_t> spatialIndex;
    std::optional<int32_t> temporalIndex;
    uint32_t synchronizationSource;
};

class RTCRtpTransformableFrame : public RefCounted<RTCRtpTransformableFrame> {
public:
    virtual ~RTCRtpTransformableFrame() = default;

    virtual Span<const uint8_t> data() const = 0;
    virtual void setData(Span<const uint8_t>) = 0;

    virtual uint64_t timestamp() const = 0;
    virtual RTCEncodedAudioFrameMetadata audioMetadata() const = 0;
    virtual RTCEncodedVideoFrameMetadata videoMetadata() const = 0;

    virtual bool isKeyFrame() const = 0;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
