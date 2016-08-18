/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "RealtimeMediaSourceSupportedConstraints.h"

#if ENABLE(MEDIA_STREAM)

#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

const AtomicString& RealtimeMediaSourceSupportedConstraints::nameForConstraint(MediaConstraintType constraint)
{
    static NeverDestroyed<AtomicString> unknownConstraintName(emptyString());
    static NeverDestroyed<AtomicString> widthConstraintName("width");
    static NeverDestroyed<AtomicString> heightConstraintName("height");
    static NeverDestroyed<AtomicString> aspectRatioConstraintName("aspectRatio");
    static NeverDestroyed<AtomicString> frameRateConstraintName("frameRate");
    static NeverDestroyed<AtomicString> facingModeConstraintName("facingMode");
    static NeverDestroyed<AtomicString> volumeConstraintName("volume");
    static NeverDestroyed<AtomicString> sampleRateConstraintName("sampleRate");
    static NeverDestroyed<AtomicString> sampleSizeConstraintName("sampleSize");
    static NeverDestroyed<AtomicString> echoCancellationConstraintName("echoCancellation");
    static NeverDestroyed<AtomicString> deviceIdConstraintName("deviceId");
    static NeverDestroyed<AtomicString> groupIdConstraintName("groupId");
    switch (constraint) {
    case MediaConstraintType::Unknown:
        return unknownConstraintName;
    case MediaConstraintType::Width:
        return widthConstraintName;
    case MediaConstraintType::Height:
        return heightConstraintName;
    case MediaConstraintType::AspectRatio:
        return aspectRatioConstraintName;
    case MediaConstraintType::FrameRate:
        return frameRateConstraintName;
    case MediaConstraintType::FacingMode:
        return facingModeConstraintName;
    case MediaConstraintType::Volume:
        return volumeConstraintName;
    case MediaConstraintType::SampleRate:
        return sampleRateConstraintName;
    case MediaConstraintType::SampleSize:
        return sampleSizeConstraintName;
    case MediaConstraintType::EchoCancellation:
        return echoCancellationConstraintName;
    case MediaConstraintType::DeviceId:
        return deviceIdConstraintName;
    case MediaConstraintType::GroupId:
        return groupIdConstraintName;
    }
}

MediaConstraintType RealtimeMediaSourceSupportedConstraints::constraintFromName(const String& constraintName)
{
    static NeverDestroyed<HashMap<AtomicString, MediaConstraintType>> nameToConstraintMap;
    HashMap<AtomicString, MediaConstraintType>& nameToConstraintMapValue = nameToConstraintMap.get();
    if (!nameToConstraintMapValue.size()) {
        nameToConstraintMapValue.add("width", MediaConstraintType::Width);
        nameToConstraintMapValue.add("height", MediaConstraintType::Height);
        nameToConstraintMapValue.add("aspectRatio", MediaConstraintType::AspectRatio);
        nameToConstraintMapValue.add("frameRate", MediaConstraintType::FrameRate);
        nameToConstraintMapValue.add("facingMode", MediaConstraintType::FacingMode);
        nameToConstraintMapValue.add("volume", MediaConstraintType::Volume);
        nameToConstraintMapValue.add("sampleRate", MediaConstraintType::SampleRate);
        nameToConstraintMapValue.add("sampleSize", MediaConstraintType::SampleSize);
        nameToConstraintMapValue.add("echoCancellation", MediaConstraintType::EchoCancellation);
        nameToConstraintMapValue.add("deviceId", MediaConstraintType::DeviceId);
        nameToConstraintMapValue.add("groupId", MediaConstraintType::GroupId);
    }
    auto iter = nameToConstraintMapValue.find(constraintName);
    return iter == nameToConstraintMapValue.end() ? MediaConstraintType::Unknown : iter->value;
}

bool RealtimeMediaSourceSupportedConstraints::supportsConstraint(MediaConstraintType constraint) const
{
    switch (constraint) {
    case MediaConstraintType::Unknown:
        return false;
    case MediaConstraintType::Width:
        return supportsWidth();
    case MediaConstraintType::Height:
        return supportsHeight();
    case MediaConstraintType::AspectRatio:
        return supportsAspectRatio();
    case MediaConstraintType::FrameRate:
        return supportsFrameRate();
    case MediaConstraintType::FacingMode:
        return supportsFacingMode();
    case MediaConstraintType::Volume:
        return supportsVolume();
    case MediaConstraintType::SampleRate:
        return supportsSampleRate();
    case MediaConstraintType::SampleSize:
        return supportsSampleSize();
    case MediaConstraintType::EchoCancellation:
        return supportsEchoCancellation();
    case MediaConstraintType::DeviceId:
        return supportsDeviceId();
    case MediaConstraintType::GroupId:
        return supportsGroupId();
    }
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
