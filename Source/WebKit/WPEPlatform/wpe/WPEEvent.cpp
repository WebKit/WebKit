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

#include "config.h"
#include "WPEEvent.h"

#include <wtf/FastMalloc.h>
#include <wtf/StdLibExtras.h>
#include <wtf/glib/GRefPtr.h>

struct WPEEventPointerButton {
    WPEModifiers modifiers { static_cast<WPEModifiers>(0) };
    unsigned button { 0 };
    unsigned pressCount { 0 };
    double x { 0 };
    double y { 0 };
};

struct WPEEventPointerMove {
    WPEModifiers modifiers { static_cast<WPEModifiers>(0) };
    double x { 0 };
    double y { 0 };
    double deltaX { 0 };
    double deltaY { 0 };
};

struct WPEEventScroll {
    WPEModifiers modifiers { static_cast<WPEModifiers>(0) };
    double deltaX { 0 };
    double deltaY { 0 };
    double x { 0 };
    double y { 0 };
    bool hasPreciseDeltas { false };
    bool isStop { false };
};

struct WPEEventKeyboard {
    WPEModifiers modifiers { static_cast<WPEModifiers>(0) };
    unsigned keycode;
    unsigned keyval;
};

struct WPEEventTouch {
    WPEModifiers modifiers { static_cast<WPEModifiers>(0) };
    uint32_t sequenceID { 0 };
    double x { 0 };
    double y { 0 };
};

/**
 * WPEEvent: (ref-func wpe_event_ref) (unref-func wpe_event_unref)
 *
 */
struct _WPEEvent {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(_WPEEvent);

    ~_WPEEvent()
    {
        if (userData.destroyFunction)
            userData.destroyFunction(userData.data);
    }

    GRefPtr<WPEView> view;
    WPEEventType type { WPE_EVENT_NONE };
    WPEInputSource source { WPE_INPUT_SOURCE_MOUSE };
    guint32 time { 0 };

    struct {
        gpointer data { nullptr };
        GDestroyNotify destroyFunction { nullptr };
    } userData;

    Variant<WPEEventPointerButton, WPEEventPointerMove, WPEEventScroll, WPEEventKeyboard, WPEEventTouch> variant;

    int referenceCount { 1 };
};

G_DEFINE_BOXED_TYPE(WPEEvent, wpe_event, wpe_event_ref, wpe_event_unref)

/**
 * wpe_event_ref:
 * @event: a #WPEEvent
 *
 * Atomically increments the reference count of @event by one.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The passed in #WPEEvent
 */
WPEEvent* wpe_event_ref(WPEEvent* event)
{
    g_atomic_int_inc(&event->referenceCount);
    return event;
}

/**
 * wpe_event_unref:
 * @event: a #WPEEvent
 *
 * Atomically decrements the reference count of @event by one.
 *
 * If the reference count drops to 0, all memory allocated by the #WPEEvent is
 * released. This function is MT-safe and may be called from any thread.
 */
void wpe_event_unref(WPEEvent* event)
{
    if (g_atomic_int_dec_and_test(&event->referenceCount))
        delete event;
}

/**
 * wpe_event_get_event_type:
 * @event: a #WPEEvent
 *
 * Get the #WPEEventType of @event
 *
 * Returns: a #WPEEventType
 */
WPEEventType wpe_event_get_event_type(WPEEvent* event)
{
    g_return_val_if_fail(event, WPE_EVENT_NONE);

    return event->type;
}

/**
 * wpe_event_get_view:
 * @event: a #WPEEvent
 *
 * Get the #WPEView associated to @event
 *
 * Returns: (transfer none): a #WPEView
 */
WPEView* wpe_event_get_view(WPEEvent* event)
{
    g_return_val_if_fail(event, nullptr);

    return event->view.get();
}

/**
 * wpe_event_set_user_data:
 * @event: a #WPEEvent
 * @user_data: data to associate with @event
 * @destroy_func: (nullable): the function to call to release @user_data
 *
 * Set @user_data to @event
 * When @event is destroyed @destroy_func is called with @user_data.
 */
