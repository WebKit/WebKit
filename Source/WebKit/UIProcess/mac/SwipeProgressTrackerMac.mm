/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#import "SwipeProgressTrackerMac.h"

#if PLATFORM(MAC)

#import "DisplayLink.h"
#import "DrawingAreaProxy.h"
#import "ViewGestureController.h"
#import "WebBackForwardListItem.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import <WebCore/AnimationFrameRate.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(SwipeProgressTracker);

static constexpr Seconds kineticProjectionDuration = 300_ms;
static constexpr double completionThresholdPoints = 187.5;
static constexpr double maximumCompletionThresholdRatio = 0.5;
static constexpr double minimumAnimationVelocity = 3.0;
static constexpr Seconds minimumAnimationDuration = 100_ms;
static constexpr Seconds maximumAnimationDuration = 400_ms;

static double easeOutCubic(double t)
{
    double p = t - 1;
    return p * p * p + 1;
}

SwipeProgressTracker::SwipeProgressTracker(WebPageProxy& webPageProxy, ViewGestureController& viewGestureController)
    : m_viewGestureController(viewGestureController)
    , m_webPageProxy(webPageProxy)
{
}

SwipeProgressTracker::~SwipeProgressTracker()
{
    stopDisplayLinkObserver();
}

void SwipeProgressTracker::startTracking(RefPtr<WebBackForwardListItem>&& targetItem, ViewGestureController::SwipeDirection direction)
{
    if (m_state != State::None)
        return;

    m_targetItem = WTF::move(targetItem);
    m_direction = direction;
    m_state = State::Pending;
}

void SwipeProgressTracker::reset()
{
    stopDisplayLinkObserver();

    m_targetItem = nullptr;
    m_state = State::None;
    m_direction = ViewGestureController::SwipeDirection::Back;

    m_progress = 0;
    m_averageVelocity = 0;

    m_velocityData.clear();

    m_animationStartProgress = 0;
    m_animationEndProgress = 0;
    m_animationStartTime = MonotonicTime();
    m_animationEndTime = MonotonicTime();

    m_cancelled = false;
}

double SwipeProgressTracker::totalSwipeDistance() const
{
    if (m_viewGestureController->hasCustomSwipeViews())
        return m_viewGestureController->customSwipeViewsWidth();

    RefPtr page = m_webPageProxy.get();
    if (!page || !page->drawingArea())
        return 0;

    return page->drawingArea()->size().width();
}

bool SwipeProgressTracker::handleEvent(PlatformScrollEvent event)
{
    Ref viewGestureController = m_viewGestureController.get();

    switch (m_state) {
    case State::None:
        return false;

    case State::Pending:
        viewGestureController->beginSwipeGesture(m_targetItem.get(), m_direction);
        m_state = State::Swiping;
        break;

    case State::Swiping:
        break;

    case State::Animating:
        stopDisplayLinkObserver();
        m_cancelled = false;
        m_state = State::Swiping;
        break;

    case State::Done:
        return true;
    }

    auto phase = event.phase();
    if (phase == WebWheelEvent::Phase::Ended || phase == WebWheelEvent::Phase::Cancelled) {
        startAnimation(phase == WebWheelEvent::Phase::Cancelled);
        return true;
    }

    double totalDistance = totalSwipeDistance();
    if (totalDistance <= 0)
        return true;

    double progressDelta = event.delta().width() / totalDistance;
    m_progress += progressDelta;

    auto velocityData = m_velocityData.velocityForNewData(WebCore::FloatPoint(m_progress, 0), 1.0, MonotonicTime::now());
    m_averageVelocity = velocityData.horizontalVelocity;

    bool swipingLeft = viewGestureController->isPhysicallySwipingLeft(m_direction);
    double maxProgress = swipingLeft ? 1 : 0;
    double minProgress = !swipingLeft ? -1 : 0;
    m_progress = std::clamp(m_progress, minProgress, maxProgress);

    viewGestureController->handleSwipeGesture(m_targetItem.get(), m_progress, m_direction);
    return true;
}

