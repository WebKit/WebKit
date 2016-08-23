/*
 * Copyright (C) 2013-2015 Apple Inc. All rights reserved.
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

#import "config.h"
#import "ViewGestureController.h"

#import "Logging.h"
#import "RemoteLayerTreeDrawingAreaProxy.h"
#import "ViewGestureControllerMessages.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <wtf/MathExtras.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringBuilder.h>

using namespace WebCore;

namespace WebKit {

static const std::chrono::seconds swipeSnapshotRemovalWatchdogAfterFirstVisuallyNonEmptyLayoutDuration = 3s;
static const std::chrono::milliseconds swipeSnapshotRemovalActiveLoadMonitoringInterval = 250ms;

#if PLATFORM(MAC)
static const std::chrono::seconds swipeSnapshotRemovalWatchdogDuration = 5s;
#else
static const std::chrono::seconds swipeSnapshotRemovalWatchdogDuration = 3s;
#endif

static HashMap<uint64_t, ViewGestureController*>& viewGestureControllersForAllPages()
{
    // The key in this map is the associated page ID.
    static NeverDestroyed<HashMap<uint64_t, ViewGestureController*>> viewGestureControllers;
    return viewGestureControllers.get();
}

ViewGestureController::ViewGestureController(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_swipeActiveLoadMonitoringTimer(RunLoop::main(), this, &ViewGestureController::checkForActiveLoads)
#if PLATFORM(MAC)
    , m_pendingSwipeTracker(webPageProxy, std::bind(&ViewGestureController::trackSwipeGesture, this, std::placeholders::_1, std::placeholders::_2))
#endif
{
    m_webPageProxy.process().addMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID(), *this);

    viewGestureControllersForAllPages().add(webPageProxy.pageID(), this);
}

ViewGestureController::~ViewGestureController()
{
    platformTeardown();

    viewGestureControllersForAllPages().remove(m_webPageProxy.pageID());

    m_webPageProxy.process().removeMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID());
}

ViewGestureController* ViewGestureController::gestureControllerForPage(uint64_t pageID)
{
    auto gestureControllerIter = viewGestureControllersForAllPages().find(pageID);
    if (gestureControllerIter == viewGestureControllersForAllPages().end())
        return nullptr;
    return gestureControllerIter->value;
}

void ViewGestureController::didFirstVisuallyNonEmptyLayoutForMainFrame()
{
    if (!m_snapshotRemovalTracker.eventOccurred(SnapshotRemovalTracker::VisuallyNonEmptyLayout))
        return;

    m_snapshotRemovalTracker.cancelOutstandingEvent(SnapshotRemovalTracker::MainFrameLoad);
    m_snapshotRemovalTracker.cancelOutstandingEvent(SnapshotRemovalTracker::SubresourceLoads);
    m_snapshotRemovalTracker.startWatchdog(swipeSnapshotRemovalWatchdogAfterFirstVisuallyNonEmptyLayoutDuration);
}

void ViewGestureController::didRepaintAfterNavigation()
{
    m_snapshotRemovalTracker.eventOccurred(SnapshotRemovalTracker::RepaintAfterNavigation);
}

void ViewGestureController::didHitRenderTreeSizeThreshold()
{
    m_snapshotRemovalTracker.eventOccurred(SnapshotRemovalTracker::RenderTreeSizeThreshold);
}

void ViewGestureController::didRestoreScrollPosition()
{
    m_snapshotRemovalTracker.eventOccurred(SnapshotRemovalTracker::ScrollPositionRestoration);
}

void ViewGestureController::didReachMainFrameLoadTerminalState()
{
    if (!m_snapshotRemovalTracker.eventOccurred(SnapshotRemovalTracker::MainFrameLoad))
        return;

    // Coming back from the page cache will result in getting a load event, but no first visually non-empty layout.
    // WebCore considers a loaded document enough to be considered visually non-empty, so that's good
    // enough for us too.
    m_snapshotRemovalTracker.cancelOutstandingEvent(SnapshotRemovalTracker::VisuallyNonEmptyLayout);

    // With Web-process scrolling, we check if the scroll position restoration succeeded by comparing the
    // requested and actual scroll position. It's possible that we will never succeed in restoring
    // the exact scroll position we wanted, in the case of a dynamic page, but we know that by
    // main frame load time that we've gotten as close as we're going to get, so stop waiting.
    // We don't want to do this with UI-side scrolling because scroll position restoration is baked into the transaction.
    // FIXME: It seems fairly dirty to type-check the DrawingArea like this.
    if (auto drawingArea = m_webPageProxy.drawingArea()) {
        if (is<RemoteLayerTreeDrawingAreaProxy>(drawingArea))
            m_snapshotRemovalTracker.cancelOutstandingEvent(SnapshotRemovalTracker::ScrollPositionRestoration);
    }

    checkForActiveLoads();
}

void ViewGestureController::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType type)
{
    bool cancelledOutstandingEvent = false;

    // Same-document navigations don't have a main frame load or first visually non-empty layout.
    cancelledOutstandingEvent |= m_snapshotRemovalTracker.cancelOutstandingEvent(SnapshotRemovalTracker::MainFrameLoad);
    cancelledOutstandingEvent |= m_snapshotRemovalTracker.cancelOutstandingEvent(SnapshotRemovalTracker::VisuallyNonEmptyLayout);

    if (!cancelledOutstandingEvent)
        return;

    if (type != SameDocumentNavigationSessionStateReplace && type != SameDocumentNavigationSessionStatePop)
        return;

    checkForActiveLoads();
}

void ViewGestureController::checkForActiveLoads()
{
    if (m_webPageProxy.pageLoadState().isLoading()) {
        if (!m_swipeActiveLoadMonitoringTimer.isActive())
            m_swipeActiveLoadMonitoringTimer.startRepeating(swipeSnapshotRemovalActiveLoadMonitoringInterval);
        return;
    }

    m_swipeActiveLoadMonitoringTimer.stop();
    m_snapshotRemovalTracker.eventOccurred(SnapshotRemovalTracker::SubresourceLoads);
}

ViewGestureController::SnapshotRemovalTracker::SnapshotRemovalTracker()
    : m_watchdogTimer(RunLoop::main(), this, &SnapshotRemovalTracker::watchdogTimerFired)
{
}

String ViewGestureController::SnapshotRemovalTracker::eventsDescription(Events event)
{
    StringBuilder description;

    if (event & ViewGestureController::SnapshotRemovalTracker::VisuallyNonEmptyLayout)
        description.append("VisuallyNonEmptyLayout ");

    if (event & ViewGestureController::SnapshotRemovalTracker::RenderTreeSizeThreshold)
        description.append("RenderTreeSizeThreshold ");

    if (event & ViewGestureController::SnapshotRemovalTracker::RepaintAfterNavigation)
        description.append("RepaintAfterNavigation ");

    if (event & ViewGestureController::SnapshotRemovalTracker::MainFrameLoad)
        description.append("MainFrameLoad ");

    if (event & ViewGestureController::SnapshotRemovalTracker::SubresourceLoads)
        description.append("SubresourceLoads ");

    if (event & ViewGestureController::SnapshotRemovalTracker::ScrollPositionRestoration)
        description.append("ScrollPositionRestoration ");
    
    return description.toString();
}


void ViewGestureController::SnapshotRemovalTracker::log(const String& log) const
{
#if !LOG_DISABLED
    auto now = std::chrono::steady_clock::now();
    double millisecondsSinceStart = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - m_startTime).count();
#endif
    LOG(ViewGestures, "Swipe Snapshot Removal (%0.2f ms) - %s", millisecondsSinceStart, log.utf8().data());
}

void ViewGestureController::SnapshotRemovalTracker::start(Events desiredEvents, std::function<void()> removalCallback)
{
    m_outstandingEvents = desiredEvents;
    m_removalCallback = WTFMove(removalCallback);
    m_startTime = std::chrono::steady_clock::now();

    log("start");

    startWatchdog(swipeSnapshotRemovalWatchdogDuration);
}

void ViewGestureController::SnapshotRemovalTracker::reset()
{
    if (m_outstandingEvents)
        log("reset; had outstanding events: " + eventsDescription(m_outstandingEvents));
    m_outstandingEvents = 0;
    m_watchdogTimer.stop();
    m_removalCallback = nullptr;
}

bool ViewGestureController::SnapshotRemovalTracker::stopWaitingForEvent(Events event, const String& logReason)
{
    ASSERT(hasOneBitSet(event));

    if (!(m_outstandingEvents & event))
        return false;

#if LOG_DISABLED
    UNUSED_PARAM(logReason);
#endif
    log(logReason +  eventsDescription(event));

    m_outstandingEvents &= ~event;

    fireRemovalCallbackIfPossible();
    return true;
}

bool ViewGestureController::SnapshotRemovalTracker::eventOccurred(Events event)
{
    return stopWaitingForEvent(event, "outstanding event occurred: ");
}

bool ViewGestureController::SnapshotRemovalTracker::cancelOutstandingEvent(Events event)
{
    return stopWaitingForEvent(event, "wait for event cancelled: ");
}

void ViewGestureController::SnapshotRemovalTracker::fireRemovalCallbackIfPossible()
{
    if (m_outstandingEvents) {
        log("deferring removal; had outstanding events: " + eventsDescription(m_outstandingEvents));
        return;
    }

    fireRemovalCallbackImmediately();
}

void ViewGestureController::SnapshotRemovalTracker::fireRemovalCallbackImmediately()
{
    m_watchdogTimer.stop();

    auto removalCallback = WTFMove(m_removalCallback);
    if (removalCallback) {
        log("removing snapshot");
        reset();
        removalCallback();
    }
}

void ViewGestureController::SnapshotRemovalTracker::watchdogTimerFired()
{
    log("watchdog timer fired");
    fireRemovalCallbackImmediately();
}

void ViewGestureController::SnapshotRemovalTracker::startWatchdog(std::chrono::seconds duration)
{
    log(String::format("(re)started watchdog timer for %lld seconds", duration.count()));
    m_watchdogTimer.startOneShot(duration.count());
}

} // namespace WebKit