void wpe_event_set_user_data(WPEEvent* event, gpointer userData, GDestroyNotify destroyFunction)
{
    g_return_if_fail(event);

    if (event->userData.data == userData && event->userData.destroyFunction == destroyFunction)
        return;

    if (event->userData.destroyFunction)
        event->userData.destroyFunction(event->userData.data);

    event->userData.data = userData;
    event->userData.destroyFunction = destroyFunction;
}

/**
 * wpe_event_get_user_data:
 * @event: a #WPEEvent
 *
 * Get user data previously set with wpe_event_set_user_data().
 *
 * Returns: (nullable): the @event user data, or %NULL
 */
gpointer wpe_event_get_user_data(WPEEvent* event)
{
    g_return_val_if_fail(event, nullptr);

    return event->userData.data;
}

/**
 * wpe_event_get_input_source:
 * @event: a #WPEEvent
 *
 * Get the device input source of @event
 *
 * Returns: a #WPEInputSource
 */
WPEInputSource wpe_event_get_input_source(WPEEvent* event)
{
    g_return_val_if_fail(event, WPE_INPUT_SOURCE_MOUSE);

    return event->source;
}

/**
 * wpe_event_get_time:
 * @event: a #WPEEvent
 *
 * Get the time stamp of @event
 *
 * Returns: a time stamp or 0
 */
guint32 wpe_event_get_time(WPEEvent* event)
{
    g_return_val_if_fail(event, 0);

    return event->time;
}

/**
 * wpe_event_get_modifiers:
 * @event: a #WPEEvent
 *
 * Get the modifiers of @event. If the #WPEEvent doesn't have
 * modifiers state 0 is returned.
 *
 * Returns: a #WPEModifiers
 */
WPEModifiers wpe_event_get_modifiers(WPEEvent* event)
{
    g_return_val_if_fail(event, static_cast<WPEModifiers>(0));

    return WTF::switchOn(event->variant,
        [](const WPEEventPointerButton& button) { return button.modifiers; },
        [](const WPEEventPointerMove& move) { return move.modifiers; },
        [](const WPEEventScroll& scroll) { return scroll.modifiers; },
        [](const WPEEventKeyboard& keyboard) { return keyboard.modifiers; },
        [](const WPEEventTouch& touch) { return touch.modifiers; },
        [](const auto&) { return static_cast<WPEModifiers>(0); }
    );
}

/**
 * wpe_event_get_position:
 * @event: a #WPEEvent
 * @x: (out): location to store x coordinate
 * @y: (out): location to store y coordinate
 *
 * Get the position of @event. If the #WPEEvent doesn't have
 * a position, %FALSE is returned.
 *
 * Returns: %TRUE if position is returned in @x and @y,
 *    or %FALSE if @event doesn't have a positon
 */
gboolean wpe_event_get_position(WPEEvent* event, double* x, double* y)
{
    g_return_val_if_fail(event, FALSE);

    return WTF::switchOn(event->variant,
        [&x, &y](const WPEEventPointerButton& button) -> gboolean {
            if (x)
                *x = button.x;
            if (y)
                *y = button.y;
            return TRUE;
        },
        [&x, &y](const WPEEventPointerMove& move) -> gboolean {
            if (x)
                *x = move.x;
            if (y)
                *y = move.y;
            return TRUE;
        },
        [&x, &y](const WPEEventScroll& scroll) -> gboolean {
            if (x)
                *x = scroll.x;
            if (y)
                *y = scroll.y;
            return TRUE;
        },
        [&x, &y](const WPEEventTouch& touch) -> gboolean {
            if (x)
                *x = touch.x;
            if (y)
                *y = touch.y;
            return TRUE;
        },
        [](const auto&) -> gboolean { return FALSE; }
    );
}

/**
 * wpe_event_pointer_button_new:
 * @type: a #WPEEventType (%WPE_EVENT_POINTER_DOWN or %WPE_EVENT_POINTER_UP)
 * @view: a #WPEView
 * @source: a #WPEInputSource
 * @time: the event timestamp
 * @modifiers: a #WPEModifiers
 * @button: the button number
 * @x: the x coordinate of the pointer
 * @y: the y coordinate of the pointer
 * @press_count: the number of button presses
 *
 * Create a #WPEEvent for a pointer button press or release.
 *
 * Returns: (transfer full): a new allocated #WPEEvent.
 */
