/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

#include "DrawingAreaProxy.h"
#include "WebBackForwardList.h"

namespace WebKit {
using namespace WebCore;

static const Seconds swipeMinAnimationDuration = 100_ms;
static const Seconds swipeMaxAnimationDuration = 400_ms;

// This is derivative of the easing function at t=0
static const double swipeAnimationDurationMultiplier = 3;

static const double swipeCancelArea = 0.5;
static const double swipeCancelVelocityThreshold = 0.001;

static const double swipeOverlayShadowOpacity = 0.06;
static const double swipeOverlayDimmingOpacity = 0.12;
static const double swipeOverlayShadowWidth = 81;
static const double swipeOverlayShadowGradientOffsets[] = { 0, 0.03125, 0.0625, 0.0938, 0.125, 0.1875, 0.25, 0.375, 0.4375, 0.5, 0.5625, 0.625, 0.6875, 0.75, 0.875, 1. };
static const double swipeOverlayShadowGradientAlpha[] = { 1, 0.99, 0.98, 0.95, 0.92, 0.82, 0.71, 0.46, 0.35, 0.25, 0.17, 0.11, 0.07, 0.04, 0.01, 0. };

static bool isEventStop(GdkEventScroll* event)
{
#if GTK_CHECK_VERSION(3, 20, 0)
    return event->is_stop;
#else
    return !event->delta_x && !event->delta_y;
#endif
}

void ViewGestureController::platformTeardown()
{
    m_swipeProgressTracker.reset();

    if (m_activeGestureType == ViewGestureType::Swipe)
        removeSwipeSnapshot();
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanStartSwipe(GdkEventScroll*)
{
    return true;
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanEndSwipe(GdkEventScroll* event)
{
    return isEventStop(event);
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanInfluenceSwipe(GdkEventScroll* event)
{
    GdkDevice* device = gdk_event_get_source_device(reinterpret_cast<GdkEvent*>(event));
    GdkInputSource source = gdk_device_get_source(device);

    // FIXME: Should it maybe be allowed on mice/trackpoints as well? The GDK_SCROLL_SMOOTH
    // requirement already filters out most mice, and it works pretty well on a trackpoint
    return event->direction == GDK_SCROLL_SMOOTH && source == GDK_SOURCE_TOUCHPAD;
}

FloatSize ViewGestureController::PendingSwipeTracker::scrollEventGetScrollingDeltas(GdkEventScroll* event)
{
    // GdkEventScroll deltas are inverted compared to NSEvent, so invert them again
    return -FloatSize(event->delta_x, event->delta_y) * Scrollbar::pixelsPerLineStep();
}

bool ViewGestureController::handleScrollWheelEvent(GdkEventScroll* event)
{
    return m_swipeProgressTracker.handleEvent(event) || m_pendingSwipeTracker.handleEvent(event);
}

void ViewGestureController::trackSwipeGesture(PlatformScrollEvent event, SwipeDirection direction, RefPtr<WebBackForwardListItem> targetItem)
{
    m_swipeProgressTracker.startTracking(WTFMove(targetItem), direction);
    m_swipeProgressTracker.handleEvent(event);
}

ViewGestureController::SwipeProgressTracker::SwipeProgressTracker(WebPageProxy& webPageProxy, ViewGestureController& viewGestureController)
    : m_viewGestureController(viewGestureController)
    , m_webPageProxy(webPageProxy)
{
}

void ViewGestureController::SwipeProgressTracker::startTracking(RefPtr<WebBackForwardListItem>&& targetItem, SwipeDirection direction)
{
    if (m_state != State::None)
        return;

    m_targetItem = targetItem;
    m_direction = direction;
    m_state = State::Pending;
}

void ViewGestureController::SwipeProgressTracker::reset()
{
    m_targetItem = nullptr;
    m_state = State::None;

    if (m_tickCallbackID) {
        GtkWidget* widget = m_webPageProxy.viewWidget();
        gtk_widget_remove_tick_callback(widget, m_tickCallbackID);
        m_tickCallbackID = 0;
    }

    m_progress = 0;
    m_startProgress = 0;
    m_endProgress = 0;

    m_startTime = 0_ms;
    m_endTime = 0_ms;
    m_prevTime = 0_ms;
    m_velocity = 0;
    m_cancelled = false;
}

bool ViewGestureController::SwipeProgressTracker::handleEvent(GdkEventScroll* event)
{
    // Don't allow scrolling while the next page is loading
    if (m_state == State::Finishing)
        return true;

    // Stop current animation, if any
    if (m_state == State::Animating) {
        GtkWidget* widget = m_webPageProxy.viewWidget();
        gtk_widget_remove_tick_callback(widget, m_tickCallbackID);
        m_tickCallbackID = 0;

        m_cancelled = false;
        m_state = State::Pending;
    }

    if (m_state == State::Pending) {
        m_viewGestureController.beginSwipeGesture(m_targetItem.get(), m_direction);
        m_state = State::Scrolling;
    }

    if (m_state != State::Scrolling)
        return false;

    if (isEventStop(event)) {
        startAnimation();
        return false;
    }

    double deltaX = -event->delta_x / Scrollbar::pixelsPerLineStep();

    Seconds time = Seconds::fromMilliseconds(event->time);
    if (time != m_prevTime)
        m_velocity = deltaX / (time - m_prevTime).milliseconds();

    m_prevTime = time;
    m_progress += deltaX;

    bool swipingLeft = m_viewGestureController.isPhysicallySwipingLeft(m_direction);
    float maxProgress = swipingLeft ? 1 : 0;
    float minProgress = !swipingLeft ? -1 : 0;
    m_progress = clampTo<float>(m_progress, minProgress, maxProgress);

    m_viewGestureController.handleSwipeGesture(m_targetItem.get(), m_progress, m_direction);

    return true;
}

bool ViewGestureController::SwipeProgressTracker::shouldCancel()
{
    bool swipingLeft = m_viewGestureController.isPhysicallySwipingLeft(m_direction);

    if (swipingLeft && m_velocity < 0)
        return true;

    if (!swipingLeft && m_velocity > 0)
        return true;

    return (abs(m_progress) < swipeCancelArea && abs(m_velocity) < swipeCancelVelocityThreshold);
}

void ViewGestureController::SwipeProgressTracker::startAnimation()
{
    m_cancelled = shouldCancel();

    m_state = State::Animating;
    m_viewGestureController.willEndSwipeGesture(*m_targetItem, m_cancelled);

    m_startProgress = m_progress;
    if (m_cancelled)
        m_endProgress = 0;
    else
        m_endProgress = m_viewGestureController.isPhysicallySwipingLeft(m_direction) ? 1 : -1;

    Seconds duration = swipeMaxAnimationDuration;
    if ((m_endProgress - m_progress) * m_velocity > 0) {
        duration = Seconds::fromMilliseconds(std::abs((m_progress - m_endProgress) / m_velocity * swipeAnimationDurationMultiplier));
        duration = clampTo<WTF::Seconds>(duration, swipeMinAnimationDuration, swipeMaxAnimationDuration);
    }

    GtkWidget* widget = m_webPageProxy.viewWidget();
    m_startTime = Seconds::fromMicroseconds(gdk_frame_clock_get_frame_time(gtk_widget_get_frame_clock(widget)));
    m_endTime = m_startTime + duration;

    m_tickCallbackID = gtk_widget_add_tick_callback(widget, [](GtkWidget*, GdkFrameClock* frameClock, gpointer userData) -> gboolean {
        auto* tracker = static_cast<SwipeProgressTracker*>(userData);
        return tracker->onAnimationTick(frameClock);
    }, this, nullptr);
}

static inline double easeOutCubic(double t)
{
    double p = t - 1;
    return p * p * p + 1;
}

gboolean ViewGestureController::SwipeProgressTracker::onAnimationTick(GdkFrameClock* frameClock)
{
    ASSERT(m_state == State::Animating);
    ASSERT(m_endTime > m_startTime);

    Seconds frameTime = Seconds::fromMicroseconds(gdk_frame_clock_get_frame_time(frameClock));

    double animationProgress = (frameTime - m_startTime) / (m_endTime - m_startTime);
    if (animationProgress > 1)
        animationProgress = 1;

    m_progress = m_startProgress + (m_endProgress - m_startProgress) * easeOutCubic(animationProgress);

    m_viewGestureController.handleSwipeGesture(m_targetItem.get(), m_progress, m_direction);
    if (frameTime >= m_endTime) {
        m_tickCallbackID = 0;
        endAnimation();
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

void ViewGestureController::SwipeProgressTracker::endAnimation()
{
    m_state = State::Finishing;
    m_viewGestureController.endSwipeGesture(m_targetItem.get(), m_cancelled);
}

void ViewGestureController::beginSwipeGesture(WebBackForwardListItem* targetItem, SwipeDirection direction)
{
    ASSERT(targetItem);

    m_webPageProxy.navigationGestureDidBegin();

    willBeginGesture(ViewGestureType::Swipe);

    if (auto* snapshot = targetItem->snapshot()) {
        m_currentSwipeSnapshot = snapshot;

        FloatSize viewSize(m_webPageProxy.viewSize());
        if (snapshot->hasImage() && shouldUseSnapshotForSize(*snapshot, viewSize, 0))
            m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_for_surface(snapshot->surface()));

        Color color = snapshot->backgroundColor();
        if (color.isValid()) {
            m_backgroundColorForCurrentSnapshot = color;
            if (!m_currentSwipeSnapshotPattern) {
                double red, green, blue, alpha;
                color.getRGBA(red, green, blue, alpha);
                m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_rgba(red, green, blue, alpha));
            }
        }
    }

    if (!m_currentSwipeSnapshotPattern)
        m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_rgb(1, 1, 1));
}

void ViewGestureController::handleSwipeGesture(WebBackForwardListItem*, double, SwipeDirection)
{
    gtk_widget_queue_draw(m_webPageProxy.viewWidget());
}

void ViewGestureController::draw(cairo_t* cr, cairo_pattern_t* pageGroup)
{
    bool swipingLeft = isPhysicallySwipingLeft(m_swipeProgressTracker.direction());
    float progress = m_swipeProgressTracker.progress();

    double width = m_webPageProxy.drawingArea()->size().width();
    double height = m_webPageProxy.drawingArea()->size().height();

    double swipingLayerOffset = (swipingLeft ? 0 : width) + floor(width * progress);

    double dimmingProgress = swipingLeft ? 1 - progress : -progress;

    double remainingSwipeDistance = dimmingProgress * width;
    double shadowFadeDistance = swipeOverlayShadowWidth;

    double shadowOpacity = swipeOverlayShadowOpacity;
    if (remainingSwipeDistance < shadowFadeDistance)
        shadowOpacity = (remainingSwipeDistance / shadowFadeDistance) * swipeOverlayShadowOpacity;

    RefPtr<cairo_pattern_t> shadowPattern = adoptRef(cairo_pattern_create_linear(0, 0, -swipeOverlayShadowWidth, 0));
    for (int i = 0; i < 16; i++) {
        double offset = swipeOverlayShadowGradientOffsets[i];
        double alpha = swipeOverlayShadowGradientAlpha[i] * shadowOpacity;
        cairo_pattern_add_color_stop_rgba(shadowPattern.get(), offset, 0, 0, 0, alpha);
    }

    cairo_save(cr);

    cairo_rectangle(cr, 0, 0, swipingLayerOffset, height);
    cairo_set_source(cr, swipingLeft ? m_currentSwipeSnapshotPattern.get() : pageGroup);
    cairo_fill_preserve(cr);

    cairo_set_source_rgba(cr, 0, 0, 0, dimmingProgress * swipeOverlayDimmingOpacity);
    cairo_fill(cr);

    cairo_translate(cr, swipingLayerOffset, 0);

    if (progress) {
        cairo_rectangle(cr, -swipeOverlayShadowWidth, 0, swipeOverlayShadowWidth, height);
        cairo_set_source(cr, shadowPattern.get());
        cairo_fill(cr);
    }

    cairo_rectangle(cr, 0, 0, width - swipingLayerOffset, height);
    cairo_set_source(cr, swipingLeft ? pageGroup : m_currentSwipeSnapshotPattern.get());
    cairo_fill(cr);

    cairo_restore(cr);
}

void ViewGestureController::removeSwipeSnapshot()
{
    m_snapshotRemovalTracker.reset();

    m_hasOutstandingRepaintRequest = false;

    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    m_currentSwipeSnapshotPattern = nullptr;

    m_currentSwipeSnapshot = nullptr;

    m_webPageProxy.navigationGestureSnapshotWasRemoved();

    m_backgroundColorForCurrentSnapshot = Color();

    didEndGesture();

    m_swipeProgressTracker.reset();
}

bool ViewGestureController::beginSimulatedSwipeInDirectionForTesting(SwipeDirection)
{
    return false;
}

bool ViewGestureController::completeSimulatedSwipeInDirectionForTesting(SwipeDirection)
{
    return false;
}

} // namespace WebKit
