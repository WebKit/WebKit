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

#include "DisplayCaptureSourceCocoa.h"
#include <CoreGraphics/CGDisplayConfiguration.h>
#include <CoreGraphics/CGDisplayStream.h>
#include <wtf/BlockPtr.h>
#include <wtf/Lock.h>
#include <wtf/OSObjectPtr.h>

typedef struct __CVBuffer *CVPixelBufferRef;
typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

class CGDisplayStreamCaptureSource : public DisplayCaptureSourceCocoa::Capturer, public CanMakeWeakPtr<CGDisplayStreamCaptureSource> {
public:
    explicit CGDisplayStreamCaptureSource() = default;
    virtual ~CGDisplayStreamCaptureSource();

protected:
    using FrameAvailableCallback = void (^)(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef, CGDisplayStreamUpdateRef);
    virtual RetainPtr<CGDisplayStreamRef> createDisplayStream() = 0;
    virtual bool checkDisplayStream() { return true; }

    CGDisplayStreamRef displayStream() const { return m_displayStream.get(); }
    void invalidateDisplayStream() { m_displayStream = nullptr; }

    FrameAvailableCallback frameAvailableHandler();
    dispatch_queue_t captureQueue();

    uint32_t width() const { return m_width; }
    uint32_t height() const { return m_height; }
    float frameRate() const { return m_frameRate; }

private:
    static void displayReconfigurationCallBack(CGDirectDisplayID, CGDisplayChangeSummaryFlags, void*);

    // DisplayCaptureSourceCocoa::Capturer
    bool start() final;
    void stop() final;
    DisplayCaptureSourceCocoa::DisplayFrameType generateFrame() final;
    void commitConfiguration(const RealtimeMediaSourceSettings&) final;

    void displayWasReconfigured(CGDirectDisplayID, CGDisplayChangeSummaryFlags);
    bool startDisplayStream();

    void newFrame(CGDisplayStreamFrameStatus, RetainPtr<IOSurfaceRef>&&);

    RetainPtr<IOSurfaceRef> m_currentFrame;
    RetainPtr<CGDisplayStreamRef> m_displayStream;
    OSObjectPtr<dispatch_queue_t> m_captureQueue;
    BlockPtr<void(CGDisplayStreamFrameStatus, uint64_t, IOSurfaceRef, CGDisplayStreamUpdateRef)> m_frameAvailableHandler;

    uint32_t m_width { 0 };
    uint32_t m_height { 0 };
    float m_frameRate { 0 };

    bool m_isRunning { false };
    bool m_observingDisplayChanges { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(MAC)
