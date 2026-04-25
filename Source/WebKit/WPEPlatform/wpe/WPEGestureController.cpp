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
#include "WPEGestureController.h"

/**
 * WPEGestureController:
 * @See_also: #WPEView
 *
 * A gesture controller.
 *
 * This interface enables implementing custom gesture detection algorithms.
 * Objects of classes implementing this interface can be supplied to
 * #WPEView so that they are being used instead of the default gesture detector.
 */

G_DEFINE_INTERFACE(WPEGestureController, wpe_gesture_controller, G_TYPE_OBJECT)

static void wpe_gesture_controller_default_init(WPEGestureControllerInterface*)
{
}

/**
 * wpe_gesture_controller_handle_event:
 * @controller: a #WPEGestureController
 * @event: a #WPEEvent
 *
 * Get the gesture detected by @controller if any was detected during processing of @event.
 * 
 * Returns: %TRUE if @event was handled by @controller and gesture information is updated,
 *    or %FALSE otherwise.
 */
gboolean wpe_gesture_controller_handle_event(WPEGestureController* controller, WPEEvent* event)
{
    g_return_val_if_fail(controller, FALSE);
    g_return_val_if_fail(event, FALSE);

    auto* controllerInterface = WPE_GESTURE_CONTROLLER_GET_IFACE(controller);
    return controllerInterface->handle_event(controller, event);
}

/**
 * wpe_gesture_controller_cancel:
 * @controller: a #WPEGestureController
 *
 * Cancels ongoing gesture detection if any.
 */
void wpe_gesture_controller_cancel(WPEGestureController* controller)
{
    auto* controllerInterface = WPE_GESTURE_CONTROLLER_GET_IFACE(controller);
    controllerInterface->cancel(controller);
}

/**
 * wpe_gesture_controller_get_gesture:
 * @controller: a #WPEGestureController
 *
 * Get currently detected gesture.
 *
 * Returns: a #WPEGesture
 */
WPEGesture wpe_gesture_controller_get_gesture(WPEGestureController* controller)
{
    g_return_val_if_fail(controller, WPE_GESTURE_NONE);

    auto* controllerInterface = WPE_GESTURE_CONTROLLER_GET_IFACE(controller);
    return controllerInterface->get_gesture(controller);
}

/**
 * wpe_gesture_controller_get_gesture_position:
 * @controller: a #WPEGestureController
 * @x: (out): location to store x coordinate
 * @y: (out): location to store y coordinate
 *
 * Get the position of currently detected gesture. If it doesn't have
 * a position, %FALSE is returned.
 *
 * Returns: %TRUE if position is returned in @x and @y,
 *    or %FALSE if currently detected gesture doesn't have a positon
 */
gboolean wpe_gesture_controller_get_gesture_position(WPEGestureController* controller, double* x, double* y)
{
    g_return_val_if_fail(controller, FALSE);
    g_return_val_if_fail(x && y, FALSE);

    auto* controllerInterface = WPE_GESTURE_CONTROLLER_GET_IFACE(controller);
    return controllerInterface->get_gesture_position(controller, x, y);
}

/**
 * wpe_gesture_controller_get_gesture_delta:
 * @controller: a #WPEGestureController
 * @x: (out): location to store delta on x axis
 * @y: (out): location to store delta on y axis
 *
 * Get the delta of currently detected gesture such as "drag" gesture.
 * If it doesn't have a delta, %FALSE is returned.
 *
 * Returns: %TRUE if delta is returned in @x and @y,
 *    or %FALSE if currently detected gesture doesn't have a delta
 */
gboolean wpe_gesture_controller_get_gesture_delta(WPEGestureController* controller, double* x, double* y)
{
    g_return_val_if_fail(controller, FALSE);
    g_return_val_if_fail(x && y, FALSE);

    auto* controllerInterface = WPE_GESTURE_CONTROLLER_GET_IFACE(controller);
    return controllerInterface->get_gesture_delta(controller, x, y);
}

/**
 * wpe_gesture_controller_is_drag_begin:
 * @controller: a #WPEGestureController
 *
 * Check if the current drag gesture is a beginning of the sequence being detected.
 *
 * Returns: %TRUE if current drag gesture is a beginning of the sequence being detected,
 *    or %FALSE otherwise
 */
gboolean wpe_gesture_controller_is_drag_begin(WPEGestureController* controller)
{
    g_return_val_if_fail(controller, FALSE);

    auto* controllerInterface = WPE_GESTURE_CONTROLLER_GET_IFACE(controller);
    return controllerInterface->is_drag_begin(controller);
}
