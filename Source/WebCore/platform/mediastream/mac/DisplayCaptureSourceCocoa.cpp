/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DisplayCaptureSourceCocoa.h"

#if ENABLE(MEDIA_STREAM)

#include "Logging.h"
#include "MediaSampleAVFObjC.h"
#include "PixelBufferConformerCV.h"
#include "PixelBufferResizer.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceSettings.h"
#include "RealtimeVideoUtilities.h"
#include "Timer.h"
#include <CoreMedia/CMSync.h>
#include <mach/mach_time.h>
#include <pal/avfoundation/MediaTimeAVFoundation.h>
#include <pal/cf/CoreMediaSoftLink.h>
#include <pal/spi/cf/CoreAudioSPI.h>
#include <sys/time.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>

#include "CoreVideoSoftLink.h"

namespace WebCore {
using namespace PAL;

DisplayCaptureSourceCocoa::DisplayCaptureSourceCocoa(String&& name)
    : RealtimeMediaSource("", Type::Video, WTFMove(name))
    , m_timer(RunLoop::current(), this, &DisplayCaptureSourceCocoa::emitFrame)
{
}

DisplayCaptureSourceCocoa::~DisplayCaptureSourceCocoa()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenter::singleton().videoFactory().unsetActiveSource(*this);
#endif
}

const RealtimeMediaSourceCapabilities& DisplayCaptureSourceCocoa::capabilities()
{
    if (!m_capabilities) {
        RealtimeMediaSourceCapabilities capabilities(settings().supportedConstraints());

        // FIXME: what should these be?
        capabilities.setWidth(CapabilityValueOrRange(72, 2880));
        capabilities.setHeight(CapabilityValueOrRange(45, 1800));
        capabilities.setFrameRate(CapabilityValueOrRange(.01, 60.0));

        m_capabilities = WTFMove(capabilities);
    }
    return m_capabilities.value();
}

const RealtimeMediaSourceSettings& DisplayCaptureSourceCocoa::settings()
{
    if (!m_currentSettings) {
        RealtimeMediaSourceSettings settings;
        settings.setFrameRate(frameRate());
        auto size = frameSize();
        if (!size.isEmpty()) {
            settings.setWidth(size.width());
            settings.setHeight(size.height());
        }
        settings.setDisplaySurface(surfaceType());
        settings.setLogicalSurface(false);

        RealtimeMediaSourceSupportedConstraints supportedConstraints;
        supportedConstraints.setSupportsFrameRate(true);
        supportedConstraints.setSupportsWidth(true);
        supportedConstraints.setSupportsHeight(true);
        supportedConstraints.setSupportsDisplaySurface(true);
        supportedConstraints.setSupportsLogicalSurface(true);

        settings.setSupportedConstraints(supportedConstraints);

        m_currentSettings = WTFMove(settings);
    }
    return m_currentSettings.value();
}

void DisplayCaptureSourceCocoa::settingsDidChange(OptionSet<RealtimeMediaSourceSettings::Flag> settings)
{
    if (settings.contains(RealtimeMediaSourceSettings::Flag::FrameRate) && m_timer.isActive())
        m_timer.startRepeating(1_s / frameRate());

    if (settings.containsAny({ RealtimeMediaSourceSettings::Flag::Width, RealtimeMediaSourceSettings::Flag::Height })) {
        m_bufferAttributes = nullptr;
        m_lastSampleBuffer = nullptr;
    }

    m_currentSettings = { };
}

void DisplayCaptureSourceCocoa::startProducingData()
{
#if PLATFORM(IOS)
    RealtimeMediaSourceCenter::singleton().videoFactory().setActiveSource(*this);
#endif

    m_startTime = MonotonicTime::now();
    m_timer.startRepeating(1_s / frameRate());
}

void DisplayCaptureSourceCocoa::stopProducingData()
{
    m_timer.stop();
    m_elapsedTime += MonotonicTime::now() - m_startTime;
    m_startTime = MonotonicTime::nan();
}

Seconds DisplayCaptureSourceCocoa::elapsedTime()
{
    if (std::isnan(m_startTime))
        return m_elapsedTime;

    return m_elapsedTime + (MonotonicTime::now() - m_startTime);
}

IntSize DisplayCaptureSourceCocoa::frameSize() const
{
    IntSize frameSize = size();
    if (frameSize.isEmpty())
        frameSize = m_intrinsicSize;

    return frameSize;
}

void DisplayCaptureSourceCocoa::setIntrinsicSize(const IntSize& size)
{
    if (m_intrinsicSize == size)
        return;

    m_intrinsicSize = size;
    m_lastSampleBuffer = nullptr;
}

