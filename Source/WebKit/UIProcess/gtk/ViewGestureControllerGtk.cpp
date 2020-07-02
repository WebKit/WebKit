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

#include "APINavigation.h"
#include "DrawingAreaProxy.h"
#include "WebBackForwardList.h"
#include <WebCore/GRefPtrGtk.h>

namespace WebKit {
using namespace WebCore;

static const Seconds swipeMinAnimationDuration = 100_ms;
static const Seconds swipeMaxAnimationDuration = 400_ms;
static const double swipeAnimationBaseVelocity = 0.002;

// GTK divides all scroll deltas by 10, compensate for that
static const double gtkScrollDeltaMultiplier = 10;
static const double swipeTouchpadBaseWidth = 400;

// This is derivative of the easing function at t=0
static const double swipeAnimationDurationMultiplier = 3;

static const double swipeCancelArea = 0.5;
static const double swipeCancelVelocityThreshold = 0.4;

static bool isEventStop(GdkEventScroll* event)
{
    return gdk_event_is_scroll_stop_event(reinterpret_cast<GdkEvent*>(event));
}

void ViewGestureController::platformTeardown()
{
    cancelSwipe();
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

    bool isDeviceAllowed = source == GDK_SOURCE_TOUCHPAD || source == GDK_SOURCE_TOUCHSCREEN || m_viewGestureController.m_isSimulatedSwipe;

    return gdk_event_get_scroll_deltas(reinterpret_cast<GdkEvent*>(event), nullptr, nullptr) && isDeviceAllowed;
}

static bool isTouchEvent(GdkEventScroll* event)
{
    GdkDevice* device = gdk_event_get_source_device(reinterpret_cast<GdkEvent*>(event));
    GdkInputSource source = gdk_device_get_source(device);

    return source == GDK_SOURCE_TOUCHSCREEN;
}

FloatSize ViewGestureController::PendingSwipeTracker::scrollEventGetScrollingDeltas(GdkEventScroll* event)
{
    double multiplier = isTouchEvent(event) ? Scrollbar::pixelsPerLineStep() : gtkScrollDeltaMultiplier;
    double xDelta, yDelta;
    gdk_event_get_scroll_deltas(reinterpret_cast<GdkEvent*>(event), &xDelta, &yDelta);

    // GdkEventScroll deltas are inverted compared to NSEvent, so invert them again
    return -FloatSize(xDelta, yDelta) * multiplier;
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
    m_distance = 0;
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

    uint32_t eventTime = gdk_event_get_time(reinterpret_cast<GdkEvent*>(event));
    double eventDeltaX;
    gdk_event_get_scroll_deltas(reinterpret_cast<GdkEvent*>(event), &eventDeltaX, nullptr);

    double deltaX = -eventDeltaX;
    if (isTouchEvent(event)) {
        m_distance = m_webPageProxy.viewSize().width();
        deltaX *= static_cast<double>(Scrollbar::pixelsPerLineStep()) / m_distance;
    } else {
        m_distance = swipeTouchpadBaseWidth;
        deltaX *= gtkScrollDeltaMultiplier / m_distance;
    }

    Seconds time = Seconds::fromMilliseconds(eventTime);
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
    double relativeVelocity = m_velocity * (swipingLeft ? 1 : -1);

    if (abs(m_progress) > swipeCancelArea)
        return (relativeVelocity * m_distance < -swipeCancelVelocityThreshold);

    return (relativeVelocity * m_distance < swipeCancelVelocityThreshold);
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

    double velocity = swipeAnimationBaseVelocity;
    if ((m_endProgress - m_progress) * m_velocity > 0)
        velocity = m_velocity;

    Seconds duration = Seconds::fromMilliseconds(std::abs((m_progress - m_endProgress) / velocity * swipeAnimationDurationMultiplier));
    duration = clampTo<Seconds>(duration, swipeMinAnimationDuration, swipeMaxAnimationDuration);

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

GRefPtr<GtkStyleContext> ViewGestureController::createStyleContext(const char* name)
{
    bool isRTL = m_webPageProxy.userInterfaceLayoutDirection() == WebCore::UserInterfaceLayoutDirection::RTL;
    GtkWidget* widget = m_webPageProxy.viewWidget();

    GRefPtr<GtkWidgetPath> path = adoptGRef(gtk_widget_path_copy(gtk_widget_get_path(widget)));

    int position = gtk_widget_path_append_type(path.get(), GTK_TYPE_WIDGET);
    gtk_widget_path_iter_set_object_name(path.get(), position, name);
    gtk_widget_path_iter_add_class(path.get(), position, isRTL ? "left" : "right");

    GtkStyleContext* context = gtk_style_context_new();
    gtk_style_context_set_path(context, path.get());

    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(m_cssProvider.get()), GTK_STYLE_PROVIDER_PRIORITY_FALLBACK);

    return adoptGRef(context);
}

static RefPtr<cairo_pattern_t> createElementPattern(GtkStyleContext* context, int width, int height)
{
    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height));
    RefPtr<cairo_t> cr = adoptRef(cairo_create(surface.get()));

    gtk_render_background(context, cr.get(), 0, 0, width, height);
    gtk_render_frame(context, cr.get(), 0, 0, width, height);

    return adoptRef(cairo_pattern_create_for_surface(surface.get()));
}

