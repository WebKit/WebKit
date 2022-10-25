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

#pragma once

#if ENABLE(GAMEPAD) && USE(LIBWPE)

#include "PlatformGamepad.h"

struct wpe_gamepad;
struct wpe_gamepad_provider;

namespace WebCore {

class GamepadLibWPE final : public PlatformGamepad {
public:
    GamepadLibWPE(struct wpe_gamepad_provider*, uintptr_t, unsigned);
    virtual ~GamepadLibWPE();

    const Vector<SharedGamepadValue>& axisValues() const final { return m_axisValues; }
    const Vector<SharedGamepadValue>& buttonValues() const final { return m_buttonValues; }

    const struct wpe_gamepad* wpeGamepad() const { return m_gamepad.get(); }

private:
    void buttonPressedOrReleased(unsigned, bool);
    void absoluteAxisChanged(unsigned, double);

    Vector<SharedGamepadValue> m_buttonValues;
    Vector<SharedGamepadValue> m_axisValues;

    std::unique_ptr<struct wpe_gamepad, void(*)(struct wpe_gamepad*)> m_gamepad;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && USE(LIBWPE)
