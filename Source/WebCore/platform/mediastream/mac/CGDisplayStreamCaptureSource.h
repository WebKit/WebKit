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

#pragma once

#if ENABLE(MEDIA_STREAM) && PLATFORM(MAC)

#include "DisplayCaptureSourceMac.h"
#include "IOSurface.h"
#include <CoreGraphics/CGDisplayConfiguration.h>
#include <CoreGraphics/CGDisplayStream.h>
#include <wtf/BlockPtr.h>
#include <wtf/Lock.h>
#include <wtf/OSObjectPtr.h>

typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class CGDisplayStreamCaptureSource : public DisplayCaptureSourceMac::Capturer, public CanMakeWeakPtr<CGDisplayStreamCaptureSource> {
public:
    explicit CGDisplayStreamCaptureSource() = default;
    ~CGDisplayStreamCaptureSource();

protected:
    using FrameAvailableCallback = void (^)(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef, CGDisplayStreamUpdateRef);
    virtual RetainPtr<CGDisplayStreamRef> createDisplayStream(float, FrameAvailableCallback, dispatch_queue_t) = 0;
    virtual bool checkDisplayStream() { return true; }

    CGDisplayStreamRef displayStream() const { return m_displayStream.get(); }
    void invalidateDisplayStream() { m_displayStream = nullptr; }

private:
    static void displayReconfigurationCallBack(CGDirectDisplayID, CGDisplayChangeSummaryFlags, void*);

    // DisplayCaptureSourceMac::Capturer
    bool start(float frameRate) final;
    void stop() final;
    DisplayCaptureSourceMac::DisplayFrameType generateFrame() final;
    void commitConfiguration(float frameRate) final;

    void displayWasReconfigured(CGDirectDisplayID, CGDisplayChangeSummaryFlags);
    bool startDisplayStream(float frameRate);
    FrameAvailableCallback frameAvailableHandler();

    class DisplaySurface {
    public:
        DisplaySurface() = default;
        explicit DisplaySurface(IOSurfaceRef surface)
            : m_surface(surface)
        {
            if (m_surface)
                IOSurfaceIncrementUseCount(m_surface.get());
        }

        ~DisplaySurface()
        {
            if (m_surface)
                IOSurfaceDecrementUseCount(m_surface.get());
        }

        DisplaySurface& operator=(IOSurfaceRef surface)
        {
            if (m_surface)
                IOSurfaceDecrementUseCount(m_surface.get());
            if (surface)
                IOSurfaceIncrementUseCount(surface);
            m_surface = surface;
            return *this;
        }

        IOSurfaceRef ioSurface() const { return m_surface.get(); }

    private:
        RetainPtr<IOSurfaceRef> m_surface;
    };

    void newFrame(CGDisplayStreamFrameStatus, DisplaySurface&&);

    DisplaySurface m_currentFrame;
    RetainPtr<CGDisplayStreamRef> m_displayStream;
    OSObjectPtr<dispatch_queue_t> m_captureQueue;
    BlockPtr<void(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef, CGDisplayStreamUpdateRef)> m_frameAvailableHandler;

    bool m_isRunning { false };
    bool m_observingDisplayChanges { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
