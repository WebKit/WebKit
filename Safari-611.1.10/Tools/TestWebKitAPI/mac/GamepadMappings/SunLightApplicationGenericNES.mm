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

const uint8_t SLAGenericNESDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x95, 0x02,        //     Report Count (2)
    0x75, 0x08,        //     Report Size (8)
                       //     (2) Axes, 1 byte each. Total - (2) bytes
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x08,        //     Report Size (8)
                       //     (1) Byte, unknown. Total - (3) bytes
    0x81, 0x01,        //     Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x0B,        //   Usage Maximum (0x0B)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x95, 0x0B,        //   Report Count (11)
    0x75, 0x01,        //   Report Size (1)
                       //   (11) buttons. Total - (4) bytes (3) bits
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x1D,        //   Report Count (29)
                       //   (29) padding bits. Total (8) bytes
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x8C,        //   Usage Page (Bar Code Scanner Page)
    0x09, 0x01,        //   Usage (0x01)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x02,        //     Usage (0x02)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x04,        //     Report Count (4)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x02,        //     Usage (0x02)
    0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xC0,              // End Collection
};

const size_t SLAGenericNESButtonCount = 11;
const size_t SLAGenericNESAxisCount = 2;
const size_t SLAGenericNESReportSize = 8;
const char* SLAGenericNESName = "Virtual Sun Light Application Generic NES Controller";

// Report layout:
// |Axis1| |Axis2| |0| |b1-b8| |b9-bb| |0| |0| |0|
static void publishReportCallback(Vector<float>& buttonValues, Vector<float>& axisValues, HIDUserDevice *userDevice)
{
    uint8_t reportData[SLAGenericNESReportSize];

    for (size_t i = 0; i < SLAGenericNESAxisCount; ++i) {
        if (axisValues[i] <= -0.5)
            reportData[i] = 0x00;
        else if (axisValues[i] >= 0.5)
            reportData[i] = 0xff;
        else
            reportData[i] = 0x7f;
    }

    reportData[2] = 0x00;

    // The next 2 bytes of the report have the following layout:
    // |b8|b7|b6|b5|b4|b3|b2|b1| |0|0|0|0|0|bb|ba|b9|

    uint8_t buttonByte = 0x00;
    for (size_t i = 0; i < 8; ++i) {
        long buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i);
    }
    reportData[3] = buttonByte;

    buttonByte = 0x00;
    for (size_t i = 8; i < 11; ++i) {
        long buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i - 8);
    }
    reportData[4] = buttonByte;

    reportData[5] = 0x00;
    reportData[6] = 0x00;
    reportData[7] = 0x00;

    auto nsReportData = adoptNS([[NSData alloc] initWithBytes:reportData length:SLAGenericNESReportSize]);
    [userDevice handleReport:nsReportData.get() error:nil];
}

GamepadMapping VirtualGamepad::sunLightApplicationGenericNESMapping()
{
    return {
        SLAGenericNESDescriptor,
        sizeof(SLAGenericNESDescriptor),
        SLAGenericNESName,
        HIDVendorID::SunLightApplication,
        HIDProductID::GenericNES,
        SLAGenericNESButtonCount,
        SLAGenericNESAxisCount,
        publishReportCallback
    };
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
