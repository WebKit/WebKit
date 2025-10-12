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
#include "WPEWaylandSeat.h"

#include "GRefPtrWPE.h"
#include "WPEDisplayWaylandPrivate.h"
#include "WPEKeymapXKB.h"
#include "WPEKeysyms.h"
#include "WPEToplevelWaylandPrivate.h"
#include <linux/input.h>
#include <wtf/MonotonicTime.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WPE {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WaylandSeat);

WaylandSeat::WaylandSeat(struct wl_seat* seat)
    : m_seat(seat)
    , m_keymap(adoptGRef(wpe_keymap_xkb_new()))
{
}

WaylandSeat::~WaylandSeat()
{
    g_clear_pointer(&m_pointer.object, wl_pointer_destroy);
    g_clear_pointer(&m_keyboard.object, wl_keyboard_destroy);
    if (m_keyboard.repeat.source)
        g_source_destroy(m_keyboard.repeat.source.get());
    wl_seat_destroy(m_seat);
}

const struct wl_pointer_listener WaylandSeat::s_pointerListener = {
    // enter
    [](void* data, struct wl_pointer*, uint32_t serial, struct wl_surface* surface, wl_fixed_t x, wl_fixed_t y)
    {
        if (!surface)
            return;

        auto* toplevel = wl_surface_get_user_data(surface);
        if (!WPE_IS_TOPLEVEL(toplevel))
            return;

        auto* toplevelWayland = WPE_TOPLEVEL_WAYLAND(toplevel);
        wpeToplevelWaylandSetHasPointer(toplevelWayland, true);

        auto& seat = *static_cast<WaylandSeat*>(data);
        seat.m_pointer.toplevel.reset(toplevelWayland);
        seat.m_pointer.x = wl_fixed_to_double(x);
        seat.m_pointer.y = wl_fixed_to_double(y);
        seat.m_pointer.modifiers = 0;
        seat.m_pointer.time = 0;
        seat.m_pointer.enterSerial = serial;

        seat.updateCursor();

        if (GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleViewUnderPointer(seat.m_pointer.toplevel.get()))
            seat.emitPointerEnter(view.get());
    },
    // leave
    [](void* data, struct wl_pointer*, uint32_t /*serial*/, struct wl_surface*)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_pointer.toplevel)
            return;

        GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleViewUnderPointer(seat.m_pointer.toplevel.get());
        wpeToplevelWaylandSetHasPointer(seat.m_pointer.toplevel.get(), false);
        seat.m_pointer.toplevel = nullptr;

        if (view)
            seat.emitPointerLeave(view.get());
    },
    // motion
    [](void* data, struct wl_pointer*, uint32_t time, wl_fixed_t fixedX, wl_fixed_t fixedY)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_pointer.toplevel)
            return;

        double x = wl_fixed_to_double(fixedX);
        double y = wl_fixed_to_double(fixedY);
        double deltaX = seat.m_pointer.x ? x - seat.m_pointer.x : 0;
        double deltaY = seat.m_pointer.y ? y - seat.m_pointer.y : 0;

        seat.m_pointer.time = time;
        seat.m_pointer.x = x;
        seat.m_pointer.y = y;

        GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleViewUnderPointer(seat.m_pointer.toplevel.get());
        if (!view)
            return;

        GRefPtr<WPEEvent> event = adoptGRef(wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, view.get(), seat.m_pointer.source, time, seat.modifiers(), x, y, deltaX, deltaY));
        wpe_view_event(view.get(), event.get());
    },
    // button
    [](void* data, struct wl_pointer*, uint32_t /*serial*/, uint32_t time, uint32_t button, uint32_t state)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_pointer.toplevel)
            return;

        switch (button) {
        case BTN_LEFT:
            button = WPE_BUTTON_PRIMARY;
            break;
        case BTN_MIDDLE:
            button = WPE_BUTTON_MIDDLE;
            break;
        case BTN_RIGHT:
            button = WPE_BUTTON_SECONDARY;
            break;
        default:
            button = button - BTN_MOUSE + 1;
        }

        uint32_t buttonModifiers = 0;
        switch (button) {
        case 1:
            buttonModifiers = WPE_MODIFIER_POINTER_BUTTON1;
            break;
        case 2:
            buttonModifiers = WPE_MODIFIER_POINTER_BUTTON2;
            break;
        case 3:
            buttonModifiers = WPE_MODIFIER_POINTER_BUTTON3;
            break;
        case 4:
            buttonModifiers = WPE_MODIFIER_POINTER_BUTTON4;
            break;
        case 5:
            buttonModifiers = WPE_MODIFIER_POINTER_BUTTON5;
            break;
        }

        if (state)
            seat.m_pointer.modifiers |= buttonModifiers;
        else
            seat.m_pointer.modifiers &= ~buttonModifiers;

        seat.m_pointer.time = time;

        GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleViewUnderPointer(seat.m_pointer.toplevel.get());
        if (!view)
            return;

        unsigned pressCount = state ? wpe_view_compute_press_count(view.get(), seat.m_pointer.x, seat.m_pointer.y, button, time) : 0;
        GRefPtr<WPEEvent> event = adoptGRef(wpe_event_pointer_button_new(state ? WPE_EVENT_POINTER_DOWN : WPE_EVENT_POINTER_UP, view.get(), seat.m_pointer.source,
            time, seat.modifiers(), button, seat.m_pointer.x, seat.m_pointer.y, pressCount));
        wpe_view_event(view.get(), event.get());
    },
    // axis
    [](void* data, struct wl_pointer*, uint32_t time, uint32_t axis, wl_fixed_t value)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_pointer.toplevel)
            return;

        switch (axis) {
        case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            seat.m_pointer.frame.deltaX = -wl_fixed_to_double(value);
            break;
        case WL_POINTER_AXIS_VERTICAL_SCROLL:
            seat.m_pointer.frame.deltaY = -wl_fixed_to_double(value);
            break;
        }

        seat.m_pointer.time = time;
        if (wl_seat_get_version(seat.m_seat) < 5)
            seat.flushScrollEvent();
    },
    // frame
    [](void* data, struct wl_pointer*)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_pointer.toplevel)
            return;

        seat.flushScrollEvent();
    },
    // axis_source
    [](void* data, struct wl_pointer*, uint32_t source)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_pointer.toplevel)
            return;

        switch (source) {
        case WL_POINTER_AXIS_SOURCE_FINGER:
            seat.m_pointer.frame.source = WPE_INPUT_SOURCE_TOUCHPAD;
            break;
        case WL_POINTER_AXIS_SOURCE_CONTINUOUS:
            seat.m_pointer.frame.source = WPE_INPUT_SOURCE_TRACKPOINT;
            break;
        case WL_POINTER_AXIS_SOURCE_WHEEL:
        default:
            seat.m_pointer.frame.source = WPE_INPUT_SOURCE_MOUSE;
            break;
        }
    },
    // axis_stop
    [](void* data, struct wl_pointer*, uint32_t time, uint32_t axis)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_pointer.toplevel)
            return;

        switch (axis) {
        case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            seat.m_pointer.frame.deltaX = 0;
            break;
        case WL_POINTER_AXIS_VERTICAL_SCROLL:
            seat.m_pointer.frame.deltaY = 0;
            break;
        }

        seat.m_pointer.frame.isStop = true;
        seat.m_pointer.time = time;
    },
    // axis_discrete
    [](void* data, struct wl_pointer*, uint32_t axis, int32_t value)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_pointer.toplevel)
            return;

        switch (axis) {
        case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            seat.m_pointer.frame.valueX = -value;
            break;
        case WL_POINTER_AXIS_VERTICAL_SCROLL:
            seat.m_pointer.frame.valueY = -value;
            break;
        }
    },
