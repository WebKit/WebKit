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
#include "Dualshock3HIDGamepad.h"

#if ENABLE(GAMEPAD) && PLATFORM(MAC)

#include "GamepadConstants.h"
#include "GamepadConstantsMac.h"
#include "Logging.h"
#include <IOKit/hid/IOHIDUsageTables.h>
#include <wtf/HexNumber.h>

namespace WebCore {

Dualshock3HIDGamepad::Dualshock3HIDGamepad(HIDDevice&& device, unsigned index)
    : HIDGamepad(WTFMove(device), index)
{
    LOG(Gamepad, "Creating Dualshock3HIDGamepad %p", this);

    m_mapping = standardGamepadMappingString();

    for (size_t i = 0; i < numberOfStandardGamepadButtonsWithHomeButton; ++i)
        m_buttonValues.append(0.0);

    static const size_t axisCount = 4;
    for (size_t i = 0; i < axisCount; ++i)
        m_axisValues.append(0.0);

    auto inputElements = hidDevice().uniqueInputElementsInDeviceTreeOrder();

    Vector<HIDElement> pointerElements;

    auto mapButton = [this] (HIDElement& element, GamepadButtonRole role) {
        m_elementMap.set(element.cookie(), makeUnique<HIDGamepadButton>(element, m_buttonValues[(size_t)role]));
    };

    // Look specifically for the axes and digital buttons
    for (auto& element : inputElements) {
        switch (element.fullUsage()) {
        case hidPointerFullUsage:
            pointerElements.append(element);
            break;
        case hidXAxisFullUsage:
            m_elementMap.set(element.cookie(), makeUnique<HIDGamepadAxis>(element, m_axisValues[0]));
            break;
        case hidYAxisFullUsage:
            m_elementMap.set(element.cookie(), makeUnique<HIDGamepadAxis>(element, m_axisValues[1]));
            break;
        case hidZAxisFullUsage:
            m_elementMap.set(element.cookie(), makeUnique<HIDGamepadAxis>(element, m_axisValues[2]));
            break;
        case hidRzAxisFullUsage:
            m_elementMap.set(element.cookie(), makeUnique<HIDGamepadAxis>(element, m_axisValues[3]));
            break;
        case hidButton1FullUsage:
            mapButton(element, GamepadButtonRole::CenterClusterLeft);
            break;
        case hidButton2FullUsage:
            mapButton(element, GamepadButtonRole::LeftStick);
            break;
        case hidButton3FullUsage:
            mapButton(element, GamepadButtonRole::RightStick);
            break;
        case hidButton4FullUsage:
            mapButton(element, GamepadButtonRole::CenterClusterRight);
            break;
        case hidButton17FullUsage:
            mapButton(element, GamepadButtonRole::CenterClusterCenter);
            break;
        default:
            break;
        }
    }

    const size_t expectedGenericPointerElements = 40;
    if (pointerElements.size() != expectedGenericPointerElements) {
        LOG(Gamepad, "Dualshock3 controller was expected to have %lu generic pointer elements, has %lu instead", expectedGenericPointerElements, pointerElements.size());
        return;
    }

    mapButton(pointerElements[5], GamepadButtonRole::LeftClusterTop);
    mapButton(pointerElements[6], GamepadButtonRole::LeftClusterRight);
    mapButton(pointerElements[7], GamepadButtonRole::LeftClusterBottom);
    mapButton(pointerElements[8], GamepadButtonRole::LeftClusterLeft);
    mapButton(pointerElements[9], GamepadButtonRole::LeftShoulderBack);
    mapButton(pointerElements[10], GamepadButtonRole::RightShoulderBack);
    mapButton(pointerElements[11], GamepadButtonRole::LeftShoulderFront);
    mapButton(pointerElements[12], GamepadButtonRole::RightShoulderFront);
    mapButton(pointerElements[13], GamepadButtonRole::RightClusterTop);
    mapButton(pointerElements[14], GamepadButtonRole::RightClusterRight);
    mapButton(pointerElements[15], GamepadButtonRole::RightClusterBottom);
    mapButton(pointerElements[16], GamepadButtonRole::RightClusterLeft);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && PLATFORM(MAC)