static int elementWidth(GtkStyleContext* context)
{
    int width;
    gtk_style_context_get(context, gtk_style_context_get_state(context), "min-width", &width, nullptr);

    return width;
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
                auto [red, green, blue, alpha] = color.toSRGBALossy();
                m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_rgba(red, green, blue, alpha));
            }
        }
    }

    if (!m_currentSwipeSnapshotPattern) {
        GdkRGBA color;
        auto* context = gtk_widget_get_style_context(m_webPageProxy.viewWidget());
        if (gtk_style_context_lookup_color(context, "theme_base_color", &color))
            m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_rgba(color.red, color.green, color.blue, color.alpha));
    }

    if (!m_currentSwipeSnapshotPattern)
        m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_rgb(1, 1, 1));

    auto size = m_webPageProxy.drawingArea()->size();

    if (!m_cssProvider) {
        m_cssProvider = adoptGRef(gtk_css_provider_new());
        gtk_css_provider_load_from_resource(m_cssProvider.get(), "/org/webkitgtk/resources/css/gtk-theme.css");
    }

    GRefPtr<GtkStyleContext> context = createStyleContext("dimming");
    m_swipeDimmingPattern = createElementPattern(context.get(), size.width(), size.height());

    context = createStyleContext("shadow");
    m_swipeShadowSize = elementWidth(context.get());
    if (m_swipeShadowSize)
        m_swipeShadowPattern = createElementPattern(context.get(), m_swipeShadowSize, size.height());

    context = createStyleContext("border");
    m_swipeBorderSize = elementWidth(context.get());
    if (m_swipeBorderSize)
        m_swipeBorderPattern = createElementPattern(context.get(), m_swipeBorderSize, size.height());

    context = createStyleContext("outline");
    m_swipeOutlineSize = elementWidth(context.get());
    if (m_swipeOutlineSize)
        m_swipeOutlinePattern = createElementPattern(context.get(), m_swipeOutlineSize, size.height());
}

void ViewGestureController::handleSwipeGesture(WebBackForwardListItem*, double, SwipeDirection)
{
    gtk_widget_queue_draw(m_webPageProxy.viewWidget());
}

void ViewGestureController::cancelSwipe()
{
    m_pendingSwipeTracker.reset("cancelling swipe");

    if (m_activeGestureType == ViewGestureType::Swipe) {
        m_swipeProgressTracker.reset();
        removeSwipeSnapshot();
    }
}

void ViewGestureController::draw(cairo_t* cr, cairo_pattern_t* pageGroup)
{
    bool swipingLeft = isPhysicallySwipingLeft(m_swipeProgressTracker.direction());
    bool swipingBack = m_swipeProgressTracker.direction() == SwipeDirection::Back;
    bool isRTL = m_webPageProxy.userInterfaceLayoutDirection() == WebCore::UserInterfaceLayoutDirection::RTL;
    float progress = m_swipeProgressTracker.progress();

    auto size = m_webPageProxy.drawingArea()->size();
    int width = size.width();
    int height = size.height();
    double scale = m_webPageProxy.deviceScaleFactor();

    double swipingLayerOffset = (swipingLeft ? 0 : width) + floor(width * progress * scale) / scale;

    double dimmingProgress = swipingLeft ? 1 - progress : -progress;
    if (isRTL)
        dimmingProgress = 1 - dimmingProgress;

    double remainingSwipeDistance = dimmingProgress * width;

    double shadowOpacity = 1;
    if (remainingSwipeDistance < m_swipeShadowSize)
        shadowOpacity = remainingSwipeDistance / m_swipeShadowSize;

    cairo_save(cr);

    if (isRTL)
        cairo_rectangle(cr, swipingLayerOffset, 0, width - swipingLayerOffset, height);
    else
        cairo_rectangle(cr, 0, 0, swipingLayerOffset, height);
    cairo_set_source(cr, swipingBack ? m_currentSwipeSnapshotPattern.get() : pageGroup);
    cairo_fill_preserve(cr);

    cairo_save(cr);
    cairo_clip(cr);
    cairo_set_source(cr, m_swipeDimmingPattern.get());
    cairo_paint_with_alpha(cr, dimmingProgress);
    cairo_restore(cr);

    cairo_translate(cr, swipingLayerOffset, 0);

    if (progress) {
        if (m_swipeShadowPattern) {
            cairo_save(cr);
            if (!isRTL)
                cairo_translate(cr, -m_swipeShadowSize, 0);

            cairo_rectangle(cr, 0, 0, m_swipeShadowSize, height);
            cairo_clip(cr);
            cairo_set_source(cr, m_swipeShadowPattern.get());
            cairo_paint_with_alpha(cr, shadowOpacity);
            cairo_restore(cr);
        }

        if (m_swipeBorderPattern) {
            cairo_save(cr);
            if (!isRTL)
                cairo_translate(cr, -m_swipeBorderSize, 0);

            cairo_rectangle(cr, 0, 0, m_swipeBorderSize, height);
            cairo_set_source(cr, m_swipeBorderPattern.get());
            cairo_fill(cr);

            cairo_restore(cr);
        }
    }

    if (isRTL) {
        cairo_translate(cr, -width, 0);
        cairo_rectangle(cr, width - swipingLayerOffset, 0, swipingLayerOffset, height);
    } else
        cairo_rectangle(cr, 0, 0, width - swipingLayerOffset, height);
    cairo_set_source(cr, swipingBack ? pageGroup : m_currentSwipeSnapshotPattern.get());
    cairo_fill(cr);

    if (progress && m_swipeOutlinePattern) {
        cairo_save(cr);

        if (isRTL)
            cairo_translate(cr, width - m_swipeOutlineSize, 0);

        cairo_rectangle(cr, 0, 0, m_swipeOutlineSize, height);
        cairo_set_source(cr, m_swipeOutlinePattern.get());
        cairo_fill(cr);

        cairo_restore(cr);
    }

    cairo_restore(cr);
}

