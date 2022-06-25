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
#include "StadiaHIDGamepad.h"

#if ENABLE(GAMEPAD) && PLATFORM(MAC)

#include "GamepadConstants.h"
#include "GamepadConstantsMac.h"
#include "Logging.h"

namespace WebCore {

StadiaHIDGamepad::StadiaHIDGamepad(HIDDevice&& device, unsigned index)
    : HIDGamepad(WTFMove(device), index)
{
    LOG(Gamepad, "Creating StadiaHIDGamepad %p", this);

    m_mapping = standardGamepadMappingString();

    m_buttonValues = Vector(19, SharedGamepadValue { 0.0 });

    constexpr size_t axisCount = 4;
    m_axisValues = Vector(axisCount, SharedGamepadValue { 0.0 });

    auto inputElements = hidDevice().uniqueInputElementsInDeviceTreeOrder();

    auto mapButton = [this] (HIDElement& element, GamepadButtonRole role) {
        m_elementMap.set(element.cookie(), makeUnique<HIDGamepadButton>(element, m_buttonValues[(size_t)role]));
    };

    for (auto& element : inputElements) {
        switch (element.fullUsage()) {
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

        case hidHatswitchFullUsage: {
            auto hatswitchValues = Vector {
                m_buttonValues[(size_t)GamepadButtonRole::LeftClusterTop],
                m_buttonValues[(size_t)GamepadButtonRole::LeftClusterRight],
                m_buttonValues[(size_t)GamepadButtonRole::LeftClusterBottom],
                m_buttonValues[(size_t)GamepadButtonRole::LeftClusterLeft]
            };

            m_elementMap.set(element.cookie(), makeUnique<HIDGamepadHatswitch>(element, WTFMove(hatswitchValues)));
            break;
        }

        case hidAcceleratorFullUsage:
            mapButton(element, GamepadButtonRole::RightShoulderBack);
            break;
        case hidBrakeFullUsage:
            mapButton(element, GamepadButtonRole::LeftShoulderBack);
            break;

        case hidButton1FullUsage:
            mapButton(element, GamepadButtonRole::RightClusterBottom);
            break;
        case hidButton2FullUsage:
            mapButton(element, GamepadButtonRole::RightClusterRight);
            break;
        case hidButton4FullUsage:
            mapButton(element, GamepadButtonRole::RightClusterLeft);
            break;
        case hidButton5FullUsage:
            mapButton(element, GamepadButtonRole::RightClusterTop);
            break;
        case hidButton7FullUsage:
            mapButton(element, GamepadButtonRole::LeftShoulderFront);
            break;
        case hidButton8FullUsage:
            mapButton(element, GamepadButtonRole::RightShoulderFront);
            break;
        case hidButton11FullUsage:
            mapButton(element, GamepadButtonRole::CenterClusterLeft);
            break;
        case hidButton12FullUsage:
            mapButton(element, GamepadButtonRole::CenterClusterRight);
            break;
        case hidButton13FullUsage:
            mapButton(element, GamepadButtonRole::CenterClusterCenter);
            break;
        case hidButton14FullUsage:
            mapButton(element, GamepadButtonRole::LeftStick);
            break;
        case hidButton15FullUsage:
            mapButton(element, GamepadButtonRole::RightStick);
            break;
        case hidButton17FullUsage:
            mapButton(element, GamepadButtonRole::Nonstandard1);
            break;
        case hidButton18FullUsage:
            mapButton(element, GamepadButtonRole::Nonstandard2);
            break;
        default:
            break;
        }
    }
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && PLATFORM(MAC)
