/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "WebKitUtilities.h"
#include "api/video_codecs/video_decoder_factory.h"

typedef struct __CVBuffer* CVPixelBufferRef;

namespace webrtc {

struct SdpVideoFormat;
class VideoDecoderFactory;

using WebKitVideoDecoder = void*;
using VideoDecoderCreateCallback = WebKitVideoDecoder(*)(const SdpVideoFormat& format);
using VideoDecoderReleaseCallback = int32_t(*)(WebKitVideoDecoder);
using VideoDecoderDecodeCallback = int32_t(*)(WebKitVideoDecoder, uint32_t timeStamp, const uint8_t*, size_t length, uint16_t width, uint16_t height);
using VideoDecoderRegisterDecodeCompleteCallback = int32_t(*)(WebKitVideoDecoder, void* decodedImageCallback);

void setVideoDecoderCallbacks(VideoDecoderCreateCallback, VideoDecoderReleaseCallback, VideoDecoderDecodeCallback, VideoDecoderRegisterDecodeCompleteCallback);

std::unique_ptr<webrtc::VideoDecoderFactory> createWebKitDecoderFactory(WebKitH265, WebKitVP9, WebKitVP9VTB);
void videoDecoderTaskComplete(void* callback, uint32_t timeStamp, CVPixelBufferRef, uint32_t timeStampRTP);


using LocalDecoder = void*;
using LocalDecoderCallback = void (^)(CVPixelBufferRef, uint32_t timeStamp, uint32_t timeStampNs);
void* createLocalH264Decoder(LocalDecoderCallback);
void* createLocalH265Decoder(LocalDecoderCallback);
void* createLocalVP9Decoder(LocalDecoderCallback);
void releaseLocalDecoder(LocalDecoder);
int32_t decodeFrame(LocalDecoder, uint32_t timeStamp, const uint8_t*, size_t);
void setDecoderFrameSize(LocalDecoder, uint16_t width, uint16_t height);

}