void ViewGestureController::removeSwipeSnapshot()
{
    m_snapshotRemovalTracker.reset();

    m_hasOutstandingRepaintRequest = false;

    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    m_currentSwipeSnapshotPattern = nullptr;
    m_swipeDimmingPattern = nullptr;
    m_swipeShadowPattern = nullptr;
    m_swipeBorderPattern = nullptr;
    m_swipeOutlinePattern = nullptr;

    m_currentSwipeSnapshot = nullptr;

    m_webPageProxy.navigationGestureSnapshotWasRemoved();

    m_backgroundColorForCurrentSnapshot = Color();

    m_pendingNavigation = nullptr;

    didEndGesture();

    m_swipeProgressTracker.reset();
}

static GUniquePtr<GdkEvent> createScrollEvent(GtkWidget* widget, double xDelta, double yDelta)
{
    GdkWindow* window = gtk_widget_get_window(widget);

    int x, y;
    gdk_window_get_root_origin(window, &x, &y);

    int width = gdk_window_get_width(window);
    int height = gdk_window_get_height(window);

    GUniquePtr<GdkEvent> event(gdk_event_new(GDK_SCROLL));
    event->scroll.time = GDK_CURRENT_TIME;
    event->scroll.x = width / 2;
    event->scroll.y = height / 2;
    event->scroll.x_root = x + width / 2;
    event->scroll.y_root = y + height / 2;
    event->scroll.direction = GDK_SCROLL_SMOOTH;
    event->scroll.delta_x = xDelta;
    event->scroll.delta_y = yDelta;
    event->scroll.state = 0;
    event->scroll.is_stop = !xDelta && !yDelta;
    event->scroll.window = GDK_WINDOW(g_object_ref(window));
    gdk_event_set_screen(event.get(), gdk_window_get_screen(window));

    GdkDevice* pointer = gdk_seat_get_pointer(gdk_display_get_default_seat(gdk_window_get_display(window)));
    gdk_event_set_device(event.get(), pointer);
    gdk_event_set_source_device(event.get(), pointer);

    return event;
}

bool ViewGestureController::beginSimulatedSwipeInDirectionForTesting(SwipeDirection direction)
{
    if (!canSwipeInDirection(direction))
        return false;

    m_isSimulatedSwipe = true;

    double delta = swipeTouchpadBaseWidth / gtkScrollDeltaMultiplier * 0.75;

    if (isPhysicallySwipingLeft(direction))
        delta = -delta;

    GUniquePtr<GdkEvent> event = createScrollEvent(m_webPageProxy.viewWidget(), delta, 0);
    gtk_widget_event(m_webPageProxy.viewWidget(), event.get());

    return true;
}

bool ViewGestureController::completeSimulatedSwipeInDirectionForTesting(SwipeDirection)
{
    GUniquePtr<GdkEvent> event = createScrollEvent(m_webPageProxy.viewWidget(), 0, 0);
    gtk_widget_event(m_webPageProxy.viewWidget(), event.get());

    m_isSimulatedSwipe = false;

    return true;
}

} // namespace WebKit
