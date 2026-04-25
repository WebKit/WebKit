/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEGestureControllerImpl.h"

#include "WPEGestureDetector.h"
#include <wtf/glib/WTFGType.h>

struct _WPEGestureControllerImplPrivate {
    WPE::GestureDetector detector;
};

static void wpe_gesture_controller_interface_init(WPEGestureControllerInterface*);

WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(
    WPEGestureControllerImpl, wpe_gesture_controller_impl, G_TYPE_OBJECT, GObject,
    G_IMPLEMENT_INTERFACE(WPE_TYPE_GESTURE_CONTROLLER, wpe_gesture_controller_interface_init))

static gboolean wpeHandleEvent(WPEGestureController* controller, WPEEvent* event)
{
    return WPE_GESTURE_CONTROLLER_IMPL(controller)->priv->detector.handleEvent(event);
}

static void wpeCancel(WPEGestureController* controller)
{
    WPE_GESTURE_CONTROLLER_IMPL(controller)->priv->detector.reset();
}

static WPEGesture wpeGetGesture(WPEGestureController* controller)
{
    return WPE_GESTURE_CONTROLLER_IMPL(controller)->priv->detector.gesture();
}

static gboolean wpeGetGesturePosition(WPEGestureController* controller, double* x, double* y)
{
    if (auto position = WPE_GESTURE_CONTROLLER_IMPL(controller)->priv->detector.position()) {
        *x = position->x;
        *y = position->y;
        return TRUE;
    }
    return FALSE;
}

static gboolean wpeGetGestureDelta(WPEGestureController* controller, double* x, double* y)
{
    if (auto delta = WPE_GESTURE_CONTROLLER_IMPL(controller)->priv->detector.delta()) {
        *x = delta->x;
        *y = delta->y;
        return TRUE;
    }
    return FALSE;
}

static gboolean wpeIsDragBegin(WPEGestureController* controller)
{
    return WPE_GESTURE_CONTROLLER_IMPL(controller)->priv->detector.dragBegin();
}

static void wpe_gesture_controller_impl_class_init(WPEGestureControllerImplClass*)
{
}

static void wpe_gesture_controller_interface_init(WPEGestureControllerInterface* interface)
{
    interface->handle_event = wpeHandleEvent;
    interface->cancel = wpeCancel;
    interface->get_gesture = wpeGetGesture;
    interface->get_gesture_position = wpeGetGesturePosition;
    interface->get_gesture_delta = wpeGetGestureDelta;
    interface->is_drag_begin = wpeIsDragBegin;
}

WPEGestureController* wpeGestureControllerImplNew()
{
    return WPE_GESTURE_CONTROLLER(g_object_new(WPE_TYPE_GESTURE_CONTROLLER_IMPL, nullptr));
}
