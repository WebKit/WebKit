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
#include "HIDDevice.h"

#if PLATFORM(MAC)

#include "HIDElement.h"
#include "Logging.h"
#include <IOKit/hid/IOHIDElement.h>
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/cf/TypeCastsCF.h>

WTF_DECLARE_CF_TYPE_TRAIT(IOHIDElement);

namespace WebCore {

HIDDevice::HIDDevice(IOHIDDeviceRef device)
    : m_rawDevice(device)
{
    CFNumberRef cfVendorID = checked_cf_cast<CFNumberRef>(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDVendorIDKey)));
    CFNumberRef cfProductID = checked_cf_cast<CFNumberRef>(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductIDKey)));

    int vendorID, productID;
    CFNumberGetValue(cfVendorID, kCFNumberIntType, &vendorID);
    CFNumberGetValue(cfProductID, kCFNumberIntType, &productID);

    if (vendorID < 0 || vendorID > std::numeric_limits<uint16_t>::max()) {
        LOG(HID, "Device attached with malformed vendor ID 0x%x. Resetting to 0.", vendorID);
        vendorID = 0;
    }
    if (productID < 0 || productID > std::numeric_limits<uint16_t>::max()) {
        LOG(HID, "Device attached with malformed product ID 0x%x. Resetting to 0.", productID);
        productID = 0;
    }

    m_vendorID = (uint16_t)vendorID;
    m_productID = (uint16_t)productID;

    CFStringRef cfProductName = checked_cf_cast<CFStringRef>(IOHIDDeviceGetProperty(device, CFSTR(kIOHIDProductKey)));
    m_productName = cfProductName ? String(cfProductName) : String("Unknown"_s);
}

Vector<HIDElement> HIDDevice::uniqueInputElementsInDeviceTreeOrder() const
{
    HashSet<IOHIDElementCookie> encounteredCookies;
    Deque<IOHIDElementRef> elementQueue;

    RetainPtr<CFArrayRef> elements = adoptCF(IOHIDDeviceCopyMatchingElements(m_rawDevice.get(), NULL, kIOHIDOptionsTypeNone));
    for (CFIndex i = 0; i < CFArrayGetCount(elements.get()); ++i)
        elementQueue.append(checked_cf_cast<IOHIDElementRef>(CFArrayGetValueAtIndex(elements.get(), i)));

    Vector<HIDElement> result;

    while (!elementQueue.isEmpty()) {
        auto element = elementQueue.takeFirst();
        IOHIDElementCookie cookie = IOHIDElementGetCookie(element);
        if (encounteredCookies.contains(cookie))
            continue;

        switch (IOHIDElementGetType(element)) {
        case kIOHIDElementTypeCollection: {
            auto children = IOHIDElementGetChildren(element);
            for (CFIndex i = CFArrayGetCount(children) - 1; i >= 0; --i)
                elementQueue.prepend(checked_cf_cast<IOHIDElementRef>(CFArrayGetValueAtIndex(children, i)));
            continue;
        }
        case kIOHIDElementTypeInput_Misc:
        case kIOHIDElementTypeInput_Button:
        case kIOHIDElementTypeInput_Axis:
            encounteredCookies.add(cookie);
            result.append(element);
            continue;
        default:
            continue;
        }
    }

    return result;
}

} // namespace WebCore

#endif // PLATFORM(MAC)
