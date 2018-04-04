/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "WebKitUtilities.h"
#include "Common/RTCUIApplicationStatusObserver.h"
#include "VideoToolbox/objc_video_decoder_factory.h"
#include "VideoToolbox/objc_video_encoder_factory.h"
#import "WebRTC/RTCVideoCodecH264.h"
#include "api/video/video_frame.h"
#include <webrtc/sdk/objc/Framework/Classes/Video/objc_frame_buffer.h>
#include <webrtc/sdk/objc/Framework/Headers/WebRTC/RTCVideoFrameBuffer.h>

#if !defined(WEBRTC_IOS)
@interface RTCUIApplicationStatusObserver : NSObject

+ (instancetype)sharedInstance;
+ (void)prepareForUse;

- (BOOL)isApplicationActive;

@end
#endif

@implementation RTCUIApplicationStatusObserver {
    BOOL _isActive;
}

+ (instancetype)sharedInstance {
    static id sharedInstance;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedInstance = [[self alloc] init];
    });

    return sharedInstance;
}

+ (void)prepareForUse {
    __unused RTCUIApplicationStatusObserver *observer = [self sharedInstance];
}

- (id)init {
    _isActive = YES;
    return self;
}

- (void)setActive {
    _isActive = YES;
}

- (void)setInactive {
    _isActive = NO;
}

- (BOOL)isApplicationActive {
    return _isActive;
}

@end

namespace webrtc {

void setApplicationStatus(bool isActive)
{
    if (isActive)
        [[RTCUIApplicationStatusObserver sharedInstance] setActive];
    else
        [[RTCUIApplicationStatusObserver sharedInstance] setInactive];
}

std::unique_ptr<webrtc::VideoEncoderFactory> createVideoToolboxEncoderFactory()
{
#if ENABLE_VCP_ENCODER
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        webrtc::VPModuleInitialize();
    });
#endif
    return std::make_unique<webrtc::ObjCVideoEncoderFactory>([[RTCVideoEncoderFactoryH264 alloc] init]);
}

std::unique_ptr<webrtc::VideoDecoderFactory> createVideoToolboxDecoderFactory()
{
    return std::make_unique<webrtc::ObjCVideoDecoderFactory>([[RTCVideoDecoderFactoryH264 alloc] init]);
}

static bool h264HardwareEncoderAllowed = true;
void setH264HardwareEncoderAllowed(bool allowed)
{
    h264HardwareEncoderAllowed = allowed;
}

bool isH264HardwareEncoderAllowed()
{
    return h264HardwareEncoderAllowed;
}

rtc::scoped_refptr<webrtc::VideoFrameBuffer> pixelBufferToFrame(CVPixelBufferRef pixelBuffer)
{
    RTCCVPixelBuffer *frameBuffer = [[RTCCVPixelBuffer alloc] initWithPixelBuffer: pixelBuffer];
    return new rtc::RefCountedObject<webrtc::ObjCFrameBuffer>(frameBuffer);
}

CVPixelBufferRef pixelBufferFromFrame(const VideoFrame& frame)
{
    auto buffer = frame.video_frame_buffer();
    auto frameBuffer = static_cast<webrtc::ObjCFrameBuffer&>(*buffer).wrapped_frame_buffer();
    if ([frameBuffer isKindOfClass: [RTCCVPixelBuffer class]])
        return [(RTCCVPixelBuffer *)frameBuffer pixelBuffer];
    return nullptr;
}

}
