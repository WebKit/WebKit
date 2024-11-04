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

#include "WPEKeymap.h"
#include "WPEView.h"
#include <libinput.h>
#include <wtf/HashMap.h>
#include <wtf/Seconds.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GWeakPtr.h>

struct udev;

namespace WPE {

namespace DRM {

class Session;

class Seat {
    WTF_MAKE_TZONE_ALLOCATED(Seat);
public:
    static std::unique_ptr<Seat> create(struct udev*, Session&);
    explicit Seat(struct libinput*);
    ~Seat();

    void setView(WPEView* view);

private:
    WPEModifiers modifiers() const;
    void processEvents();
    void processEvent(struct libinput_event*);
    void handlePointerMotionEvent(struct libinput_event_pointer*);
    void handlePointerButtonEvent(struct libinput_event_pointer*);
    void handlePointerScrollWheelEvent(struct libinput_event_pointer*);
    void handlePointerScrollContinuousEvent(struct libinput_event_pointer*, WPEInputSource);
    void handleKeyEvent(struct libinput_event_keyboard*);
    void handleKey(uint32_t time, uint32_t key, bool pressed, bool fromRepeat);
    void handleTouchDownEvent(struct libinput_event_touch*);
    void handleTouchUpEvent(struct libinput_event_touch*);
    void handleTouchMotionEvent(struct libinput_event_touch*);
    void handleTouchCancelEvent(struct libinput_event_touch*);

    struct libinput* m_libinput { nullptr };
    GRefPtr<GSource> m_inputSource;
    GRefPtr<WPEKeymap> m_keymap;
    GWeakPtr<WPEView> m_view;

    struct {
        WPEInputSource source { WPE_INPUT_SOURCE_MOUSE };
        double x { 0 };
        double y { 0 };
        uint32_t modifiers { 0 };
        uint32_t time { 0 };
    } m_pointer;

    struct {
        WPEInputSource source { WPE_INPUT_SOURCE_KEYBOARD };
        uint32_t modifiers { 0 };
        uint32_t time { 0 };

        struct {
	    uint32_t key { 0 };
            GRefPtr<GSource> source;
            Seconds deadline;
        } repeat;
    } m_keyboard;

    struct {
        WPEInputSource source { WPE_INPUT_SOURCE_TOUCHSCREEN };
        uint32_t time { 0 };
        HashMap<int32_t, std::pair<double, double>, IntHash<int32_t>, WTF::SignedWithZeroKeyHashTraits<int32_t>> points;
    } m_touch;
};

} // namespace DRM

} // namespace WPE
