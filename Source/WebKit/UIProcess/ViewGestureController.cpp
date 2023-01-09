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

#include "config.h"
#include "ViewGestureController.h"

#include "APINavigation.h"
#include "Logging.h"
#include "ViewGestureControllerMessages.h"
#include "WebBackForwardList.h"
#include "WebFullScreenManagerProxy.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringConcatenateNumbers.h>

#if PLATFORM(COCOA)
#include "RemoteLayerTreeDrawingAreaProxy.h"
#endif

#if !PLATFORM(IOS_FAMILY)
#include "DrawingAreaProxy.h"
#include "ProvisionalPageProxy.h"
#include "ViewGestureGeometryCollectorMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

static const Seconds swipeSnapshotRemovalWatchdogAfterFirstVisuallyNonEmptyLayoutDuration { 3_s };
static const Seconds swipeSnapshotRemovalActiveLoadMonitoringInterval { 250_ms };
static const Seconds swipeSnapshotRemovalWatchdogDuration = 3_s;

#if !PLATFORM(IOS_FAMILY)
static const float minimumHorizontalSwipeDistance = 15;
static const float minimumScrollEventRatioForSwipe = 0.5;

static const float swipeSnapshotRemovalRenderTreeSizeTargetFraction = 0.5;
#endif

static HashMap<WebPageProxyIdentifier, ViewGestureController*>& viewGestureControllersForAllPages()
{
    // The key in this map is the associated page ID.
    static NeverDestroyed<HashMap<WebPageProxyIdentifier, ViewGestureController*>> viewGestureControllers;
    return viewGestureControllers.get();
}

ViewGestureController::ViewGestureController(WebPageProxy& webPageProxy)
    : m_webPageProxy(webPageProxy)
    , m_swipeActiveLoadMonitoringTimer(RunLoop::main(), this, &ViewGestureController::checkForActiveLoads)
#if !PLATFORM(IOS_FAMILY)
    , m_pendingSwipeTracker(webPageProxy, *this)
#endif
#if PLATFORM(GTK)
    , m_swipeProgressTracker(webPageProxy, *this)
#endif
{
    if (webPageProxy.hasRunningProcess())
        connectToProcess();

    viewGestureControllersForAllPages().add(webPageProxy.identifier(), this);
}

ViewGestureController::~ViewGestureController()
{
    platformTeardown();

    viewGestureControllersForAllPages().remove(m_webPageProxy.identifier());

    disconnectFromProcess();
}

void ViewGestureController::disconnectFromProcess()
{
    if (!m_isConnectedToProcess)
        return;

    m_webPageProxy.process().removeMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.webPageID());
    m_isConnectedToProcess = false;
}

void ViewGestureController::connectToProcess()
{
    if (m_isConnectedToProcess)
        return;

    m_webPageProxy.process().addMessageReceiver(Messages::ViewGestureController::messageReceiverName(), m_webPageProxy.webPageID(), *this);
    m_isConnectedToProcess = true;
}

ViewGestureController* ViewGestureController::controllerForGesture(WebPageProxyIdentifier pageID, ViewGestureController::GestureID gestureID)
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
    LOG(ViewGestures, "ViewGestureController::willBeginGesture %d", (int)type);

    m_activeGestureType = type;
    m_currentGestureID = takeNextGestureID();
}

void ViewGestureController::didEndGesture()
{
    LOG(ViewGestures, "ViewGestureController::didEndGesture");

    m_activeGestureType = ViewGestureType::None;
    m_currentGestureID = 0;
}

