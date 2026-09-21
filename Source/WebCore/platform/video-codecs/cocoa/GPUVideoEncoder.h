/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <WebCore/VideoCodecType.h>
#include <WebCore/VideoEncoderScalabilityMode.h>
#include <WebCore/VideoFrame.h>
#include <optional>
#include <span>
#include <utility>
#include <wtf/ThreadSafeWeakPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

typedef struct CF_BRIDGED_TYPE(id) __CVBuffer* CVPixelBufferRef;

namespace WebCore {

struct GPUVideoEncoderFrameInfo {
    uint32_t width { 0 };
    uint32_t height { 0 };
    int64_t timeStamp { 0 };
    std::optional<uint64_t> duration { };
    int64_t captureTimeMS { 0 };
    bool isKeyFrame { false };
    VideoFrame::Rotation rotation { VideoFrame::Rotation::None };
    bool isScreenshare { false };
    int qp { -1 };
    std::optional<uint8_t> temporalIndex { };
};

using GPUVideoEncoderCallback = Function<void(std::span<const uint8_t>, const GPUVideoEncoderFrameInfo&)>;
using GPUVideoEncoderDescriptionCallback = Function<void(std::span<const uint8_t>)>;
using GPUVideoEncoderErrorCallback = Function<void(bool isFrameDropped)>; // isFrameDropped is false in case of encoder error, and true if encoder decided to drop frame (say to ensure bitrate is respected).

class GPUVideoEncoder : public ThreadSafeRefCountedAndCanMakeThreadSafeWeakPtr<GPUVideoEncoder> {
public:
    virtual ~GPUVideoEncoder() = default;

    struct CreationInfo {
        VideoCodecType codecType;
        bool useWebCoreEncoder;
        bool useAnnexB;
        VideoEncoderScalabilityMode scalabilityMode;
        bool isLowLatencyEnabled;
        Ref<WorkQueue> queue;
    };
    WEBCORE_EXPORT static RefPtr<GPUVideoEncoder> create(CreationInfo&&, const Vector<std::pair<String, String>>& parameters, GPUVideoEncoderCallback&&, GPUVideoEncoderDescriptionCallback&&, GPUVideoEncoderErrorCallback&&);

    virtual void initialize(uint16_t width, uint16_t height, unsigned startBitrate, unsigned maxBitrate, unsigned minBitrate, uint32_t maxFramerate) = 0;
    virtual void encodeFrame(CVPixelBufferRef, int64_t timeStampNs, int64_t timeStamp, std::optional<uint64_t> duration, VideoFrame::Rotation, bool isKeyframeRequired) = 0;
    virtual void setRates(uint32_t bitRate, uint32_t frameRate) = 0;
    virtual void flush() = 0;

protected:
    GPUVideoEncoder() = default;
};

}
