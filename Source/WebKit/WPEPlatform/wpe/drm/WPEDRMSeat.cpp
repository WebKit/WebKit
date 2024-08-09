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
#include "WPEDRMSeat.h"

#include "WPEDRMSession.h"
#include "WPEKeymapXKB.h"
#include "WPEViewDRMPrivate.h"
#include <linux/input.h>
#include <wtf/MonotonicTime.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WPE {

namespace DRM {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Seat);

static struct libinput_interface s_libinputInterface = {
    // open_restricted
    [](const char* path, int flags, void* userData) -> int
    {
        auto& session = *static_cast<Session*>(userData);
        return session.openDevice(path, flags);
    },
    // close_restricted
    [](int fd, void* userData)
    {
        auto& session = *static_cast<Session*>(userData);
        session.closeDevice(fd);
    }
};

std::unique_ptr<Seat> Seat::create(struct udev* udev, Session& session)
{
    auto* libinput = libinput_udev_create_context(&s_libinputInterface, &session, udev);
    if (!libinput)
        return nullptr;

    if (libinput_udev_assign_seat(libinput, session.seatID()) == -1) {
        libinput_unref(libinput);
        return nullptr;
    }

    return makeUnique<Seat>(libinput);
}

struct EventSource {
    static GSourceFuncs sourceFuncs;

    GSource source;
    GPollFD pfd;
    struct libinput* libinput;
};

