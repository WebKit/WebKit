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

#include "config.h"
#include "PageTimelineAgent.h"

#include "DocumentView.h"
#include "FrameSnapshotting.h"
#include "ImageBuffer.h"
#include "ImageUtilities.h"
#include "InspectorBackendClient.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "PageInspectorController.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "TimelineRecordFactory.h"
#include "WebDebuggerAgent.h"

#if PLATFORM(IOS_FAMILY)
#include "WebCoreThreadInternal.h"
#include <wtf/RuntimeApplicationChecks.h>
#endif

#if PLATFORM(COCOA)
#include "RunLoopObserver.h"
#endif

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageTimelineAgent);

#if PLATFORM(COCOA)
static CFRunLoopRef currentRunLoop()
{
#if PLATFORM(IOS_FAMILY)
    // A race condition during WebView deallocation can lead to a crash if the layer sync run loop
    // observer is added to the main run loop <rdar://problem/9798550>. However, for responsiveness,
    // we still allow this, see <rdar://problem/7403328>. Since the race condition and subsequent
    // crash are especially troublesome for Apple Books, we never allow the observer to be added to the
    // main run loop in Apple Books.
    if (WTF::CocoaApplication::isAppleBooks())
        return WebThreadRunLoop();
#endif
    return CFRunLoopGetCurrent();
}
#endif

PageTimelineAgent::PageTimelineAgent(PageAgentContext& context)
    : InspectorTimelineAgent(context)
    , m_inspectedPage(context.inspectedPage)
{
}

PageTimelineAgent::~PageTimelineAgent() = default;

bool PageTimelineAgent::enabled() const
{
    return Ref { m_instrumentingAgents.get() }->enabledPageTimelineAgent() == this && InspectorTimelineAgent::enabled();
}

void PageTimelineAgent::internalEnable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledPageTimelineAgent(this);

    InspectorTimelineAgent::internalEnable();
}

void PageTimelineAgent::internalDisable()
{
    Ref { m_instrumentingAgents.get() }->setEnabledPageTimelineAgent(nullptr);

    m_autoCaptureEnabled = false;

    InspectorTimelineAgent::internalDisable();
}

bool PageTimelineAgent::tracking() const
{
    return Ref { m_instrumentingAgents.get() }->trackingPageTimelineAgent() == this && InspectorTimelineAgent::tracking();
}

void PageTimelineAgent::internalStart(std::optional<int>&& maxCallStackDepth)
{
    Ref { m_instrumentingAgents.get() }->setTrackingPageTimelineAgent(this);

    // FIXME: Abstract away platform-specific code once https://bugs.webkit.org/show_bug.cgi?id=142748 is fixed.

#if PLATFORM(COCOA)
    m_frameStartObserver = makeUnique<RunLoopObserver>(RunLoopObserver::WellKnownOrder::InspectorFrameBegin, [weakThis = WeakPtr { *this }] {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis)
            return;
        if (!checkedThis->tracking() || protect(checkedThis->environment())->debugger()->isPaused())
            return;
        if (!checkedThis->m_runLoopNestingLevel) {
            checkedThis->pushCurrentRecord(JSON::Object::create(), TimelineRecordType::RenderingFrame, false);
            checkedThis->m_runLoopNestingLevel++;
        }
    });

    m_frameStartObserver->schedule(currentRunLoop(), { RunLoopObserver::Activity::Entry, RunLoopObserver::Activity::AfterWaiting });

    // Create a runloop record and increment the runloop nesting level, to capture the current turn of the main runloop
    // (which is the outer runloop if recording started while paused in the debugger).
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::RenderingFrame, false);

    m_runLoopNestingLevel = 1;
#elif USE(GLIB_EVENT_LOOP)
    m_runLoopObserver = RunLoop::EventObserver::create([weakThis = WeakPtr { *this }](RunLoop::Event event, const String& name) {
        CheckedPtr checkedThis = weakThis.get();
        if (!checkedThis)
            return;
        if (!checkedThis->tracking() || protect(checkedThis->environment())->debugger()->isPaused())
            return;

        switch (event) {
        case RunLoop::Event::WillDispatch:
            checkedThis->pushCurrentRecord(TimelineRecordFactory::createRenderingFrameData(name), TimelineRecordType::RenderingFrame, false);
            break;
        case RunLoop::Event::DidDispatch:
            if (checkedThis->m_startedComposite)
                checkedThis->didComposite();
            checkedThis->didCompleteCurrentRecord(TimelineRecordType::RenderingFrame);
            break;
        }
    });
    RunLoop::currentSingleton().observeEvent(*m_runLoopObserver);
#endif

    InspectorTimelineAgent::internalStart(WTF::move(maxCallStackDepth));

    if (auto* client = m_inspectedPage->inspectorController().inspectorBackendClient())
        client->timelineRecordingChanged(true);
}

void PageTimelineAgent::internalStop()
{
    Ref { m_instrumentingAgents.get() }->setTrackingPageTimelineAgent(nullptr);

    m_autoCapturePhase = AutoCapturePhase::None;

#if PLATFORM(COCOA)
    m_frameStartObserver = nullptr;
    m_runLoopNestingLevel = 0;
#elif USE(GLIB_EVENT_LOOP)
    m_runLoopObserver = nullptr;
#endif
    m_startedComposite = false;

    InspectorTimelineAgent::internalStop();

    if (auto* client = m_inspectedPage->inspectorController().inspectorBackendClient())
        client->timelineRecordingChanged(false);
}

