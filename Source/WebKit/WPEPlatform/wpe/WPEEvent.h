/*
 * Copyright (C) 2023 Igalia S.L.
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

#ifndef WPEEvent_h
#define WPEEvent_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>
#include <wpe/WPEView.h>

G_BEGIN_DECLS

#define WPE_TYPE_EVENT (wpe_event_get_type())

typedef struct _WPEEvent WPEEvent;

/**
 * WPEEventType:
 * @WPE_EVENT_NONE:
 * @WPE_EVENT_POINTER_DOWN:
 * @WPE_EVENT_POINTER_UP:
 * @WPE_EVENT_POINTER_MOVE:
 * @WPE_EVENT_POINTER_ENTER:
 * @WPE_EVENT_POINTER_LEAVE:
 * @WPE_EVENT_SCROLL:
 * @WPE_EVENT_KEYBOARD_KEY_DOWN:
 * @WPE_EVENT_KEYBOARD_KEY_UP:
 * @WPE_EVENT_TOUCH_DOWN:
 * @WPE_EVENT_TOUCH_UP:
 * @WPE_EVENT_TOUCH_MOVE:
 * @WPE_EVENT_TOUCH_CANCEL:
 *
 * The type of a #WPEEvent
 */
typedef enum {
    WPE_EVENT_NONE,

    WPE_EVENT_POINTER_DOWN,
    WPE_EVENT_POINTER_UP,
    WPE_EVENT_POINTER_MOVE,
    WPE_EVENT_POINTER_ENTER,
    WPE_EVENT_POINTER_LEAVE,

    WPE_EVENT_SCROLL,

    WPE_EVENT_KEYBOARD_KEY_DOWN,
    WPE_EVENT_KEYBOARD_KEY_UP,

    WPE_EVENT_TOUCH_DOWN,
    WPE_EVENT_TOUCH_UP,
    WPE_EVENT_TOUCH_MOVE,
    WPE_EVENT_TOUCH_CANCEL
} WPEEventType;

/**
 * WPEInputSource:
 * @WPE_INPUT_SOURCE_MOUSE:
 * @WPE_INPUT_SOURCE_PEN:
 * @WPE_INPUT_SOURCE_KEYBOARD:
 * @WPE_INPUT_SOURCE_TOUCHSCREEN:
 * @WPE_INPUT_SOURCE_TOUCHPAD:
 * @WPE_INPUT_SOURCE_TRACKPOINT:
 *
 * The type of a device input source.
 */
typedef enum {
    WPE_INPUT_SOURCE_MOUSE,
    WPE_INPUT_SOURCE_PEN,
    WPE_INPUT_SOURCE_KEYBOARD,
    WPE_INPUT_SOURCE_TOUCHSCREEN,
    WPE_INPUT_SOURCE_TOUCHPAD,
    WPE_INPUT_SOURCE_TRACKPOINT,
    WPE_INPUT_SOURCE_TABLET_PAD
} WPEInputSource;

/**
 * WPEModifiers:
 * @WPE_MODIFIER_KEYBOARD_CONTROL: the Control key.
 * @WPE_MODIFIER_KEYBOARD_SHIFT: the Shift key.
 * @WPE_MODIFIER_KEYBOARD_ALT: the Alt key.
 * @WPE_MODIFIER_KEYBOARD_META: the Meta modifier.
 * @WPE_MODIFIER_KEYBOARD_CAPS_LOCK: the CapsLock key.
 * @WPE_MODIFIER_POINTER_BUTTON1: the first pointer button.
 * @WPE_MODIFIER_POINTER_BUTTON2: the second pointer button.
 * @WPE_MODIFIER_POINTER_BUTTON3: the third pointer button.
 * @WPE_MODIFIER_POINTER_BUTTON4: the fourth pointer button.
 * @WPE_MODIFIER_POINTER_BUTTON5: the fifth pointer button.
 *
 * Flags to indicate the state of modifier keys and pointer buttons.
 */
typedef enum {
    WPE_MODIFIER_KEYBOARD_CONTROL = 1 << 0,
    WPE_MODIFIER_KEYBOARD_SHIFT = 1 << 1,
    WPE_MODIFIER_KEYBOARD_ALT = 1 << 2,
    WPE_MODIFIER_KEYBOARD_META = 1 << 3,
    WPE_MODIFIER_KEYBOARD_CAPS_LOCK = 1 << 4,

    WPE_MODIFIER_POINTER_BUTTON1 = 1 << 8,
    WPE_MODIFIER_POINTER_BUTTON2 = 1 << 9,
    WPE_MODIFIER_POINTER_BUTTON3 = 1 << 10,
    WPE_MODIFIER_POINTER_BUTTON4 = 1 << 11,
    WPE_MODIFIER_POINTER_BUTTON5 = 1 << 12
} WPEModifiers;