#ifdef WL_POINTER_AXIS_VALUE120_SINCE_VERSION
    // axis_value120
    [](void* data, struct wl_pointer*, uint32_t axis, int32_t value)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_pointer.toplevel)
            return;

        switch (axis) {
        case WL_POINTER_AXIS_HORIZONTAL_SCROLL:
            seat.m_pointer.frame.valueX = -value / 120;
            break;
        case WL_POINTER_AXIS_VERTICAL_SCROLL:
            seat.m_pointer.frame.valueY = -value / 120;
            break;
        }
    },
#endif
#ifdef WL_POINTER_AXIS_RELATIVE_DIRECTION_SINCE_VERSION
    // axis_relative_direction
    [](void*, struct wl_pointer*, uint32_t, uint32_t)
    {
    },
#endif
};

const struct wl_keyboard_listener WaylandSeat::s_keyboardListener = {
    // keymap
    [](void* data, struct wl_keyboard*, uint32_t format, int fd, uint32_t size)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        wpe_keymap_xkb_update(WPE_KEYMAP_XKB(seat.m_keymap.get()), format, fd, size);
    },
    // enter
    [](void* data, struct wl_keyboard*, uint32_t serial, struct wl_surface* surface, struct wl_array*)
    {
        if (!surface)
            return;

        auto* toplevel = wl_surface_get_user_data(surface);
        if (!WPE_IS_TOPLEVEL(toplevel))
            return;

        auto* toplevelWayland = WPE_TOPLEVEL_WAYLAND(toplevel);
        wpeToplevelWaylandSetIsFocused(toplevelWayland, true);

        auto& seat = *static_cast<WaylandSeat*>(data);
        seat.m_keyboard.toplevel.reset(toplevelWayland);
        seat.m_keyboard.repeat.key = 0;
        seat.m_keyboard.serial = serial;

        if (GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleFocusedView(toplevelWayland))
            wpe_view_focus_in(view.get());
    },
    // leave
    [](void* data, struct wl_keyboard*, uint32_t /*serial*/, struct wl_surface*)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_keyboard.toplevel)
            return;

        if (seat.m_keyboard.repeat.source)
            g_source_set_ready_time(seat.m_keyboard.repeat.source.get(), -1);

        GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleFocusedView(seat.m_keyboard.toplevel.get());
        wpeToplevelWaylandSetIsFocused(seat.m_keyboard.toplevel.get(), false);

        seat.m_keyboard.toplevel = nullptr;
        seat.m_keyboard.repeat.key = 0;
        seat.m_keyboard.repeat.deadline = { };
        seat.m_keyboard.serial = 0;

        if (view)
            wpe_view_focus_out(view.get());
    },
    // key
    [](void* data, struct wl_keyboard*, uint32_t /*serial*/, uint32_t time, uint32_t key, uint32_t state)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_keyboard.toplevel)
            return;

        seat.m_keyboard.time = time;
        seat.handleKeyEvent(time, key + 8, state, false);
    },
    // modifiers
    [](void* data, struct wl_keyboard*, uint32_t /*serial*/, uint32_t depressedMods, uint32_t latchedMods, uint32_t lockedMods, uint32_t group)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        auto* xkbState = wpe_keymap_xkb_get_xkb_state(WPE_KEYMAP_XKB(seat.m_keymap.get()));
        xkb_state_update_mask(xkbState, depressedMods, latchedMods, lockedMods, group, 0, 0);
        seat.m_keyboard.modifiers = wpe_keymap_get_modifiers(seat.m_keymap.get());

        if (!seat.m_keyboard.toplevel)
            return;

        if (!seat.m_keyboard.capsLockUpEvent.key)
            return;

        if (GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleFocusedView(seat.m_keyboard.toplevel.get())) {
            if (seat.m_keyboard.modifiers & WPE_MODIFIER_KEYBOARD_CAPS_LOCK)
                seat.m_keyboard.capsLockUpEvent.modifiers |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;
            else
                seat.m_keyboard.capsLockUpEvent.modifiers &= ~WPE_MODIFIER_KEYBOARD_CAPS_LOCK;

            GRefPtr<WPEEvent> event = adoptGRef(wpe_event_keyboard_new(WPE_EVENT_KEYBOARD_KEY_UP, view.get(), seat.m_keyboard.source, seat.m_keyboard.capsLockUpEvent.time,
                static_cast<WPEModifiers>(seat.m_keyboard.capsLockUpEvent.modifiers), seat.m_keyboard.capsLockUpEvent.key, seat.m_keyboard.capsLockUpEvent.keyval));
            wpe_view_event(view.get(), event.get());
        }
        seat.m_keyboard.capsLockUpEvent = { };
    },
    // repeat_info
    [](void* data, struct wl_keyboard*, int32_t rate, int32_t delay)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        seat.m_keyboard.repeat.rate = rate;
        seat.m_keyboard.repeat.delay = delay;
    }
};

