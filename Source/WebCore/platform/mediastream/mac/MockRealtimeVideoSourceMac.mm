/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "MockRealtimeVideoSourceMac.h"

#if ENABLE(MEDIA_STREAM)
#import "GraphicsContextCG.h"
#import "ImageBuffer.h"
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "NotImplemented.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceSettings.h"
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>
#import <objc/runtime.h>

#import "CoreMediaSoftLink.h"
#import "CoreVideoSoftLink.h"

namespace WebCore {

static const int videoSampleRate = 90000;

RefPtr<MockRealtimeVideoSource> MockRealtimeVideoSource::create(const String& name, const MediaConstraints* constraints)
{
    auto source = adoptRef(new MockRealtimeVideoSourceMac(name));
    if (constraints && source->applyConstraints(*constraints))
        source = nullptr;

    return source;
}

MockRealtimeVideoSourceMac::MockRealtimeVideoSourceMac(const String& name)
    : MockRealtimeVideoSource(name)
{
}

RefPtr<MockRealtimeVideoSource> MockRealtimeVideoSource::createMuted(const String& name)
{
    auto source = adoptRef(new MockRealtimeVideoSource(name));
    source->m_muted = true;
    return source;
}

RetainPtr<CMSampleBufferRef> MockRealtimeVideoSourceMac::CMSampleBufferFromPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    if (!pixelBuffer)
        return nullptr;

    CMTime sampleTime = CMTimeMake((elapsedTime() + .1) * videoSampleRate, videoSampleRate);
    CMSampleTimingInfo timingInfo = { kCMTimeInvalid, sampleTime, sampleTime };

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    OSStatus status = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, &formatDescription);
    if (status != noErr) {
        LOG_ERROR("Failed to initialize CMVideoFormatDescription with error code: %d", status);
        return nullptr;
    }

    CMSampleBufferRef sampleBuffer;
    status = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, formatDescription, &timingInfo, &sampleBuffer);
    CFRelease(formatDescription);
    if (status != noErr) {
        LOG_ERROR("Failed to initialize CMSampleBuffer with error code: %d", status);
        return nullptr;
    }

    return adoptCF(sampleBuffer);
}

RetainPtr<CVPixelBufferRef> MockRealtimeVideoSourceMac::pixelBufferFromCGImage(CGImageRef image) const
{
    static CGColorSpaceRef deviceRGBColorSpace = CGColorSpaceCreateDeviceRGB();

    CGSize frameSize = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
    CFDictionaryRef options = (__bridge CFDictionaryRef) @{
        (__bridge NSString *)kCVPixelBufferCGImageCompatibilityKey: @(NO),
        (__bridge NSString *)kCVPixelBufferCGBitmapContextCompatibilityKey: @(NO)
    };
    CVPixelBufferRef pixelBuffer;
    CVReturn status = CVPixelBufferCreate(kCFAllocatorDefault, frameSize.width, frameSize.height, kCVPixelFormatType_32ARGB, options, &pixelBuffer);
    if (status != kCVReturnSuccess)
        return nullptr;

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    void* data = CVPixelBufferGetBaseAddress(pixelBuffer);
    auto context = adoptCF(CGBitmapContextCreate(data, frameSize.width, frameSize.height, 8, CVPixelBufferGetBytesPerRow(pixelBuffer), deviceRGBColorSpace, (CGBitmapInfo) kCGImageAlphaNoneSkipFirst));
    CGContextDrawImage(context.get(), CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image)), image);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

    return adoptCF(pixelBuffer);
}

void MockRealtimeVideoSourceMac::updateSampleBuffer()
{
    auto pixelBuffer = pixelBufferFromCGImage(imageBuffer()->copyImage()->nativeImage().get());
    auto sampleBuffer = CMSampleBufferFromPixelBuffer(pixelBuffer.get());
    
    mediaDataUpdated(MediaSampleAVFObjC::create(sampleBuffer.get()));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