Inspector::Protocol::ErrorStringOr<void> PageTimelineAgent::setAutoCaptureEnabled(bool enabled)
{
    m_autoCaptureEnabled = enabled;

    return { };
}

void PageTimelineAgent::didInvalidateLayout()
{
    appendRecord(JSON::Object::create(), TimelineRecordType::InvalidateLayout, true);
}

void PageTimelineAgent::willLayout()
{
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::Layout, true);
}

void PageTimelineAgent::didLayout(const Vector<FloatQuad>& layoutAreas)
{
    auto* entry = lastRecordEntry();
    if (!entry)
        return;

    ASSERT(entry->type == TimelineRecordType::Layout);
    ASSERT(!layoutAreas.isEmpty());
    if (!layoutAreas.isEmpty())
        TimelineRecordFactory::appendLayoutRoot(entry->data.get(), layoutAreas[0]);

    didCompleteCurrentRecord(TimelineRecordType::Layout);
}

void PageTimelineAgent::didScheduleStyleRecalculation()
{
    appendRecord(JSON::Object::create(), TimelineRecordType::ScheduleStyleRecalculation, true);
}

void PageTimelineAgent::willRecalculateStyle()
{
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::RecalculateStyles, true);
}

void PageTimelineAgent::didRecalculateStyle()
{
    didCompleteCurrentRecord(TimelineRecordType::RecalculateStyles);
}

void PageTimelineAgent::willComposite()
{
    ASSERT(!m_startedComposite);
    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::Composite, true);
    m_startedComposite = true;
}

void PageTimelineAgent::didComposite()
{
    if (m_startedComposite)
        didCompleteCurrentRecord(TimelineRecordType::Composite);
    m_startedComposite = false;

    if (instruments().contains(Inspector::Protocol::Timeline::Instrument::Screenshot))
        captureScreenshot();
}

void PageTimelineAgent::willPaint()
{
    if (m_isCapturingScreenshot)
        return;

    pushCurrentRecord(JSON::Object::create(), TimelineRecordType::Paint, true);
}

void PageTimelineAgent::didPaint(RenderObject& renderer, const LayoutRect& clipRect)
{
    if (m_isCapturingScreenshot)
        return;

    auto* entry = lastRecordEntry();
    if (!entry)
        return;

    ASSERT(entry->type == TimelineRecordType::Paint);

    auto clipQuadInRootView = protect(renderer.view().frameView())->contentsToRootView(renderer.localToAbsoluteQuad({ clipRect }));
    entry->data = TimelineRecordFactory::createPaintData(clipQuadInRootView);

    didCompleteCurrentRecord(TimelineRecordType::Paint);
}

void PageTimelineAgent::mainFrameStartedLoading()
{
    if (!m_autoCaptureEnabled)
        return;

    if (instruments().isEmpty())
        return;

    m_autoCapturePhase = AutoCapturePhase::BeforeLoad;

    // Pre-emptively disable breakpoints. The frontend must re-enable them.
    if (auto* webDebuggerAgent = Ref { m_instrumentingAgents.get() }->enabledWebDebuggerAgent())
        std::ignore = webDebuggerAgent->setBreakpointsActive(false);

    // Inform the frontend we started an auto capture. The frontend must stop capture.
    autoCaptureStarted();

    toggleInstruments(InstrumentState::Start);
}

void PageTimelineAgent::mainFrameNavigated()
{
    if (m_autoCapturePhase == AutoCapturePhase::BeforeLoad) {
        m_autoCapturePhase = AutoCapturePhase::FirstNavigation;
        toggleInstruments(InstrumentState::Start);
        m_autoCapturePhase = AutoCapturePhase::AfterFirstNavigation;
    }
}

void PageTimelineAgent::didCompleteRenderingFrame()
{
#if PLATFORM(COCOA)
    if (!tracking() || protect(environment())->debugger()->isPaused())
        return;

    ASSERT(m_runLoopNestingLevel > 0);
    m_runLoopNestingLevel--;
    if (m_runLoopNestingLevel)
        return;

    if (m_startedComposite)
        didComposite();

    didCompleteCurrentRecord(TimelineRecordType::RenderingFrame);
#endif
}

bool PageTimelineAgent::shouldStartHeapInstrument() const
{
    if (m_autoCapturePhase == AutoCapturePhase::BeforeLoad || m_autoCapturePhase == AutoCapturePhase::AfterFirstNavigation)
        return false;
    return InspectorTimelineAgent::shouldStartHeapInstrument();
}

void PageTimelineAgent::captureScreenshot()
{
    SetForScope isTakingScreenshot(m_isCapturingScreenshot, true);

    auto snapshotStartTime = timestamp();

    Ref inspectedPage = m_inspectedPage.get();
    RefPtr localMainFrame = inspectedPage->localMainFrame();
    if (!localMainFrame)
        return;

    RefPtr localMainFrameView = localMainFrame->view();
    if (!localMainFrameView)
        return;

    if (RefPtr snapshot = snapshotFrameRect(*localMainFrame, localMainFrameView->unobscuredContentRect(), { { }, PixelFormat::BGRA8, DestinationColorSpace::SRGB() })) {
        Ref snapshotRecord = TimelineRecordFactory::createScreenshotData(encodeDataURL(WTF::move(snapshot), "image/png"_s));
        pushCurrentRecord(WTF::move(snapshotRecord), TimelineRecordType::Screenshot, false, snapshotStartTime);
        didCompleteCurrentRecord(TimelineRecordType::Screenshot);
    }
}

} // namespace WebCore
