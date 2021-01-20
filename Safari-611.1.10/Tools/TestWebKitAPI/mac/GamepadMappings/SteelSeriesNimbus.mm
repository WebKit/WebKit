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

// HID descriptors are an interesting language.
// This is the descriptor dumped from an MFi Nimbus controller, which is fairly representative of a modern controller.
// I got this formatting by dumping the raw descriptor from my device into https://eleccelerator.com/usbdescreqparser/
// In addition to the raw HID language, I've added commentary along the way to further document how this works.
const uint8_t NimbusDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
    0x09, 0x05,        //   Usage (Game Pad)
    0xA1, 0x02,        //   Collection (Logical)
    0x75, 0x08,        //     Report Size (8) - All reports past this point will be 1 byte per element until Report Size changes.
    0x95, 0x04,        //     Report Count (4) - The upcoming report will have 4 elements, 1 byte each.
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x35, 0x00,        //     Physical Minimum (0)
    0x46, 0xFF, 0x00,  //     Physical Maximum (255)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x90,        //     Usage (D-pad Up) - These 4 d-pad elements will each be a button.
    0x09, 0x92,        //     Usage (D-pad Right)
    0x09, 0x91,        //     Usage (D-pad Down)
    0x09, 0x93,        //     Usage (D-pad Left)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) - Lock in the above input report (4 bytes)
    0x95, 0x08,        //     Report Count (8) - The next report will have 8 elements, 1 byte each (cascaded from above)
    0x05, 0x09,        //     Usage Page (Button)
    0x19, 0x01,        //     Usage Minimum (0x01) - And it will be buttons...
    0x29, 0x08,        //     Usage Maximum (0x08) - 8 of them!
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) - Lock in the above input report (8 bytes)
    0x15, 0x00,        //     Logical Minimum (0) - The next elements are pure digital instead of 'analog'
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x01,        //     Physical Maximum (1)
    0x75, 0x01,        //     Report Size (1) - The report size is 1 bit
    0x95, 0x01,        //     Report Count (1) - And there is one such element
    0x05, 0x0C,        //     Usage Page (Consumer)
    0x0A, 0x23, 0x02,  //     Usage (AC Home) - And it is an "AC Home" element, which is basically a fancy button
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) - Lock in the above input report (1 bit)
    0x95, 0x07,        //     Report Count (7) - The next report will have 7 elements (of 1 bit each)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) - And it's constant padding. Lock it in for (7 bits).
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x35, 0x00,        //     Physical Minimum (0)
    0x45, 0x01,        //     Physical Maximum (1)
    0x75, 0x01,        //     Report Size (1) - The next report will also be 1 bit each (like the AC Home button above)
    0x95, 0x04,        //     Report Count (4) - But there will be 4 of them
    0x05, 0x08,        //     Usage Page (LEDs) - And they are LEDs (This will probably be an output report)
    0x1A, 0x00, 0xFF,  //     Usage Minimum (0xFF00) 0 ...
    0x2A, 0x03, 0xFF,  //     Usage Maximum (0xFF03) ... to 3
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile) - Lock in the output report of (4 bits)
    0x75, 0x04,        //     Report Size (4) - Also 4 elements in the next report
    0x95, 0x01,        //     Report Count (1) - Also 1 bit each
    0x91, 0x01,        //     Output (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile) - Lock in the constant padding of (4 bits)
    0x15, 0x81,        //     Logical Minimum (-127) - The next elements will have a neg-to-pos output range
    0x25, 0x7F,        //     Logical Maximum (127)
    0x35, 0x81,        //     Physical Minimum (-127)
    0x45, 0x7F,        //     Physical Maximum (127)
    0x05, 0x01,        //     Usage Page (Generic Desktop Ctrls)
    0x09, 0x01,        //     Usage (Pointer)
    0xA1, 0x00,        //     Collection (Physical)
    0x75, 0x08,        //       Report Size (8) - The report size is 1 byte per element (so that -127 to 127 maps to the 256 possible values)
    0x95, 0x04,        //       Report Count (4) - And there will be 4 of them
    0x09, 0x30,        //       Usage (X) - And they are the first 4 axis usages
    0x09, 0x31,        //       Usage (Y)
    0x09, 0x32,        //       Usage (Z)
    0x09, 0x35,        //       Usage (Rz)
    0x81, 0x02,        //       Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position) - Lock in the above input report (4 bytes)
    0xC0,              //     End Collection
    0xC0,              //   End Collection
    0xC0,              // End Collection
};

// The total input report size from the above descriptor is:
// 4 bytes + 8 bytes + (1 bit + 7 bits) + 4 bytes = 17 bytes total.
// The total output report size from the above descriptor is:
// (4 bits + 4 bits) == 1 byte total
const size_t NimbusButtonCount = 13;
const size_t NimbusAxisCount = 4;
const size_t NimbusInputReportSize = 17;
const char* NimbusName = "Virtual Nimbus";

// Report layout:
// |b1| |b2| |b3| |b4| |b5| |b6| |b7| |b8| |b9| |b10| |b11| |b12| |b13| |Axis1| |Axis2| |Axis3| |Axis4|

static void publishReportCallback(Vector<float>& buttonValues, Vector<float>& axisValues, HIDUserDevice *userDevice)
{
    uint8_t reportData[NimbusInputReportSize];

    for (size_t i = 0; i < 12; ++i)
        reportData[i] = (uint8_t)(buttonValues[i] * 255);
    reportData[12] = buttonValues[12] == 0.0 ? 0x01 : 0x00;
    for (size_t i = 13; i < 17; ++i)
        reportData[i] = (uint8_t)((int8_t)(axisValues[i - 13] * 127));

    auto nsReportData = adoptNS([[NSData alloc] initWithBytes:reportData length:NimbusInputReportSize]);
    [userDevice handleReport:nsReportData.get() error:nil];
}

GamepadMapping VirtualGamepad::steelSeriesNimbusMapping()
{
    return {
        NimbusDescriptor,
        sizeof(NimbusDescriptor),
        NimbusName,
        HIDVendorID::SteelSeries2,
        HIDProductID::Nimbus,
        NimbusButtonCount,
        NimbusAxisCount,
        publishReportCallback
    };
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
