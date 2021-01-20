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

const uint8_t XboxOneDescriptor[] = {
    0x05, 0x01,        // Usage Page (Generic Desktop Ctrls)
    0x09, 0x05,        // Usage (Game Pad)
    0xA1, 0x01,        // Collection (Application)
                       //   (1) byte for Report ID - Total (1) byte
    0x85, 0x01,        //   Report ID (1)
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x30,        //     Usage (X)
    0x09, 0x31,        //     Usage (Y)
    0x15, 0x00,        //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
    0x95, 0x02,        //     Report Count (2)
    0x75, 0x10,        //     Report Size (16)
                       //     (2) Axes, (2) bytes each - Total (5) bytes
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x09, 0x01,        //   Usage (Pointer)
    0xA1, 0x00,        //   Collection (Physical)
    0x09, 0x32,        //     Usage (Z)
    0x09, 0x35,        //     Usage (Rz)
    0x15, 0x00,        //     Logical Minimum (0)
    0x27, 0xFF, 0xFF, 0x00, 0x00,  //     Logical Maximum (65534)
    0x95, 0x02,        //     Report Count (2)
    0x75, 0x10,        //     Report Size (16)
                       //     (2) Axes, (2) bytes each, for (4) bytes - Total (9) bytes
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x02,        //   Usage Page (Sim Ctrls)
    0x09, 0xC5,        //   Usage (Brake)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x0A,        //   Report Size (10)
                       //   10-bit "Brake" - Total (10) bytes (2) bits
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
                       //   6-bit padding - Total (11) bytes
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x02,        //   Usage Page (Sim Ctrls)
    0x09, 0xC4,        //   Usage (Accelerator)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x03,  //   Logical Maximum (1023)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x0A,        //   Report Size (10)
                       //   10-bit "Accelerator" - Total (12) bytes (2) bits
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x06,        //   Report Size (6)
    0x95, 0x01,        //   Report Count (1)
                       //   6-bit padding - Total (13) bytes
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x01,        //   Usage Page (Generic Desktop Ctrls)
    0x09, 0x39,        //   Usage (Hat switch)
    0x15, 0x01,        //   Logical Minimum (1)
    0x25, 0x08,        //   Logical Maximum (8)
    0x35, 0x00,        //   Physical Minimum (0)
    0x46, 0x3B, 0x01,  //   Physical Maximum (315)
    0x66, 0x14, 0x00,  //   Unit (System: English Rotation, Length: Centimeter)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
                       //   4-bit "Hatswitch" - Total (13) bytes (4) bits
    0x81, 0x42,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,Null State)
    0x75, 0x04,        //   Report Size (4)
    0x95, 0x01,        //   Report Count (1)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x35, 0x00,        //   Physical Minimum (0)
    0x45, 0x00,        //   Physical Maximum (0)
    0x65, 0x00,        //   Unit (None)
                       //   4-bit padding - Total (14) bytes
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x09,        //   Usage Page (Button)
    0x19, 0x01,        //   Usage Minimum (0x01)
    0x29, 0x0F,        //   Usage Maximum (0x0F)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x0F,        //   Report Count (15)
                       //   15-bits for 15 buttons - Total (15) bytes (7) bits
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x01,        //   Report Size (1)
    0x95, 0x01,        //   Report Count (1)
                       //   1-bit padding - Total (16) bytes
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,        //   Usage Page (Consumer)
    0x0A, 0x24, 0x02,  //   Usage (AC Back)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x01,        //   Logical Maximum (1)
    0x95, 0x01,        //   Report Count (1)
    0x75, 0x01,        //   Report Size (1)
                       //   1-bit for AC Back button - Total (16) bytes (1) bit
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //   Logical Minimum (0)
    0x25, 0x00,        //   Logical Maximum (0)
    0x75, 0x07,        //   Report Size (7)
    0x95, 0x01,        //   Report Count (1)
                       //   7-bit padding - Total (17) bytes
    0x81, 0x03,        //   Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x05, 0x0C,        //   Usage Page (Consumer)
    0x09, 0x01,        //   Usage (Consumer Control)
    0x85, 0x02,        //   Report ID (2)
    0xA1, 0x01,        //   Collection (Application)
    0x05, 0x0C,        //     Usage Page (Consumer)
    0x0A, 0x23, 0x02,  //     Usage (AC Home)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x95, 0x01,        //     Report Count (1)
    0x75, 0x01,        //     Report Size (1)
    0x81, 0x02,        //     Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x00,        //     Logical Maximum (0)
    0x75, 0x07,        //     Report Size (7)
    0x95, 0x01,        //     Report Count (1)
    0x81, 0x03,        //     Input (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              //   End Collection
    0x05, 0x0F,        //   Usage Page (PID Page)
    0x09, 0x21,        //   Usage (0x21)
    0x85, 0x03,        //   Report ID (3)
    0xA1, 0x02,        //   Collection (Logical)
    0x09, 0x97,        //     Usage (0x97)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x01,        //     Logical Maximum (1)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x00,        //     Logical Maximum (0)
    0x75, 0x04,        //     Report Size (4)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x03,        //     Output (Const,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x70,        //     Usage (0x70)
    0x15, 0x00,        //     Logical Minimum (0)
    0x25, 0x64,        //     Logical Maximum (100)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x04,        //     Report Count (4)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0x50,        //     Usage (0x50)
    0x66, 0x01, 0x10,  //     Unit (System: SI Linear, Time: Seconds)
    0x55, 0x0E,        //     Unit Exponent (-2)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x09, 0xA7,        //     Usage (0xA7)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0x65, 0x00,        //     Unit (None)
    0x55, 0x00,        //     Unit Exponent (0)
    0x09, 0x7C,        //     Usage (0x7C)
    0x15, 0x00,        //     Logical Minimum (0)
    0x26, 0xFF, 0x00,  //     Logical Maximum (255)
    0x75, 0x08,        //     Report Size (8)
    0x95, 0x01,        //     Report Count (1)
    0x91, 0x02,        //     Output (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position,Non-volatile)
    0xC0,              //   End Collection
    0x85, 0x04,        //   Report ID (4)
    0x05, 0x06,        //   Usage Page (Generic Dev Ctrls)
    0x09, 0x20,        //   Usage (Battery Strength)
    0x15, 0x00,        //   Logical Minimum (0)
    0x26, 0xFF, 0x00,  //   Logical Maximum (255)
    0x75, 0x08,        //   Report Size (8)
    0x95, 0x01,        //   Report Count (1)
    0x81, 0x02,        //   Input (Data,Var,Abs,No Wrap,Linear,Preferred State,No Null Position)
    0xC0,              // End Collection
};