bool SwipeProgressTracker::shouldCancel()
{
    double totalDistance = totalSwipeDistance();
    if (totalDistance <= 0)
        return true;

    bool swipingLeft = protect(m_viewGestureController)->isPhysicallySwipingLeft(m_direction);
    double effectiveVelocity = m_averageVelocity * (swipingLeft ? 1 : -1);
    double projectedProgress = std::abs(m_progress) + effectiveVelocity * kineticProjectionDuration.seconds();
    double threshold = std::min(completionThresholdPoints / totalDistance, maximumCompletionThresholdRatio);
    return projectedProgress < threshold;
}

void SwipeProgressTracker::startAnimation(bool forceCancelled)
{
    Ref viewGestureController = m_viewGestureController.get();

    m_cancelled = forceCancelled || shouldCancel();

    m_state = State::Animating;
    viewGestureController->willEndSwipeGesture(*protect(m_targetItem), m_cancelled);

    m_animationStartProgress = m_progress;
    if (m_cancelled)
        m_animationEndProgress = 0;
    else
        m_animationEndProgress = viewGestureController->isPhysicallySwipingLeft(m_direction) ? 1 : -1;

    double animationVelocity = minimumAnimationVelocity;
    if ((m_animationEndProgress - m_animationStartProgress) * m_averageVelocity > 0)
        animationVelocity = std::max(std::abs(m_averageVelocity), minimumAnimationVelocity);

    double distance = std::abs(m_animationEndProgress - m_animationStartProgress);
    Seconds duration = std::clamp(Seconds(distance / animationVelocity), minimumAnimationDuration, maximumAnimationDuration);

    m_animationStartTime = MonotonicTime::now();
    m_animationEndTime = m_animationStartTime + duration;

    startDisplayLinkObserver();
}

void SwipeProgressTracker::animationTimerFired()
{
    if (m_state != State::Animating)
        return;
    ASSERT(m_animationEndTime > m_animationStartTime);

    auto now = MonotonicTime::now();
    double animationProgress = std::min((now - m_animationStartTime) / (m_animationEndTime - m_animationStartTime), 1.0);

    m_progress = m_animationStartProgress + (m_animationEndProgress - m_animationStartProgress) * easeOutCubic(animationProgress);

    protect(m_viewGestureController)->handleSwipeGesture(m_targetItem.get(), m_progress, m_direction);

    if (now >= m_animationEndTime)
        endAnimation();
}

void SwipeProgressTracker::endAnimation()
{
    stopDisplayLinkObserver();
    m_state = State::Done;
    protect(m_viewGestureController)->endSwipeGesture(m_targetItem.get(), m_cancelled);
}

void SwipeProgressTracker::displayLinkFired(WebCore::PlatformDisplayID, WebCore::DisplayUpdate, bool, bool)
{
    RefPtr page = m_webPageProxy.get();
    if (!page)
        return;

    RunLoop::mainSingleton().dispatch([pageID = page->identifier()]() {
        RefPtr controller = ViewGestureController::controllerForPage(pageID);
        if (controller && controller->swipeProgressTracker())
            protect(*controller->swipeProgressTracker())->animationTimerFired();
    });
}

void SwipeProgressTracker::startDisplayLinkObserver()
{
    RefPtr page = m_webPageProxy.get();
    if (!page)
        return;

    m_displayLinkObserverID = DisplayLinkObserverID::generate();
    auto displayID = page->displayID().value_or(0);
    page->configuration().processPool().displayLinks().startDisplayLink(*this, *m_displayLinkObserverID, displayID, WebCore::FullSpeedFramesPerSecond);
}

void SwipeProgressTracker::stopDisplayLinkObserver()
{
    if (!m_displayLinkObserverID)
        return;

    RefPtr page = m_webPageProxy.get();
    if (!page) {
        m_displayLinkObserverID = std::nullopt;
        return;
    }

    auto displayID = page->displayID().value_or(0);
    page->configuration().processPool().displayLinks().stopDisplayLink(*this, *m_displayLinkObserverID, displayID);
    m_displayLinkObserverID = std::nullopt;
}

} // namespace WebKit

#endif // PLATFORM(MAC)
