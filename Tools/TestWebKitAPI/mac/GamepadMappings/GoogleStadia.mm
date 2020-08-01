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

const uint8_t StadiaDescriptor[] = {
    0x05, 0x01,         // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,         // Usage (Game Pad)
    0xA1, 0x01,         // Collection (Application)
                        //   (1) byte report ID - (1) byte total
    0x85, 0x03,         //   Report ID (3)
    0x05, 0x01,         //   Usage Page (Generic Desktop Ctrls)
    0x75, 0x04,         //   Report Size (4)
    0x95, 0x01,         //   Report Count (1)
    0x25, 0x07,         //   Logical Maximum (7)
    0x46, 0x3B, 0x01,   //   Physical Maximum (315)
    0x65, 0x14,         //   Unit (System: English Rotation, Length: Centimeter)
    0x09, 0x39,         //   Usage (Hat switch)
                        //   (4) bit hatswitch - (1) byte (4) bits total
    0x81, 0x42,         //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x45, 0x00,         //   Physical Maximum (0)
    0x65, 0x00,         //   Unit (None)
    0x75, 0x01,         //   Report Size (1)
    0x95, 0x04,         //   Report Count (4)
                        //   (4) bit padding - (2) bytes total
    0x81, 0x01,         //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,         //   Usage Page (Button)
    0x15, 0x00,         //   Logical Minimum (0)
    0x25, 0x01,         //   Logical Maximum (1)
    0x75, 0x01,         //   Report Size (1)
    0x95, 0x0F,         //   Report Count (15)
    0x09, 0x12,         //   Usage (0x12)
    0x09, 0x11,         //   Usage (0x11)
    0x09, 0x14,         //   Usage (0x14)
    0x09, 0x13,         //   Usage (0x13)
    0x09, 0x0D,         //   Usage (0x0D)
    0x09, 0x0C,         //   Usage (0x0C)
    0x09, 0x0B,         //   Usage (0x0B)
    0x09, 0x0F,         //   Usage (0x0F)
    0x09, 0x0E,         //   Usage (0x0E)
    0x09, 0x08,         //   Usage (0x08)
    0x09, 0x07,         //   Usage (0x07)
    0x09, 0x05,         //   Usage (0x05)
    0x09, 0x04,         //   Usage (0x04)
    0x09, 0x02,         //   Usage (0x02)
    0x09, 0x01,         //   Usage (0x01)
                        //   (15) buttons - (3) bytes (7) bits total
    0x81, 0x02,         //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x75, 0x01,         //   Report Size (1)
    0x95, 0x01,         //   Report Count (1)
                        //   (1) padding - (4) bytes total
    0x81, 0x01,         //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,         //   Usage Page (Generic Desktop Ctrls)
    0x15, 0x01,         //   Logical Minimum (1)
    0x26, 0xFF, 0x00,   //   Logical Maximum (255)
    0x09, 0x01,         //   Usage (Pointer)
    0xA1, 0x00,         //   Collection (Physical)
    0x09, 0x30,         //     Usage (X)
    0x09, 0x31,         //     Usage (Y)
    0x75, 0x08,         //     Report Size (8)
    0x95, 0x02,         //     Report Count (2)
                        //     (2) axes 1 byte each - (6) bytes total
    0x81, 0x02,         //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,               //   End Collection
    0x09, 0x01,         //   Usage (Pointer)
    0xA1, 0x00,         //   Collection (Physical)
    0x09, 0x32,         //     Usage (Z)
    0x09, 0x35,         //     Usage (Rz)
    0x75, 0x08,         //     Report Size (8)
    0x95, 0x02,         //     Report Count (2)
                        //     (2) axes 1 byte each - (8) bytes total
    0x81, 0x02,         //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,               //   End Collection
    0x05, 0x02,         //   Usage Page (Sim Ctrls)
    0x75, 0x08,         //   Report Size (8)
    0x95, 0x02,         //   Report Count (2)
    0x15, 0x00,         //   Logical Minimum (0)
    0x26, 0xFF, 0x00,   //   Logical Maximum (255)
    0x09, 0xC5,         //   Usage (Brake)
    0x09, 0xC4,         //   Usage (Accelerator)
                        //   (2) "analog buttons"" 1 byte each - (10) bytes total
    0x81, 0x02,         //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,         //   Usage Page (Consumer)
    0x15, 0x00,         //   Logical Minimum (0)
    0x25, 0x01,         //   Logical Maximum (1)
    0x09, 0xE9,         //   Usage (Volume Increment)
    0x09, 0xEA,         //   Usage (Volume Decrement)
    0x75, 0x01,         //   Report Size (1)
    0x95, 0x02,         //   Report Count (2)
                        //   (2) 1 bit buttons - (10) bytes (2) bits total
    0x81, 0x02,         //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x09, 0xCD,         //   Usage (Play/Pause)
    0x95, 0x01,         //   Report Count (1)
                        //   (1) 1 bit buttons - (10) bytes (3) bits total
    0x81, 0x02,         //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x95, 0x05,         //   Report Count (5)
                        //   (5) bits padding - (11) bytes total
    0x81, 0x01,         //   Input (Const,Array,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x85, 0x05,         //   Report ID (5)
    0x06, 0x0F, 0x00,   //   Usage Page (PID Page)
    0x09, 0x97,         //   Usage (0x97)
    0x75, 0x10,         //   Report Size (16)
    0x95, 0x02,         //   Report Count (2)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //   Logical Maximum (65534)
    0x91, 0x02,         //   Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,               // End Collection
};