void ViewGestureController::setAlternateBackForwardListSourcePage(WebPageProxy* page)
{
    m_alternateBackForwardListSourcePage = page;
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
    m_didStartProvisionalLoad = true;
    m_snapshotRemovalTracker.resume();
#if !PLATFORM(IOS_FAMILY)
    requestRenderTreeSizeNotificationIfNeeded();
#endif

    if (auto loadCallback = std::exchange(m_loadCallback, nullptr))
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

void ViewGestureController::didReachNavigationTerminalState(API::Navigation* navigation)
{
    if (!m_pendingNavigation || navigation != m_pendingNavigation)
        return;

    if (m_snapshotRemovalTracker.isPaused() && m_snapshotRemovalTracker.hasRemovalCallback()) {
        removeSwipeSnapshot();
        return;
    }

    if (!m_snapshotRemovalTracker.eventOccurred(SnapshotRemovalTracker::MainFrameLoad))
        return;

    // Coming back from the back/forward cache will result in getting a load event, but no first visually non-empty layout.
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

    if (type != SameDocumentNavigationType::SessionStateReplace && type != SameDocumentNavigationType::SessionStatePop)
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

    if (event & ViewGestureController::SnapshotRemovalTracker::SwipeAnimationEnd)
        description.append("SwipeAnimationEnd ");

    return description.toString();
}


void ViewGestureController::SnapshotRemovalTracker::log(StringView log) const
{
    RELEASE_LOG(ViewGestures, "Swipe Snapshot Removal (%0.2f ms) - %s", (MonotonicTime::now() - m_startTime).milliseconds(), log.utf8().data());
}

void ViewGestureController::SnapshotRemovalTracker::resume()
{
    if (isPaused() && m_outstandingEvents)
        log("resume"_s);
    m_paused = false;
}

void ViewGestureController::SnapshotRemovalTracker::start(Events desiredEvents, WTF::Function<void()>&& removalCallback)
{
    m_outstandingEvents = desiredEvents;
    m_removalCallback = WTFMove(removalCallback);
    m_startTime = MonotonicTime::now();

    log("start"_s);

    startWatchdog(swipeSnapshotRemovalWatchdogDuration);

    // Initially start out paused; we'll resume when the load is committed.
    // This avoids processing callbacks from earlier loads.
    pause();
}

void ViewGestureController::SnapshotRemovalTracker::reset()
{
    if (m_outstandingEvents)
        log(makeString("reset; had outstanding events: ", eventsDescription(m_outstandingEvents)));
    m_outstandingEvents = 0;
    m_watchdogTimer.stop();
    m_removalCallback = nullptr;
}

bool ViewGestureController::SnapshotRemovalTracker::stopWaitingForEvent(Events event, ASCIILiteral logReason, ShouldIgnoreEventIfPaused shouldIgnoreEventIfPaused)
{
    ASSERT(hasOneBitSet(event));

    if (!(m_outstandingEvents & event))
        return false;

    if (shouldIgnoreEventIfPaused == ShouldIgnoreEventIfPaused::Yes && isPaused()) {
        log(makeString("is paused; ignoring event: ", eventsDescription(event)));
        return false;
    }

    log(makeString(logReason, eventsDescription(event)));

    m_outstandingEvents &= ~event;

    fireRemovalCallbackIfPossible();
    return true;
}

bool ViewGestureController::SnapshotRemovalTracker::eventOccurred(Events event, ShouldIgnoreEventIfPaused shouldIgnoreEventIfPaused)
{
    return stopWaitingForEvent(event, "outstanding event occurred: "_s, shouldIgnoreEventIfPaused);
}

bool ViewGestureController::SnapshotRemovalTracker::cancelOutstandingEvent(Events event)
{
    return stopWaitingForEvent(event, "wait for event cancelled: "_s);
}

bool ViewGestureController::SnapshotRemovalTracker::hasOutstandingEvent(Event event)
{
    return m_outstandingEvents & event;
}

void ViewGestureController::SnapshotRemovalTracker::fireRemovalCallbackIfPossible()
{
    if (m_outstandingEvents) {
        log(makeString("deferring removal; had outstanding events: ", eventsDescription(m_outstandingEvents)));
        return;
    }

    fireRemovalCallbackImmediately();
}

void ViewGestureController::SnapshotRemovalTracker::fireRemovalCallbackImmediately()
{
    m_watchdogTimer.stop();

    auto removalCallback = WTFMove(m_removalCallback);
    if (removalCallback) {
        log("removing snapshot"_s);
        reset();
        removalCallback();
    }
}

void ViewGestureController::SnapshotRemovalTracker::watchdogTimerFired()
{
    log("watchdog timer fired"_s);
    fireRemovalCallbackImmediately();
}

void ViewGestureController::SnapshotRemovalTracker::startWatchdog(Seconds duration)
{
    log(makeString("(re)started watchdog timer for ", duration.seconds(), " seconds"));
    m_watchdogTimer.startOneShot(duration);
}

#if !PLATFORM(IOS_FAMILY)
static bool deltaShouldCancelSwipe(FloatSize delta)
{
    return std::abs(delta.height()) >= std::abs(delta.width()) * minimumScrollEventRatioForSwipe;
}

ViewGestureController::PendingSwipeTracker::PendingSwipeTracker(WebPageProxy& webPageProxy, ViewGestureController& viewGestureController)
    : m_viewGestureController(viewGestureController)
    , m_webPageProxy(webPageProxy)
{
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanBecomeSwipe(PlatformScrollEvent event, ViewGestureController::SwipeDirection& potentialSwipeDirection)
{
    if (!scrollEventCanStartSwipe(event) || !scrollEventCanInfluenceSwipe(event))
        return false;

    FloatSize size = scrollEventGetScrollingDeltas(event);

    if (deltaShouldCancelSwipe(size))
        return false;

    bool isPinnedToLeft = m_shouldIgnorePinnedState || m_webPageProxy.pinnedState().left();
    bool isPinnedToRight = m_shouldIgnorePinnedState || m_webPageProxy.pinnedState().right();

    bool tryingToSwipeBack = size.width() > 0 && isPinnedToLeft;
    bool tryingToSwipeForward = size.width() < 0 && isPinnedToRight;
    if (m_webPageProxy.userInterfaceLayoutDirection() != WebCore::UserInterfaceLayoutDirection::LTR)
        std::swap(tryingToSwipeBack, tryingToSwipeForward);

    if (!tryingToSwipeBack && !tryingToSwipeForward)
        return false;

    potentialSwipeDirection = tryingToSwipeBack ? SwipeDirection::Back : SwipeDirection::Forward;
    return m_viewGestureController.canSwipeInDirection(potentialSwipeDirection);
}

bool ViewGestureController::PendingSwipeTracker::handleEvent(PlatformScrollEvent event)
{
    LOG(ViewGestures, "PendingSwipeTracker::handleEvent - state %d", (int)m_state);

    if (scrollEventCanEndSwipe(event)) {
        reset("gesture ended");
        return false;
    }

    if (m_state == State::None) {
        LOG(ViewGestures, "PendingSwipeTracker::handleEvent - scroll can become swipe %d shouldIgnorePinnedState %d, page will handle scrolls %d", scrollEventCanBecomeSwipe(event, m_direction), m_shouldIgnorePinnedState, m_webPageProxy.willHandleHorizontalScrollEvents());

        if (!scrollEventCanBecomeSwipe(event, m_direction))
            return false;

        if (!m_shouldIgnorePinnedState && m_webPageProxy.willHandleHorizontalScrollEvents()) {
            m_state = State::WaitingForWebCore;
            LOG(ViewGestures, "Swipe Start Hysteresis - waiting for WebCore to handle event");
        }
    }

    if (m_state == State::WaitingForWebCore)
        return false;

    return tryToStartSwipe(event);
}

void ViewGestureController::PendingSwipeTracker::eventWasNotHandledByWebCore(PlatformScrollEvent event)
{
    LOG(ViewGestures, "Swipe Start Hysteresis - WebCore didn't handle event, state %d", (int)m_state);

    if (m_state != State::WaitingForWebCore)
        return;

    m_state = State::None;
    m_cumulativeDelta = FloatSize();
    tryToStartSwipe(event);
}

bool ViewGestureController::PendingSwipeTracker::tryToStartSwipe(PlatformScrollEvent event)
{
    ASSERT(m_state != State::WaitingForWebCore);

    if (m_state == State::None) {
        SwipeDirection direction;
        if (!scrollEventCanBecomeSwipe(event, direction))
            return false;
    }

    if (!scrollEventCanInfluenceSwipe(event))
        return false;

    m_cumulativeDelta += scrollEventGetScrollingDeltas(event);
    LOG(ViewGestures, "Swipe Start Hysteresis - consumed event, cumulative delta (%0.2f, %0.2f)", m_cumulativeDelta.width(), m_cumulativeDelta.height());

    if (deltaShouldCancelSwipe(m_cumulativeDelta)) {
        reset("cumulative delta became too vertical");
        return false;
    }

    if (std::abs(m_cumulativeDelta.width()) >= minimumHorizontalSwipeDistance)
        m_viewGestureController.startSwipeGesture(event, m_direction);
    else
        m_state = State::InsufficientMagnitude;

    return true;
}

void ViewGestureController::PendingSwipeTracker::reset(const char* resetReasonForLogging)
{
    if (m_state != State::None)
        LOG(ViewGestures, "Swipe Start Hysteresis - reset; %s", resetReasonForLogging);

    m_state = State::None;
    m_cumulativeDelta = FloatSize();
}

void ViewGestureController::startSwipeGesture(PlatformScrollEvent event, SwipeDirection direction)
{
    ASSERT(m_activeGestureType == ViewGestureType::None);

    m_pendingSwipeTracker.reset("starting to track swipe");

    m_webPageProxy.recordAutomaticNavigationSnapshot();

    RefPtr<WebBackForwardListItem> targetItem = (direction == SwipeDirection::Back) ? m_webPageProxy.backForwardList().goBackItemSkippingItemsWithoutUserGesture() : m_webPageProxy.backForwardList().goForwardItemSkippingItemsWithoutUserGesture();
    if (!targetItem)
        return;

    trackSwipeGesture(event, direction, targetItem);
}

bool ViewGestureController::isPhysicallySwipingLeft(SwipeDirection direction) const
{
    bool isLTR = m_webPageProxy.userInterfaceLayoutDirection() == WebCore::UserInterfaceLayoutDirection::LTR;
    bool isSwipingForward = direction == SwipeDirection::Forward;
    return isLTR != isSwipingForward;
}

bool ViewGestureController::shouldUseSnapshotForSize(ViewSnapshot& snapshot, FloatSize swipeLayerSize, float topContentInset)
{
    float deviceScaleFactor = m_webPageProxy.deviceScaleFactor();
    if (snapshot.deviceScaleFactor() != deviceScaleFactor)
        return false;

    FloatSize unobscuredSwipeLayerSizeInDeviceCoordinates = swipeLayerSize - FloatSize(0, topContentInset);
    unobscuredSwipeLayerSizeInDeviceCoordinates.scale(deviceScaleFactor);
    if (snapshot.size() != unobscuredSwipeLayerSizeInDeviceCoordinates)
        return false;

    return true;
}

void ViewGestureController::forceRepaintIfNeeded()
{
    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    if (m_hasOutstandingRepaintRequest)
        return;

    m_hasOutstandingRepaintRequest = true;

    auto pageID = m_webPageProxy.identifier();
    GestureID gestureID = m_currentGestureID;
    m_webPageProxy.forceRepaint([pageID, gestureID] () {
        if (auto gestureController = controllerForGesture(pageID, gestureID))
            gestureController->removeSwipeSnapshot();
    });
}

void ViewGestureController::willEndSwipeGesture(WebBackForwardListItem& targetItem, bool cancelled)
{
    m_webPageProxy.navigationGestureWillEnd(!cancelled, targetItem);

    if (cancelled)
        return;

    uint64_t renderTreeSize = 0;
    if (ViewSnapshot* snapshot = targetItem.snapshot())
        renderTreeSize = snapshot->renderTreeSize();
    auto renderTreeSizeThreshold = renderTreeSize * swipeSnapshotRemovalRenderTreeSizeTargetFraction;

    m_didStartProvisionalLoad = false;
    m_pendingNavigation = m_webPageProxy.goToBackForwardItem(targetItem);

    auto* currentItem = m_webPageProxy.backForwardList().currentItem();
    // The main frame will not be navigated so hide the snapshot right away.
    if (currentItem && currentItem->itemIsClone(targetItem)) {
        removeSwipeSnapshot();
        return;
    }

    SnapshotRemovalTracker::Events desiredEvents = SnapshotRemovalTracker::VisuallyNonEmptyLayout
        | SnapshotRemovalTracker::MainFrameLoad
        | SnapshotRemovalTracker::SubresourceLoads
        | SnapshotRemovalTracker::ScrollPositionRestoration
        | SnapshotRemovalTracker::SwipeAnimationEnd;

    if (renderTreeSizeThreshold) {
        desiredEvents |= SnapshotRemovalTracker::RenderTreeSizeThreshold;
        m_snapshotRemovalTracker.setRenderTreeSizeThreshold(renderTreeSizeThreshold);
    }

    m_snapshotRemovalTracker.start(desiredEvents, [this] { this->forceRepaintIfNeeded(); });

    // FIXME: Like on iOS, we should ensure that even if one of the timeouts fires,
    // we never show the old page content, instead showing the snapshot background color.

    if (ViewSnapshot* snapshot = targetItem.snapshot())
        m_backgroundColorForCurrentSnapshot = snapshot->backgroundColor();
}

void ViewGestureController::endSwipeGesture(WebBackForwardListItem* targetItem, bool cancelled)
{
    ASSERT(m_activeGestureType == ViewGestureType::Swipe);
    ASSERT(targetItem);

#if PLATFORM(MAC)
    m_swipeCancellationTracker = nullptr;
#endif

    m_didCallEndSwipeGesture = true;

    if (cancelled) {
        removeSwipeSnapshot();
        m_webPageProxy.navigationGestureDidEnd(false, *targetItem);
        return;
    }

    m_webPageProxy.navigationGestureDidEnd(true, *targetItem);

    m_snapshotRemovalTracker.eventOccurred(SnapshotRemovalTracker::SwipeAnimationEnd, SnapshotRemovalTracker::ShouldIgnoreEventIfPaused::No);

    // removeSwipeSnapshot() was called between willEndSwipeGesture() and endSwipeGesture().
    // We couldn't remove it then, because the animation was still running, but now we can!
    if (m_removeSnapshotImmediatelyWhenGestureEnds) {
        removeSwipeSnapshot();
        return;
    }
}

void ViewGestureController::requestRenderTreeSizeNotificationIfNeeded()
{
    if (!m_snapshotRemovalTracker.hasOutstandingEvent(SnapshotRemovalTracker::RenderTreeSizeThreshold))
        return;

    auto threshold = m_snapshotRemovalTracker.renderTreeSizeThreshold();
    auto* messageSender = m_webPageProxy.provisionalPageProxy() ? static_cast<IPC::MessageSender*>(m_webPageProxy.provisionalPageProxy()) : &m_webPageProxy;
    messageSender->send(Messages::ViewGestureGeometryCollector::SetRenderTreeSizeNotificationThreshold(threshold));
}

FloatPoint ViewGestureController::scaledMagnificationOrigin(FloatPoint origin, double scale)
{
    FloatPoint scaledMagnificationOrigin(m_initialMagnificationOrigin);
    scaledMagnificationOrigin.moveBy(m_visibleContentRect.location());
    float magnificationOriginScale = 1 - (scale / m_initialMagnification);
    scaledMagnificationOrigin.scale(magnificationOriginScale);
    scaledMagnificationOrigin.move(origin - m_initialMagnificationOrigin);
    return scaledMagnificationOrigin;
}

void ViewGestureController::didCollectGeometryForMagnificationGesture(FloatRect visibleContentRect, bool frameHandlesMagnificationGesture)
{
    willBeginGesture(ViewGestureType::Magnification);
    m_visibleContentRect = visibleContentRect;
    m_visibleContentRectIsValid = true;
    m_frameHandlesMagnificationGesture = frameHandlesMagnificationGesture;
}

void ViewGestureController::prepareMagnificationGesture(FloatPoint origin)
{
    m_magnification = m_webPageProxy.pageScaleFactor();
    m_webPageProxy.send(Messages::ViewGestureGeometryCollector::CollectGeometryForMagnificationGesture());

    m_initialMagnification = m_magnification;
    m_initialMagnificationOrigin = FloatPoint(origin);

#if PLATFORM(MAC)
    m_lastMagnificationGestureWasSmartMagnification = false;
#endif
}

void ViewGestureController::applyMagnification()
{
    if (m_activeGestureType != ViewGestureType::Magnification)
        return;

    if (m_frameHandlesMagnificationGesture)
        m_webPageProxy.scalePage(m_magnification, roundedIntPoint(m_magnificationOrigin));
    else
        m_webPageProxy.drawingArea()->adjustTransientZoom(m_magnification, scaledMagnificationOrigin(m_magnificationOrigin, m_magnification));
}

void ViewGestureController::endMagnificationGesture()
{
    if (m_activeGestureType != ViewGestureType::Magnification)
        return;

    double newMagnification = clampTo<double>(m_magnification, minMagnification, maxMagnification);

    if (m_frameHandlesMagnificationGesture)
        m_webPageProxy.scalePage(newMagnification, roundedIntPoint(m_magnificationOrigin));
    else {
        if (auto drawingArea = m_webPageProxy.drawingArea())
            drawingArea->commitTransientZoom(newMagnification, scaledMagnificationOrigin(m_magnificationOrigin, newMagnification));
    }

#if PLATFORM(MAC)
    m_webPageProxy.didEndMagnificationGesture();
#endif

    didEndGesture();
    m_visibleContentRectIsValid = false;
}

double ViewGestureController::magnification() const
{
    if (m_activeGestureType == ViewGestureType::Magnification)
        return m_magnification;

    return m_webPageProxy.pageScaleFactor();
}

#endif // !PLATFORM(IOS_FAMILY)

} // namespace WebKit