const struct wl_touch_listener WaylandSeat::s_touchListener = {
    // down
    [](void* data, struct wl_touch*, uint32_t, uint32_t time, struct wl_surface* surface, int32_t id, wl_fixed_t x, wl_fixed_t y)
    {
        if (!surface)
            return;

        auto* toplevel = wl_surface_get_user_data(surface);
        if (!WPE_IS_TOPLEVEL(toplevel))
            return;

        auto& seat = *static_cast<WaylandSeat*>(data);
        seat.m_touch.toplevel.reset(WPE_TOPLEVEL_WAYLAND(toplevel));

        auto addResult = seat.m_touch.points.add(id, std::pair<double, double>(wl_fixed_to_double(x), wl_fixed_to_double(y)));
        if (seat.m_touch.points.size() == 1)
            wpeToplevelWaylandSetIsUnderTouch(seat.m_touch.toplevel.get(), true);

        GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleViewUnderTouch(seat.m_touch.toplevel.get());
        if (!view)
            return;

        GRefPtr<WPEEvent> event = adoptGRef(wpe_event_touch_new(WPE_EVENT_TOUCH_DOWN, view.get(), seat.m_touch.source, time, seat.modifiers(),
            id, addResult.iterator->value.first, addResult.iterator->value.second));
        wpe_view_event(view.get(), event.get());
    },
    // up
    [](void* data, struct wl_touch*, uint32_t, uint32_t time, int32_t id)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_touch.toplevel)
            return;

        const auto& iter = seat.m_touch.points.find(id);
        if (iter == seat.m_touch.points.end())
            return;

        GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleViewUnderTouch(seat.m_touch.toplevel.get());
        if (seat.m_touch.points.size() == 1) {
            wpeToplevelWaylandSetIsUnderTouch(seat.m_touch.toplevel.get(), false);
            seat.m_touch.toplevel = nullptr;
        }

        if (view) {
            GRefPtr<WPEEvent> event = adoptGRef(wpe_event_touch_new(WPE_EVENT_TOUCH_UP, view.get(), seat.m_touch.source, time, seat.modifiers(),
                id, iter->value.first, iter->value.second));
            wpe_view_event(view.get(), event.get());
        }
        seat.m_touch.points.remove(id);
    },
    // motion
    [](void* data, struct wl_touch*, uint32_t time, int32_t id, wl_fixed_t x, wl_fixed_t y)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_touch.toplevel)
            return;

        auto iter = seat.m_touch.points.find(id);
        if (iter == seat.m_touch.points.end())
            return;

        iter->value = { wl_fixed_to_double(x), wl_fixed_to_double(y) };

        GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleViewUnderTouch(seat.m_touch.toplevel.get());
        if (!view)
            return;

        GRefPtr<WPEEvent> event = adoptGRef(wpe_event_touch_new(WPE_EVENT_TOUCH_MOVE, view.get(), seat.m_touch.source, time, seat.modifiers(),
            id, iter->value.first, iter->value.second));
        wpe_view_event(view.get(), event.get());
    },
    // frame
    [](void*, struct wl_touch*)
    {
    },
    // cancel
    [](void* data, struct wl_touch*)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);
        if (!seat.m_touch.toplevel)
            return;

        if (GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleFocusedView(seat.m_touch.toplevel.get())) {
            for (const auto& iter : seat.m_touch.points) {
                GRefPtr<WPEEvent> event = adoptGRef(wpe_event_touch_new(WPE_EVENT_TOUCH_CANCEL, view.get(), seat.m_touch.source, 0, seat.modifiers(),
                    iter.key, iter.value.first, iter.value.second));
                wpe_view_event(view.get(), event.get());
            }
        }

        seat.m_touch.points.clear();
    },
    // shape
    [](void*, struct wl_touch*, int32_t, wl_fixed_t, wl_fixed_t)
    {
    },
    // orientation
    [](void*, struct wl_touch*, int32_t, wl_fixed_t)
    {
    }
};

