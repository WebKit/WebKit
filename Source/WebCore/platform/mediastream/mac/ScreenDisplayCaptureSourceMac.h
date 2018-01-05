/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "DisplayCaptureSourceCocoa.h"
#include <CoreGraphics/CGDisplayConfiguration.h>
#include <CoreGraphics/CGDisplayStream.h>
#include <wtf/Lock.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/WeakPtr.h>

typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class ScreenDisplayCaptureSourceMac : public DisplayCaptureSourceCocoa {
public:
    static CaptureSourceOrError create(const String&, const MediaConstraints*);

    WEBCORE_EXPORT static VideoCaptureFactory& factory();

    WEBCORE_EXPORT static std::optional<CGDirectDisplayID> updateDisplayID(CGDirectDisplayID);

private:
    ScreenDisplayCaptureSourceMac(uint32_t);
    virtual ~ScreenDisplayCaptureSourceMac();

    static void displayReconfigurationCallBack(CGDirectDisplayID, CGDisplayChangeSummaryFlags, void*);

    void displayWasReconfigured(CGDirectDisplayID, CGDisplayChangeSummaryFlags);

    void frameAvailable(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef, CGDisplayStreamUpdateRef);

    void generateFrame() final;
    void startProducingData() final;
    void stopProducingData() final;
    bool applySize(const IntSize&) final;
    bool applyFrameRate(double) final;
    void commitConfiguration() final;

    RetainPtr<CMSampleBufferRef> sampleBufferFromPixelBuffer(CVPixelBufferRef);
    RetainPtr<CVPixelBufferRef> pixelBufferFromIOSurface(IOSurfaceRef);
    bool createDisplayStream();
    void startDisplayStream();
    
    class DisplaySurface {
    public:
        DisplaySurface() = default;
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

    mutable Lock m_currentFrameMutex;
    DisplaySurface m_currentFrame;
    RetainPtr<CGDisplayStreamRef> m_displayStream;
    RetainPtr<CFMutableDictionaryRef> m_bufferAttributes;
    CGDisplayStreamFrameAvailableHandler m_frameAvailableBlock;
    WeakPtrFactory<ScreenDisplayCaptureSourceMac> m_weakFactory;
    MediaTime m_presentationTimeStamp;
    MediaTime m_frameDuration;

    OSObjectPtr<dispatch_queue_t> m_captureQueue;

    double m_lastFrameTime { NAN };
    uint32_t m_displayID { 0 };
    bool m_isRunning { false };
    bool m_observingDisplayChanges { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
