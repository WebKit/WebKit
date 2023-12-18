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

#pragma once

#include "WPEEvent.h"
#include "WPEKeymap.h"
#include "WPEView.h"
#include <wayland-client.h>
#include <wtf/FastMalloc.h>
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>
#include <wtf/glib/GRefPtr.h>

namespace WPE {

class WaylandSeat {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit WaylandSeat(struct wl_seat*);
    ~WaylandSeat();

    struct wl_seat* seat() const { return m_seat; }
    WPEKeymap* keymap() const { return m_keymap.get(); }

    void startListening();

    void setCursor(struct wl_surface*, int32_t, int32_t);

private:
    static const struct wl_seat_listener s_listener;
    static const struct wl_pointer_listener s_pointerListener;
    static const struct wl_keyboard_listener s_keyboardListener;
    static const struct wl_touch_listener s_touchListener;

    void updateCursor();
    WPEModifiers modifiers() const;
    void flushScrollEvent();
    void handleKeyEvent(uint32_t time, uint32_t key, uint32_t state, bool fromRepeat);
    bool keyRepeat(Seconds& delay, Seconds& interval);

    struct wl_seat* m_seat { nullptr };
    GRefPtr<WPEKeymap> m_keymap;
    struct {
        struct wl_pointer* object { nullptr };
        WPEInputSource source { WPE_INPUT_SOURCE_MOUSE };
        GRefPtr<WPEView> view;
        double x { 0 };
        double y { 0 };
        uint32_t modifiers { 0 };
        uint32_t time { 0 };
        uint32_t enterSerial { 0 };

        struct {
            WPEEvent* event { nullptr };
            double deltaX { 0 };
            double deltaY { 0 };
            int32_t valueX { 0 };
            int32_t valueY { 0 };
            bool isStop { false };
            WPEInputSource source { WPE_INPUT_SOURCE_MOUSE };

        } frame;
    } m_pointer;
    struct {
        struct wl_keyboard* object { nullptr };
        WPEInputSource source { WPE_INPUT_SOURCE_KEYBOARD };
        GRefPtr<WPEView> view;
        uint32_t modifiers { 0 };
        uint32_t time { 0 };

        struct {
            std::optional<int32_t> rate;
            std::optional<int32_t> delay;

            uint32_t key { 0 };
            GRefPtr<GSource> source;
            Seconds deadline;
        } repeat;

        struct {
            uint32_t key { 0 };
            unsigned keyval { 0 };
            uint32_t modifiers { 0 };
            uint32_t time { 0 };
        } capsLockUpEvent;
    } m_keyboard;
    struct {
        struct wl_touch* object { nullptr };
        WPEInputSource source { WPE_INPUT_SOURCE_TOUCHSCREEN };
        GRefPtr<WPEView> view;
        HashMap<int32_t, std::pair<double, double>> points;
    } m_touch;
};

} // namespace WPE