const struct wl_seat_listener WaylandSeat::s_listener = {
    // capabilities
    [](void* data, struct wl_seat* wlSeat, uint32_t capabilities)
    {
        auto& seat = *static_cast<WaylandSeat*>(data);

        // WL_SEAT_CAPABILITY_POINTER
        const bool hasPointer = capabilities & WL_SEAT_CAPABILITY_POINTER;
        if (hasPointer && !seat.m_pointer.object) {
            seat.m_pointer.object = wl_seat_get_pointer(wlSeat);
            seat.m_pointer.source = WPE_INPUT_SOURCE_MOUSE;
            wl_pointer_add_listener(seat.m_pointer.object, &s_pointerListener, data);
        } else if (!hasPointer && seat.m_pointer.object) {
            wl_pointer_release(seat.m_pointer.object);
            seat.m_pointer = { };
        }

        // WL_SEAT_CAPABILITY_KEYBOARD
        const bool hasKeyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
        if (hasKeyboard && !seat.m_keyboard.object) {
            seat.m_keyboard.object = wl_seat_get_keyboard(wlSeat);
            seat.m_keyboard.source = WPE_INPUT_SOURCE_KEYBOARD;
            wl_keyboard_add_listener(seat.m_keyboard.object, &s_keyboardListener, data);
        } else if (!hasKeyboard && seat.m_keyboard.object) {
            wl_keyboard_release(seat.m_keyboard.object);
            seat.m_keyboard = { };
        }

        // WL_SEAT_CAPABILITY_TOUCH
        const bool hasTouch = capabilities & WL_SEAT_CAPABILITY_TOUCH;
        if (hasTouch && !seat.m_touch.object) {
            seat.m_touch.object = wl_seat_get_touch(wlSeat);
            seat.m_touch.source = WPE_INPUT_SOURCE_TOUCHSCREEN;
            wl_touch_add_listener(seat.m_touch.object, &s_touchListener, data);
        } else if (!hasTouch && seat.m_touch.object) {
            wl_touch_release(seat.m_touch.object);
            seat.m_touch = { };
        }

        if (seat.m_capabilitiesChangedCallback)
            seat.m_capabilitiesChangedCallback(seat.availableInputDevices());
    },
    // name
    [](void*, struct wl_seat*, const char*) { }
};

