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
#include "PlatformGamepadWPE.h"

#include "GamepadProviderWPE.h"

#if ENABLE(GAMEPAD) && ENABLE(WPE_PLATFORM)
#include <WebCore/SharedGamepadValue.h>
#include <wpe/wpe-platform.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PlatformGamepadWPE);

PlatformGamepadWPE::PlatformGamepadWPE(WPEGamepad* gamepad, unsigned index)
    : PlatformGamepad(index)
    , m_gamepad(gamepad)
    , m_effectDelayTimer(RunLoop::currentSingleton(), "PlatformGamepadWPE::EffectDelayTimer"_s, this, &PlatformGamepadWPE::effectDelayTimerFired)
    , m_effectDurationTimer(RunLoop::currentSingleton(), "PlatformGamepadWPE::EffectDurationTimer"_s, this, &PlatformGamepadWPE::effectDurationTimerFired)
{
    m_connectTime = m_lastUpdateTime = MonotonicTime::now();

    m_id = String::fromUTF8(wpe_gamepad_get_name(m_gamepad.get()));
    m_mapping = "standard"_s;

    m_buttonValues.resize(WPE_GAMEPAD_BUTTON_CENTER_CLUSTER_CENTER + 1);
    for (auto& value : m_buttonValues)
        value.setValue(0.0);

    m_axisValues.resize(WPE_GAMEPAD_AXIS_RIGHT_Y + 1);
    for (auto& value : m_axisValues)
        value.setValue(0.0);

    if (wpe_gamepad_has_rumble(m_gamepad.get()))
        m_supportedEffectTypes.add(GamepadHapticEffectType::DualRumble);

    g_signal_connect_swapped(m_gamepad.get(), "button-event", G_CALLBACK(+[](PlatformGamepadWPE* gamepad, WPEGamepadButton button, gboolean isPressed) {
        gamepad->buttonEvent(static_cast<size_t>(button), isPressed);
    }), this);
    g_signal_connect_swapped(m_gamepad.get(), "axis-event", G_CALLBACK(+[](PlatformGamepadWPE* gamepad, WPEGamepadAxis axis, gdouble value) {
        gamepad->axisEvent(static_cast<size_t>(axis), value);
    }), this);
}

PlatformGamepadWPE::~PlatformGamepadWPE()
{
    g_signal_handlers_disconnect_by_data(m_gamepad.get(), this);
}

void PlatformGamepadWPE::buttonEvent(size_t button, bool isPressed)
{
    m_lastUpdateTime = MonotonicTime::now();
    m_buttonValues[button].setValue(isPressed ? 1.0 : 0.0);
    GamepadProviderWPE::singleton().notifyInput(*this, isPressed ? GamepadProviderWPE::ShouldMakeGamepadsVisible::Yes : GamepadProviderWPE::ShouldMakeGamepadsVisible::No);
}

void PlatformGamepadWPE::axisEvent(size_t axis, double value)
{
    m_lastUpdateTime = MonotonicTime::now();
    m_axisValues[axis].setValue(value);
    GamepadProviderWPE::singleton().notifyInput(*this, GamepadProviderWPE::ShouldMakeGamepadsVisible::No);
}

void PlatformGamepadWPE::playEffect(GamepadHapticEffectType type, const GamepadEffectParameters& parameters, CompletionHandler<void(bool)>&& completionHandler)
{
    if (!m_supportedEffectTypes.contains(type))
        return completionHandler(false);

    if (m_effectCompletionHandler)
        stopEffects({ });

    m_effectCompletionHandler = WTF::move(completionHandler);
    if (parameters.startDelay) {
        m_pendingEffectParameters = parameters;
        m_effectDelayTimer.startOneShot(Seconds::fromMilliseconds(parameters.startDelay));
        return;
    }

    startRumble(parameters);
}

void PlatformGamepadWPE::stopEffects(CompletionHandler<void()>&& completionHandler)
{
    m_effectDelayTimer.stop();
    m_effectDurationTimer.stop();
    if (m_effectCompletionHandler)
        m_effectCompletionHandler(false);

    wpe_gamepad_rumble(m_gamepad.get(), 0, 0, 0);

    if (completionHandler)
        completionHandler();
}

void PlatformGamepadWPE::effectDelayTimerFired()
{
    startRumble(std::exchange(m_pendingEffectParameters, { }));
}

void PlatformGamepadWPE::startRumble(const GamepadEffectParameters& parameters)
{
    wpe_gamepad_rumble(m_gamepad.get(), parameters.strongMagnitude, parameters.weakMagnitude, static_cast<guint>(parameters.duration));

    if (parameters.duration)
        m_effectDurationTimer.startOneShot(Seconds::fromMilliseconds(parameters.duration));
    else
        m_effectCompletionHandler(true);
}

void PlatformGamepadWPE::effectDurationTimerFired()
{
    m_effectCompletionHandler(true);
}

} // namespace WebKit

#endif // ENABLE(GAMEPAD) && ENABLE(WPE_PLATFORM)