const size_t XboxOneButtonCount = 18;
const size_t XboxOneAxisCount = 4;
const size_t XboxOneReportSize = 17;
const char* XboxOneName = "Virtual Xbox One";

// Report layout:
// |ReportID| |Axis1-L| |Axis1-H| |Axis2-L| |Axis2-H| |Axis3-L| |Axis3-H| |Axis4-L| |Axis4-H|
// |B0 "Brake"L| |B0 "Brake"H| |B1 "Accelerator"L| |B1 "Accelerator"L| |HatSwitch| |Buttons0-8| |Buttons9-15| |AC Back button|

static void publishReportCallback(Vector<float>& buttonValues, Vector<float>& axisValues, HIDUserDevice *userDevice)
{
    uint8_t reportData[XboxOneReportSize];
    size_t reportIndex = 0;

    // Report ID
    reportData[reportIndex++] = 0x01;

    // First 4 axes (2 bytes each)
    // Each axis has a range of 0-65534
    // We track -1 to 1 internally
    // Low byte first, high byte second
    for (size_t i = 0; i < 4; ++i) {
        uint16_t intValue = (axisValues[i] + 1.0) * 32767;
        reportData[reportIndex++] = intValue & 0x00ff;
        reportData[reportIndex++] = intValue >> 8;
    }

    // 10bit + 6bit padding "button"
    uint16_t intValue = buttonValues[0] * 1023;
    reportData[reportIndex++] = intValue & 0x00ff;
    reportData[reportIndex++] = intValue >> 8;

    // 10bit + 6bit padding "button"
    intValue = buttonValues[1] * 1023;
    reportData[reportIndex++] = intValue & 0x00ff;
    reportData[reportIndex++] = intValue >> 8;

    // Don't support changing the hat switch value yet, but fill in a 0 value for it
    reportData[reportIndex++] = 0x00;

    // The "15 buttons" take up 2 bytes, but there aren't nearly 15 physical buttons on the device
    // |b8|b7|b6|b5|b4|b3|b2|b1| |0|bf|be|bd|bc|bb|ba|b9|

    uint8_t buttonByte = 0x00;
    for (size_t i = 2; i < 10; ++i) {
        long buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i - 2);
    }
    reportData[reportIndex++] = buttonByte;

    buttonByte = 0x00;
    for (size_t i = 10; i < 17; ++i) {
        long buttonBit = lround(buttonValues[i]);
        buttonByte |= buttonBit << (i - 10);
    }
    reportData[reportIndex++] = buttonByte;

    // The final byte is the final standalone AC back button
    reportData[reportIndex++] = lround(buttonValues[17]) & 0x01;

    auto nsReportData = adoptNS([[NSData alloc] initWithBytes:reportData length:XboxOneReportSize]);
    [userDevice handleReport:nsReportData.get() error:nil];
}

GamepadMapping VirtualGamepad::microsoftXboxOneMapping()
{
    return {
        XboxOneDescriptor,
        sizeof(XboxOneDescriptor),
        XboxOneName,
        HIDVendorID::Microsoft,
        HIDProductID::XboxOne1,
        XboxOneButtonCount,
        XboxOneAxisCount,
        publishReportCallback
    };
}

} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
