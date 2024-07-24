/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if USE(LIBWEBRTC)

#include "LibWebRTCMacros.h"
#include <webrtc/api/scoped_refptr.h>
#include <wtf/RetainPtr.h>

using CVPixelBufferRef = struct __CVBuffer*;

namespace webrtc {
class VideoFrame;
class VideoFrameBuffer;
}

namespace WebCore {

enum class BufferType { I420, I010 };
WEBCORE_EXPORT RetainPtr<CVPixelBufferRef> copyPixelBufferForFrame(const webrtc::VideoFrame&);
RetainPtr<CVPixelBufferRef> createPixelBufferFromFrame(const webrtc::VideoFrame&, const std::function<CVPixelBufferRef(size_t, size_t, BufferType)>& createPixelBuffer);
RetainPtr<CVPixelBufferRef> createPixelBufferFromFrameBuffer(webrtc::VideoFrameBuffer&, const std::function<CVPixelBufferRef(size_t, size_t, BufferType)>& createPixelBuffer);
rtc::scoped_refptr<webrtc::VideoFrameBuffer> pixelBufferToFrame(CVPixelBufferRef);
bool copyVideoFrameBuffer(webrtc::VideoFrameBuffer&, uint8_t*);

typedef CVPixelBufferRef (*GetBufferCallback)(void*);
typedef void (*ReleaseBufferCallback)(void*);
rtc::scoped_refptr<webrtc::VideoFrameBuffer> toWebRTCVideoFrameBuffer(void*, GetBufferCallback, ReleaseBufferCallback, int width, int height);
WEBCORE_EXPORT void* videoFrameBufferProvider(const webrtc::VideoFrame&);

struct I420BufferLayout {
    size_t offsetY { 0 };
    size_t strideY { 0 };
    size_t offsetU { 0 };
    size_t strideU { 0 };
    size_t offsetV { 0 };
    size_t strideV { 0 };
};

struct I420ABufferLayout : I420BufferLayout {
    size_t offsetA { 0 };
    size_t strideA { 0 };
};

RetainPtr<CVPixelBufferRef> createPixelBufferFromI420Buffer(const uint8_t* buffer, size_t length, size_t width, size_t height, I420BufferLayout);
RetainPtr<CVPixelBufferRef> createPixelBufferFromI420ABuffer(const uint8_t* buffer, size_t length, size_t width, size_t height, I420ABufferLayout);

}

#endif // USE(LIBWEBRTC)
