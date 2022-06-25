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

#if USE(GTK4)
static const float swipeOverlayShadowOpacity = 0.06;
static const float swipeOverlayBorderOpacity = 0.05;
static const float swipeOverlayOutlineOpacity = 0.05;
static const float swipeOverlayDimmingOpacity = 0.12;
static const float swipeOverlayShadowWidth = 57;
static const GskColorStop swipeOverlayShadowStops[] = {
    { 0,     { 0, 0, 0, 0.08  } },
    { 0.125, { 0, 0, 0, 0.053 } },
    { 0.428, { 0, 0, 0, 0.026 } },
    { 0.714, { 0, 0, 0, 0.01  } },
    { 1,     { 0, 0, 0, 0     } }
};
#endif

static bool isEventStop(PlatformGtkScrollData* event)
{
    return event->isEnd;
}

void ViewGestureController::platformTeardown()
{
    cancelSwipe();
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanStartSwipe(PlatformGtkScrollData*)
{
    return true;
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanEndSwipe(PlatformGtkScrollData* event)
{
    return isEventStop(event);
}

bool ViewGestureController::PendingSwipeTracker::scrollEventCanInfluenceSwipe(PlatformGtkScrollData* event)
{
    return event->source == GDK_SOURCE_TOUCHPAD || event->source == GDK_SOURCE_TOUCHSCREEN;
}

static bool isTouchEvent(PlatformGtkScrollData* event)
{
    return event->source == GDK_SOURCE_TOUCHSCREEN;
}

FloatSize ViewGestureController::PendingSwipeTracker::scrollEventGetScrollingDeltas(PlatformGtkScrollData* event)
{
    double multiplier = isTouchEvent(event) ? Scrollbar::pixelsPerLineStep() : gtkScrollDeltaMultiplier;
    // GTK deltas are inverted compared to NSEvent, so invert them again
    return -event->delta * multiplier;
}

bool ViewGestureController::handleScrollWheelEvent(PlatformGtkScrollData* event)
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

bool ViewGestureController::SwipeProgressTracker::handleEvent(PlatformGtkScrollData* event)
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
        return true;
    }

    uint32_t eventTime = event->eventTime;

    double deltaX = -event->delta.width();
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

#if !USE(GTK4)
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

static RefPtr<cairo_pattern_t> createElementPattern(GtkStyleContext* context, int width, int height, int scale)
{
    RefPtr<cairo_surface_t> surface = adoptRef(cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width * scale, height * scale));
    RefPtr<cairo_t> cr = adoptRef(cairo_create(surface.get()));

    cairo_surface_set_device_scale(surface.get(), scale, scale);

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

#endif

void ViewGestureController::beginSwipeGesture(WebBackForwardListItem* targetItem, SwipeDirection direction)
{
    ASSERT(targetItem);

    m_webPageProxy.navigationGestureDidBegin();

    willBeginGesture(ViewGestureType::Swipe);

    FloatSize viewSize(m_webPageProxy.viewSize());

#if USE(GTK4)
    graphene_rect_t bounds = { 0, 0, viewSize.width(), viewSize.height() };
#endif

    if (auto* snapshot = targetItem->snapshot()) {
        m_currentSwipeSnapshot = snapshot;

        if (snapshot->hasImage() && shouldUseSnapshotForSize(*snapshot, viewSize, 0))
#if USE(GTK4)
            m_currentSwipeSnapshotPattern = gsk_texture_node_new(snapshot->texture(), &bounds);
#else
            m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_for_surface(snapshot->surface()));
#endif

        Color color = snapshot->backgroundColor();
        if (color.isValid()) {
            m_backgroundColorForCurrentSnapshot = color;
            if (!m_currentSwipeSnapshotPattern) {
                auto [red, green, blue, alpha] = color.toColorTypeLossy<SRGBA<float>>().resolved();
#if USE(GTK4)
                GdkRGBA rgba = { red, green, blue, alpha };
                m_currentSwipeSnapshotPattern = adoptGRef(gsk_color_node_new(&rgba, &bounds));
#else
                m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_rgba(red, green, blue, alpha));
#endif
            }
        }
    }

    if (!m_currentSwipeSnapshotPattern) {
        GdkRGBA color;
        auto* context = gtk_widget_get_style_context(m_webPageProxy.viewWidget());
        if (gtk_style_context_lookup_color(context, "theme_base_color", &color))
#if USE(GTK4)
            m_currentSwipeSnapshotPattern = adoptGRef(gsk_color_node_new(&color, &bounds));
#else
            m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_rgba(color.red, color.green, color.blue, color.alpha));