void WaylandSeat::startListening()
{
    wl_seat_add_listener(m_seat, &s_listener, this);
}

void WaylandSeat::setCursor(struct wl_surface* surface, int32_t hotspotX, int32_t hotspotY)
{
    if (!m_pointer.object)
        return;
    wl_pointer_set_cursor(m_pointer.object, m_pointer.enterSerial, surface, hotspotX, hotspotY);
}

void WaylandSeat::updateCursor()
{
    if (auto* cursor = wpeDisplayWaylandGetCursor(WPE_DISPLAY_WAYLAND(wpe_toplevel_get_display(WPE_TOPLEVEL(m_pointer.toplevel.get())))))
        cursor->update();
}

void WaylandSeat::emitPointerEnter(WPEView* view) const
{
    if (!m_pointer.toplevel)
        return;

    GRefPtr<WPEEvent> event = adoptGRef(wpe_event_pointer_move_new(WPE_EVENT_POINTER_ENTER, view, m_pointer.source, 0, modifiers(), m_pointer.x, m_pointer.y, 0, 0));
    wpe_view_event(view, event.get());
}

void WaylandSeat::emitPointerLeave(WPEView* view) const
{
    GRefPtr<WPEEvent> event = adoptGRef(wpe_event_pointer_move_new(WPE_EVENT_POINTER_LEAVE, view, m_pointer.source, 0, modifiers(), -1, -1, 0, 0));
    wpe_view_event(view, event.get());
}

