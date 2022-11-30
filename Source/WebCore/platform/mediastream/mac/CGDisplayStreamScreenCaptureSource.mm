/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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
#include "CGDisplayStreamScreenCaptureSource.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#import "GraphicsContextCG.h"
#import "ImageBuffer.h"
#import "Logging.h"
#import "MediaConstraints.h"
#import "MediaSampleAVFObjC.h"
#import "NotImplemented.h"
#import "PlatformLayer.h"
#import "PlatformScreen.h"
#import "RealtimeMediaSourceSettings.h"
#import "RealtimeVideoUtilities.h"
#import <pal/spi/cg/CoreGraphicsSPI.h>
#import <wtf/text/StringToIntegerConversion.h>

#import "CoreVideoSoftLink.h"

namespace WebCore {

static std::optional<CGDirectDisplayID> updateDisplayID(CGDirectDisplayID displayID)
{
    uint32_t displayCount = 0;
    auto err = CGGetActiveDisplayList(0, nullptr, &displayCount);
    if (err) {
        RELEASE_LOG(WebRTC, "CGGetActiveDisplayList() returned error %d when trying to get display count", static_cast<int>(err));
        return std::nullopt;
    }

    if (!displayCount) {
        RELEASE_LOG(WebRTC, "CGGetActiveDisplayList() returned a display count of 0");
        return std::nullopt;
    }

    Vector<CGDirectDisplayID> activeDisplays(displayCount);
    err = CGGetActiveDisplayList(displayCount, activeDisplays.data(), &displayCount);
    if (err) {
        RELEASE_LOG(WebRTC, "CGGetActiveDisplayList() returned error %d when trying to get the active display list", static_cast<int>(err));
        return std::nullopt;
    }

    auto displayMask = CGDisplayIDToOpenGLDisplayMask(displayID);
    for (auto display : activeDisplays) {
        if (displayMask == CGDisplayIDToOpenGLDisplayMask(display))
            return display;
    }

    return std::nullopt;
}

Expected<UniqueRef<DisplayCaptureSourceCocoa::Capturer>, String> CGDisplayStreamScreenCaptureSource::create(const String& deviceID)
{
    auto displayID = parseInteger<uint32_t>(deviceID);
    if (!displayID)
        return makeUnexpected("Invalid display device ID"_s);

    auto actualDisplayID = updateDisplayID(displayID.value());
    if (!actualDisplayID)
        return makeUnexpected("Invalid display ID"_s);

    return UniqueRef<DisplayCaptureSourceCocoa::Capturer>(makeUniqueRef<CGDisplayStreamScreenCaptureSource>(actualDisplayID.value()));
}

CGDisplayStreamScreenCaptureSource::CGDisplayStreamScreenCaptureSource(uint32_t displayID)
    : CGDisplayStreamCaptureSource()
    , m_displayID(displayID)
{
}

bool CGDisplayStreamScreenCaptureSource::checkDisplayStream()
{
    auto actualDisplayID = updateDisplayID(m_displayID);
    if (!actualDisplayID) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "invalid display ID: ", m_displayID);
        return false;
    }

    if (m_displayID != actualDisplayID.value()) {
        m_displayID = actualDisplayID.value();
        invalidateDisplayStream();
        ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "display ID changed to ", static_cast<int>(m_displayID));
    }
    
    return true;
}

