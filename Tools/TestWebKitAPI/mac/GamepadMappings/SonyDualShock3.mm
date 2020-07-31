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

const uint8_t Dualshock3Descriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x04,        // Usage (Joystick)
    0xA1, 0x01,        // Collection (Application)
    0xA1, 0x02,        //   Collection (Logical)
                       //     Report ID - Total (1) byte
    0x85, 0x01,        //     Report ID (1)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
                       //     (1) byte report for unknown pointer - Total (2) bytes
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x13,        //     Report Count (19)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x01,        //     Physical Maximum (1)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01)
    0x29, 0x13,        //     Usage Maximum (0x13)
                       //     (19) bits for digital buttons - Total (4) bytes (3) bits
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,        //     Report Size (1)
    0x95, 0x0D,        //     Report Count (13)
    0x06, 0x00, 0xFF,  //     Usage Page (Vendor Defined 0xFF00)
                       //     (13) bits padding - Total (6) bytes
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x01,        //     Usage (Pointer)
    0xA1, 0x00,        //     Collection (Physical)
    0x75, 0x08,        //       Report Size (8)
    0x95, 0x04,        //       Report Count (4)
    0x35, 0x00,        //       Physical Minimum (0)
    0x46, 0xFF, 0x00,  //       Physical Maximum (255)
    0x09, 0x30,        //       Usage (X)
    0x09, 0x31,        //       Usage (Y)
    0x09, 0x32,        //       Usage (Z)
    0x09, 0x35,        //       Usage (Rz)
                       //       (4) axes at 1 byte each - Total (10) bytes
    0x81, 0x02,        //       Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //     End Collection
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x27,        //     Report Count (39)
    0x09, 0x01,        //     Usage (Pointer)
                       //     (39) pointers at 1 byte each - Total (49) bytes
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xA1, 0x02,        //   Collection (Logical)
    0x85, 0x02,        //     Report ID (2)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xA1, 0x02,        //   Collection (Logical)
    0x85, 0xEE,        //     Report ID (-18)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xA1, 0x02,        //   Collection (Logical)
    0x85, 0xEF,        //     Report ID (-17)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x30,        //     Report Count (48)
    0x09, 0x01,        //     Usage (Pointer)
    0xB1, 0x02,        //     Feature (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0xC0,              // End Collection
};

// FIXME: Hatswitch is not yet addressable by test
const size_t Dualshock3ButtonCount = 19;
const size_t Dualshock3AxisCount = 4;
const size_t Dualshock3ReportSize = 49;
const char* Dualshock3Name = "Virtual Dualshock3";

// Report layout:
// |ReportID| |<none>| |b1-b8| |b9-b16| |(5 bits padding high) b17-b19| |padding| |Axis1| |Axis2| |Axis3| |Axis4|
// (39x) |Generic pointer|
// Analog button readouts show up at specific places in those 39 generic 1 byte pointers
// Physical buttons on the device map to button bits 1-17, but there is no actuator for b18 and b19
// 12 of those physical buttons also have an analog value mapping into the generic pointers

static void publishReportCallback(Vector<float>& buttonValues, Vector<float>& axisValues, HIDUserDevice *userDevice)
{
    uint8_t reportData[Dualshock3ReportSize];
    bzero(reportData, Dualshock3ReportSize);
    size_t reportIndex = 0;

    // Report ID
    reportData[reportIndex++] = 0x01;

    // Constant
    reportData[reportIndex++] = 0x00;

    // Digital buttons span the next 3 bytes
    uint8_t buttonByte = 0x00;
    long buttonBit;
    for (size_t i = 0; i < 8; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << i;
    }
    reportData[reportIndex++] = buttonByte;

    buttonByte = 0x00;
    for (size_t i = 8; i < 16; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i - 8);
    }
    reportData[reportIndex++] = buttonByte;

    buttonByte = 0x00;
    for (size_t i = 16; i < 19; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i - 16);
    }
    reportData[reportIndex++] = buttonByte;

    // Padding byte
    reportData[reportIndex++] = 0x00;

    // Axes (1 byte each)
    for (size_t i = 0; i < 4; ++i)
        reportData[reportIndex++] = (uint8_t)(axisValues[i] * 255);

    // First 4 generic points are always 0x00
    reportData[reportIndex++] = 0x00;
    reportData[reportIndex++] = 0x00;
    reportData[reportIndex++] = 0x00;
    reportData[reportIndex++] = 0x00;

    // The next twelve generic pointers map to specific buttons in order
    for (size_t i = 4; i < 16; ++i)
        reportData[reportIndex++] = (uint8_t)(buttonValues[i] * 255);

    auto nsReportData = adoptNS([[NSData alloc] initWithBytes:reportData length:Dualshock3ReportSize]);
    [userDevice handleReport:nsReportData.get() error:nil];
}

GamepadMapping VirtualGamepad::sonyDualshock3Mapping()
{
    return {
        Dualshock3Descriptor,
        sizeof(Dualshock3Descriptor),
        Dualshock3Name,
        HIDVendorID::Sony,
        HIDProductID::Dualshock3,
        Dualshock3ButtonCount,
        Dualshock3AxisCount,
        publishReportCallback
    };
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
