/*
 * Copyright (C) 2015-2018 Apple Inc. All rights reserved.
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
#import "PixelBufferResizer.h"
#import "PlatformLayer.h"
#import "RealtimeMediaSourceSettings.h"
#import "RealtimeVideoUtilities.h"
#import <QuartzCore/CALayer.h>
#import <QuartzCore/CATransaction.h>
#import <objc/runtime.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import "CoreVideoSoftLink.h"

namespace WebCore {
using namespace PAL;

static const int videoSampleRate = 90000;

CaptureSourceOrError MockRealtimeVideoSource::create(const String& deviceID, const String& name, const MediaConstraints* constraints)
{
    auto source = adoptRef(*new MockRealtimeVideoSourceMac(deviceID, name));
    // FIXME: We should report error messages
    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}

MockRealtimeVideoSourceMac::MockRealtimeVideoSourceMac(const String& deviceID, const String& name)
    : MockRealtimeVideoSource(deviceID, name)
{
}

RetainPtr<CMSampleBufferRef> MockRealtimeVideoSourceMac::CMSampleBufferFromPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    if (!pixelBuffer)
        return nullptr;
    
    CMTime sampleTime = CMTimeMake(((elapsedTime() + 100_ms) * videoSampleRate).seconds(), videoSampleRate);
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
    static CGColorSpaceRef sRGBColorSpace = sRGBColorSpaceRef();

    CGSize imageSize = CGSizeMake(CGImageGetWidth(image), CGImageGetHeight(image));
    if (!m_bufferPool) {
        CVPixelBufferPoolRef bufferPool;
        CFDictionaryRef sourcePixelBufferOptions = (__bridge CFDictionaryRef) @{
            (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey : @(kCVPixelFormatType_32ARGB),
            (__bridge NSString *)kCVPixelBufferWidthKey : @(imageSize.width),
            (__bridge NSString *)kCVPixelBufferHeightKey : @(imageSize.height),
#if PLATFORM(IOS)
            (__bridge NSString *)kCVPixelFormatOpenGLESCompatibility : @(YES),
#else
            (__bridge NSString *)kCVPixelBufferOpenGLCompatibilityKey : @(YES),
#endif
            (__bridge NSString *)kCVPixelBufferIOSurfacePropertiesKey : @{/*empty dictionary*/}
        };

        CFDictionaryRef pixelBufferPoolOptions = (__bridge CFDictionaryRef) @{
           (__bridge NSString *)kCVPixelBufferPoolMinimumBufferCountKey : @(4)
        };

        CVReturn status = CVPixelBufferPoolCreate(kCFAllocatorDefault, pixelBufferPoolOptions, sourcePixelBufferOptions, &bufferPool);
        if (status != kCVReturnSuccess)
            return nullptr;

        m_bufferPool = adoptCF(bufferPool);
    }

    CVPixelBufferRef pixelBuffer;
    CVReturn status = CVPixelBufferPoolCreatePixelBuffer(nullptr, m_bufferPool.get(), &pixelBuffer);
    if (status != kCVReturnSuccess)
        return nullptr;

    CVPixelBufferLockBaseAddress(pixelBuffer, 0);
    void* data = CVPixelBufferGetBaseAddress(pixelBuffer);
    auto context = adoptCF(CGBitmapContextCreate(data, imageSize.width, imageSize.height, 8, CVPixelBufferGetBytesPerRow(pixelBuffer), sRGBColorSpace, (CGBitmapInfo) kCGImageAlphaNoneSkipFirst));
    CGContextDrawImage(context.get(), CGRectMake(0, 0, CGImageGetWidth(image), CGImageGetHeight(image)), image);
    CVPixelBufferUnlockBaseAddress(pixelBuffer, 0);

    return adoptCF(pixelBuffer);
}

void MockRealtimeVideoSourceMac::updateSampleBuffer()
{
    auto imageBuffer = this->imageBuffer();
    if (!imageBuffer)
        return;

    auto pixelBuffer = pixelBufferFromCGImage(imageBuffer->copyImage()->nativeImage().get());
    if (m_pixelBufferResizer)
        pixelBuffer = m_pixelBufferResizer->resize(pixelBuffer.get());
    else {
        if (!m_pixelBufferConformer) {
            m_pixelBufferConformer = std::make_unique<PixelBufferConformerCV>((__bridge CFDictionaryRef)@{
                (__bridge NSString *)kCVPixelBufferPixelFormatTypeKey: @(preferedPixelBufferFormat())
            });
        }

        pixelBuffer = m_pixelBufferConformer->convert(pixelBuffer.get());
    }
    auto sampleBuffer = CMSampleBufferFromPixelBuffer(pixelBuffer.get());

    // We use m_deviceOrientation to emulate sensor orientation
    dispatchMediaSampleToObservers(MediaSampleAVFObjC::create(sampleBuffer.get(), m_deviceOrientation));
}

void MockRealtimeVideoSourceMac::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height }))
        m_bufferPool = nullptr;
    MockRealtimeVideoSource::settingsDidChange(settings);
}

void MockRealtimeVideoSourceMac::orientationChanged(int orientation)
{
    // FIXME: Do something with m_deviceOrientation. See bug 169822.
    switch (orientation) {
    case 0:
        m_deviceOrientation = MediaSample::VideoRotation::None;
        break;
    case 90:
        m_deviceOrientation = MediaSample::VideoRotation::Right;
        break;
    case -90:
        m_deviceOrientation = MediaSample::VideoRotation::Left;
        break;
    case 180:
        m_deviceOrientation = MediaSample::VideoRotation::UpsideDown;
        break;
    default:
        return;
    }
}

void MockRealtimeVideoSourceMac::monitorOrientation(OrientationNotifier& notifier)
{
    notifier.addObserver(*this);
    orientationChanged(notifier.orientation());
}

void MockRealtimeVideoSourceMac::setSizeAndFrameRateWithPreset(IntSize requestedSize, double, RefPtr<VideoPreset> preset)
{
    if (!preset)
        return;

    if (preset->size != requestedSize) {
        if (m_pixelBufferResizer && !m_pixelBufferResizer->canResizeTo(requestedSize))
            m_pixelBufferResizer = nullptr;

        if (!m_pixelBufferResizer)
            m_pixelBufferResizer = std::make_unique<PixelBufferResizer>(requestedSize, preferedPixelBufferFormat());
    } else
        m_pixelBufferResizer = nullptr;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