RetainPtr<CGDisplayStreamRef> CGDisplayStreamScreenCaptureSource::createDisplayStream()
{
    static const int screenQueueMaximumLength = 6;

    ASSERT(!displayStream());
    ASSERT(m_displayID == updateDisplayID(m_displayID));

    auto width = this->width();
    auto height = this->height();
    if (!width || !height) {
        auto displayMode = adoptCF(CGDisplayCopyDisplayMode(m_displayID));
        width = CGDisplayModeGetPixelsWide(displayMode.get());
        height = CGDisplayModeGetPixelsHigh(displayMode.get());
    }
    if (!width || !height) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "unable to get screen width/height");
        return nullptr;
    }
    ASSERT(frameRate());
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "frame rate ", frameRate(), ", size ", width, "x", height);

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    NSDictionary* streamOptions = @{
        (__bridge NSString *)kCGDisplayStreamMinimumFrameTime : @(1 / frameRate()),
        (__bridge NSString *)kCGDisplayStreamQueueDepth : @(screenQueueMaximumLength),
        (__bridge NSString *)kCGDisplayStreamColorSpace : (__bridge id)sRGBColorSpaceRef(),
        (__bridge NSString *)kCGDisplayStreamShowCursor : @YES,
    };

    auto stream = adoptCF(CGDisplayStreamCreateWithDispatchQueue(m_displayID, width, height, preferedPixelBufferFormat(), (__bridge CFDictionaryRef)streamOptions, captureQueue(), frameAvailableHandler()));
    ALLOW_DEPRECATED_DECLARATIONS_END
    return stream;
}

IntSize CGDisplayStreamScreenCaptureSource::intrinsicSize() const
{
    auto displayMode = adoptCF(CGDisplayCopyDisplayMode(m_displayID));
    auto screenWidth = CGDisplayModeGetPixelsWide(displayMode.get());
    auto screenHeight = CGDisplayModeGetPixelsHigh(displayMode.get());

    return { Checked<int>(screenWidth), Checked<int>(screenHeight) };
}

std::optional<CaptureDevice> CGDisplayStreamScreenCaptureSource::screenCaptureDeviceWithPersistentID(const String& deviceID)
{
    auto displayID = parseInteger<uint32_t>(deviceID);
    if (!displayID) {
        RELEASE_LOG(WebRTC, "CGDisplayStreamScreenCaptureSource::screenCaptureDeviceWithPersistentID: display ID does not convert to 32-bit integer");
        return std::nullopt;
    }

    auto actualDisplayID = updateDisplayID(displayID.value());
    if (!actualDisplayID)
        return std::nullopt;

    auto device = CaptureDevice(String::number(actualDisplayID.value()), CaptureDevice::DeviceType::Screen, "ScreenCaptureDevice"_s);
    device.setEnabled(true);

    return device;
}

void CGDisplayStreamScreenCaptureSource::screenCaptureDevices(Vector<CaptureDevice>& displays)
{
    auto screenID = displayID([NSScreen mainScreen]);
    if (CGDisplayIDToOpenGLDisplayMask(screenID)) {
        CaptureDevice displayDevice(String::number(screenID), CaptureDevice::DeviceType::Screen, makeString("Screen 0"));
        displayDevice.setEnabled(true);
        displays.append(WTFMove(displayDevice));
        return;
    }

    uint32_t displayCount = 0;
    auto err = CGGetActiveDisplayList(0, nullptr, &displayCount);
    if (err) {
        RELEASE_LOG(WebRTC, "CGGetActiveDisplayList() returned error %d when trying to get display count", (int)err);
        return;
    }

    if (!displayCount) {
        RELEASE_LOG(WebRTC, "CGGetActiveDisplayList() returned a display count of 0");
        return;
    }

    Vector<CGDirectDisplayID> activeDisplays(displayCount);
    err = CGGetActiveDisplayList(displayCount, activeDisplays.data(), &displayCount);
    if (err) {
        RELEASE_LOG(WebRTC, "CGGetActiveDisplayList() returned error %d when trying to get the active display list", (int)err);
        return;
    }

    int count = 0;
    for (auto displayID : activeDisplays) {
        CaptureDevice displayDevice(String::number(displayID), CaptureDevice::DeviceType::Screen, makeString("Screen ", String::number(count++)));
        displayDevice.setEnabled(CGDisplayIDToOpenGLDisplayMask(displayID));
        displays.append(WTFMove(displayDevice));
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