WPEModifiers WaylandSeat::modifiers() const
{
    uint32_t mask = m_keyboard.modifiers;

    if (m_pointer.object)
        mask |= m_pointer.modifiers;

    return static_cast<WPEModifiers>(mask);
}

void WaylandSeat::flushScrollEvent()
{
    if (GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleViewUnderPointer(m_pointer.toplevel.get())) {
        GRefPtr<WPEEvent> event;
        if (m_pointer.frame.valueX || m_pointer.frame.valueY) {
            event = adoptGRef(wpe_event_scroll_new(view.get(), m_pointer.frame.source, m_pointer.time, modifiers(), m_pointer.frame.valueX, m_pointer.frame.valueY,
                FALSE, FALSE, m_pointer.x, m_pointer.y));
        } else if (m_pointer.frame.isStop || m_pointer.frame.deltaX || m_pointer.frame.deltaY) {
            event = adoptGRef(wpe_event_scroll_new(view.get(), m_pointer.frame.source, m_pointer.time, modifiers(), m_pointer.frame.deltaX, m_pointer.frame.deltaY,
                TRUE, m_pointer.frame.isStop && !m_pointer.frame.deltaX && !m_pointer.frame.deltaY, m_pointer.x, m_pointer.y));
        }

        if (event)
            wpe_view_event(view.get(), event.get());
    }

    m_pointer.frame = { };
}

