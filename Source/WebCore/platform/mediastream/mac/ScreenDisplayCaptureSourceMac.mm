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
#include "ScreenDisplayCaptureSourceMac.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#include "GraphicsContextCG.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "MediaSampleAVFObjC.h"
#include "NotImplemented.h"
#include "PlatformLayer.h"
#include "PlatformScreen.h"
#include "RealtimeMediaSourceSettings.h"
#include "RealtimeVideoUtilities.h"

#include "CoreVideoSoftLink.h"

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
        RELEASE_LOG(Media, "CGGetActiveDisplayList() returned error %d when trying to get display count", static_cast<int>(err));
        return WTF::nullopt;
    }

    if (!displayCount) {
        RELEASE_LOG(Media, "CGGetActiveDisplayList() returned a display count of 0");
        return WTF::nullopt;
    }

    CGDirectDisplayID activeDisplays[displayCount];
    err = CGGetActiveDisplayList(displayCount, &(activeDisplays[0]), &displayCount);
    if (err) {
        RELEASE_LOG(Media, "CGGetActiveDisplayList() returned error %d when trying to get the active display list", static_cast<int>(err));
        return WTF::nullopt;
    }

    auto displayMask = CGDisplayIDToOpenGLDisplayMask(displayID);
    for (auto display : activeDisplays) {
        if (displayMask == CGDisplayIDToOpenGLDisplayMask(display))
            return display;
    }

    return WTF::nullopt;
}

CaptureSourceOrError ScreenDisplayCaptureSourceMac::create(String&& deviceID, const MediaConstraints* constraints)
{
    bool ok;
    auto displayID = deviceID.toUIntStrict(&ok);
    if (!ok) {
        RELEASE_LOG(Media, "ScreenDisplayCaptureSourceMac::create: Display ID does not convert to 32-bit integer");
        return { };
    }

    auto actualDisplayID = updateDisplayID(displayID);
    if (!actualDisplayID)
        return { };

    auto source = adoptRef(*new ScreenDisplayCaptureSourceMac(actualDisplayID.value(), "Screen"_s)); // FIXME: figure out what title to use
    if (constraints && source->applyConstraints(*constraints))
        return { };

    return CaptureSourceOrError(WTFMove(source));
}

ScreenDisplayCaptureSourceMac::ScreenDisplayCaptureSourceMac(uint32_t displayID, String&& title)
    : DisplayCaptureSourceCocoa(WTFMove(title))
    , m_displayID(displayID)
{
}

ScreenDisplayCaptureSourceMac::~ScreenDisplayCaptureSourceMac()
{
    if (m_observingDisplayChanges)
        CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallBack, this);

    m_currentFrame = nullptr;
}

bool ScreenDisplayCaptureSourceMac::createDisplayStream()
{
    static const int screenQueueMaximumLength = 6;

    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    auto actualDisplayID = updateDisplayID(m_displayID);
    if (!actualDisplayID) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "invalid display ID: ", m_displayID);
        captureFailed();
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
            captureFailed();
            return false;
        }
        setIntrinsicSize(IntSize(screenWidth, screenHeight));

        if (!m_captureQueue)
            m_captureQueue = adoptOSObject(dispatch_queue_create("ScreenDisplayCaptureSourceMac Capture Queue", DISPATCH_QUEUE_SERIAL));

        NSDictionary* streamOptions = @{
            (__bridge NSString *)kCGDisplayStreamMinimumFrameTime : @(1 / frameRate()),
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
            captureFailed();
            return false;
        }
    }

    if (!m_observingDisplayChanges) {
        CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallBack, this);
        m_observingDisplayChanges = true;
    }

    return true;
}

void ScreenDisplayCaptureSourceMac::startProducingData()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    DisplayCaptureSourceCocoa::startProducingData();

    if (m_isRunning)
        return;

    startDisplayStream();
}

void ScreenDisplayCaptureSourceMac::stopProducingData()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);
    DisplayCaptureSourceCocoa::stopProducingData();

    if (!m_isRunning)
        return;

    if (m_displayStream)
        CGDisplayStreamStop(m_displayStream.get());

    m_isRunning = false;
}

