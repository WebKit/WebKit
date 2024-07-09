/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaTrackCapabilities.h"

#if ENABLE(MEDIA_STREAM)

#include "JSMeteringMode.h"
#include "RealtimeMediaSourceCapabilities.h"

namespace WebCore {

static DoubleRange capabilityDoubleRange(const DoubleCapabilityRange& value)
{
    auto min = value.min();
    auto max = value.max();

    ASSERT(min != std::numeric_limits<double>::min() || max != std::numeric_limits<double>::max());

    DoubleRange range;
    if (min != std::numeric_limits<double>::min())
        range.min = min;
    if (max != std::numeric_limits<double>::max())
        range.max = max;

    return range;
}

static LongRange capabilityLongRange(const LongCapabilityRange& value)
{
    return { value.max(), value.min() };
}

static Vector<String> capabilityStringVector(const Vector<VideoFacingMode>& modes)
{
    return modes.map([](auto& mode) {
        return convertEnumerationToString(mode);
    });
}

static Vector<String> capabilityStringVector(const Vector<MeteringMode>& modes)
{
    return modes.map([](auto& mode) {
        return convertEnumerationToString(mode);
    });
}

static Vector<bool> capabilityBooleanVector(RealtimeMediaSourceCapabilities::EchoCancellation cancellation)
{
    Vector<bool> result;
    result.reserveInitialCapacity(2);
    result.append(true);
    if (cancellation == RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite)
        result.append(false);
    return result;
}

static Vector<bool> capabilityBooleanVector(RealtimeMediaSourceCapabilities::BackgroundBlur backgroundBlur)
{
    Vector<bool> result;
    result.reserveInitialCapacity(2);
    switch (backgroundBlur) {
    case RealtimeMediaSourceCapabilities::BackgroundBlur::On:
        result.append(true);
        break;
    case RealtimeMediaSourceCapabilities::BackgroundBlur::Off:
        result.append(false);
        break;
    case RealtimeMediaSourceCapabilities::BackgroundBlur::OnOff:
        result.append(false);
        result.append(true);
        break;
    }
    return result;
}

static Vector<bool> powerEfficientCapabilityVector(bool powerEfficient)
{
    Vector<bool> result;
    result.reserveInitialCapacity(2);
    result.append(false);
    if (powerEfficient)
        result.append(true);

    return result;
}

MediaTrackCapabilities toMediaTrackCapabilities(const RealtimeMediaSourceCapabilities& capabilities, const String& groupId)
{
    MediaTrackCapabilities result;
    if (capabilities.supportsWidth())
        result.width = capabilityLongRange(capabilities.width());
    if (capabilities.supportsHeight())
        result.height = capabilityLongRange(capabilities.height());
    if (capabilities.supportsAspectRatio())
        result.aspectRatio = capabilityDoubleRange(capabilities.aspectRatio());
    if (capabilities.supportsFrameRate())
        result.frameRate = capabilityDoubleRange(capabilities.frameRate());
    if (capabilities.supportsFacingMode())
        result.facingMode = capabilityStringVector(capabilities.facingMode());
    if (capabilities.supportsVolume())
        result.volume = capabilityDoubleRange(capabilities.volume());
    if (capabilities.supportsSampleRate())
        result.sampleRate = capabilityLongRange(capabilities.sampleRate());
    if (capabilities.supportsSampleSize())
        result.sampleSize = capabilityLongRange(capabilities.sampleSize());
    if (capabilities.supportsEchoCancellation())
        result.echoCancellation = capabilityBooleanVector(capabilities.echoCancellation());
    if (capabilities.supportsDeviceId())
        result.deviceId = capabilities.deviceId();
    if (capabilities.supportsGroupId())
        result.groupId = groupId;
    if (capabilities.supportsFocusDistance())
        result.focusDistance = capabilityDoubleRange(capabilities.focusDistance());
    if (capabilities.supportsWhiteBalanceMode())
        result.whiteBalanceMode = capabilityStringVector(capabilities.whiteBalanceModes());
    if (capabilities.supportsZoom())
        result.zoom = capabilityDoubleRange(capabilities.zoom());
    if (capabilities.supportsTorch())
        result.torch = capabilities.torch();
    if (capabilities.supportsBackgroundBlur())
        result.backgroundBlur = capabilityBooleanVector(capabilities.backgroundBlur());

    if (capabilities.supportsPowerEfficient())
        result.powerEfficient = powerEfficientCapabilityVector(capabilities.powerEfficient());

    return result;
}

}

#endif
