/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "Device.h"

#if PLATFORM(IOS_FAMILY)

#include <mutex>
#include <pal/spi/ios/MobileGestaltSPI.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

bool deviceClassIsSmallScreen()
{
    static auto deviceClass = MGGetSInt32Answer(kMGQDeviceClassNumber, MGDeviceClassInvalid);
    return deviceClass == MGDeviceClassiPhone || deviceClass == MGDeviceClassiPod || deviceClass == MGDeviceClassWatch;
}

String deviceName()
{
#if ENABLE(MOBILE_GESTALT_DEVICE_NAME)
    static NeverDestroyed<RetainPtr<CFStringRef>> deviceName;
    static std::once_flag onceKey;
    std::call_once(onceKey, [] {
        deviceName.get() = adoptCF(static_cast<CFStringRef>(MGCopyAnswer(kMGQDeviceName, nullptr)));
    });
    return deviceName.get().get();
#else
    return "iPhone"_s;
#endif
}

bool deviceHasIPadCapability()
{
    static bool deviceHasIPadCapability = MGGetBoolAnswer(kMGQiPadCapability);
    return deviceHasIPadCapability;
}

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY)