GSourceFuncs EventSource::sourceFuncs = {
    // prepare
    [](GSource* base, gint* timeout) -> gboolean
    {
        *timeout = -1;
        auto* source = reinterpret_cast<EventSource*>(base);
        return libinput_next_event_type(source->libinput) != LIBINPUT_EVENT_NONE;
    },
    // check
    [](GSource* base) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        return !!(source->pfd.revents & G_IO_IN);
    },
    // dispatch
    [](GSource* base, GSourceFunc callback, gpointer userData) -> gboolean
    {
        auto* source = reinterpret_cast<EventSource*>(base);
        if (libinput_dispatch(source->libinput))
            return G_SOURCE_REMOVE;
        return callback(userData);
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

Seat::Seat(struct libinput* libinput)
    : m_libinput(libinput)
    , m_keymap(adoptGRef(wpe_keymap_xkb_new()))
{
    // FIXME: consider moving input handling to a separate thread.
    m_inputSource = adoptGRef(g_source_new(&EventSource::sourceFuncs, sizeof(EventSource)));
    auto& eventSource = *reinterpret_cast<EventSource*>(m_inputSource.get());
    eventSource.libinput = m_libinput;
    eventSource.pfd.fd = libinput_get_fd(m_libinput);
    eventSource.pfd.events = G_IO_IN;
    eventSource.pfd.revents = 0;
    g_source_add_poll(&eventSource.source, &eventSource.pfd);
    g_source_set_priority(&eventSource.source, G_PRIORITY_DEFAULT);
    g_source_set_can_recurse(&eventSource.source, TRUE);
    g_source_set_callback(&eventSource.source, [](gpointer userData) -> gboolean {
        auto& seat = *static_cast<Seat*>(userData);
        seat.processEvents();
        return G_SOURCE_CONTINUE;
    }, this, nullptr);
    g_source_attach(&eventSource.source, g_main_context_get_thread_default());
    processEvents();
}

Seat::~Seat()
{
    if (m_keyboard.repeat.source)
        g_source_destroy(m_keyboard.repeat.source.get());
    if (m_inputSource)
        g_source_destroy(m_inputSource.get());
    libinput_unref(m_libinput);
}

void Seat::setView(WPEView* view)
{
    if (m_view)
        wpe_view_focus_out(m_view.get());
    m_view.reset(view);
    if (m_view)
        wpe_view_focus_in(m_view.get());
}

WPEModifiers Seat::modifiers() const
{
    uint32_t mask = m_keyboard.modifiers;
    if (m_pointer.modifiers)
        mask |= m_pointer.modifiers;
    return static_cast<WPEModifiers>(mask);
}

void Seat::processEvents()
{
    while (auto* event = libinput_get_event(m_libinput)) {
        processEvent(event);
        libinput_event_destroy(event);
    }
}

void Seat::processEvent(struct libinput_event* event)
{
    switch (libinput_event_get_type(event)) {
    case LIBINPUT_EVENT_POINTER_MOTION:
        handlePointerMotionEvent(libinput_event_get_pointer_event(event));
        break;
    case LIBINPUT_EVENT_POINTER_BUTTON:
        handlePointerButtonEvent(libinput_event_get_pointer_event(event));
        break;
    case LIBINPUT_EVENT_POINTER_SCROLL_WHEEL:
        handlePointerScrollWheelEvent(libinput_event_get_pointer_event(event));
        break;
    case LIBINPUT_EVENT_POINTER_SCROLL_FINGER:
        handlePointerScrollContinuousEvent(libinput_event_get_pointer_event(event), WPE_INPUT_SOURCE_TOUCHPAD);
        break;
    case LIBINPUT_EVENT_POINTER_SCROLL_CONTINUOUS:
        handlePointerScrollContinuousEvent(libinput_event_get_pointer_event(event), WPE_INPUT_SOURCE_TRACKPOINT);
        break;
    case LIBINPUT_EVENT_KEYBOARD_KEY:
        handleKeyEvent(libinput_event_get_keyboard_event(event));
        break;
    default:
        break;
    }
}

void Seat::handlePointerMotionEvent(struct libinput_event_pointer* event)
{
    if (!m_view)
        return;

    m_pointer.time = libinput_event_pointer_get_time(event);

    auto deltaX = libinput_event_pointer_get_dx(event);
    auto deltaY = libinput_event_pointer_get_dy(event);
    auto scale = wpe_view_get_scale(m_view.get());
    double x = std::clamp<double>(m_pointer.x + deltaX, 0, wpe_view_get_width(m_view.get()) * scale);
    double y = std::clamp<double>(m_pointer.y + deltaY, 0, wpe_view_get_height(m_view.get()) * scale);
    if (x == m_pointer.x && y == m_pointer.y)
        return;

    m_pointer.x = x;
    m_pointer.y = y;
    wpeViewDRMUpdateCursor(WPE_VIEW_DRM(m_view.get()), m_pointer.x, m_pointer.y);

    auto* wpeEvent = wpe_event_pointer_move_new(WPE_EVENT_POINTER_MOVE, m_view.get(), m_pointer.source, m_pointer.time, modifiers(),
        m_pointer.x / scale, m_pointer.y / scale, deltaX / scale, deltaY / scale);
    wpe_view_event(m_view.get(), wpeEvent);
    wpe_event_unref(wpeEvent);
}

void Seat::handlePointerButtonEvent(struct libinput_event_pointer* event)
{
    if (!m_view)
        return;

    uint32_t button = libinput_event_pointer_get_button(event);
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

    auto state = libinput_event_pointer_get_button_state(event);
    switch (state) {
    case LIBINPUT_BUTTON_STATE_PRESSED:
        m_pointer.modifiers |= buttonModifiers;
        break;
    case LIBINPUT_BUTTON_STATE_RELEASED:
        m_pointer.modifiers &= ~buttonModifiers;
    }

    m_pointer.time = libinput_event_pointer_get_time(event);

    auto scale = wpe_view_get_scale(m_view.get());
    unsigned pressCount = state == LIBINPUT_BUTTON_STATE_PRESSED ? wpe_view_compute_press_count(m_view.get(), m_pointer.x / scale, m_pointer.y / scale, button, m_pointer.time) : 0;
    auto* wpeEvent = wpe_event_pointer_button_new(state == LIBINPUT_BUTTON_STATE_PRESSED ? WPE_EVENT_POINTER_DOWN : WPE_EVENT_POINTER_UP, m_view.get(), m_pointer.source,
        m_pointer.time, modifiers(), button, m_pointer.x / scale, m_pointer.y / scale, pressCount);
    wpe_view_event(m_view.get(), wpeEvent);
    wpe_event_unref(wpeEvent);
}

void Seat::handlePointerScrollWheelEvent(struct libinput_event_pointer* event)
{
    if (!m_view)
        return;

    double valueX = 0;
    if (libinput_event_pointer_has_axis(event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL))
        valueX = libinput_event_pointer_get_scroll_value_v120(event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
    double valueY = 0;
    if (libinput_event_pointer_has_axis(event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL))
        valueY = libinput_event_pointer_get_scroll_value_v120(event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);

    if (!valueX && !valueY)
        return;

    m_pointer.time = libinput_event_pointer_get_time(event);

    auto scale = wpe_view_get_scale(m_view.get());
    auto* wpeEvent = wpe_event_scroll_new(m_view.get(), WPE_INPUT_SOURCE_MOUSE, m_pointer.time, modifiers(), -valueX / 120, -valueY / 120,
        FALSE, FALSE, m_pointer.x / scale, m_pointer.y / scale);
    wpe_view_event(m_view.get(), wpeEvent);
    wpe_event_unref(wpeEvent);
}

void Seat::handlePointerScrollContinuousEvent(struct libinput_event_pointer* event, WPEInputSource source)
{
    if (!m_view)
        return;

    double deltaX = 0;
    bool finishedHorizontal = false;
    if (libinput_event_pointer_has_axis(event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL)) {
        deltaX = libinput_event_pointer_get_scroll_value(event, LIBINPUT_POINTER_AXIS_SCROLL_HORIZONTAL);
        if (std::fabs(deltaX) < std::numeric_limits<double>::epsilon()) {
            finishedHorizontal = true;
            deltaX = 0;
        }
    }
    double deltaY = 0;
    bool finishedVertical = false;
    if (libinput_event_pointer_has_axis(event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL)) {
        deltaY = libinput_event_pointer_get_scroll_value(event, LIBINPUT_POINTER_AXIS_SCROLL_VERTICAL);
        if (std::fabs(deltaY) < std::numeric_limits<double>::epsilon()) {
            finishedVertical = true;
            deltaY = 0;
        }
    }

    m_pointer.time = libinput_event_pointer_get_time(event);

    auto scale = wpe_view_get_scale(m_view.get());
    auto* wpeEvent = wpe_event_scroll_new(m_view.get(), source, m_pointer.time, modifiers(), deltaX, deltaY,
        TRUE, finishedHorizontal && finishedVertical, m_pointer.x / scale, m_pointer.y / scale);
    wpe_view_event(m_view.get(), wpeEvent);
    wpe_event_unref(wpeEvent);
}

void Seat::handleKeyEvent(struct libinput_event_keyboard* event)
{
    auto keyState = libinput_event_keyboard_get_key_state(event);
    auto seatKeyCount = libinput_event_keyboard_get_seat_key_count(event);
    if ((keyState == LIBINPUT_KEY_STATE_PRESSED && seatKeyCount != 1) || (keyState == LIBINPUT_KEY_STATE_RELEASED && seatKeyCount))
        return;

    uint32_t key = libinput_event_keyboard_get_key(event) + 8;
    auto* xkbState = wpe_keymap_xkb_get_xkb_state(WPE_KEYMAP_XKB(m_keymap.get()));
    xkb_state_update_key(xkbState, key, keyState == LIBINPUT_KEY_STATE_PRESSED ? XKB_KEY_DOWN : XKB_KEY_UP);
    m_keyboard.modifiers = wpe_keymap_get_modifiers(m_keymap.get());

    if (m_view)
        handleKey(libinput_event_keyboard_get_time(event), key, keyState == LIBINPUT_KEY_STATE_PRESSED, false);
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

void Seat::handleKey(uint32_t time, uint32_t key, bool pressed, bool fromRepeat)
{
    auto beginTime = MonotonicTime::now().secondsSinceEpoch();

    auto* keymap = WPE_KEYMAP_XKB(m_keymap.get());
    auto* xkbState = wpe_keymap_xkb_get_xkb_state(keymap);
    unsigned keyval = xkb_state_key_get_one_sym(xkbState, key);
    if (keyval == XKB_KEY_NoSymbol)
        return;

    GRefPtr<WPEView> view = m_view.get();
    auto* event = wpe_event_keyboard_new(pressed ? WPE_EVENT_KEYBOARD_KEY_DOWN : WPE_EVENT_KEYBOARD_KEY_UP, view.get(), m_keyboard.source, time,
        modifiers(), key, keyval);
    wpe_view_event(view.get(), event);
    wpe_event_unref(event);

    auto* xkbKeymap = wpe_keymap_xkb_get_xkb_keymap(keymap);
    if (!xkb_keymap_key_repeats(xkbKeymap, key))
        return;

    if (!fromRepeat) {
        if (pressed)
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
        g_source_set_name(m_keyboard.repeat.source.get(), "WPE DRM key repeat timer");
        g_source_set_callback(m_keyboard.repeat.source.get(), [](gpointer userData) -> gboolean {
            auto& seat = *static_cast<Seat*>(userData);
            seat.handleKey(seat.m_keyboard.time, seat.m_keyboard.repeat.key, true, true);
            if (g_source_is_destroyed(seat.m_keyboard.repeat.source.get()))
                return G_SOURCE_REMOVE;
            return G_SOURCE_CONTINUE;
        }, this, nullptr);
        g_source_attach(m_keyboard.repeat.source.get(), g_main_context_get_thread_default());
    }

    // FIXME: make this configurable.
    static const Seconds delay = 400_ms;
    static const Seconds interval = 80_ms;

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

} // namespace DRM

} // namespace WPE
