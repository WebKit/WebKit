/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include "MobileGestaltParameters.h"

#if PLATFORM(IOS_FAMILY)

#include "Logging.h"
#include <pal/spi/ios/MobileGestaltSPI.h>

namespace WebKit {

void MobileGestaltParameters::initialize()
{
    mainScreenScale = MGGetFloat32Answer(kMGQMainScreenScale, 0);
    mainScreenPitch = MGGetSInt32Answer(kMGQMainScreenPitch, 0);
    mainScreenClass = MGGetSInt32Answer(kMGQMainScreenClass, MGScreenClassPad2);
    appleInternalInstallCapability = MGGetBoolAnswer(kMGQAppleInternalInstallCapability);
    iPadCapability = MGGetBoolAnswer(kMGQiPadCapability);
    deviceName = adoptCF(static_cast<CFStringRef>(MGCopyAnswer(kMGQDeviceName, nullptr))).get();
    deviceClassNumber = MGGetSInt32Answer(kMGQDeviceClassNumber, MGDeviceClassInvalid);
    hasExtendedColorDisplay = MGGetBoolAnswer(kMGQHasExtendedColorDisplay);
    deviceCornerRadius = MGGetFloat32Answer(kMGQDeviceCornerRadius, 0);
    supportsForceTouch = MGGetBoolAnswer(kMGQSupportsForceTouch);
    bluetoothCapability = MGGetBoolAnswer(kMGQBluetoothCapability);
    deviceProximityCapability = MGGetBoolAnswer(kMGQDeviceProximityCapability);
    deviceSupportsARKit = MGGetBoolAnswer(kMGQDeviceSupportsARKit);
    timeSyncCapability = MGGetBoolAnswer(kMGQTimeSyncCapability);
    wAPICapability = MGGetBoolAnswer(kMGQWAPICapability);
    mainDisplayRotation = MGGetSInt32Answer(kMGQMainDisplayRotation, 0);
}

void MobileGestaltParameters::populate()
{
    if (MGSetAnswer(kMGQMainScreenScale, adoptCF(CFNumberCreate(nullptr, kCFNumberFloatType, &mainScreenScale)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQMainScreenScale");
    if (MGSetAnswer(kMGQMainScreenPitch, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt32Type, &mainScreenPitch)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQMainScreenPitch");
    if (MGSetAnswer(kMGQMainScreenClass, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt32Type, &mainScreenClass)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQMainScreenClass");
    if (MGSetAnswer(kMGQAppleInternalInstallCapability, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt8Type, &appleInternalInstallCapability)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQAppleInternalInstallCapability");
    if (MGSetAnswer(kMGQiPadCapability, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt8Type, &iPadCapability)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQiPadCapability");
    if (MGSetAnswer(kMGQDeviceName, deviceName.createCFString().get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQDeviceName");
    if (MGSetAnswer(kMGQDeviceClassNumber, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt32Type, &deviceClassNumber)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQDeviceClassNumber");
    if (MGSetAnswer(kMGQHasExtendedColorDisplay, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt8Type, &hasExtendedColorDisplay)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQHasExtendedColorDisplay");
    if (MGSetAnswer(kMGQDeviceCornerRadius, adoptCF(CFNumberCreate(nullptr, kCFNumberFloatType, &deviceCornerRadius)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQDeviceCornerRadius");
    if (MGSetAnswer(kMGQSupportsForceTouch, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt8Type, &supportsForceTouch)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQSupportsForceTouch");
    if (MGSetAnswer(kMGQBluetoothCapability, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt8Type, &bluetoothCapability)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQBluetoothCapability");
    if (MGSetAnswer(kMGQDeviceProximityCapability, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt8Type, &deviceProximityCapability)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQDeviceProximityCapability");
    if (MGSetAnswer(kMGQDeviceSupportsARKit, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt8Type, &deviceSupportsARKit)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQDeviceSupportsARKit");
    if (MGSetAnswer(kMGQTimeSyncCapability, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt8Type, &timeSyncCapability)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQTimeSyncCapability");
    if (MGSetAnswer(kMGQWAPICapability, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt8Type, &wAPICapability)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQWAPICapability");
    if (MGSetAnswer(kMGQMainDisplayRotation, adoptCF(CFNumberCreate(nullptr, kCFNumberSInt32Type, &mainDisplayRotation)).get()))
        RELEASE_LOG_ERROR(Process, "Could not set kMGQMainDisplayRotation");
}

}

#endif // PLATFORM(IOS_FAMILY)
