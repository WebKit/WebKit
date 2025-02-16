/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2014 University of Washington.
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "InspectorTimelineAgent.h"

namespace WebCore {

class Page;
class RunLoopObserver;

class PageTimelineAgent final : public InspectorTimelineAgent {
    WTF_MAKE_NONCOPYABLE(PageTimelineAgent);
    WTF_MAKE_TZONE_ALLOCATED(PageTimelineAgent);
public:
    PageTimelineAgent(PageAgentContext&);
    ~PageTimelineAgent();

    // TimelineBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<void> setAutoCaptureEnabled(bool) override;

    // InspectorInstrumentation
    void didInvalidateLayout();
    void willLayout();
    void didLayout(RenderObject&);
    void willComposite();
    void didComposite();
    void willPaint();
    void didPaint(RenderObject&, const LayoutRect&);
    void willRecalculateStyle();
    void didRecalculateStyle();
    void didScheduleStyleRecalculation();
    void mainFrameStartedLoading();
    void mainFrameNavigated();
    void didCompleteRenderingFrame();

private:
    bool enabled() const override;
    void internalEnable() override;
    void internalDisable() override;

    bool tracking() const override;
    void internalStart(std::optional<int>&& maxCallStackDepth) override;
    void internalStop() override;

    bool shouldStartHeapInstrument() const override;

    void captureScreenshot();

    WeakRef<Page> m_inspectedPage;

    bool m_autoCaptureEnabled { false };
    enum class AutoCapturePhase { None, BeforeLoad, FirstNavigation, AfterFirstNavigation };
    AutoCapturePhase m_autoCapturePhase { AutoCapturePhase::None };

#if PLATFORM(COCOA)
    std::unique_ptr<WebCore::RunLoopObserver> m_frameStartObserver;
    std::unique_ptr<WebCore::RunLoopObserver> m_frameStopObserver;
    int m_runLoopNestingLevel { 0 };
#elif USE(GLIB_EVENT_LOOP)
    std::unique_ptr<RunLoop::Observer> m_runLoopObserver;
#endif
    bool m_startedComposite { false };
    bool m_isCapturingScreenshot { false };
};

} // namespace WebCore
