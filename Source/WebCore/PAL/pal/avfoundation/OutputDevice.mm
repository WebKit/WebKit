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

#include "config.h"
#include "OutputDevice.h"

#if USE(AVFOUNDATION)

#include <pal/spi/cocoa/AVFoundationSPI.h>

#include <pal/cocoa/AVFoundationSoftLink.h>

// FIXME(rdar://70358894): Remove once -allowsHeadTrackedSpatialAudio lands:
@interface AVOutputDevice (AllowsHeadTrackedSpatialAudio)
- (BOOL)allowsHeadTrackedSpatialAudio;
@end

namespace PAL {

OutputDevice::OutputDevice(RetainPtr<AVOutputDevice>&& device)
    : m_device(WTFMove(device))
{
}

String OutputDevice::name() const
{
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return [m_device name];
ALLOW_DEPRECATED_DECLARATIONS_END
}

uint8_t OutputDevice::deviceFeatures() const
{
    auto avDeviceFeatures = [m_device deviceFeatures];
    uint8_t deviceFeatures { 0 };
    if (avDeviceFeatures & AVOutputDeviceFeatureAudio)
        deviceFeatures |= (uint8_t)DeviceFeatures::Audio;
    if (avDeviceFeatures & AVOutputDeviceFeatureScreen)
        deviceFeatures |= (uint8_t)DeviceFeatures::Screen;
    if (avDeviceFeatures & AVOutputDeviceFeatureVideo)
        deviceFeatures |= (uint8_t)DeviceFeatures::Video;
    return deviceFeatures;
}

bool OutputDevice::supportsSpatialAudio() const
{
#if HAVE(AVOUTPUTDEVICE_SPATIALAUDIO)
    if (![m_device respondsToSelector:@selector(supportsHeadTrackedSpatialAudio)]
        || ![m_device supportsHeadTrackedSpatialAudio])
        return false;

    return ![m_device respondsToSelector:@selector(allowsHeadTrackedSpatialAudio)]
        || [m_device allowsHeadTrackedSpatialAudio];
#else
    return false;
#endif
}

}

#endif
