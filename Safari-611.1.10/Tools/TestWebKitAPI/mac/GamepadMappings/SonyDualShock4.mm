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

const uint8_t Dualshock4Descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
                       //   Report ID - Total (1) byte
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x30,        //   Usage (X)
    0x09, 0x31,        //   Usage (Y)
    0x09, 0x32,        //   Usage (Z)
    0x09, 0x35,        //   Usage (Rz)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x04,        //   Report Count (4)
                       //   (4) Axes at 1 byte each - Total (5) bytes
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x07,        //   Logical Maximum (7)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
                       //   4-bits for Hatswitch - Total (5) bytes (4) bits
    0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x0E,        //   Usage Maximum (0x0E)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0E,        //   Report Count (14)
                       //   14 buttons - Total (7) bytes (2) bits
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
                       //   6-bits padding - Total (8) bytes
    0x81, 0x01,        //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x33,        //   Usage (Rx)
    0x09, 0x34,        //   Usage (Ry)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x02,        //   Report Count (2)
                       //   (2) axes 1 byte each - Total (10) bytes
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x06, 0x04, 0xFF,  //   Usage Page (Vendor Defined 0xFF04)
    0x85, 0x02,        //   Report ID (2)
    0x09, 0x24,        //   Usage (0x24)
    0x95, 0x24,        //   Report Count (36)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA3,        //   Report ID (-93)
    0x09, 0x25,        //   Usage (0x25)
    0x95, 0x30,        //   Report Count (48)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x05,        //   Report ID (5)
    0x09, 0x26,        //   Usage (0x26)
    0x95, 0x28,        //   Report Count (40)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x06,        //   Report ID (6)
    0x09, 0x27,        //   Usage (0x27)
    0x95, 0x34,        //   Report Count (52)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x07,        //   Report ID (7)
    0x09, 0x28,        //   Usage (0x28)
    0x95, 0x30,        //   Report Count (48)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x08,        //   Report ID (8)
    0x09, 0x29,        //   Usage (0x29)
    0x95, 0x2F,        //   Report Count (47)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x09,        //   Report ID (9)
    0x09, 0x2A,        //   Usage (0x2A)
    0x95, 0x13,        //   Report Count (19)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x06, 0x03, 0xFF,  //   Usage Page (Vendor Defined 0xFF03)
    0x85, 0x03,        //   Report ID (3)
    0x09, 0x21,        //   Usage (0x21)
    0x95, 0x26,        //   Report Count (38)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x04,        //   Report ID (4)
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x2E,        //   Report Count (46)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF0,        //   Report ID (-16)
    0x09, 0x47,        //   Usage (0x47)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF1,        //   Report ID (-15)
    0x09, 0x48,        //   Usage (0x48)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xF2,        //   Report ID (-14)
    0x09, 0x49,        //   Usage (0x49)
    0x95, 0x0F,        //   Report Count (15)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x06, 0x00, 0xFF,  //   Usage Page (Vendor Defined 0xFF00)
    0x85, 0x11,        //   Report ID (17)
    0x09, 0x20,        //   Usage (0x20)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x4D,        //   Report Count (77)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x21,        //   Usage (0x21)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x12,        //   Report ID (18)
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x8D,        //   Report Count (-115)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x23,        //   Usage (0x23)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x13,        //   Report ID (19)
    0x09, 0x24,        //   Usage (0x24)
    0x95, 0xCD,        //   Report Count (-51)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x25,        //   Usage (0x25)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x14,        //   Report ID (20)
    0x09, 0x26,        //   Usage (0x26)
    0x96, 0x0D, 0x01,  //   Report Count (269)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x27,        //   Usage (0x27)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x15,        //   Report ID (21)
    0x09, 0x28,        //   Usage (0x28)
    0x96, 0x4D, 0x01,  //   Report Count (333)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x29,        //   Usage (0x29)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x16,        //   Report ID (22)
    0x09, 0x2A,        //   Usage (0x2A)
    0x96, 0x8D, 0x01,  //   Report Count (397)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x2B,        //   Usage (0x2B)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x17,        //   Report ID (23)
    0x09, 0x2C,        //   Usage (0x2C)
    0x96, 0xCD, 0x01,  //   Report Count (461)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x2D,        //   Usage (0x2D)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x18,        //   Report ID (24)
    0x09, 0x2E,        //   Usage (0x2E)
    0x96, 0x0D, 0x02,  //   Report Count (525)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x2F,        //   Usage (0x2F)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x19,        //   Report ID (25)
    0x09, 0x30,        //   Usage (0x30)
    0x96, 0x22, 0x02,  //   Report Count (546)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0x31,        //   Usage (0x31)
    0x91, 0x02,        //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x06, 0x80, 0xFF,  //   Usage Page (Vendor Defined 0xFF80)
    0x85, 0x82,        //   Report ID (-126)
    0x09, 0x22,        //   Usage (0x22)
    0x95, 0x3F,        //   Report Count (63)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x83,        //   Report ID (-125)
    0x09, 0x23,        //   Usage (0x23)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x84,        //   Report ID (-124)
    0x09, 0x24,        //   Usage (0x24)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x90,        //   Report ID (-112)
    0x09, 0x30,        //   Usage (0x30)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x91,        //   Report ID (-111)
    0x09, 0x31,        //   Usage (0x31)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x92,        //   Report ID (-110)
    0x09, 0x32,        //   Usage (0x32)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x93,        //   Report ID (-109)
    0x09, 0x33,        //   Usage (0x33)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0x94,        //   Report ID (-108)
    0x09, 0x34,        //   Usage (0x34)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA0,        //   Report ID (-96)
    0x09, 0x40,        //   Usage (0x40)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA4,        //   Report ID (-92)
    0x09, 0x44,        //   Usage (0x44)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA7,        //   Report ID (-89)
    0x09, 0x45,        //   Usage (0x45)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA8,        //   Report ID (-88)
    0x09, 0x45,        //   Usage (0x45)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xA9,        //   Report ID (-87)
    0x09, 0x45,        //   Usage (0x45)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xAA,        //   Report ID (-86)
    0x09, 0x45,        //   Usage (0x45)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xAB,        //   Report ID (-85)
    0x09, 0x45,        //   Usage (0x45)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xAC,        //   Report ID (-84)
    0x09, 0x45,        //   Usage (0x45)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xAD,        //   Report ID (-83)
    0x09, 0x45,        //   Usage (0x45)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xB3,        //   Report ID (-77)
    0x09, 0x45,        //   Usage (0x45)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xB4,        //   Report ID (-76)
    0x09, 0x46,        //   Usage (0x46)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xB5,        //   Report ID (-75)
    0x09, 0x47,        //   Usage (0x47)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xD0,        //   Report ID (-48)
    0x09, 0x40,        //   Usage (0x40)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x85, 0xD4,        //   Report ID (-44)
    0x09, 0x44,        //   Usage (0x44)
    0xB1, 0x02,        //   Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              // End Collection
};