WPEEvent* wpe_event_pointer_button_new(WPEEventType type, WPEView* view, WPEInputSource source, guint32 time, WPEModifiers modifiers, guint button, double x, double y, guint pressCount)
{
    g_return_val_if_fail(type == WPE_EVENT_POINTER_DOWN || type == WPE_EVENT_POINTER_UP, nullptr);
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);
    g_return_val_if_fail(!pressCount || type == WPE_EVENT_POINTER_DOWN, nullptr);

    return new _WPEEvent { view, type, source, time, { nullptr, nullptr }, WPEEventPointerButton { modifiers, button, pressCount, x, y }, 1 };
}

/**
 * wpe_event_pointer_button_get_button:
 * @event: a #WPEEvent
 *
 * Get the button number of @event.
 * Note that @event must be a pointer button event (%WPE_EVENT_POINTER_DOWN or %WPE_EVENT_POINTER_UP).
 *
 * Returns: the button of @event
 */
guint wpe_event_pointer_button_get_button(WPEEvent* event)
{
    g_return_val_if_fail(event, 0);
    g_return_val_if_fail(event->type == WPE_EVENT_POINTER_DOWN || event->type == WPE_EVENT_POINTER_UP, 0);

    return std::get<WPEEventPointerButton>(event->variant).button;
}

/**
 * wpe_event_pointer_button_get_press_count:
 * @event: a #WPEEvent
 *
 * Get the numbbr of button presses for @event
 * Note that @event must be a pointer button press event (%WPE_EVENT_POINTER_DOWN).
 *
 * Returns: the press count of @event
 */
guint wpe_event_pointer_button_get_press_count(WPEEvent* event)
{
    g_return_val_if_fail(event, 0);
    g_return_val_if_fail(event->type == WPE_EVENT_POINTER_DOWN, 0);

    return std::get<WPEEventPointerButton>(event->variant).pressCount;
}

/**
 * wpe_event_pointer_move_new:
 * @type: a #WPEEventType (%WPE_EVENT_POINTER_MOVE, %WPE_EVENT_POINTER_ENTER or %WPE_EVENT_POINTER_LEAVE)
 * @view: a #WPEView
 * @source: a #WPEInputSource
 * @time: the event timestamp
 * @modifiers: a #WPEModifiers
 * @x: the x coordinate of the pointer
 * @y: the y coordinate of the pointer
 * @delta_x: the x coordainte movement delta
 * @delta_y: the y coordinate movement delta
 *
 * Create a #WPEEvent for a pointer move.
 *
 * Returns: (transfer full): a new allocated #WPEEvent.
 */
WPEEvent* wpe_event_pointer_move_new(WPEEventType type, WPEView* view, WPEInputSource source, guint32 time, WPEModifiers modifiers, double x, double y, double deltaX, double deltaY)
{
    g_return_val_if_fail(type == WPE_EVENT_POINTER_MOVE || type == WPE_EVENT_POINTER_ENTER || type == WPE_EVENT_POINTER_LEAVE, nullptr);
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    return new _WPEEvent { view, type, source, time, { nullptr, nullptr }, WPEEventPointerMove { modifiers, x, y, deltaX, deltaY }, 1 };
}

/**
 * wpe_event_pointer_move_get_delta:
 * @event: a #WPEEvent
 * @delta_x: (out): location to store the x coordinate delta
 * @delta_y: (out): location to store the y coordinate delta
 *
 * Get the movement delta of @event
 * Note that @event must be a pointer move event (%WPE_EVENT_POINTER_MOVE, %WPE_EVENT_POINTER_ENTER or %WPE_EVENT_POINTER_LEAVE)
 */
