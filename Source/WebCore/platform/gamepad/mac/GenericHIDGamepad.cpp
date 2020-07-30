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
#include "GenericHIDGamepad.h"

#if ENABLE(GAMEPAD) && PLATFORM(MAC)

#include <IOKit/hid/IOHIDUsageTables.h>
#include <wtf/HexNumber.h>

namespace WebCore {

GenericHIDGamepad::GenericHIDGamepad(HIDDevice&& device, unsigned index)
    : HIDGamepad(WTFMove(device), index)
{
    auto inputElements = hidDevice().uniqueInputElementsInDeviceTreeOrder();

    for (auto& element : inputElements) {
        switch (element.usagePage()) {
        case kHIDPage_GenericDesktop:
            maybeAddGenericDesktopElement(element);
            continue;
        case kHIDPage_Button:
            maybeAddButtonElement(element);
            continue;
        default:
            continue;
        }
    }
}

String GenericHIDGamepad::id()
{
    // Currently the spec has no formatting for the id string.
    // This string formatting matches Firefox.
    return makeString(hex(hidDevice().vendorID(), Lowercase), '-', hex(hidDevice().productID(), Lowercase), '-', hidDevice().productName());
}

void GenericHIDGamepad::maybeAddGenericDesktopElement(HIDElement& element)
{
    switch (element.usage()) {
    case kHIDUsage_GD_X:
    case kHIDUsage_GD_Y:
    case kHIDUsage_GD_Z:
    case kHIDUsage_GD_Rx:
    case kHIDUsage_GD_Ry:
    case kHIDUsage_GD_Rz:
        m_axisValues.append(0.0);
        m_elementMap.set(element.cookie(), makeUnique<HIDGamepadAxis>(element, m_axisValues.last()));
        break;
    case kHIDUsage_GD_DPadUp:
    case kHIDUsage_GD_DPadDown:
    case kHIDUsage_GD_DPadRight:
    case kHIDUsage_GD_DPadLeft:
        m_buttonValues.append(0.0);
        m_elementMap.set(element.cookie(), makeUnique<HIDGamepadButton>(element, m_buttonValues.last()));
        break;
    default:
        break;
    }
}

void GenericHIDGamepad::maybeAddButtonElement(HIDElement& element)
{
    // If it's in the button page, we assume it's actually a button.
    m_buttonValues.append(0.0);
    m_elementMap.set(element.cookie(), makeUnique<HIDGamepadButton>(element, m_buttonValues.last()));
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && PLATFORM(MAC)