// FIXME: Hatswitch is not yet addressable by test
const size_t StadiaButtonCount = 19;
const size_t StadiaAxisCount = 4;
const size_t StadiaReportSize = 11;
const char* StadiaName = "Virtual Stadia";

// Report layout:
// |ReportID| |Pad, 4bit hat| |8 buttons| |Pad 7 buttons| |X| |Y| |Z| |Rz| |Brake| |Accelerator| |Pad, vup vdown pause|

// Brake and acclerator are analog values for back shoulder buttons, which are also represented in the 15 digital buttons
// I cannot identify physical actuators for volume-up, volume-down, and pause.

static void publishReportCallback(Vector<float>& buttonValues, Vector<float>& axisValues, HIDUserDevice *userDevice)
{
    uint8_t reportData[StadiaReportSize];
    bzero(reportData, StadiaReportSize);
    size_t reportIndex = 0;

    // Report ID (Normal input report is #3)
    reportData[reportIndex++] = 0x03;

    // No interface to change hatswitch value at this time
    reportData[reportIndex++] = 0x08;

    // Digital buttons span the next 2 bytes
    uint8_t buttonByte = 0x00;
    long buttonBit;
    for (size_t i = 0; i < 8; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << i;
    }
    reportData[reportIndex++] = buttonByte;

    buttonByte = 0x00;
    for (size_t i = 8; i < 15; ++i) {
        buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i - 8);
    }
    reportData[reportIndex++] = buttonByte;

    // Axes (1 byte each)
    for (size_t i = 0; i < 4; ++i)
        reportData[reportIndex++] = (uint8_t)(axisValues[i] * 255);

    // Brake and Accelerator are also button 14 and 15
    reportData[reportIndex++] = (uint8_t)(buttonValues[13] * 255);
    reportData[reportIndex++] = (uint8_t)(buttonValues[14] * 255);

    // Final byte has 5 padding bits, 3 button bits in it, but no way to actuate them
    reportData[reportIndex++] = 0x00;

    auto nsReportData = adoptNS([[NSData alloc] initWithBytes:reportData length:StadiaReportSize]);
    [userDevice handleReport:nsReportData.get() error:nil];
}

GamepadMapping VirtualGamepad::googleStadiaMapping()
{
    return {
        StadiaDescriptor,
        sizeof(StadiaDescriptor),
        StadiaName,
        HIDVendorID::Google,
        HIDProductID::StadiaA,
        StadiaButtonCount,
        StadiaAxisCount,
        publishReportCallback
    };
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