void wpe_event_pointer_move_get_delta(WPEEvent* event, double* deltaX, double* deltaY)
{
    g_return_if_fail(event);
    g_return_if_fail(event->type == WPE_EVENT_POINTER_MOVE || event->type == WPE_EVENT_POINTER_ENTER || event->type == WPE_EVENT_POINTER_LEAVE);

    const auto& move = std::get<WPEEventPointerMove>(event->variant);
    if (deltaX)
        *deltaX = move.deltaX;
    if (deltaY)
        *deltaY = move.deltaY;
}

/**
 * wpe_event_scroll_new:
 * @view: a #WPEView
 * @source: a #WPEInputSource
 * @time: the event timestamp
 * @modifiers: a #WPEModifiers
 * @delta_x: delta on the X axis
 * @delta_y: delta on the Y axis
 * @precise_deltas: whether it has precise deltas
 * @is_stop: whether it's a stop event
 * @x: the x coordinate of the pointer, or 0 if not a pointing device
 * @y: the y coordinate of the pointer, or 0 if not a pointing device
 *
 * Create a #WPEEvent for a scroll.
 *
 * Returns: (transfer full): a new allocated #WPEEvent.
 */
WPEEvent* wpe_event_scroll_new(WPEView* view, WPEInputSource source, guint32 time, WPEModifiers modifiers, double deltaX, double deltaY, gboolean preciseDeltas, gboolean isStop, double x, double y)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    return new _WPEEvent { view, WPE_EVENT_SCROLL, source, time, { nullptr, nullptr }, WPEEventScroll { modifiers, deltaX, deltaY, x, y, !!preciseDeltas, !!isStop }, 1 };
}

/**
 * wpe_event_scroll_get_deltas:
 * @event: a #WPEEvent
 * @delta_x: (out): location to store the x coordinate delta
 * @delta_y: (out): location to store the y coordinate delta
 *
 * Get the scroll deltas of @event
 * Note that @event must be a scroll event (%WPE_EVENT_SCROLL)
 */
void wpe_event_scroll_get_deltas(WPEEvent* event, double* deltaX, double* deltaY)
{
    g_return_if_fail(event);
    g_return_if_fail(event->type == WPE_EVENT_SCROLL);

    const auto& scroll = std::get<WPEEventScroll>(event->variant);
    if (deltaX)
        *deltaX = scroll.deltaX;
    if (deltaY)
        *deltaY = scroll.deltaY;
}

/**
 * wpe_event_scroll_has_precise_deltas:
 * @event: a #WPEEvent
 *
 * Get whether @event has precise scroll deltas.
 * Note that @event must be a scroll event (%WPE_EVENT_SCROLL)
 *
 * Returns: %TRUE if @event has precise scroll deltas, or %FALSE otherwise
 */
gboolean wpe_event_scroll_has_precise_deltas(WPEEvent* event)
{
    g_return_val_if_fail(event, FALSE);
    g_return_val_if_fail(event->type == WPE_EVENT_SCROLL, FALSE);

    return std::get<WPEEventScroll>(event->variant).hasPreciseDeltas;
}

/**
 * wpe_event_scroll_is_stop:
 * @event: a #WPEEvent
 *
 * Get whether @event is a stop scroll event
 *
 * Returns: %TRUE if @event is a stop scroll event, or %FALSE otherwise
 */
gboolean wpe_event_scroll_is_stop(WPEEvent* event)
{
    g_return_val_if_fail(event, FALSE);
    g_return_val_if_fail(event->type == WPE_EVENT_SCROLL, FALSE);

    return std::get<WPEEventScroll>(event->variant).isStop;
}

/**
 * wpe_event_keyboard_new:
 * @type: a #WPEEventType (%WPE_EVENT_KEYBOARD_KEY_DOWN or %WPE_EVENT_KEYBOARD_KEY_UP)
 * @view: a #WPEView
 * @source: a #WPEInputSource
 * @time: the event timestamp
 * @modifiers: a #WPEModifiers
 * @keycode: the hardware key code
 * @keyval: the translated key value
 *
 * Create a #WPEEvent for a keyboard key press or release
 *
 * Returns: (transfer full): a new allocated #WPEEvent.
 */
