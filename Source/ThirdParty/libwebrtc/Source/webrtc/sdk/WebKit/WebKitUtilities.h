/*
 * Copyright (C) 2018-2021 Apple Inc. All rights reserved.
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

#include "api/video/video_frame_buffer.h"
#include "api/video/video_rotation.h"
#include "api/scoped_refptr.h"
#include <CoreFoundation/CFBase.h>
#include <functional>

using CVPixelBufferRef = struct __CVBuffer*;

namespace webrtc {

class VideoFrame;

enum class WebKitH265 { Off, On };
enum class WebKitVP9 { Off, Profile0, Profile0And2 };
enum class WebKitVP9VTB { Off, On };
enum class WebKitH264LowLatency { Off, On };

void setApplicationStatus(bool isActive);

void setH264HardwareEncoderAllowed(bool);
bool isH264HardwareEncoderAllowed();

enum class BufferType { I420, I010 };
CVPixelBufferRef createPixelBufferFromFrame(const VideoFrame&, const std::function<CVPixelBufferRef(size_t, size_t, BufferType)>& createPixelBuffer) CF_RETURNS_RETAINED;
CVPixelBufferRef createPixelBufferFromFrameBuffer(VideoFrameBuffer&, const std::function<CVPixelBufferRef(size_t, size_t, BufferType)>& createPixelBuffer) CF_RETURNS_RETAINED;
CVPixelBufferRef pixelBufferFromFrame(const VideoFrame&) CF_RETURNS_RETAINED;
rtc::scoped_refptr<webrtc::VideoFrameBuffer> pixelBufferToFrame(CVPixelBufferRef);
bool copyVideoFrameBuffer(VideoFrameBuffer&, uint8_t*);

typedef CVPixelBufferRef (*GetBufferCallback)(void*);
typedef void (*ReleaseBufferCallback)(void*);
rtc::scoped_refptr<VideoFrameBuffer> toWebRTCVideoFrameBuffer(void*, GetBufferCallback, ReleaseBufferCallback, int width, int height);
void* videoFrameBufferProvider(const VideoFrame&);

bool convertBGRAToYUV(CVPixelBufferRef sourceBuffer, CVPixelBufferRef destinationBuffer);

struct I420BufferLayout {
    size_t offsetY { 0 };
    size_t strideY { 0 };
    size_t offsetU { 0 };
    size_t strideU { 0 };
    size_t offsetV { 0 };
    size_t strideV { 0 };
};

CVPixelBufferRef pixelBufferFromI420Buffer(const uint8_t* buffer, size_t length, size_t width, size_t height, I420BufferLayout) CF_RETURNS_RETAINED;
bool copyPixelBufferInI420Buffer(uint8_t* buffer, size_t length, CVPixelBufferRef, I420BufferLayout);

}
