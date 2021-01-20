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

const uint8_t F310Descriptor[] = {
    0x05, 0x01,         // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,         // Usage (Joystick)
    0xA1, 0x01,         // Collection (Application)
    0xA1, 0x02,         //   Collection (Logical)
    0x75, 0x08,         //     Report Size (8)
    0x95, 0x04,         //     Report Count (4)
    0x15, 0x00,         //     Logical Minimum (0)
    0x26, 0xFF, 0x00,   //     Logical Maximum (255)
    0x35, 0x00,         //     Physical Minimum (0)
    0x46, 0xFF, 0x00,   //     Physical Maximum (255)
    0x09, 0x30,         //     Usage (X)
    0x09, 0x31,         //     Usage (Y)
    0x09, 0x32,         //     Usage (Z)
    0x09, 0x35,         //     Usage (Rz)
                        //     (4) axes, 1 byte each. Total - (4) bytes
    0x81, 0x02,         //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x04,         //     Report Size (4)
    0x95, 0x01,         //     Report Count (1)
    0x25, 0x07,         //     Logical Maximum (7)
    0x46, 0x3B, 0x01,   //     Physical Maximum (315)
    0x65, 0x14,         //     Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x39,         //     Usage (Hat switch)
                        //     (1) Hat switch, 4 bits. Total - (4) bytes (4) bits
    0x81, 0x42,         //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x65, 0x00,         //     Unit (None)
    0x75, 0x01,         //     Report Size (1)
    0x95, 0x0C,         //     Report Count (12)
    0x25, 0x01,         //     Logical Maximum (1)
    0x45, 0x01,         //     Physical Maximum (1)
    0x05, 0x09,         //     Usage Page (Button)
    0x19, 0x01,         //     Usage Minimum (0x01)
    0x29, 0x0C,         //     Usage Maximum (0x0C)
                        //     (12) Button bits. Total - (6) bytes
    0x81, 0x02,         //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x00, 0xFF,   //     Usage Page (Vendor Defined 0xFF00)
    0x75, 0x01,         //     Report Size (1)
    0x95, 0x10,         //     Report Count (16)
    0x25, 0x01,         //     Logical Maximum (1)
    0x45, 0x01,         //     Physical Maximum (1)
    0x09, 0x01,         //     Usage (0x01)
                        //     (2) unknown bytes. Total - (8) bytes
    0x81, 0x02,         //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,               //   End Collection
    0xA1, 0x02,         //   Collection (Logical)
    0x75, 0x08,         //     Report Size (8)
    0x95, 0x07,         //     Report Count (7)
    0x46, 0xFF, 0x00,   //     Physical Maximum (255)
    0x26, 0xFF, 0x00,   //     Logical Maximum (255)
    0x09, 0x02,         //     Usage (0x02)
    0x91, 0x02,         //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,               //   End Collection
    0xC0,               // End Collection
};

const size_t F310ButtonCount = 13;
const size_t F310AxisCount = 4;
const size_t F310ReportSize = 8;
const char* F310Name = "Virtual Logitech F310";

// Report layout:
// |X| |Y| |Z| |Rz| |b1-b4, hatswitch| |b5-b12| |0x00| |0xff|

static void publishReportCallback(Vector<float>& buttonValues, Vector<float>& axisValues, HIDUserDevice *userDevice)
{
    uint8_t reportData[F310ReportSize];

    // First 4 bytes are the axis values
    for (size_t i = 0; i < 4; ++i)
        reportData[i] = (uint8_t)((int8_t)(axisValues[i] * 127));

    // Next byte is the hatswitch and first 4 buttons
    // FIXME: No API test configurability of the hatswitch yet
    uint8_t buttonByte = 0x08;
    long buttonBit;
    for (size_t i = 0; i < 4; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i + 4);
    }
    reportData[4] = buttonByte;

    // Next byte is buttons 5-12
    buttonByte = 0x00;
    for (size_t i = 4; i < 12; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i - 4);
    }
    reportData[5] = buttonByte;

    // Final 2 bytes are constant
    reportData[6] = 0x00;
    reportData[7] = 0xff;

    auto nsReportData = adoptNS([[NSData alloc] initWithBytes:reportData length:F310ReportSize]);
    [userDevice handleReport:nsReportData.get() error:nil];
}

GamepadMapping VirtualGamepad::logitechF310Mapping()
{
    return {
        F310Descriptor,
        sizeof(F310Descriptor),
        F310Name,
        HIDVendorID::Logitech,
        HIDProductID::F310,
        F310ButtonCount,
        F310AxisCount,
        publishReportCallback
    };
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
