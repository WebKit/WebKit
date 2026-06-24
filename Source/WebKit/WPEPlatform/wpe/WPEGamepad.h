/*
 * Copyright (C) 2025 Igalia S.L.
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

#ifndef WPEGamepad_h
#define WPEGamepad_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

#define WPE_TYPE_GAMEPAD (wpe_gamepad_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEGamepad, wpe_gamepad, WPE, GAMEPAD, GObject)

struct _WPEGamepadClass
{
    GObjectClass parent_class;

    void (* start_input_monitor) (WPEGamepad *gamepad);
    void (* stop_input_monitor)  (WPEGamepad *gamepad);
    gboolean (* has_rumble)      (WPEGamepad *gamepad);
    gboolean (* rumble)          (WPEGamepad *gamepad,
                                  gdouble     strong_magnitude,
                                  gdouble     weak_magnitude,
                                  guint       duration_ms);

    gpointer padding[32];
};

/**
 * WPEGamepadButton:
 * @WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_BOTTOM: bottom button in the right cluster.
 * @WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_RIGHT: right button in the right cluster.
 * @WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_LEFT: left button in the right cluster.
 * @WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_TOP: top button in the right cluster.
 * @WPE_GAMEPAD_BUTTON_LEFT_SHOULDER_FRONT: top left front button.
 * @WPE_GAMEPAD_BUTTON_RIGHT_SHOULDER_FRONT: top right front button.
 * @WPE_GAMEPAD_BUTTON_LEFT_SHOULDER_BACK: bottom left front button.
 * @WPE_GAMEPAD_BUTTON_RIGHT_SHOULDER_BACK: bottom right front button.
 * @WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_LEFT: left button in the center cluster.
 * @WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_RIGHT: right button in the center cluster.
 * @WPE_GAMEPAD_BUTTON_LEFT_THUMB: left stick pressed button.
 * @WPE_GAMEPAD_BUTTON_RIGHT_THUMB: right stick pressed button.
 * @WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_TOP: top button in the left cluster.
 * @WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_BOTTOM: bottom button in the left cluster.
 * @WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_LEFT: left button in the left cluster.
 * @WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_RIGHT: right button in the left cluster.
 * @WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_CENTER: center button in the center cluster.
 *
 * Available buttons in a [class@Gamepad], following the W3C standard gamepad
 * mapping.
 */
typedef enum {
    WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_BOTTOM,
    WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_RIGHT,
    WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_LEFT,
    WPE_GAMEPAD_BUTTON_RIGHT_CLUSTER_TOP,
    WPE_GAMEPAD_BUTTON_LEFT_SHOULDER_FRONT,
    WPE_GAMEPAD_BUTTON_RIGHT_SHOULDER_FRONT,
    WPE_GAMEPAD_BUTTON_LEFT_SHOULDER_BACK,
    WPE_GAMEPAD_BUTTON_RIGHT_SHOULDER_BACK,
    WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_LEFT,
    WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_RIGHT,
    WPE_GAMEPAD_BUTTON_LEFT_THUMB,
    WPE_GAMEPAD_BUTTON_RIGHT_THUMB,
    WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_TOP,
    WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_BOTTOM,
    WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_LEFT,
    WPE_GAMEPAD_BUTTON_LEFT_CLUSTER_RIGHT,
    WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_CENTER
} WPEGamepadButton;

/**
 * WPEGamepadAxis:
 * @WPE_GAMEPAD_AXIS_LEFT_X: horizontal axis for the left stick (negative left/positive right).
 * @WPE_GAMEPAD_AXIS_LEFT_Y: vertical axis for the left stick (negative up/positive down).
 * @WPE_GAMEPAD_AXIS_RIGHT_X: horizontal axis for the right stick (negative left/positive right).
 * @WPE_GAMEPAD_AXIS_RIGHT_Y: vertical axis for the right stick (negative up/positive down).
 *
 * Available axes in a [class@Gamepad], following the W3C standard gamepad
 * mapping.
 */
typedef enum {
    WPE_GAMEPAD_AXIS_LEFT_X,
    WPE_GAMEPAD_AXIS_LEFT_Y,
    WPE_GAMEPAD_AXIS_RIGHT_X,
    WPE_GAMEPAD_AXIS_RIGHT_Y
} WPEGamepadAxis;

WPE_API const char *wpe_gamepad_get_name            (WPEGamepad      *gamepad);
WPE_API void        wpe_gamepad_start_input_monitor (WPEGamepad      *gamepad);
WPE_API void        wpe_gamepad_stop_input_monitor  (WPEGamepad      *gamepad);
WPE_API void        wpe_gamepad_button_event        (WPEGamepad      *gamepad,
                                                     WPEGamepadButton button,
                                                     gboolean         is_pressed);
WPE_API void        wpe_gamepad_axis_event          (WPEGamepad      *gamepad,
                                                     WPEGamepadAxis   axis,
                                                     gdouble          value);
WPE_API gboolean    wpe_gamepad_has_rumble          (WPEGamepad      *gamepad);
WPE_API gboolean    wpe_gamepad_rumble              (WPEGamepad      *gamepad,
                                                     gdouble          strong_magnitude,
                                                     gdouble          weak_magnitude,
                                                     guint            duration_ms);

G_END_DECLS

#endif /* WPEGamepad_h */
