/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#include "HIDGamepad.h"

#if ENABLE(GAMEPAD) && PLATFORM(MAC)

#include "GenericHIDGamepad.h"
#include "Logging.h"
#include <IOKit/hid/IOHIDElement.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDValue.h>
#include <wtf/cf/TypeCastsCF.h>
#include <wtf/text/CString.h>

namespace WebCore {

std::unique_ptr<HIDGamepad> HIDGamepad::create(IOHIDDeviceRef rawDevice, unsigned index)
{
    auto device = HIDDevice { rawDevice };

    // When we support specific mapping for a particular device, here is where we'll decide what kind of HIDGamepad to make.
    // For now, it's only the GenericHIDGamepad
    auto newGamepad = makeUnique<GenericHIDGamepad>(WTFMove(device), index);

    newGamepad->initialize();
    return WTFMove(newGamepad);
}

HIDGamepad::HIDGamepad(HIDDevice&& device, unsigned index)
    : PlatformGamepad(index)
    , m_device(WTFMove(device))
{
    m_connectTime = m_lastUpdateTime = MonotonicTime::now();
}

void HIDGamepad::initialize()
{
    m_id = id();

    for (auto& element : m_elementMap.values())
        element->refreshCurrentValue();
}

HIDInputType HIDGamepad::valueChanged(IOHIDValueRef value)
{
    IOHIDElementCookie cookie = IOHIDElementGetCookie(IOHIDValueGetElement(value));
    HIDGamepadElement* element = m_elementMap.get(cookie);

    // This might be an element we don't currently handle as input so we can skip it.
    if (!element)
        return HIDInputType::NotAButtonPress;

    m_lastUpdateTime = MonotonicTime::now();

    return element->gamepadValueChanged(value);
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && PLATFORM(MAC)