// FIXME: Hatswitch is not yet addressable by test
const size_t Dualshock4ButtonCount = 14;
const size_t Dualshock4AxisCount = 6;
const size_t Dualshock4ReportSize = 10;
const char* Dualshock4Name = "Virtual Dualshock4";

// Report layout:
// |ReportID| |Axis1| |Axis2| |Axis3| |Axis4| |b1-b4 high, HS low| |b5-b12| |b13-b14| |Axis5| |Axis6|

static void publishReportCallback(Vector<float>& buttonValues, Vector<float>& axisValues, HIDUserDevice *userDevice)
{
    uint8_t reportData[Dualshock4ReportSize];
    size_t reportIndex = 0;

    // Report ID
    reportData[reportIndex++] = 0x01;

    // First 4 axes (1 byte each)
    for (size_t i = 0; i < 4; ++i)
        reportData[reportIndex++] = (uint8_t)(axisValues[i] * 255);

    // The hatswitch, 14 buttons, and 6 bits of padding take up 3 bytes
    // The bit layout is as follows:
    // |b4|b3|b2|b1|hs|hs|hs|hs| |bc|bb|ba|b9|b8|b7|b6|b5| |0|0|0|0|0|0|be|bd|

    // For now, always leave the hatswitch in the neutral position of "8"
    uint8_t buttonByte = 0x08;
    long buttonBit;
    for (size_t i = 0; i < 4; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (4 + i);
    }
    reportData[reportIndex++] = buttonByte;

    buttonByte = 0x00;
    for (size_t i = 4; i < 12; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i - 4);
    }
    reportData[reportIndex++] = buttonByte;

    buttonByte = 0x00;
    for (size_t i = 12; i < 13; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i - 12);
    }
    reportData[reportIndex++] = buttonByte;

    // Final 2 axes (1 byte each)
    for (size_t i = 4; i < 5; ++i)
        reportData[reportIndex++] = (uint8_t)(axisValues[i] * 255);

    auto nsReportData = adoptNS([[NSData alloc] initWithBytes:reportData length:Dualshock4ReportSize]);
    [userDevice handleReport:nsReportData.get() error:nil];
}

GamepadMapping VirtualGamepad::sonyDualshock4Mapping()
{
    return {
        Dualshock4Descriptor,
        sizeof(Dualshock4Descriptor),
        Dualshock4Name,
        HIDVendorID::Sony,
        HIDProductID::Dualshock4_1,
        Dualshock4ButtonCount,
        Dualshock4AxisCount,
        publishReportCallback
    };
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
