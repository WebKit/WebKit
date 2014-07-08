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

#if ENABLE(GAMEPAD)

#include <IOKit/hid/IOHIDElement.h>
#include <IOKit/hid/IOHIDUsageTables.h>
#include <IOKit/hid/IOHIDValue.h>
#include <wtf/CurrentTime.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

HIDGamepad::HIDGamepad(IOHIDDeviceRef hidDevice, unsigned index)
    : PlatformGamepad(index)
    , m_hidDevice(hidDevice)
{
    m_connectTime = m_lastUpdateTime = monotonicallyIncreasingTime();

    CFNumberRef cfVendorID = (CFNumberRef)IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDVendorIDKey));
    CFNumberRef cfProductID = (CFNumberRef)IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDProductIDKey));

    int vendorID, productID;
    CFNumberGetValue(cfVendorID, kCFNumberIntType, &vendorID);
    CFNumberGetValue(cfProductID, kCFNumberIntType, &productID);

    CFStringRef cfProductName = (CFStringRef)IOHIDDeviceGetProperty(hidDevice, CFSTR(kIOHIDProductKey));
    String productName(cfProductName);

    // Currently the spec has no formatting for the id string.
    // This string formatting matches Firefox.
    m_id = String::format("%x-%x-%s", vendorID, productID, productName.utf8().data());

    initElements();
}

void HIDGamepad::getCurrentValueForElement(const HIDGamepadElement& gamepadElement)
{
    IOHIDElementRef element = gamepadElement.iohidElement.get();
    IOHIDValueRef value;
    if (IOHIDDeviceGetValue(IOHIDElementGetDevice(element), element, &value) == kIOReturnSuccess)
        valueChanged(value);
}

void HIDGamepad::initElements()
{
    RetainPtr<CFArrayRef> elements = adoptCF(IOHIDDeviceCopyMatchingElements(m_hidDevice.get(), NULL, kIOHIDOptionsTypeNone));
    initElementsFromArray(elements.get());

    // Buttons are specified to appear highest priority first in the array.
    std::sort(m_buttons.begin(), m_buttons.end(), [](const std::unique_ptr<HIDGamepadButton>& a, const std::unique_ptr<HIDGamepadButton>& b) {
        return a->priority < b->priority;
    });

    m_axisValues.resize(m_axes.size());
    m_buttonValues.resize(m_buttons.size());

    for (auto& button : m_buttons)
        getCurrentValueForElement(*button);

    for (auto& axis : m_axes)
        getCurrentValueForElement(*axis);
}

void HIDGamepad::initElementsFromArray(CFArrayRef elements)
{
    for (CFIndex i = 0, count = CFArrayGetCount(elements); i < count; ++i) {
        IOHIDElementRef element = (IOHIDElementRef)CFArrayGetValueAtIndex(elements, i);
        if (CFGetTypeID(element) != IOHIDElementGetTypeID())
            continue;

        // As a physical element can appear in the device twice (in different collections) and can be
        // represented by different IOHIDElementRef objects, we look at the IOHIDElementCookie which
        // is meant to be unique for each physical element.
        IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
        if (m_elementMap.contains(cookie))
            continue;

        IOHIDElementType type = IOHIDElementGetType(element);

        if ((type == kIOHIDElementTypeInput_Misc || type == kIOHIDElementTypeInput_Button) && maybeAddButton(element))
            continue;

        if ((type == kIOHIDElementTypeInput_Misc || type == kIOHIDElementTypeInput_Axis) && maybeAddAxis(element))
            continue;

        if (type == kIOHIDElementTypeCollection)
            initElementsFromArray(IOHIDElementGetChildren(element));
    }
}

bool HIDGamepad::maybeAddButton(IOHIDElementRef element)
{
    uint32_t usagePage = IOHIDElementGetUsagePage(element);
    if (usagePage != kHIDPage_Button)
        return false;

    uint32_t usage = IOHIDElementGetUsage(element);
    if (!usage)
        return false;

    CFIndex min = IOHIDElementGetLogicalMin(element);
    CFIndex max = IOHIDElementGetLogicalMax(element);

    m_buttons.append(std::make_unique<HIDGamepadButton>(usage, min, max, element));

    IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
    m_elementMap.set(cookie, m_buttons.last().get());

    return true;
}

bool HIDGamepad::maybeAddAxis(IOHIDElementRef element)
{
    uint32_t usagePage = IOHIDElementGetUsagePage(element);
    if (usagePage != kHIDPage_GenericDesktop)
        return false;

    uint32_t usage = IOHIDElementGetUsage(element);
    // This range covers the standard axis usages.
    if (usage < kHIDUsage_GD_X || usage > kHIDUsage_GD_Rz)
        return false;

    CFIndex min = IOHIDElementGetPhysicalMin(element);
    CFIndex max = IOHIDElementGetPhysicalMax(element);

    m_axes.append(std::make_unique<HIDGamepadAxis>(min, max, element));

    IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
    m_elementMap.set(cookie, m_axes.last().get());

    return true;
}

void HIDGamepad::valueChanged(IOHIDValueRef value)
{
    IOHIDElementCookie cookie = IOHIDElementGetCookie(IOHIDValueGetElement(value));
    HIDGamepadElement* element = m_elementMap.get(cookie);

    // This might be an element we don't currently handle as input so we can skip it.
    if (!element)
        return;

    element->rawValue = IOHIDValueGetScaledValue(value, kIOHIDValueScaleTypePhysical);

    if (element->isButton()) {
        for (unsigned i = 0; i < m_buttons.size(); ++i) {
            if (m_buttons[i].get() == element)
                m_buttonValues[i] = element->normalizedValue();
        }
    } else if (element->isAxis()) {
        for (unsigned i = 0; i < m_axes.size(); ++i) {
            if (m_axes[i].get() == element)
                m_axisValues[i] = element->normalizedValue();
        }
    } else
        ASSERT_NOT_REACHED();

    m_lastUpdateTime = monotonicallyIncreasingTime();
}

} // namespace WebCore

#endif // ENABLE(GAMEPAD)