WPEEvent* wpe_event_keyboard_new(WPEEventType type, WPEView* view, WPEInputSource source, guint32 time, WPEModifiers modifiers, guint keycode, guint keyval)
{
    g_return_val_if_fail(type == WPE_EVENT_KEYBOARD_KEY_DOWN || type == WPE_EVENT_KEYBOARD_KEY_UP, nullptr);
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    return new _WPEEvent { view, type, source, time, { nullptr, nullptr }, WPEEventKeyboard { modifiers, keycode, keyval }, 1 };
}

/**
 * wpe_event_keyboard_get_keycode:
 * @event: a #WPEEvent
 *
 * Get the hardware key code of @event
 * Note that @event must be a keyboard event (%WPE_EVENT_KEYBOARD_KEY_DOWN or %WPE_EVENT_KEYBOARD_KEY_UP)
 *
 * Returns: the hardware key code of @event
 */
guint wpe_event_keyboard_get_keycode(WPEEvent* event)
{
    g_return_val_if_fail(event, 0);
    g_return_val_if_fail(event->type == WPE_EVENT_KEYBOARD_KEY_DOWN || event->type == WPE_EVENT_KEYBOARD_KEY_UP, 0);

    return std::get<WPEEventKeyboard>(event->variant).keycode;
}

/**
 * wpe_event_keyboard_get_keyval:
 * @event: a #WPEEvent
 *
 * Get the translated key value of @event
 * Note that @event must be a keyboard event (%WPE_EVENT_KEYBOARD_KEY_DOWN or %WPE_EVENT_KEYBOARD_KEY_UP)
 *
 * Returns: the translated key value of @event
 */
guint wpe_event_keyboard_get_keyval(WPEEvent* event)
{
    g_return_val_if_fail(event, 0);
    g_return_val_if_fail(event->type == WPE_EVENT_KEYBOARD_KEY_DOWN || event->type == WPE_EVENT_KEYBOARD_KEY_UP, 0);

    return std::get<WPEEventKeyboard>(event->variant).keyval;
}

/**
 * wpe_event_touch_new:
 * @type: a #WPEEventType (%WPE_EVENT_TOUCH_DOWN, %WPE_EVENT_TOUCH_UP, %WPE_EVENT_TOUCH_MOVER or %WPE_EVENT_TOUCH_CANCEL)
 * @view: a #WPEView
 * @source: a #WPEInputSource
 * @time: the event timestamp
 * @modifiers: a #WPEModifiers
 * @sequence_id: the sequence event identifier
 * @x: the x coordinate of the touch
 * @y: the y coordinate of the touch
 *
 * Create a #WPEEvent for a touch
 *
 * Returns: (transfer full): a new allocated #WPEEvent.
 */
WPEEvent* wpe_event_touch_new(WPEEventType type, WPEView* view, WPEInputSource source, guint32 time, WPEModifiers modifiers, guint32 sequenceID, double x, double y)
{
    g_return_val_if_fail(type == WPE_EVENT_TOUCH_DOWN || type == WPE_EVENT_TOUCH_UP || type == WPE_EVENT_TOUCH_MOVE || type == WPE_EVENT_TOUCH_CANCEL, nullptr);
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);

    return new _WPEEvent { view, type, source, time, { nullptr, nullptr }, WPEEventTouch { modifiers, sequenceID, x, y }, 1 };
}

/**
 * wpe_event_touch_get_sequence_id:
 * @event: a #WPEEvent
 *
 * Get the sequence identifier of @event.
 * Note that @event must be a touch event (%WPE_EVENT_TOUCH_DOWN, %WPE_EVENT_TOUCH_UP, %WPE_EVENT_TOUCH_MOVER or %WPE_EVENT_TOUCH_CANCEL)
 *
 * Returns: the event sequence identifier
 */
guint32 wpe_event_touch_get_sequence_id(WPEEvent* event)
{
    g_return_val_if_fail(event, 0);
    g_return_val_if_fail(event->type == WPE_EVENT_TOUCH_DOWN || event->type == WPE_EVENT_TOUCH_UP || event->type == WPE_EVENT_TOUCH_MOVE || event->type == WPE_EVENT_TOUCH_CANCEL, 0);

    return std::get<WPEEventTouch>(event->variant).sequenceID;
}
