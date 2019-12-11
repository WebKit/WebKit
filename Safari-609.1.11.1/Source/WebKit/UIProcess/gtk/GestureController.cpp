/*
 * Copyright (C) 2014 Igalia S.L.
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
#include "GestureController.h"

#include <WebCore/Scrollbar.h>
#include <gtk/gtk.h>

namespace WebKit {
using namespace WebCore;

static const double maximumZoom = 3.0;

GestureController::GestureController(GtkWidget* widget, std::unique_ptr<GestureControllerClient>&& client)
    : m_client(WTFMove(client))
    , m_dragGesture(widget, *m_client)
    , m_swipeGesture(widget, *m_client)
    , m_zoomGesture(widget, *m_client)
    , m_longpressGesture(widget, *m_client)
{
}

bool GestureController::handleEvent(GdkEvent* event)
{
    bool wasProcessingGestures = isProcessingGestures();
    bool touchEnd;
    m_dragGesture.handleEvent(event);
    m_swipeGesture.handleEvent(event);
    m_zoomGesture.handleEvent(event);
    m_longpressGesture.handleEvent(event);
    touchEnd = (gdk_event_get_event_type(event) == GDK_TOUCH_END) || (gdk_event_get_event_type(event) == GDK_TOUCH_CANCEL);
    return touchEnd ? wasProcessingGestures : isProcessingGestures();
}

bool GestureController::isProcessingGestures() const
{
    return m_dragGesture.isActive() || m_swipeGesture.isActive() || m_zoomGesture.isActive() || m_longpressGesture.isActive();
}

GestureController::Gesture::Gesture(GtkGesture* gesture, GestureControllerClient& client)
    : m_gesture(adoptGRef(gesture))
    , m_client(client)
{
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(m_gesture.get()), GTK_PHASE_NONE);
}

void GestureController::Gesture::reset()
{
    gtk_event_controller_reset(GTK_EVENT_CONTROLLER(m_gesture.get()));
}

bool GestureController::Gesture::isActive() const
{
    return gtk_gesture_is_active(m_gesture.get());
}

void GestureController::Gesture::handleEvent(GdkEvent* event)
{
    gtk_event_controller_handle_event(GTK_EVENT_CONTROLLER(m_gesture.get()), event);
}

void GestureController::DragGesture::startDrag(GdkEvent* event)
{
    ASSERT(!m_inDrag);
    m_client.startDrag(reinterpret_cast<GdkEventTouch*>(event), m_start);
}

void GestureController::DragGesture::handleDrag(GdkEvent* event, double x, double y)
{
    ASSERT(m_inDrag);
    m_client.drag(reinterpret_cast<GdkEventTouch*>(event), m_start,
        FloatPoint::narrowPrecision((m_offset.x() - x) / Scrollbar::pixelsPerLineStep(), (m_offset.y() - y) / Scrollbar::pixelsPerLineStep()));
}

void GestureController::DragGesture::cancelDrag()
{
    ASSERT(m_inDrag);
    m_client.cancelDrag();
}

void GestureController::DragGesture::handleTap(GdkEvent* event)
{
    ASSERT(!m_inDrag);
    m_client.tap(reinterpret_cast<GdkEventTouch*>(event));
}

void GestureController::DragGesture::begin(DragGesture* dragGesture, double x, double y, GtkGesture* gesture)
{
    GdkEventSequence* sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    gtk_gesture_set_sequence_state(gesture, sequence, GTK_EVENT_SEQUENCE_CLAIMED);
    dragGesture->m_inDrag = false;
    dragGesture->m_start.set(x, y);
    dragGesture->m_offset.set(0, 0);

    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    unsigned delay;
    g_object_get(gtk_widget_get_settings(widget), "gtk-long-press-time", &delay, nullptr);
    dragGesture->m_longPressTimeout.startOneShot(1_ms * delay);
    dragGesture->startDrag(const_cast<GdkEvent*>(gtk_gesture_get_last_event(gesture, sequence)));
}

void GestureController::DragGesture::update(DragGesture* dragGesture, double x, double y, GtkGesture* gesture)
{
    GdkEventSequence* sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    gtk_gesture_set_sequence_state(gesture, sequence, GTK_EVENT_SEQUENCE_CLAIMED);

    GtkWidget* widget = gtk_event_controller_get_widget(GTK_EVENT_CONTROLLER(gesture));
    if (!dragGesture->m_inDrag && gtk_drag_check_threshold(widget, dragGesture->m_start.x(), dragGesture->m_start.y(), dragGesture->m_start.x() + x, dragGesture->m_start.y() + y)) {
        dragGesture->m_inDrag = true;
        dragGesture->m_longPressTimeout.stop();
    }

    if (dragGesture->m_inDrag)
        dragGesture->handleDrag(const_cast<GdkEvent*>(gtk_gesture_get_last_event(gesture, sequence)), x, y);
    dragGesture->m_offset.set(x, y);
}

void GestureController::DragGesture::end(DragGesture* dragGesture, GdkEventSequence* sequence, GtkGesture* gesture)
{
    dragGesture->m_longPressTimeout.stop();
    if (!gtk_gesture_handles_sequence(gesture, sequence)) {
        gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_DENIED);
        return;
    }
    if (!dragGesture->m_inDrag) {
        dragGesture->handleTap(const_cast<GdkEvent*>(gtk_gesture_get_last_event(gesture, sequence)));
        gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_DENIED);
    }
}

void GestureController::DragGesture::cancel(DragGesture* dragGesture, GdkEventSequence* sequence, GtkGesture* gesture)
{
    dragGesture->m_longPressTimeout.stop();
    dragGesture->cancelDrag();
}

void GestureController::DragGesture::longPressFired()
{
    m_inDrag = true;
}

GestureController::DragGesture::DragGesture(GtkWidget* widget, GestureControllerClient& client)
    : Gesture(gtk_gesture_drag_new(widget), client)
    , m_longPressTimeout(RunLoop::main(), this, &GestureController::DragGesture::longPressFired)
{
    gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(m_gesture.get()), TRUE);
    g_signal_connect_swapped(m_gesture.get(), "drag-begin", G_CALLBACK(begin), this);
    g_signal_connect_swapped(m_gesture.get(), "drag-update", G_CALLBACK(update), this);
    g_signal_connect_swapped(m_gesture.get(), "end", G_CALLBACK(end), this);
    g_signal_connect_swapped(m_gesture.get(), "cancel", G_CALLBACK(cancel), this);
}

void GestureController::SwipeGesture::startMomentumScroll(GdkEvent* event, double velocityX, double velocityY)
{
    m_client.swipe(reinterpret_cast<GdkEventTouch*>(event), FloatPoint::narrowPrecision(velocityX, velocityY));
}

void GestureController::SwipeGesture::swipe(SwipeGesture* swipeGesture, double velocityX, double velocityY, GtkGesture* gesture)
{
    GdkEventSequence* sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    if (!gtk_gesture_handles_sequence(gesture, sequence))
        return;

    gtk_gesture_set_sequence_state(gesture, sequence, GTK_EVENT_SEQUENCE_CLAIMED);

    swipeGesture->startMomentumScroll(const_cast<GdkEvent*>(gtk_gesture_get_last_event(gesture, sequence)), velocityX, velocityY);
}

GestureController::SwipeGesture::SwipeGesture(GtkWidget* widget, GestureControllerClient& client)
    : Gesture(gtk_gesture_swipe_new(widget), client)
{
    gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(m_gesture.get()), TRUE);
    g_signal_connect_swapped(m_gesture.get(), "swipe", G_CALLBACK(swipe), this);
}

void GestureController::ZoomGesture::begin(ZoomGesture* zoomGesture, GdkEventSequence*, GtkGesture* gesture)
{
    gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);
    zoomGesture->startZoom();
}

IntPoint GestureController::ZoomGesture::center() const
{
    double x, y;
    gtk_gesture_get_bounding_box_center(m_gesture.get(), &x, &y);
    return IntPoint(x, y);
}

void GestureController::ZoomGesture::startZoom()
{
    m_client.startZoom(center(), m_initialScale, m_initialPoint);
}

void GestureController::ZoomGesture::handleZoom()
{
    FloatPoint scaledZoomCenter(m_initialPoint);
    scaledZoomCenter.scale(m_scale);

    m_client.zoom(m_scale, WebCore::roundedIntPoint(FloatPoint(scaledZoomCenter - m_viewPoint)));
}

void GestureController::ZoomGesture::scaleChanged(ZoomGesture* zoomGesture, double scale, GtkGesture*)
{
    zoomGesture->m_scale = zoomGesture->m_initialScale * scale;
    if (zoomGesture->m_scale < 1.0)
        zoomGesture->m_scale = 1.0;
    if (zoomGesture->m_scale > maximumZoom)
        zoomGesture->m_scale = maximumZoom;

    zoomGesture->m_viewPoint = zoomGesture->center();

    if (zoomGesture->m_idle.isActive())
        return;

    zoomGesture->m_idle.startOneShot(0_s);
}

GestureController::ZoomGesture::ZoomGesture(GtkWidget* widget, GestureControllerClient& client)
    : Gesture(gtk_gesture_zoom_new(widget), client)
    , m_idle(RunLoop::main(), this, &GestureController::ZoomGesture::handleZoom)
{
    g_signal_connect_swapped(m_gesture.get(), "begin", G_CALLBACK(begin), this);
    g_signal_connect_swapped(m_gesture.get(), "scale-changed", G_CALLBACK(scaleChanged), this);
}

void GestureController::LongPressGesture::longPressed(GdkEvent* event)
{
    m_client.longPress(reinterpret_cast<GdkEventTouch*>(event));
}

void GestureController::LongPressGesture::pressed(LongPressGesture* longpressGesture, double x, double y, GtkGesture* gesture)
{
    GdkEventSequence* sequence = gtk_gesture_single_get_current_sequence(GTK_GESTURE_SINGLE(gesture));
    if (!gtk_gesture_handles_sequence(gesture, sequence))
        return;

    gtk_gesture_set_sequence_state(gesture, sequence, GTK_EVENT_SEQUENCE_CLAIMED);

    longpressGesture->longPressed(const_cast<GdkEvent*>(gtk_gesture_get_last_event(gesture, sequence)));
}

GestureController::LongPressGesture::LongPressGesture(GtkWidget* widget, GestureControllerClient& client)
    : Gesture(gtk_gesture_long_press_new(widget), client)
{
    gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(m_gesture.get()), TRUE);
    g_signal_connect_swapped(m_gesture.get(), "pressed", G_CALLBACK(pressed), this);
}

} // namespace WebKit
