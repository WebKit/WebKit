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
#include <wtf/text/AtomString.h>
#include <wtf/text/AtomStringHash.h>

namespace WebCore {

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
    case MediaConstraintType::DisplaySurface:
        return supportsDisplaySurface();
    case MediaConstraintType::LogicalSurface:
        return supportsLogicalSurface();
    }

    ASSERT_NOT_REACHED();
    return false;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
