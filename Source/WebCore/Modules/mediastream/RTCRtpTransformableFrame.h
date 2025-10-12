/*
 * Copyright (C) 2020-2025 Apple Inc. All rights reserved.
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

#include <WebCore/RTCRtpScriptTransformer.h>
#include <span>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct RTCEncodedAudioFrameMetadata {
    std::optional<uint32_t> synchronizationSource;
    std::optional<uint8_t> payloadType;
    std::optional<Vector<uint32_t>> contributingSources;
    std::optional<uint16_t> sequenceNumber;
    std::optional<uint32_t> rtpTimestamp;
    String mimeType;
};

struct RTCEncodedVideoFrameMetadata {
    std::optional<int64_t> frameId;
    std::optional<Vector<int64_t>> dependencies;
    std::optional<uint16_t> width;
    std::optional<uint16_t> height;
    std::optional<int32_t> spatialIndex;
    std::optional<int32_t> temporalIndex;
    std::optional<uint32_t> synchronizationSource;
    std::optional<uint8_t> payloadType;
    std::optional<Vector<uint32_t>> contributingSources;
    std::optional<int64_t> timestamp;
    std::optional<uint32_t> rtpTimestamp;
    String mimeType;
};

class RTCRtpScriptTransformer;
class RTCRtpTransformableFrame : public ThreadSafeRefCounted<RTCRtpTransformableFrame> {
public:
    virtual ~RTCRtpTransformableFrame() = default;

    virtual std::span<const uint8_t> data() const = 0;
    virtual void setData(std::span<const uint8_t>) = 0;

    virtual uint64_t timestamp() const = 0;
    virtual RTCEncodedAudioFrameMetadata audioMetadata() const = 0;
    virtual RTCEncodedVideoFrameMetadata videoMetadata() const = 0;

    virtual bool isKeyFrame() const = 0;

    virtual Ref<RTCRtpTransformableFrame> clone() = 0;
    virtual void setOptions(const RTCEncodedAudioFrameMetadata&) = 0;
    virtual void setOptions(const RTCEncodedVideoFrameMetadata&) = 0;

    void setTransformer(RTCRtpScriptTransformer&);
    bool isFromTransformer(RTCRtpScriptTransformer& transformer) const { return &transformer == m_transformer.get(); }

private:
    WeakPtr<RTCRtpScriptTransformer> m_transformer;
};

inline void RTCRtpTransformableFrame::setTransformer(RTCRtpScriptTransformer& transformer)
{
    ASSERT(!m_transformer);
    m_transformer = transformer;
};

} // namespace WebCore

#endif // ENABLE(WEB_RTC)
