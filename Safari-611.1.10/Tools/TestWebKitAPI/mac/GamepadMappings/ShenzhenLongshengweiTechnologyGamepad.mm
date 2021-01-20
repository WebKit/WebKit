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

#import "config.h"
#import "VirtualGamepad.h"

#if HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)

#import <HID/HIDUserDevice.h>

namespace TestWebKitAPI {

const uint8_t SLTGamepadDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,        // Usage (Joystick)
    0xA1, 0x01,        // Collection (Application)
    0xA1, 0x02,        //   Collection (Logical)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x05,        //     Report Count (5)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x35, 0x00,        //     Physical Minimum (0)
    0x46, 0xFF, 0x00,  //     Physical Maximum (255)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
                       //     (5) 1 byte axes (yes, it replicates the X axis 4 times) - Total 5 bytes.
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x25, 0x07,        //     Logical Maximum (7)
    0x46, 0x3B, 0x01,  //     Physical Maximum (315)
    0x65, 0x14,        //     Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x00,        //     Usage (Undefined)
                       //     4 bits, unknown - Total (5) bytes (4) bits
    0x81, 0x42,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00,        //     Unit (None)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x0A,        //     Report Count (10)
    0x25, 0x01,        //     Logical Maximum (1)
    0x45, 0x01,        //     Physical Maximum (1)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x0A,        //     Usage Maximum (0x0A)
                       //     (10) 1-bit buttons - Total (6) bytes (6) bits
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF,  //     Usage Page (Vendor Defined 0xFF00)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x0A,        //     Report Count (10)
    0x25, 0x01,        //     Logical Maximum (1)
    0x45, 0x01,        //     Physical Maximum (1)
    0x09, 0x01,        //     Usage (0x01)
                       //     (10) vendor defined bits - Total (8) bytes
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0xA1, 0x02,        //   Collection (Logical)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x04,        //     Report Count (4)
    0x46, 0xFF, 0x00,  //     Physical Maximum (255)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x09, 0x02,        //     Usage (0x02)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xC0,              // End Collection
};

const size_t SLTGamepadButtonCount = 10;
const size_t SLTGamepadAxisCount = 5;
const size_t SLTGamepadReportSize = 8;
const char* SLTGamepadName = "Virtual Shenzhen Longshengwei Technology Gamepad";

// Report layout:
// |Axis1| |Axis2| |Axis3| |Axis4| |Axis5| |b1-b4| |b5-b10| |0|
static void publishReportCallback(Vector<float>& buttonValues, Vector<float>& axisValues, HIDUserDevice *userDevice)
{
    uint8_t reportData[SLTGamepadReportSize];

    // First 3 report bytes are constant, no matter what the values of axis 0, 1, or 2 are.
    reportData[0] = 0x01;
    reportData[1] = 0x7f;
    reportData[2] = 0x7f;

    for (size_t i = 3; i < SLTGamepadAxisCount; ++i) {
        if (axisValues[i] <= -0.5)
            reportData[i] = 0x00;
        else if (axisValues[i] >= 0.5)
            reportData[i] = 0xff;
        else
            reportData[i] = 0x7f;
    }

    // The final 3 bytes of the report have the following layout:
    // (Note, button 7 and button 8 do not physically exist on the device and tehrefore can never get activated)
    // |b4|b3|b2|b1|1|1|1|1| |0|0|b6|b5|0|0|ba|b9| |0|0|0|0|0|0|0|0|

    uint8_t buttonByte = 0x0f;
    long buttonBit;
    for (size_t i = 0; i < 4; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (4 + i);
    }
    reportData[5] = buttonByte;

    buttonByte = 0;
    buttonByte |= lround(buttonValues[4]) << 4;
    buttonByte |= lround(buttonValues[5]) << 5;
    buttonByte |= lround(buttonValues[8]);
    buttonByte |= lround(buttonValues[9]) << 1;
    reportData[6] = buttonByte;

    reportData[7] = 0x00;

    auto nsReportData = adoptNS([[NSData alloc] initWithBytes:reportData length:SLTGamepadReportSize]);
    [userDevice handleReport:nsReportData.get() error:nil];
}

GamepadMapping VirtualGamepad::shenzhenLongshengweiTechnologyGamepadMapping()
{
    return {
        SLTGamepadDescriptor,
        sizeof(SLTGamepadDescriptor),
        SLTGamepadName,
        HIDVendorID::ShenzhenLongshengweiTechnology,
        HIDProductID::Gamepad,
        SLTGamepadButtonCount,
        SLTGamepadAxisCount,
        publishReportCallback
    };
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
