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
#import "WebBackForwardList.h"
#import "WebFullScreenManagerProxy.h"
#import "WebPageProxy.h"
#import "WebProcessProxy.h"
#import <wtf/MathExtras.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/StringBuilder.h>

namespace WebKit {
using namespace WebCore;

static const Seconds swipeSnapshotRemovalWatchdogAfterFirstVisuallyNonEmptyLayoutDuration { 3_s };
static const Seconds swipeSnapshotRemovalActiveLoadMonitoringInterval { 250_ms };

#if PLATFORM(MAC)
static const Seconds swipeSnapshotRemovalWatchdogDuration = 5_s;
#else
static const Seconds swipeSnapshotRemovalWatchdogDuration = 3_s;
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
    , m_pendingSwipeTracker(webPageProxy, *this)
#endif
{
    connectToProcess();

    viewGestureControllersForAllPages().add(webPageProxy.pageID(), this);
}

ViewGestureController::~ViewGestureController()
{
    platformTeardown();

    viewGestureControllersForAllPages().remove(m_webPageProxy.pageID());

    disconnectFromProcess();
}

void ViewGestureController::disconnectFromProcess()
{
    if (!m_isConnectedToProcess)
        return;

    m_webPageProxy.process().removeMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID());
    m_isConnectedToProcess = false;
}

void ViewGestureController::connectToProcess()
{
    if (m_isConnectedToProcess)
        return;

    m_webPageProxy.process().addMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.pageID(), *this);
    m_isConnectedToProcess = true;
}

ViewGestureController* ViewGestureController::controllerForGesture(uint64_t pageID, ViewGestureController::GestureID gestureID)
{
    auto gestureControllerIter = viewGestureControllersForAllPages().find(pageID);
    if (gestureControllerIter == viewGestureControllersForAllPages().end())
        return nullptr;
    if (gestureControllerIter->value->m_currentGestureID != gestureID)
        return nullptr;
    return gestureControllerIter->value;
}

ViewGestureController::GestureID ViewGestureController::takeNextGestureID()
{
    static GestureID nextGestureID;
    return ++nextGestureID;
}

void ViewGestureController::willBeginGesture(ViewGestureType type)
{
    m_activeGestureType = type;
    m_currentGestureID = takeNextGestureID();
}

void ViewGestureController::didEndGesture()
{
    m_activeGestureType = ViewGestureType::None;
    m_currentGestureID = 0;
}
    
void ViewGestureController::setAlternateBackForwardListSourcePage(WebPageProxy* page)
{
    m_alternateBackForwardListSourcePage = makeWeakPtr(page);
}
    
bool ViewGestureController::canSwipeInDirection(SwipeDirection direction) const
{
    if (!m_swipeGestureEnabled)
        return false;

#if ENABLE(FULLSCREEN_API)
    if (m_webPageProxy.fullScreenManager() && m_webPageProxy.fullScreenManager()->isFullScreen())
        return false;
#endif

    RefPtr<WebPageProxy> alternateBackForwardListSourcePage = m_alternateBackForwardListSourcePage.get();
    auto& backForwardList = alternateBackForwardListSourcePage ? alternateBackForwardListSourcePage->backForwardList() : m_webPageProxy.backForwardList();
    if (direction == SwipeDirection::Back)
        return !!backForwardList.backItem();
    return !!backForwardList.forwardItem();
}

void ViewGestureController::didStartProvisionalOrSameDocumentLoadForMainFrame()
{
    m_snapshotRemovalTracker.resume();
#if PLATFORM(MAC)
    requestRenderTreeSizeNotificationIfNeeded();
#endif

    if (auto loadCallback = WTFMove(m_loadCallback))
        loadCallback();
}

void ViewGestureController::didStartProvisionalLoadForMainFrame()
{
    didStartProvisionalOrSameDocumentLoadForMainFrame();
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
    if (m_snapshotRemovalTracker.isPaused()) {
        removeSwipeSnapshot();
        return;
    }

    if (!m_snapshotRemovalTracker.eventOccurred(SnapshotRemovalTracker::MainFrameLoad))
        return;

    // Coming back from the page cache will result in getting a load event, but no first visually non-empty layout.
    // WebCore considers a loaded document enough to be considered visually non-empty, so that's good
    // enough for us too.
    m_snapshotRemovalTracker.cancelOutstandingEvent(SnapshotRemovalTracker::VisuallyNonEmptyLayout);

    checkForActiveLoads();
}

void ViewGestureController::didSameDocumentNavigationForMainFrame(SameDocumentNavigationType type)
{
    didStartProvisionalOrSameDocumentLoadForMainFrame();

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
    auto sinceStart = MonotonicTime::now() - m_startTime;
    RELEASE_LOG(ViewGestures, "Swipe Snapshot Removal (%0.2f ms) - %{public}s", sinceStart.milliseconds(), log.utf8().data());
}
    
void ViewGestureController::SnapshotRemovalTracker::resume()
{
    if (isPaused() && m_outstandingEvents)
        log("resume");
    m_paused = false;
}

void ViewGestureController::SnapshotRemovalTracker::start(Events desiredEvents, WTF::Function<void()>&& removalCallback)
{
    m_outstandingEvents = desiredEvents;
    m_removalCallback = WTFMove(removalCallback);
    m_startTime = MonotonicTime::now();

    log("start");

    startWatchdog(swipeSnapshotRemovalWatchdogDuration);
    
    // Initially start out paused; we'll resume when the load is committed.
    // This avoids processing callbacks from earlier loads.
    pause();
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

    if (isPaused()) {
        log("is paused; ignoring event: " + eventsDescription(event));
        return false;
    }

    log(logReason + eventsDescription(event));

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

bool ViewGestureController::SnapshotRemovalTracker::hasOutstandingEvent(Event event)
{
    return m_outstandingEvents & event;
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

void ViewGestureController::SnapshotRemovalTracker::startWatchdog(Seconds duration)
{
    log(String::format("(re)started watchdog timer for %.1f seconds", duration.seconds()));
    m_watchdogTimer.startOneShot(duration);
}

} // namespace WebKit
