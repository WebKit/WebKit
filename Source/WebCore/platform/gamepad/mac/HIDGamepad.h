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

#pragma once

#if ENABLE(GAMEPAD) && PLATFORM(MAC)

#include "PlatformGamepad.h"
#include <IOKit/hid/IOHIDDevice.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

struct HIDGamepadElement {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;
    HIDGamepadElement(double theMin, double theMax, IOHIDElementRef element)
        : min(theMin)
        , max(theMax)
        , rawValue(theMin)
        , iohidElement(element)
    {
    }
    
    virtual ~HIDGamepadElement()
    {
    }

    double min;
    double max;
    double rawValue;
    RetainPtr<IOHIDElementRef> iohidElement;

    virtual bool isButton() const { return false; }
    virtual bool isAxis() const { return false; }

    virtual double normalizedValue() = 0;
};

struct HIDGamepadButton : HIDGamepadElement {
    HIDGamepadButton(uint32_t thePriority, double min, double max, IOHIDElementRef element)
        : HIDGamepadElement(min, max, element)
        , priority(thePriority)
    {
    }

    uint32_t priority;

    bool isButton() const final { return true; }

    // Buttons normalize to the range (0.0) - (1.0)
    double normalizedValue() override
    {
        return (rawValue - min) / (max - min);
    }
};

struct HIDGamepadAxis : HIDGamepadElement {
    HIDGamepadAxis(double min, double max, IOHIDElementRef element)
        : HIDGamepadElement(min, max, element)
    {
    }

    bool isAxis() const final { return true; }

    // Axes normalize to the range (-1.0) - (1.0)
    double normalizedValue() override
    {
        return (((rawValue - min) / (max - min)) * 2) - 1;
    }
};

enum class HIDInputType {
    ButtonPress,
    NotAButtonPress,
};

class HIDGamepad : public PlatformGamepad {
public:
    HIDGamepad(IOHIDDeviceRef, unsigned index);

    IOHIDDeviceRef hidDevice() const { return m_hidDevice.get(); }

    HIDInputType valueChanged(IOHIDValueRef);

    const Vector<double>& axisValues() const final { return m_axisValues; }
    const Vector<double>& buttonValues() const final { return m_buttonValues; }

    const char* source() const final { return "HID"_s; }

private:
    void initElements();
    void initElementsFromArray(CFArrayRef);

    bool maybeAddButton(IOHIDElementRef);
    bool maybeAddAxis(IOHIDElementRef);

    void getCurrentValueForElement(const HIDGamepadElement&);

    RetainPtr<IOHIDDeviceRef> m_hidDevice;

    HashMap<IOHIDElementCookie, HIDGamepadElement*> m_elementMap;

    Vector<UniqueRef<HIDGamepadButton>> m_buttons;
    Vector<UniqueRef<HIDGamepadAxis>> m_axes;
    Vector<double> m_buttonValues;
    Vector<double> m_axisValues;
};

} // namespace WebCore

#endif // ENABLE(GAMEPAD) && PLATFORM(MAC)