DisplayCaptureSourceCocoa::DisplayFrameType ScreenDisplayCaptureSourceMac::generateFrame()
{
    return DisplayCaptureSourceCocoa::DisplayFrameType { RetainPtr<IOSurfaceRef> { m_currentFrame.ioSurface() } };
}

void ScreenDisplayCaptureSourceMac::startDisplayStream()
{
    auto actualDisplayID = updateDisplayID(m_displayID);
    if (!actualDisplayID)
        return;

    if (m_displayID != actualDisplayID.value()) {
        m_displayID = actualDisplayID.value();
        ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER, "display ID changed to ", static_cast<int>(m_displayID));
    }

    if (!m_displayStream && !createDisplayStream())
        return;

    auto err = CGDisplayStreamStart(m_displayStream.get());
    if (err) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "CGDisplayStreamStart failed with error ", static_cast<int>(err));
        captureFailed();
        return;
    }

    m_isRunning = true;
}

void ScreenDisplayCaptureSourceMac::commitConfiguration()
{
    if (m_isRunning && !m_displayStream)
        startDisplayStream();
}

void ScreenDisplayCaptureSourceMac::displayWasReconfigured(CGDirectDisplayID, CGDisplayChangeSummaryFlags)
{
    // FIXME: implement!
}

void ScreenDisplayCaptureSourceMac::displayReconfigurationCallBack(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo)
{
    if (userInfo)
        reinterpret_cast<ScreenDisplayCaptureSourceMac *>(userInfo)->displayWasReconfigured(display, flags);
}

void ScreenDisplayCaptureSourceMac::newFrame(CGDisplayStreamFrameStatus status, DisplaySurface&& newFrame)
{
    switch (status) {
    case kCGDisplayStreamFrameStatusFrameComplete:
        break;

    case kCGDisplayStreamFrameStatusFrameIdle:
        break;

    case kCGDisplayStreamFrameStatusFrameBlank:
        RELEASE_LOG(Media, "ScreenDisplayCaptureSourceMac::frameAvailable: kCGDisplayStreamFrameStatusFrameBlank");
        break;

    case kCGDisplayStreamFrameStatusStopped:
        RELEASE_LOG(Media, "ScreenDisplayCaptureSourceMac::frameAvailable: kCGDisplayStreamFrameStatusStopped");
        break;
    }

    m_currentFrame = WTFMove(newFrame);
}

Optional<CaptureDevice> ScreenDisplayCaptureSourceMac::screenCaptureDeviceWithPersistentID(const String& deviceID)
{
    bool ok;
    auto displayID = deviceID.toUIntStrict(&ok);
    if (!ok) {
        RELEASE_LOG(Media, "ScreenDisplayCaptureSourceMac::screenCaptureDeviceWithPersistentID: display ID does not convert to 32-bit integer");
        return WTF::nullopt;
    }

    auto actualDisplayID = updateDisplayID(displayID);
    if (!actualDisplayID)
        return WTF::nullopt;

    auto device = CaptureDevice(String::number(actualDisplayID.value()), CaptureDevice::DeviceType::Screen, "ScreenCaptureDevice"_s);
    device.setEnabled(true);

    return device;
}

void ScreenDisplayCaptureSourceMac::screenCaptureDevices(Vector<CaptureDevice>& displays)
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
        RELEASE_LOG(Media, "CGGetActiveDisplayList() returned error %d when trying to get display count", (int)err);
        return;
    }

    if (!displayCount) {
        RELEASE_LOG(Media, "CGGetActiveDisplayList() returned a display count of 0");
        return;
    }

    CGDirectDisplayID activeDisplays[displayCount];
    err = CGGetActiveDisplayList(displayCount, &(activeDisplays[0]), &displayCount);
    if (err) {
        RELEASE_LOG(Media, "CGGetActiveDisplayList() returned error %d when trying to get the active display list", (int)err);
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
