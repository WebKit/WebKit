/*
 * Copyright (C) 2017-2019 Apple Inc. All rights reserved.
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
#include "ScreenDisplayCapturerMac.h"

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

#import "CoreVideoSoftLink.h"

extern "C" {
size_t CGDisplayModeGetPixelsWide(CGDisplayModeRef);
size_t CGDisplayModeGetPixelsHigh(CGDisplayModeRef);
}

namespace WebCore {

static Optional<CGDirectDisplayID> updateDisplayID(CGDirectDisplayID displayID)
{
    uint32_t displayCount = 0;
    auto err = CGGetActiveDisplayList(0, nullptr, &displayCount);
    if (err) {
        RELEASE_LOG(WebRTC, "CGGetActiveDisplayList() returned error %d when trying to get display count", static_cast<int>(err));
        return WTF::nullopt;
    }

    if (!displayCount) {
        RELEASE_LOG(WebRTC, "CGGetActiveDisplayList() returned a display count of 0");
        return WTF::nullopt;
    }

    Vector<CGDirectDisplayID> activeDisplays(displayCount);
    err = CGGetActiveDisplayList(displayCount, activeDisplays.data(), &displayCount);
    if (err) {
        RELEASE_LOG(WebRTC, "CGGetActiveDisplayList() returned error %d when trying to get the active display list", static_cast<int>(err));
        return WTF::nullopt;
    }

    auto displayMask = CGDisplayIDToOpenGLDisplayMask(displayID);
    for (auto display : activeDisplays) {
        if (displayMask == CGDisplayIDToOpenGLDisplayMask(display))
            return display;
    }

    return WTF::nullopt;
}

Expected<UniqueRef<DisplayCaptureSourceCocoa::Capturer>, String> ScreenDisplayCapturerMac::create(const String& deviceID)
{
    bool ok;
    auto displayID = deviceID.toUIntStrict(&ok);
    if (!ok)
        return makeUnexpected("Invalid display device ID"_s);

    auto actualDisplayID = updateDisplayID(displayID);
    if (!actualDisplayID)
        return makeUnexpected("Invalid display ID"_s);

    return UniqueRef<DisplayCaptureSourceCocoa::Capturer>(makeUniqueRef<ScreenDisplayCapturerMac>(actualDisplayID.value()));
}

ScreenDisplayCapturerMac::ScreenDisplayCapturerMac(uint32_t displayID)
    : m_displayID(displayID)
{
}

ScreenDisplayCapturerMac::~ScreenDisplayCapturerMac()
{
    if (m_observingDisplayChanges)
        CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallBack, this);

    m_currentFrame = nullptr;
}

bool ScreenDisplayCapturerMac::createDisplayStream(float frameRate)
{
    static const int screenQueueMaximumLength = 6;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    auto actualDisplayID = updateDisplayID(m_displayID);
    if (!actualDisplayID) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "invalid display ID: ", m_displayID);
        return false;
    }

    if (m_displayID != actualDisplayID.value()) {
        m_displayID = actualDisplayID.value();
        ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "display ID changed to ", static_cast<int>(m_displayID));
        m_displayStream = nullptr;
    }

    if (!m_displayStream) {
        auto displayMode = adoptCF(CGDisplayCopyDisplayMode(m_displayID));
        auto screenWidth = CGDisplayModeGetPixelsWide(displayMode.get());
        auto screenHeight = CGDisplayModeGetPixelsHigh(displayMode.get());
        if (!screenWidth || !screenHeight) {
            ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "unable to get screen width/height");
            return false;
        }

        if (!m_captureQueue)
            m_captureQueue = adoptOSObject(dispatch_queue_create("ScreenDisplayCapturerMac Capture Queue", DISPATCH_QUEUE_SERIAL));

        NSDictionary* streamOptions = @{
            (__bridge NSString *)kCGDisplayStreamMinimumFrameTime : @(1 / frameRate),
            (__bridge NSString *)kCGDisplayStreamQueueDepth : @(screenQueueMaximumLength),
            (__bridge NSString *)kCGDisplayStreamColorSpace : (__bridge id)sRGBColorSpaceRef(),
            (__bridge NSString *)kCGDisplayStreamShowCursor : @YES,
        };

        auto weakThis = makeWeakPtr(*this);
        auto frameAvailableBlock = ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef) {

            if (!frameSurface || !displayTime)
                return;

            size_t count;
            auto* rects = CGDisplayStreamUpdateGetRects(updateRef, kCGDisplayStreamUpdateDirtyRects, &count);
            if (!rects || !count)
                return;

            RunLoop::main().dispatch([weakThis, status, frame = DisplaySurface { frameSurface }]() mutable {
                if (!weakThis)
                    return;
                weakThis->newFrame(status, WTFMove(frame));
            });
        };

        m_displayStream = adoptCF(CGDisplayStreamCreateWithDispatchQueue(m_displayID, screenWidth, screenHeight, preferedPixelBufferFormat(), (__bridge CFDictionaryRef)streamOptions, m_captureQueue.get(), frameAvailableBlock));
        if (!m_displayStream) {
            ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "CGDisplayStreamCreate failed");
            return false;
        }
    }

    if (!m_observingDisplayChanges) {
        CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallBack, this);
        m_observingDisplayChanges = true;
    }

    return true;
}

bool ScreenDisplayCapturerMac::start(float frameRate)
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    if (m_isRunning)
        return true;

    return startDisplayStream(frameRate);
}

void ScreenDisplayCapturerMac::stop()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    if (!m_isRunning)
        return;

    if (m_displayStream)
        CGDisplayStreamStop(m_displayStream.get());

    m_isRunning = false;
}

DisplayCaptureSourceCocoa::DisplayFrameType ScreenDisplayCapturerMac::generateFrame()
{
    return DisplayCaptureSourceCocoa::DisplayFrameType { RetainPtr<IOSurfaceRef> { m_currentFrame.ioSurface() } };
}

bool ScreenDisplayCapturerMac::startDisplayStream(float frameRate)
{
    auto actualDisplayID = updateDisplayID(m_displayID);
    if (!actualDisplayID)
        return false;

    if (m_displayID != actualDisplayID.value()) {
        m_displayID = actualDisplayID.value();
        ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "display ID changed to ", static_cast<int>(m_displayID));
    }

    if (!m_displayStream && !createDisplayStream(frameRate))
        return false;

    auto err = CGDisplayStreamStart(m_displayStream.get());
    if (err) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "CGDisplayStreamStart failed with error ", static_cast<int>(err));
        return false;
    }

    m_isRunning = true;
    return true;
}

void ScreenDisplayCapturerMac::commitConfiguration(float frameRate)
{
    if (m_isRunning && !m_displayStream)
        startDisplayStream(frameRate);
}

void ScreenDisplayCapturerMac::displayWasReconfigured(CGDirectDisplayID, CGDisplayChangeSummaryFlags)
{
    // FIXME: implement!
}

void ScreenDisplayCapturerMac::displayReconfigurationCallBack(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo)
{
    if (userInfo)
        reinterpret_cast<ScreenDisplayCapturerMac *>(userInfo)->displayWasReconfigured(display, flags);
}

void ScreenDisplayCapturerMac::newFrame(CGDisplayStreamFrameStatus status, DisplaySurface&& newFrame)
{
    switch (status) {
    case kCGDisplayStreamFrameStatusFrameComplete:
        break;

    case kCGDisplayStreamFrameStatusFrameIdle:
        break;

    case kCGDisplayStreamFrameStatusFrameBlank:
        RELEASE_LOG(WebRTC, "ScreenDisplayCapturerMac::frameAvailable: kCGDisplayStreamFrameStatusFrameBlank");
        break;

    case kCGDisplayStreamFrameStatusStopped:
        RELEASE_LOG(WebRTC, "ScreenDisplayCapturerMac::frameAvailable: kCGDisplayStreamFrameStatusStopped");
        break;
    }

    m_currentFrame = WTFMove(newFrame);
}

Optional<CaptureDevice> ScreenDisplayCapturerMac::screenCaptureDeviceWithPersistentID(const String& deviceID)
{
    bool ok;
    auto displayID = deviceID.toUIntStrict(&ok);
    if (!ok) {
        RELEASE_LOG(WebRTC, "ScreenDisplayCapturerMac::screenCaptureDeviceWithPersistentID: display ID does not convert to 32-bit integer");
        return WTF::nullopt;
    }

    auto actualDisplayID = updateDisplayID(displayID);
    if (!actualDisplayID)
        return WTF::nullopt;

    auto device = CaptureDevice(String::number(actualDisplayID.value()), CaptureDevice::DeviceType::Screen, "ScreenCaptureDevice"_s);
    device.setEnabled(true);

    return device;
}

void ScreenDisplayCapturerMac::screenCaptureDevices(Vector<CaptureDevice>& displays)
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