/**
 * WPE_BUTTON_PRIMARY:
 *
 * The primary button. This is typically the left mouse button, or the
 * right button in a left-handed setup.
 */
#define WPE_BUTTON_PRIMARY (1)

/**
 * WPE_BUTTON_MIDDLE:
 *
 * The middle button.
 */
#define WPE_BUTTON_MIDDLE (2)

/**
 * WPE_BUTTON_SECONDARY:
 *
 * The secondary button. This is typically the right mouse button, or the
 * left button in a left-handed setup.
 */
#define WPE_BUTTON_SECONDARY (3)


WPE_API GType          wpe_event_get_type                       (void);
WPE_API WPEEvent      *wpe_event_ref                            (WPEEvent      *event);
WPE_API void           wpe_event_unref                          (WPEEvent      *event);
WPE_API WPEEventType   wpe_event_get_event_type                 (WPEEvent      *event);
WPE_API WPEView       *wpe_event_get_view                       (WPEEvent      *event);
WPE_API WPEInputSource wpe_event_get_input_source               (WPEEvent      *event);
WPE_API guint32        wpe_event_get_time                       (WPEEvent      *event);
WPE_API WPEModifiers   wpe_event_get_modifiers                  (WPEEvent      *event);
WPE_API gboolean       wpe_event_get_position                   (WPEEvent      *event,
                                                                 double        *x,
                                                                 double        *y);

WPE_API WPEEvent      *wpe_event_pointer_button_new             (WPEEventType   type,
                                                                 WPEView       *view,
                                                                 WPEInputSource source,
                                                                 guint32        time,
                                                                 WPEModifiers   modifiers,
                                                                 guint          button,
                                                                 double         x,
                                                                 double         y,
                                                                 guint          press_count);
WPE_API guint          wpe_event_pointer_button_get_button      (WPEEvent      *event);
WPE_API guint          wpe_event_pointer_button_get_press_count (WPEEvent      *event);

WPE_API WPEEvent      *wpe_event_pointer_move_new               (WPEEventType   type,
                                                                 WPEView       *view,
                                                                 WPEInputSource source,
                                                                 guint32        time,
                                                                 WPEModifiers   modifiers,
                                                                 double         x,
                                                                 double         y,
                                                                 double         delta_x,
                                                                 double         delta_y);
WPE_API void           wpe_event_pointer_move_get_delta         (WPEEvent      *event,
                                                                 double        *delta_x,
                                                                 double        *delta_y);

WPE_API WPEEvent      *wpe_event_scroll_new                     (WPEView       *view,
                                                                 WPEInputSource source,
                                                                 guint32        time,
                                                                 WPEModifiers   modifiers,
                                                                 double         delta_x,
                                                                 double         delta_y,
                                                                 gboolean       precise_deltas,
                                                                 gboolean       is_stop,
                                                                 double         x,
                                                                 double         y);
WPE_API void           wpe_event_scroll_get_deltas              (WPEEvent      *event,
                                                                 double        *delta_x,
                                                                 double        *delta_y);
WPE_API gboolean       wpe_event_scroll_has_precise_deltas      (WPEEvent      *event);
WPE_API gboolean       wpe_event_scroll_is_stop                 (WPEEvent      *event);

WPE_API WPEEvent      *wpe_event_keyboard_new                   (WPEEventType   type,
                                                                 WPEView       *view,
                                                                 WPEInputSource source,
                                                                 guint32        time,
                                                                 WPEModifiers   modifiers,
                                                                 guint          keycode,
                                                                 guint          keyval);
WPE_API guint          wpe_event_keyboard_get_keycode           (WPEEvent      *event);
WPE_API guint          wpe_event_keyboard_get_keyval            (WPEEvent      *event);

WPE_API WPEEvent      *wpe_event_touch_new                      (WPEEventType   type,
                                                                 WPEView       *view,
                                                                 WPEInputSource source,
                                                                 guint32        time,
                                                                 WPEModifiers   modifiers,
                                                                 guint32        sequence_id,
                                                                 double         x,
                                                                 double         y);
WPE_API guint32        wpe_event_touch_get_sequence_id          (WPEEvent      *event);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(WPEEvent, wpe_event_unref)

G_END_DECLS

#endif /* WPEEvent_h */