#endif
    }

    if (!m_currentSwipeSnapshotPattern) {
#if USE(GTK4)
        GdkRGBA color = { 1, 1, 1, 1 };
        m_currentSwipeSnapshotPattern = adoptGRef(gsk_color_node_new(&color, &bounds));
#else
        m_currentSwipeSnapshotPattern = adoptRef(cairo_pattern_create_rgb(1, 1, 1));
#endif
    }

#if !USE(GTK4)
    auto size = m_webPageProxy.drawingArea()->size();

    if (!m_cssProvider) {
        m_cssProvider = adoptGRef(gtk_css_provider_new());
        gtk_css_provider_load_from_resource(m_cssProvider.get(), "/org/webkitgtk/resources/css/gtk-theme.css");
    }

    int scale = gtk_widget_get_scale_factor(m_webPageProxy.viewWidget());

    GRefPtr<GtkStyleContext> context = createStyleContext("dimming");
    m_swipeDimmingPattern = createElementPattern(context.get(), size.width(), size.height(), scale);

    context = createStyleContext("shadow");
    m_swipeShadowSize = elementWidth(context.get());
    if (m_swipeShadowSize)
        m_swipeShadowPattern = createElementPattern(context.get(), m_swipeShadowSize, size.height(), scale);

    context = createStyleContext("border");
    m_swipeBorderSize = elementWidth(context.get());
    if (m_swipeBorderSize)
        m_swipeBorderPattern = createElementPattern(context.get(), m_swipeBorderSize, size.height(), scale);

    context = createStyleContext("outline");
    m_swipeOutlineSize = elementWidth(context.get());
    if (m_swipeOutlineSize)
        m_swipeOutlinePattern = createElementPattern(context.get(), m_swipeOutlineSize, size.height(), scale);
#endif
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

#if USE(GTK4)
void ViewGestureController::snapshot(GtkSnapshot* snapshot, GskRenderNode* pageRenderNode)
{
    bool swipingLeft = isPhysicallySwipingLeft(m_swipeProgressTracker.direction());
    bool swipingBack = m_swipeProgressTracker.direction() == SwipeDirection::Back;
    bool isRTL = m_webPageProxy.userInterfaceLayoutDirection() == WebCore::UserInterfaceLayoutDirection::RTL;
    float progress = m_swipeProgressTracker.progress();

    auto size = m_webPageProxy.drawingArea()->size();
    int width = size.width();
    int height = size.height();
    double scale = m_webPageProxy.deviceScaleFactor();

    float swipingLayerOffset = (swipingLeft ? 0 : width) + floor(width * progress * scale) / scale;

    float dimmingProgress = swipingLeft ? 1 - progress : -progress;
    if (isRTL) {
        dimmingProgress = 1 - dimmingProgress;
        swipingLayerOffset = -(width - swipingLayerOffset);
    }

    float remainingSwipeDistance = dimmingProgress * width;

    float shadowOpacity = 1;
    if (remainingSwipeDistance < swipeOverlayShadowWidth)
        shadowOpacity = remainingSwipeDistance / swipeOverlayShadowWidth;

    gtk_snapshot_save(snapshot);

    graphene_rect_t clip = { 0, 0, static_cast<float>(size.width()), static_cast<float>(size.height()) };
    gtk_snapshot_push_clip(snapshot, &clip);

    graphene_point_t translation = { swipingLayerOffset, 0 };
    if (!swipingBack) {
        gtk_snapshot_append_node(snapshot, pageRenderNode);
        gtk_snapshot_translate(snapshot, &translation);
    }

    gtk_snapshot_append_node(snapshot, m_currentSwipeSnapshotPattern.get());

    if (swipingBack) {
        gtk_snapshot_translate(snapshot, &translation);
        gtk_snapshot_append_node(snapshot, pageRenderNode);
    }

    if (!progress) {
        gtk_snapshot_pop(snapshot);
        gtk_snapshot_restore(snapshot);
        return;
    }

    if (isRTL) {
        translation = { static_cast<float>(width), 0 };
        gtk_snapshot_translate(snapshot, &translation);
    } else
        gtk_snapshot_scale(snapshot, -1, 1);

    GdkRGBA dimming = { 0, 0, 0, swipeOverlayDimmingOpacity * dimmingProgress };
    GdkRGBA border = { 0, 0, 0, swipeOverlayBorderOpacity };
    GdkRGBA outline = { 1, 1, 1, swipeOverlayOutlineOpacity };

    graphene_rect_t dimmingRect = { 0, 0,  static_cast<float>(width), static_cast<float>(height) };
    graphene_rect_t borderRect = { 0, 0, 1, static_cast<float>(height) };
    graphene_rect_t outlineRect = { -1, 0, 1, static_cast<float>(height) };
    graphene_rect_t shadowRect = { 0, 0, swipeOverlayShadowWidth, static_cast<float>(height) };
    graphene_point_t shadowStart = { 0, 0 };
    graphene_point_t shadowEnd = { swipeOverlayShadowWidth, 0 };

    gtk_snapshot_append_color(snapshot, &dimming, &dimmingRect);
    gtk_snapshot_append_color(snapshot, &border, &borderRect);
    gtk_snapshot_append_color(snapshot, &outline, &outlineRect);
    gtk_snapshot_push_opacity(snapshot, shadowOpacity);
    gtk_snapshot_append_linear_gradient(snapshot, &shadowRect, &shadowStart, &shadowEnd,
        swipeOverlayShadowStops, G_N_ELEMENTS(swipeOverlayShadowStops));
    gtk_snapshot_pop(snapshot);

    gtk_snapshot_pop(snapshot);
    gtk_snapshot_restore(snapshot);
}
#else
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
#endif

