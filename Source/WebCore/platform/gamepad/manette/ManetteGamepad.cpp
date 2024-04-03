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

#include "config.h"
#include "ManetteGamepad.h"

#if ENABLE(GAMEPAD) && OS(LINUX)

#include "ManetteGamepadProvider.h"
#include <linux/input-event-codes.h>
#include <wtf/HexNumber.h>
#include <wtf/text/CString.h>

namespace WebCore {

static ManetteGamepad::StandardGamepadAxis toStandardGamepadAxis(uint16_t axis)
{
    switch (axis) {
    case ABS_X:
        return ManetteGamepad::StandardGamepadAxis::LeftStickX;
    case ABS_Y:
        return ManetteGamepad::StandardGamepadAxis::LeftStickY;
    case ABS_RX:
        return ManetteGamepad::StandardGamepadAxis::RightStickX;
    case ABS_RY:
        return ManetteGamepad::StandardGamepadAxis::RightStickY;
    default:
        break;
    }
    return ManetteGamepad::StandardGamepadAxis::Unknown;
}

static void onAbsoluteAxisEvent(ManetteDevice* device, ManetteEvent* event, ManetteGamepad* gamepad)
{
    uint16_t axis;
    double value;
    if (!manette_event_get_absolute(event, &axis, &value))
        return;

    gamepad->absoluteAxisChanged(device, toStandardGamepadAxis(axis), value);
}

static ManetteGamepad::StandardGamepadButton toStandardGamepadButton(uint16_t manetteButton)
{
    switch (manetteButton) {
    case BTN_A:
        return ManetteGamepad::StandardGamepadButton::A;
    case BTN_B:
        return ManetteGamepad::StandardGamepadButton::B;
    case BTN_X:
        return ManetteGamepad::StandardGamepadButton::X;
    case BTN_Y:
        return ManetteGamepad::StandardGamepadButton::Y;
    case BTN_TL:
        return ManetteGamepad::StandardGamepadButton::LeftShoulder;
    case BTN_TR:
        return ManetteGamepad::StandardGamepadButton::RightShoulder;
    case BTN_TL2:
        return ManetteGamepad::StandardGamepadButton::LeftTrigger;
    case BTN_TR2:
        return ManetteGamepad::StandardGamepadButton::RightTrigger;
    case BTN_SELECT:
        return ManetteGamepad::StandardGamepadButton::Select;
    case BTN_START:
        return ManetteGamepad::StandardGamepadButton::Start;
    case BTN_THUMBL:
        return ManetteGamepad::StandardGamepadButton::LeftStick;
    case BTN_THUMBR:
        return ManetteGamepad::StandardGamepadButton::RightStick;
    case BTN_DPAD_UP:
        return ManetteGamepad::StandardGamepadButton::DPadUp;
    case BTN_DPAD_DOWN:
        return ManetteGamepad::StandardGamepadButton::DPadDown;
    case BTN_DPAD_LEFT:
        return ManetteGamepad::StandardGamepadButton::DPadLeft;
    case BTN_DPAD_RIGHT:
        return ManetteGamepad::StandardGamepadButton::DPadRight;
    case BTN_MODE:
        return ManetteGamepad::StandardGamepadButton::Mode;
    default:
        break;
    }
    return ManetteGamepad::StandardGamepadButton::Unknown;
}

static void onButtonPressEvent(ManetteDevice* device, ManetteEvent* event, ManetteGamepad* gamepad)
{
    uint16_t button;
    if (!manette_event_get_button(event, &button))
        return;

    gamepad->buttonPressedOrReleased(device, toStandardGamepadButton(button), true);
}

static void onButtonReleaseEvent(ManetteDevice* device, ManetteEvent* event, ManetteGamepad* gamepad)
{
    uint16_t button;
    if (!manette_event_get_button(event, &button))
        return;

    gamepad->buttonPressedOrReleased(device, toStandardGamepadButton(button), false);
}

ManetteGamepad::ManetteGamepad(ManetteDevice* device, unsigned index)
    : PlatformGamepad(index)
    , m_device(device)
{
    ASSERT(index < 4);

    m_connectTime = m_lastUpdateTime = MonotonicTime::now();

    m_id = String::fromUTF8(manette_device_get_name(m_device.get()));
    m_mapping = String::fromUTF8("standard");

    m_axisValues.resize(static_cast<size_t>(StandardGamepadAxis::Count));
    for (auto& value : m_axisValues)
        value.setValue(0.0);

    m_buttonValues.resize(static_cast<size_t>(StandardGamepadButton::Count));
    for (auto& value : m_buttonValues)
        value.setValue(0.0);

    g_signal_connect(device, "button-press-event", G_CALLBACK(onButtonPressEvent), this);
    g_signal_connect(device, "button-release-event", G_CALLBACK(onButtonReleaseEvent), this);
    g_signal_connect(device, "absolute-axis-event", G_CALLBACK(onAbsoluteAxisEvent), this);
}

ManetteGamepad::~ManetteGamepad()
{
    g_signal_handlers_disconnect_by_data(m_device.get(), this);
}

void ManetteGamepad::buttonPressedOrReleased(ManetteDevice*, StandardGamepadButton button, bool pressed)
{
    if (button == StandardGamepadButton::Unknown)
        return;

    m_lastUpdateTime = MonotonicTime::now();
    m_buttonValues[static_cast<int>(button)].setValue(pressed ? 1.0 : 0.0);

    ManetteGamepadProvider::singleton().gamepadHadInput(*this, pressed ? ManetteGamepadProvider::ShouldMakeGamepadsVisible::Yes : ManetteGamepadProvider::ShouldMakeGamepadsVisible::No);
}

void ManetteGamepad::absoluteAxisChanged(ManetteDevice*, StandardGamepadAxis axis, double value)
{
    if (axis == StandardGamepadAxis::Unknown)
        return;

    m_lastUpdateTime = MonotonicTime::now();
    m_axisValues[static_cast<int>(axis)].setValue(value);

    ManetteGamepadProvider::singleton().gamepadHadInput(*this, ManetteGamepadProvider::ShouldMakeGamepadsVisible::Yes);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && OS(LINUX)
