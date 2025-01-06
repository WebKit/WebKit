/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM) && HAVE(REPLAYKIT)

#include "DisplayCaptureSourceCocoa.h"
#include "Timer.h"
#include <wtf/RetainPtr.h>

OBJC_CLASS RPScreenRecorder;
OBJC_CLASS WebCoreReplayKitScreenRecorderHelper;

namespace WebCore {

class ReplayKitCaptureSource final : public DisplayCaptureSourceCocoa::Capturer, public CanMakeWeakPtr<ReplayKitCaptureSource> {
public:
    explicit ReplayKitCaptureSource(CapturerObserver&);
    virtual ~ReplayKitCaptureSource();

    static bool isAvailable();

    static std::optional<CaptureDevice> screenCaptureDeviceWithPersistentID(const String&);
    static void screenCaptureDevices(Vector<CaptureDevice>&);

    void captureStateDidChange();

private:

    // DisplayCaptureSourceCocoa::Capturer
    bool start() final;
    void stop() final;
    DisplayCaptureSourceCocoa::DisplayFrameType generateFrame() final;
    CaptureDevice::DeviceType deviceType() const final { return CaptureDevice::DeviceType::Screen; }
    DisplaySurfaceType surfaceType() const final { return DisplaySurfaceType::Monitor; }
    virtual void commitConfiguration(const RealtimeMediaSourceSettings&) { }
    virtual IntSize intrinsicSize() const { return m_intrinsicSize; }

    // LoggerHelper
    ASCIILiteral logClassName() const final { return "ReplayKitCaptureSource"_s; }

    void screenRecorderDidOutputVideoSample(RetainPtr<CMSampleBufferRef>&&);
    void startCaptureWatchdogTimer();
    void verifyCaptureIsActive();

    RetainPtr<CMSampleBufferRef> m_currentFrame;
    RetainPtr<RPScreenRecorder> m_screenRecorder;
    RetainPtr<WebCoreReplayKitScreenRecorderHelper> m_recorderHelper;

    Timer m_captureWatchdogTimer;
    uint64_t m_frameCount { 0 };
    uint64_t m_lastFrameCount { 0 };
    IntSize m_intrinsicSize;
    bool m_isRunning { false };
    bool m_interrupted { false };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && HAVE(REPLAYKIT)
