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

#if HAVE(GTK_GESTURES)

#include "NativeWebMouseEvent.h"
#include "NativeWebWheelEvent.h"
#include "WebPageProxy.h"
#include <WebCore/FloatPoint.h>
#include <WebCore/Scrollbar.h>
#include <gtk/gtk.h>

using namespace WebCore;

namespace WebKit {

GestureController::GestureController(WebPageProxy& page)
    : m_dragGesture(page)
    , m_zoomGesture(page)
{
}

bool GestureController::handleEvent(const GdkEvent* event)
{
    bool wasProcessingGestures = isProcessingGestures();
    m_dragGesture.handleEvent(event);
    m_zoomGesture.handleEvent(event);
    return event->type == GDK_TOUCH_END ? wasProcessingGestures : isProcessingGestures();
}

bool GestureController::isProcessingGestures() const
{
    return m_dragGesture.isActive() || m_zoomGesture.isActive();
}

GestureController::Gesture::Gesture(GtkGesture* gesture, WebPageProxy& page)
    : m_gesture(adoptGRef(gesture))
    , m_page(page)
{
    gtk_event_controller_set_propagation_phase(GTK_EVENT_CONTROLLER(m_gesture.get()), GTK_PHASE_NONE);
}

bool GestureController::Gesture::isActive() const
{
    return gtk_gesture_is_active(m_gesture.get());
}

void GestureController::Gesture::handleEvent(const GdkEvent* event)
{
    gtk_event_controller_handle_event(GTK_EVENT_CONTROLLER(m_gesture.get()), event);
}

void GestureController::DragGesture::handleDrag(const GdkEvent* event, double x, double y)
{
    ASSERT(m_inDrag);
    GUniquePtr<GdkEvent> scrollEvent(gdk_event_new(GDK_SCROLL));
    scrollEvent->scroll.time = event->touch.time;
    scrollEvent->scroll.x = m_start.x();
    scrollEvent->scroll.y = m_start.y();
    scrollEvent->scroll.x_root = event->touch.x_root;
    scrollEvent->scroll.y_root = event->touch.y_root;
    scrollEvent->scroll.direction = GDK_SCROLL_SMOOTH;
    scrollEvent->scroll.delta_x = (m_offset.x() - x) / Scrollbar::pixelsPerLineStep();
    scrollEvent->scroll.delta_y = (m_offset.y() - y) / Scrollbar::pixelsPerLineStep();
    scrollEvent->scroll.state = event->touch.state;
    m_page.handleWheelEvent(NativeWebWheelEvent(scrollEvent.get()));
}

void GestureController::DragGesture::handleTap(const GdkEvent* event)
{
    ASSERT(!m_inDrag);
    GUniquePtr<GdkEvent> pointerEvent(gdk_event_new(GDK_MOTION_NOTIFY));
    pointerEvent->motion.time = event->touch.time;
    pointerEvent->motion.x = event->touch.x;
    pointerEvent->motion.y = event->touch.y;
    pointerEvent->motion.x_root = event->touch.x_root;
    pointerEvent->motion.y_root = event->touch.y_root;
    pointerEvent->motion.state = event->touch.state;
    m_page.handleMouseEvent(NativeWebMouseEvent(pointerEvent.get(), 0));

    pointerEvent.reset(gdk_event_new(GDK_BUTTON_PRESS));
    pointerEvent->button.button = 1;
    pointerEvent->button.time = event->touch.time;
    pointerEvent->button.x = event->touch.x;
    pointerEvent->button.y = event->touch.y;
    pointerEvent->button.x_root = event->touch.x_root;
    pointerEvent->button.y_root = event->touch.y_root;
    m_page.handleMouseEvent(NativeWebMouseEvent(pointerEvent.get(), 1));

    pointerEvent->type = GDK_BUTTON_RELEASE;
    m_page.handleMouseEvent(NativeWebMouseEvent(pointerEvent.get(), 0));
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
    dragGesture->m_longPressTimeout.startOneShot(delay / 1000.0);
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
        dragGesture->handleDrag(gtk_gesture_get_last_event(gesture, sequence), x, y);
    dragGesture->m_offset.set(x, y);
}

void GestureController::DragGesture::end(DragGesture* dragGesture, GdkEventSequence* sequence, GtkGesture* gesture)
{
    dragGesture->m_longPressTimeout.stop();
    if (!dragGesture->m_inDrag) {
        dragGesture->handleTap(gtk_gesture_get_last_event(gesture, sequence));
        gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_DENIED);
    } else if (!gtk_gesture_handles_sequence(gesture, sequence))
        gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_DENIED);
}

void GestureController::DragGesture::longPressFired()
{
    m_inDrag = true;
}

GestureController::DragGesture::DragGesture(WebPageProxy& page)
    : Gesture(gtk_gesture_drag_new(page.viewWidget()), page)
    , m_longPressTimeout(RunLoop::main(), this, &GestureController::DragGesture::longPressFired)
    , m_inDrag(false)
{
    gtk_gesture_single_set_touch_only(GTK_GESTURE_SINGLE(m_gesture.get()), TRUE);
    g_signal_connect_swapped(m_gesture.get(), "drag-begin", G_CALLBACK(begin), this);
    g_signal_connect_swapped(m_gesture.get(), "drag-update", G_CALLBACK(update), this);
    g_signal_connect_swapped(m_gesture.get(), "end", G_CALLBACK(end), this);
}

IntPoint GestureController::ZoomGesture::center() const
{
    double x, y;
    gtk_gesture_get_bounding_box_center(m_gesture.get(), &x, &y);
    return IntPoint(x, y);
}

void GestureController::ZoomGesture::begin(ZoomGesture* zoomGesture, GdkEventSequence*, GtkGesture* gesture)
{
    gtk_gesture_set_state(gesture, GTK_EVENT_SEQUENCE_CLAIMED);

    zoomGesture->m_initialScale = zoomGesture->m_page.pageScaleFactor();
    zoomGesture->m_page.getCenterForZoomGesture(zoomGesture->center(), zoomGesture->m_initialPoint);
}

void GestureController::ZoomGesture::handleZoom()
{
    IntPoint scaledOriginOffset = m_viewPoint;
    scaledOriginOffset.scale(1 / m_scale, 1 / m_scale);

    IntPoint newOrigin = m_initialPoint;
    newOrigin.moveBy(-scaledOriginOffset);
    newOrigin.scale(m_scale, m_scale);

    m_page.scalePage(m_scale, newOrigin);
}

void GestureController::ZoomGesture::scaleChanged(ZoomGesture* zoomGesture, double scale, GtkGesture*)
{
    zoomGesture->m_scale = zoomGesture->m_initialScale * scale;
    zoomGesture->m_viewPoint = zoomGesture->center();
    if (zoomGesture->m_idle.isActive())
        return;

    zoomGesture->m_idle.startOneShot(0);
}

GestureController::ZoomGesture::ZoomGesture(WebPageProxy& page)
    : Gesture(gtk_gesture_zoom_new(page.viewWidget()), page)
    , m_initialScale(0)
    , m_scale(0)
    , m_idle(RunLoop::main(), this, &GestureController::ZoomGesture::handleZoom)
{
    g_signal_connect_swapped(m_gesture.get(), "begin", G_CALLBACK(begin), this);
    g_signal_connect_swapped(m_gesture.get(), "scale-changed", G_CALLBACK(scaleChanged), this);
}

} // namespace WebKit

#endif // HAVE(GTK_GESTURES)
