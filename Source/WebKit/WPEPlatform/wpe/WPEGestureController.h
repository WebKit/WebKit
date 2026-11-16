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

#ifndef WPEGestureController_h
#define WPEGestureController_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

/**
 * WPEGesture:
 * @WPE_GESTURE_NONE: no gesture.
 * @WPE_GESTURE_TAP: tap gesture that has a corresponding position.
 * @WPE_GESTURE_DRAG: drag gesture that has a corresponding position and delta.
 */
typedef enum {
    WPE_GESTURE_NONE,

    WPE_GESTURE_TAP,
    WPE_GESTURE_DRAG,
} WPEGesture;

typedef struct _WPEEvent WPEEvent;

#define WPE_TYPE_GESTURE_CONTROLLER (wpe_gesture_controller_get_type())
WPE_API G_DECLARE_INTERFACE (WPEGestureController, wpe_gesture_controller, WPE, GESTURE_CONTROLLER, GObject)

struct _WPEGestureControllerInterface
{
    GTypeInterface parent_interface;

    gboolean    (* handle_event)         (WPEGestureController *controller,
                                          WPEEvent             *event );
    void        (* cancel)               (WPEGestureController *controller);
    WPEGesture  (* get_gesture)          (WPEGestureController *controller);
    gboolean    (* get_gesture_position) (WPEGestureController *controller,
                                          double               *x,
                                          double               *y);
    gboolean    (* get_gesture_delta)    (WPEGestureController *controller,
                                          double               *x,
                                          double               *y);
    gboolean    (* is_drag_begin)        (WPEGestureController *controller);
};

WPE_API gboolean    wpe_gesture_controller_handle_event         (WPEGestureController *controller,
                                                                 WPEEvent             *event);
WPE_API void        wpe_gesture_controller_cancel               (WPEGestureController *controller);
WPE_API WPEGesture  wpe_gesture_controller_get_gesture          (WPEGestureController *controller);
WPE_API gboolean    wpe_gesture_controller_get_gesture_position (WPEGestureController *controller,
                                                                 double               *x,
                                                                 double               *y);
WPE_API gboolean    wpe_gesture_controller_get_gesture_delta    (WPEGestureController *controller,
                                                                 double               *x,
                                                                 double               *y);
WPE_API gboolean    wpe_gesture_controller_is_drag_begin        (WPEGestureController *controller);

G_END_DECLS

#endif /* WPEGestureController_h */
