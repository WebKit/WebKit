/*
 * Copyright (C) 2020 Igalia S.L. All rights reserved.
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

#if ENABLE(GAMEPAD) && OS(LINUX)

#include "GamepadEffectParameters.h"
#include "PlatformGamepad.h"

#include <libmanette.h>
#include <wtf/RunLoop.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/glib/GRefPtr.h>

namespace WebCore {

class ManetteGamepad final : public PlatformGamepad {
    WTF_MAKE_TZONE_ALLOCATED(ManetteGamepad);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(ManetteGamepad);
public:
    // Refer https://www.w3.org/TR/gamepad/#gamepadbutton-interface
    enum class StandardGamepadAxis : int8_t {
        Unknown = -1,
        LeftStickX,
        LeftStickY,
        RightStickX,
        RightStickY,
    };
    enum class StandardGamepadButton : int8_t {
        Unknown = -1,
        A,
        B,
        X,
        Y,
        LeftShoulder,
        RightShoulder,
        LeftTrigger,
        RightTrigger,
        Select,
        Start,
        LeftStick,
        RightStick,
        DPadUp,
        DPadDown,
        DPadLeft,
        DPadRight,
        Mode,
    };

    ManetteGamepad(ManetteDevice*, unsigned index);
    virtual ~ManetteGamepad();

    void absoluteAxisChanged(ManetteDevice*, StandardGamepadAxis, double value);
    void buttonPressedOrReleased(ManetteDevice*, StandardGamepadButton, bool pressed);

private:
    const Vector<SharedGamepadValue>& axisValues() const LIFETIME_BOUND final { return m_axisValues; }
    const Vector<SharedGamepadValue>& buttonValues() const LIFETIME_BOUND final { return m_buttonValues; }
    void playEffect(GamepadHapticEffectType, const GamepadEffectParameters&, CompletionHandler<void(bool)>&&) final;
    void stopEffects(CompletionHandler<void()>&&) final;
    void effectDelayTimerFired();
    void effectDurationTimerFired();
    void startRumble(const GamepadEffectParameters&);

    GRefPtr<ManetteDevice> m_device;

    Vector<SharedGamepadValue> m_buttonValues;
    Vector<SharedGamepadValue> m_axisValues;
    RunLoop::Timer m_effectDelayTimer;
    RunLoop::Timer m_effectDurationTimer;
    CompletionHandler<void(bool)> m_effectCompletionHandler;
    GamepadEffectParameters m_pendingEffectParameters;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && OS(LINUX)
