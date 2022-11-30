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
#include "CGDisplayStreamCaptureSource.h"

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

namespace WebCore {

CGDisplayStreamCaptureSource::~CGDisplayStreamCaptureSource()
{
    m_currentFrame = nullptr;
}

bool CGDisplayStreamCaptureSource::start()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    if (m_isRunning)
        return true;

    return startDisplayStream();
}

void CGDisplayStreamCaptureSource::stop()
{
    ALWAYS_LOG_IF(loggerPtr(), LOGIDENTIFIER);

    if (!m_isRunning)
        return;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if (m_displayStream)
        CGDisplayStreamStop(m_displayStream.get());
    ALLOW_DEPRECATED_DECLARATIONS_END

    m_isRunning = false;
}

DisplayCaptureSourceCocoa::DisplayFrameType CGDisplayStreamCaptureSource::generateFrame()
{
    return m_currentFrame;
}

bool CGDisplayStreamCaptureSource::startDisplayStream()
{
    if (!checkDisplayStream())
        return false;

    if (!m_displayStream) {
        m_displayStream = createDisplayStream();
        if (!m_displayStream)
            return false;

        if (!m_observingDisplayChanges) {
            CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallBack, this);
            m_observingDisplayChanges = true;
        }
    }

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    auto err = CGDisplayStreamStart(m_displayStream.get());
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (err) {
        ERROR_LOG_IF(loggerPtr(), LOGIDENTIFIER, "CGDisplayStreamStart failed with error ", static_cast<int>(err));
        return false;
    }

    m_isRunning = true;
    return true;
}

void CGDisplayStreamCaptureSource::commitConfiguration(const RealtimeMediaSourceSettings& settings)
{
    if (m_width == settings.width() && m_height == settings.height() && m_frameRate == settings.frameRate())
        return;

    m_width = settings.width();
    m_height = settings.height();
    m_frameRate = settings.frameRate();

    if (m_displayStream) {
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        CGDisplayStreamStop(m_displayStream.get());
        ALLOW_DEPRECATED_DECLARATIONS_END
        m_displayStream = nullptr;
    }

    if (m_isRunning)
        startDisplayStream();
}

void CGDisplayStreamCaptureSource::displayWasReconfigured(CGDirectDisplayID, CGDisplayChangeSummaryFlags)
{
    // FIXME: implement!
}

void CGDisplayStreamCaptureSource::displayReconfigurationCallBack(CGDirectDisplayID display, CGDisplayChangeSummaryFlags flags, void *userInfo)
{
    if (userInfo)
        reinterpret_cast<CGDisplayStreamCaptureSource *>(userInfo)->displayWasReconfigured(display, flags);
}

void CGDisplayStreamCaptureSource::newFrame(CGDisplayStreamFrameStatus status, RetainPtr<IOSurfaceRef>&& newFrame)
{
    switch (status) {
    case kCGDisplayStreamFrameStatusFrameComplete:
        break;

    case kCGDisplayStreamFrameStatusFrameIdle:
        break;

    case kCGDisplayStreamFrameStatusFrameBlank:
        RELEASE_LOG(WebRTC, "CGDisplayStreamCaptureSource::frameAvailable: kCGDisplayStreamFrameStatusFrameBlank");
        break;

    case kCGDisplayStreamFrameStatusStopped:
        RELEASE_LOG(WebRTC, "CGDisplayStreamCaptureSource::frameAvailable: kCGDisplayStreamFrameStatusStopped");
        break;
    }

    m_currentFrame = WTFMove(newFrame);
}

CGDisplayStreamFrameAvailableHandler CGDisplayStreamCaptureSource::frameAvailableHandler()
{
    if (m_frameAvailableHandler)
        return m_frameAvailableHandler.get();

    WeakPtr weakThis { *this };
    m_frameAvailableHandler = ^(CGDisplayStreamFrameStatus status, uint64_t displayTime, IOSurfaceRef frameSurface, CGDisplayStreamUpdateRef updateRef)
    {
        if (!frameSurface || !displayTime)
            return;

        size_t count;
        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        auto* rects = CGDisplayStreamUpdateGetRects(updateRef, kCGDisplayStreamUpdateDirtyRects, &count);
        ALLOW_DEPRECATED_DECLARATIONS_END
        if (!rects || !count)
            return;

        RunLoop::main().dispatch([weakThis, status, frame = retainPtr(frameSurface)]() mutable {
            if (!weakThis)
                return;
            weakThis->newFrame(status, WTFMove(frame));
        });
    };

    return m_frameAvailableHandler.get();
}

dispatch_queue_t CGDisplayStreamCaptureSource::captureQueue()
{
    if (!m_captureQueue)
        m_captureQueue = adoptOSObject(dispatch_queue_create("CGDisplayStreamCaptureSource Capture Queue", DISPATCH_QUEUE_SERIAL));

    return m_captureQueue.get();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
