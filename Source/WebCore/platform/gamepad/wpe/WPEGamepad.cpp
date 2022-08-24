/*
 * Copyright (C) 2020 RDK Management  All rights reserved.
 * Copyright (C) 2022 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEGamepad.h"

#if ENABLE(GAMEPAD)

#include "WPEGamepadProvider.h"
#include <wpe/wpe.h>

namespace WebCore {

WPEGamepad::WPEGamepad(struct wpe_gamepad_provider* provider, uintptr_t gamepadId, unsigned index)
    : PlatformGamepad(index)
    , m_buttonValues(WPE_GAMEPAD_BUTTON_COUNT)
    , m_axisValues(WPE_GAMEPAD_AXIS_COUNT)
    , m_gamepad(wpe_gamepad_create(provider, gamepadId), wpe_gamepad_destroy)
{
    ASSERT(m_gamepad);

    m_connectTime = m_lastUpdateTime = MonotonicTime::now();

    m_id = String::fromUTF8(wpe_gamepad_get_id(m_gamepad.get()));
    m_mapping = String::fromUTF8("standard");

    static const struct wpe_gamepad_client_interface s_client = {
        // button_event
        [](void* data, enum wpe_gamepad_button button, bool pressed) {
            auto& self = *static_cast<WPEGamepad*>(data);
            self.buttonPressedOrReleased(static_cast<unsigned>(button), pressed);
        },
        // axis_event
        [](void* data, enum wpe_gamepad_axis axis, double value) {
            auto& self = *static_cast<WPEGamepad*>(data);
            self.absoluteAxisChanged(static_cast<unsigned>(axis), value);
        },
        nullptr, nullptr, nullptr,
    };
    wpe_gamepad_set_client(m_gamepad.get(), &s_client, this);
}

WPEGamepad::~WPEGamepad()
{
    wpe_gamepad_set_client(m_gamepad.get(), nullptr, nullptr);
}

void WPEGamepad::buttonPressedOrReleased(unsigned button, bool pressed)
{
    m_lastUpdateTime = MonotonicTime::now();
    m_buttonValues[button].setValue(pressed ? 1.0 : 0.0);

    WPEGamepadProvider::singleton().scheduleInputNotification(*this, pressed ? WPEGamepadProvider::ShouldMakeGamepadsVisible::Yes : WPEGamepadProvider::ShouldMakeGamepadsVisible::No);
}

void WPEGamepad::absoluteAxisChanged(unsigned axis, double value)
{
    m_lastUpdateTime = MonotonicTime::now();
    m_axisValues[axis].setValue(value);

    WPEGamepadProvider::singleton().scheduleInputNotification(*this, WPEGamepadProvider::ShouldMakeGamepadsVisible::No);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
