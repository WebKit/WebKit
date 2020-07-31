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

#pragma once

#if HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)

#include <dispatch/dispatch.h>
#include <wtf/FastMalloc.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>

OBJC_CLASS HIDDevice;
OBJC_CLASS HIDUserDevice;
OBJC_CLASS NSString;

namespace TestWebKitAPI {

enum class HIDVendorID : uint16_t {
    Microsoft = 0x045e,
    ShenzhenLongshengweiTechnology = 0x0079,
    Sony = 0x054c,
    SteelSeries1 = 0x0111,
    SteelSeries2 = 0x1038,
    SunLightApplication = 0x12bd,
    Fake = 0xffff,
};

// Technically different products from different vendors can have the same product ID,
// But in practice that probably won't happen.
enum class HIDProductID : uint16_t {
    StratusXL1 = 0x1418,
    StratusXL2 = 0x1419,
    Nimbus = 0x1420,
    Gamepad = 0x0011,
    GenericNES = 0xd015,
    XboxOne1 = 0x02ea,
    XboxOne2 = 0x02e0,
    XboxOne3 = 0x02fd,
    Dualshock3 = 0x0268,
    Dualshock4_1 = 0x05c4,
    Dualshock4_2 = 0x09cc,
};

typedef void (*PublishReportCallback)(Vector<float>&, Vector<float>&, HIDUserDevice*);

struct GamepadMapping {
    const uint8_t* descriptorData;
    size_t descriptorDataSize;
    const char* name;
    HIDVendorID vendorID;
    HIDProductID productID;
    size_t buttonCount;
    size_t axisCount;
    PublishReportCallback publishReportCallback;
};

class VirtualGamepad {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static GamepadMapping microsoftXboxOneMapping();
    static GamepadMapping shenzhenLongshengweiTechnologyGamepadMapping();
    static GamepadMapping sonyDualshock3Mapping();
    static GamepadMapping sonyDualshock4Mapping();
    static GamepadMapping steelSeriesNimbusMapping();
    static GamepadMapping sunLightApplicationGenericNESMapping();

    VirtualGamepad(const GamepadMapping&);
    ~VirtualGamepad();

    size_t buttonCount() const;
    size_t axisCount() const;

    // Acceptable values are 0.0 to 1.0
    void setButtonValue(size_t index, float value);

    // Acceptable values are -1.0 to 1.0
    void setAxisValue(size_t index, float value);

    void publishReport();

private:
    RetainPtr<NSString> m_uniqueID;
    RetainPtr<HIDUserDevice> m_userDevice;
    RetainPtr<HIDDevice> m_device;
    OSObjectPtr<dispatch_queue_t> m_dispatchQueue;

    Vector<float> m_buttonValues;
    Vector<float> m_axisValues;

    GamepadMapping m_gamepadMapping;
};
} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