void DisplayCaptureSourceCocoa::emitFrame()
{
    if (muted())
        return;

    auto pixelBuffer = generateFrame();
    if (!pixelBuffer)
        return;

    if (m_lastSampleBuffer && m_lastFullSizedPixelBuffer && CFEqual(m_lastFullSizedPixelBuffer.get(), pixelBuffer.get())) {
        videoSampleAvailable(MediaSampleAVFObjC::create(m_lastSampleBuffer.get()));
        return;
    }

    m_lastFullSizedPixelBuffer = pixelBuffer;

    int width = WTF::safeCast<int>(CVPixelBufferGetWidth(pixelBuffer.get()));
    int height = WTF::safeCast<int>(CVPixelBufferGetHeight(pixelBuffer.get()));
    auto requestedSize = frameSize();
    if (width != requestedSize.width() || height != requestedSize.height()) {
        if (m_pixelBufferResizer && !m_pixelBufferResizer->canResizeTo(requestedSize))
            m_pixelBufferResizer = nullptr;

        if (!m_pixelBufferResizer)
            m_pixelBufferResizer = std::make_unique<PixelBufferResizer>(requestedSize, preferedPixelBufferFormat());

        pixelBuffer = m_pixelBufferResizer->resize(pixelBuffer.get());
    } else {
        m_pixelBufferResizer = nullptr;

        auto pixelFormatType = CVPixelBufferGetPixelFormatType(pixelBuffer.get());
        if (pixelFormatType != preferedPixelBufferFormat()) {
            if (!m_pixelBufferConformer) {
                auto preferredFromat = preferedPixelBufferFormat();
                auto conformerAttributes = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
                auto videoType = adoptCF(CFNumberCreate(nullptr,  kCFNumberSInt32Type,  &preferredFromat));
                CFDictionarySetValue(conformerAttributes.get(), kCVPixelBufferPixelFormatTypeKey, videoType.get());

                m_pixelBufferConformer = std::make_unique<PixelBufferConformerCV>(conformerAttributes.get());
            }

            pixelBuffer = m_pixelBufferConformer->convert(pixelBuffer.get());
        }
    }
    if (!pixelBuffer)
        return;

    m_lastSampleBuffer = sampleBufferFromPixelBuffer(pixelBuffer.get());
    if (!m_lastSampleBuffer)
        return;

    videoSampleAvailable(MediaSampleAVFObjC::create(m_lastSampleBuffer.get()));
}

RetainPtr<CMSampleBufferRef> DisplayCaptureSourceCocoa::sampleBufferFromPixelBuffer(CVPixelBufferRef pixelBuffer)
{
    if (!pixelBuffer)
        return nullptr;

    CMTime sampleTime = CMTimeMake(((elapsedTime() + 100_ms) * 100).seconds(), 100);
    CMSampleTimingInfo timingInfo = { kCMTimeInvalid, sampleTime, sampleTime };

    CMVideoFormatDescriptionRef formatDescription = nullptr;
    auto status = CMVideoFormatDescriptionCreateForImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, &formatDescription);
    if (status) {
        RELEASE_LOG(Media, "Failed to initialize CMVideoFormatDescription with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    CMSampleBufferRef sampleBuffer;
    status = CMSampleBufferCreateReadyWithImageBuffer(kCFAllocatorDefault, (CVImageBufferRef)pixelBuffer, formatDescription, &timingInfo, &sampleBuffer);
    CFRelease(formatDescription);
    if (status) {
        RELEASE_LOG(Media, "Failed to initialize CMSampleBuffer with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    return adoptCF(sampleBuffer);
}

#if HAVE(IOSURFACE) && PLATFORM(MAC)
static int32_t roundUpToMacroblockMultiple(int32_t size)
{
    return (size + 15) & ~15;
}

RetainPtr<CVPixelBufferRef> DisplayCaptureSourceCocoa::pixelBufferFromIOSurface(IOSurfaceRef surface)
{
    if (!m_bufferAttributes) {
        m_bufferAttributes = adoptCF(CFDictionaryCreateMutable(nullptr, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));

        auto format = IOSurfaceGetPixelFormat(surface);
        if (format == kCVPixelFormatType_420YpCbCr8BiPlanarVideoRange || format == kCVPixelFormatType_420YpCbCr8BiPlanarFullRange) {

            // If the width x height isn't a multiple of 16 x 16 and the surface has extra memory in the planes, set pixel buffer attributes to reflect it.
            auto width = IOSurfaceGetWidth(surface);
            auto height = IOSurfaceGetHeight(surface);
            int32_t extendedRight = roundUpToMacroblockMultiple(width) - width;
            int32_t extendedBottom = roundUpToMacroblockMultiple(height) - height;

            if ((IOSurfaceGetBytesPerRowOfPlane(surface, 0) >= width + extendedRight)
                && (IOSurfaceGetBytesPerRowOfPlane(surface, 1) >= width + extendedRight)
                && (IOSurfaceGetAllocSize(surface) >= (height + extendedBottom) * IOSurfaceGetBytesPerRowOfPlane(surface, 0) * 3 / 2)) {
                auto cfInt = adoptCF(CFNumberCreate(nullptr,  kCFNumberIntType,  &extendedRight));
                CFDictionarySetValue(m_bufferAttributes.get(), kCVPixelBufferExtendedPixelsRightKey, cfInt.get());
                cfInt = adoptCF(CFNumberCreate(nullptr,  kCFNumberIntType,  &extendedBottom));
                CFDictionarySetValue(m_bufferAttributes.get(), kCVPixelBufferExtendedPixelsBottomKey, cfInt.get());
            }
        }

        CFDictionarySetValue(m_bufferAttributes.get(), kCVPixelBufferOpenGLCompatibilityKey, kCFBooleanTrue);
    }

    CVPixelBufferRef pixelBuffer;
    auto status = CVPixelBufferCreateWithIOSurface(kCFAllocatorDefault, surface, m_bufferAttributes.get(), &pixelBuffer);
    if (status) {
        RELEASE_LOG(Media, "CVPixelBufferCreateWithIOSurface failed with error code: %d", static_cast<int>(status));
        return nullptr;
    }

    return adoptCF(pixelBuffer);
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
