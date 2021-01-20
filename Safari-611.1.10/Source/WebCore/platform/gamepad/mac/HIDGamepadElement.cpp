/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "HIDGamepadElement.h"

#if ENABLE(GAMEPAD) && PLATFORM(MAC)

namespace WebCore {

#pragma mark HIDGamepadElement

HIDGamepadElement::HIDGamepadElement(const HIDElement& element)
    : HIDElement(element)
{
}

void HIDGamepadElement::refreshCurrentValue()
{
    IOHIDValueRef value;
    if (IOHIDDeviceGetValue(IOHIDElementGetDevice(rawElement()), rawElement(), &value) == kIOReturnSuccess)
        gamepadValueChanged(value);
}

double HIDGamepadElement::normalizedValue()
{
    // Default normalization (and the normalization buttons will use) is 0 to 1.0
    return (double)(physicalValue() - physicalMin()) / (double)(physicalMax() - physicalMin());
}

#pragma mark HIDGamepadButton

HIDInputType HIDGamepadButton::gamepadValueChanged(IOHIDValueRef value)
{
    valueChanged(value);
    m_value.setValue(normalizedValue());
    return m_value.value() > 0.5 ? HIDInputType::ButtonPress : HIDInputType::NotAButtonPress;
}

#pragma mark HIDGamepadAxis

HIDInputType HIDGamepadAxis::gamepadValueChanged(IOHIDValueRef value)
{
    valueChanged(value);
    m_value.setValue(normalizedValue());
    return HIDInputType::NotAButtonPress;
}

double HIDGamepadAxis::normalizedValue()
{
    // Web Gamepad axes have a value of -1.0 to 1.0
    return (HIDGamepadElement::normalizedValue() * 2.0) - 1.0;
}

#pragma mark HIDGamepadHatswitch

HIDInputType HIDGamepadHatswitch::gamepadValueChanged(IOHIDValueRef value)
{
    valueChanged(value);

    for (size_t i = 0; i < 4; ++i)
        m_buttonValues[i].setValue(0.0);

    switch (physicalValue()) {
    case 0:
        m_buttonValues[0].setValue(1.0);
        break;
    case 45:
        m_buttonValues[0].setValue(1.0);
        m_buttonValues[1].setValue(1.0);
        break;
    case 90:
        m_buttonValues[1].setValue(1.0);
        break;
    case 135:
        m_buttonValues[1].setValue(1.0);
        m_buttonValues[2].setValue(1.0);
        break;
    case 180:
        m_buttonValues[2].setValue(1.0);
        break;
    case 225:
        m_buttonValues[2].setValue(1.0);
        m_buttonValues[3].setValue(1.0);
        break;
    case 270:
        m_buttonValues[3].setValue(1.0);
        break;
    case 315:
        m_buttonValues[3].setValue(1.0);
        m_buttonValues[0].setValue(1.0);
        break;
    default:
        break;
    };

    return HIDInputType::ButtonPress;
}

double HIDGamepadHatswitch::normalizedValue()
{
    // Hatswitch normalizedValue should never be accessed.
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && PLATFORM(MAC)