void ViewGestureController::removeSwipeSnapshot()
{
    m_snapshotRemovalTracker.reset();

    m_hasOutstandingRepaintRequest = false;

    if (m_activeGestureType != ViewGestureType::Swipe)
        return;

    m_currentSwipeSnapshotPattern = nullptr;
#if !USE(GTK4)
    m_swipeDimmingPattern = nullptr;
    m_swipeShadowPattern = nullptr;
    m_swipeBorderPattern = nullptr;
    m_swipeOutlinePattern = nullptr;
#endif

    m_currentSwipeSnapshot = nullptr;

    m_webPageProxy.navigationGestureSnapshotWasRemoved();

    m_backgroundColorForCurrentSnapshot = Color();

    m_pendingNavigation = nullptr;

    didEndGesture();

    m_swipeProgressTracker.reset();
}

bool ViewGestureController::beginSimulatedSwipeInDirectionForTesting(SwipeDirection direction)
{
    if (!canSwipeInDirection(direction))
        return false;

    double deltaX = swipeTouchpadBaseWidth / gtkScrollDeltaMultiplier * 0.75;

    if (isPhysicallySwipingLeft(direction))
        deltaX = -deltaX;

    FloatSize delta(deltaX, 0);
    PlatformGtkScrollData scrollData = { .delta = delta, .eventTime = GDK_CURRENT_TIME, .source = GDK_SOURCE_TOUCHPAD, .isEnd = false };
    handleScrollWheelEvent(&scrollData);

    return true;
}

bool ViewGestureController::completeSimulatedSwipeInDirectionForTesting(SwipeDirection)
{
    PlatformGtkScrollData scrollData = { .delta = FloatSize(), .eventTime = GDK_CURRENT_TIME, .source = GDK_SOURCE_TOUCHPAD, .isEnd = true };
    handleScrollWheelEvent(&scrollData);

    return true;
}

void ViewGestureController::setMagnification(double scale, FloatPoint origin)
{
    ASSERT(m_activeGestureType == ViewGestureType::None || m_activeGestureType == ViewGestureType::Magnification);

    if (m_activeGestureType == ViewGestureType::None) {
        prepareMagnificationGesture(origin);

        return;
    }

    // We're still waiting for the DidCollectGeometry callback.
    if (!m_visibleContentRectIsValid)
        return;

    willBeginGesture(ViewGestureType::Magnification);

    double absoluteScale = scale * m_initialMagnification;
    m_magnification = clampTo<double>(absoluteScale, minMagnification, maxMagnification);

    m_magnificationOrigin = origin;

    applyMagnification();
}

void ViewGestureController::endMagnification()
{
    endMagnificationGesture();
}

} // namespace WebKit