static GSourceFuncs s_keyRepeatSourceFuncs = {
    nullptr, // prepare
    nullptr, // check
    // dispatch
    [](GSource* source, GSourceFunc callback, gpointer userData) -> gboolean
    {
        if (g_source_get_ready_time(source) == -1)
            return G_SOURCE_CONTINUE;
        g_source_set_ready_time(source, -1);
        return callback(userData);
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

void WaylandSeat::handleKeyEvent(uint32_t time, uint32_t key, uint32_t state, bool fromRepeat)
{
    if (!m_keyboard.toplevel)
        return;

    GRefPtr<WPEView> view = wpeToplevelWaylandGetVisibleFocusedView(m_keyboard.toplevel.get());
    if (!view)
        return;

    auto beginTime = MonotonicTime::now().secondsSinceEpoch();

    auto* keymap = WPE_KEYMAP_XKB(m_keymap.get());
    auto* xkbState = wpe_keymap_xkb_get_xkb_state(keymap);
    unsigned keyval = xkb_state_key_get_one_sym(xkbState, key);
    if (keyval == XKB_KEY_NoSymbol)
        return;

    unsigned eventModifiers = modifiers();
    switch (keyval) {
    case WPE_KEY_Control_L:
    case WPE_KEY_Control_R:
        if (state)
            eventModifiers |= WPE_MODIFIER_KEYBOARD_CONTROL;
        else
            eventModifiers &= ~WPE_MODIFIER_KEYBOARD_CONTROL;
        break;
    case WPE_KEY_Shift_L:
    case WPE_KEY_Shift_R:
        if (state)
            eventModifiers |= WPE_MODIFIER_KEYBOARD_SHIFT;
        else
            eventModifiers &= ~WPE_MODIFIER_KEYBOARD_SHIFT;
        break;
    case WPE_KEY_Alt_L:
    case WPE_KEY_Alt_R:
        if (state)
            eventModifiers |= WPE_MODIFIER_KEYBOARD_ALT;
        else
            eventModifiers &= ~WPE_MODIFIER_KEYBOARD_ALT;
        break;
    case WPE_KEY_Meta_L:
    case WPE_KEY_Meta_R:
        if (state)
            eventModifiers |= WPE_MODIFIER_KEYBOARD_META;
        else
            eventModifiers &= ~WPE_MODIFIER_KEYBOARD_META;
        break;
    case WPE_KEY_Caps_Lock:
        if (state)
            eventModifiers |= WPE_MODIFIER_KEYBOARD_CAPS_LOCK;
        else {
            // Delay caps lock release events after modifiers are updated.
            m_keyboard.capsLockUpEvent = { key, keyval, eventModifiers, time };
            return;
        }
        break;
    }

    GRefPtr<WPEEvent> event = adoptGRef(wpe_event_keyboard_new(state ? WPE_EVENT_KEYBOARD_KEY_DOWN : WPE_EVENT_KEYBOARD_KEY_UP, view.get(), m_keyboard.source, time,
        static_cast<WPEModifiers>(eventModifiers), key, keyval));
    wpe_view_event(view.get(), event.get());

    if (!m_keyboard.toplevel || !wpeToplevelWaylandGetVisibleFocusedView(m_keyboard.toplevel.get()))
        return;

    auto* xkbKeymap = wpe_keymap_xkb_get_xkb_keymap(keymap);
    if (!xkb_keymap_key_repeats(xkbKeymap, key))
        return;

    Seconds delay, interval;
    if (!keyRepeat(delay, interval))
        return;

    if (!fromRepeat) {
        if (state)
            m_keyboard.repeat.key = key;
        else if (m_keyboard.repeat.key == key)
            m_keyboard.repeat.key = 0;
    }

    if (!m_keyboard.repeat.key)
        return;

    if (!m_keyboard.repeat.source) {
        m_keyboard.repeat.source = adoptGRef(g_source_new(&s_keyRepeatSourceFuncs, sizeof(GSource)));
        // Use RunLoopTimer + 1 to ensure the key relase event has more priority.
        g_source_set_priority(m_keyboard.repeat.source.get(), RunLoopSourcePriority::RunLoopTimer + 1);
        g_source_set_name(m_keyboard.repeat.source.get(), "WPE wayland key repeat timer");
        g_source_set_callback(m_keyboard.repeat.source.get(), [](gpointer userData) -> gboolean {
            auto& seat = *static_cast<WaylandSeat*>(userData);
            seat.handleKeyEvent(seat.m_keyboard.time, seat.m_keyboard.repeat.key, 1, true);
            if (g_source_is_destroyed(seat.m_keyboard.repeat.source.get()))
                return G_SOURCE_REMOVE;
            return G_SOURCE_CONTINUE;
        }, this, nullptr);
        g_source_attach(m_keyboard.repeat.source.get(), g_main_context_get_thread_default());
    }

    auto now = MonotonicTime::now().secondsSinceEpoch();
    if (!fromRepeat)
        m_keyboard.repeat.deadline = beginTime + delay;
    else if (m_keyboard.repeat.deadline + interval > now)
        m_keyboard.repeat.deadline += interval;
    else
        m_keyboard.repeat.deadline = now;

    auto delta = m_keyboard.repeat.deadline - now;
    if (delta) {
        gint64 currentTime = g_get_monotonic_time();
        gint64 readyTime = currentTime + std::min<gint64>(G_MAXINT64 - currentTime, delta.microsecondsAs<gint64>());
        g_source_set_ready_time(m_keyboard.repeat.source.get(), readyTime);
    } else
        g_source_set_ready_time(m_keyboard.repeat.source.get(), 0);
}

bool WaylandSeat::keyRepeat(Seconds& delay, Seconds& interval)
{
    if (!m_keyboard.repeat.rate) {
        delay = 400_ms;
        interval = 80_ms;
        return true;
    }

    if (!m_keyboard.repeat.rate.value())
        return false;

    // delay is the number of milliseconds a key should be held down for before key repeat kicks in.
    delay = Seconds::fromMilliseconds(m_keyboard.repeat.delay.value());
    // rate is the number of characters per second.
    interval = Seconds(1. / m_keyboard.repeat.rate.value());
    return true;
}

WPEAvailableInputDevices WaylandSeat::availableInputDevices() const
{
    WPEAvailableInputDevices source = WPE_AVAILABLE_INPUT_DEVICE_NONE;
    if (m_pointer.object)
        source = WPE_AVAILABLE_INPUT_DEVICE_MOUSE;
    if (m_keyboard.object)
        source = static_cast<WPEAvailableInputDevices>(source | WPE_AVAILABLE_INPUT_DEVICE_KEYBOARD);
    if (m_touch.object)
        source = static_cast<WPEAvailableInputDevices>(source | WPE_AVAILABLE_INPUT_DEVICE_TOUCHSCREEN);
    return source;
}

} // namespace WPE
