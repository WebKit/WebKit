/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "MediaConstraintType.h"

#if ENABLE(MEDIA_STREAM)

namespace WebCore {

String convertToString(MediaConstraintType type)
{
    switch (type) {
    case MediaConstraintType::Unknown:
        return ""_s;
    case MediaConstraintType::Width:
        return "width"_s;
    case MediaConstraintType::Height:
        return "height"_s;
    case MediaConstraintType::AspectRatio:
        return "aspectRatio"_s;
    case MediaConstraintType::FrameRate:
        return "frameRate"_s;
    case MediaConstraintType::FacingMode:
        return "facingMode"_s;
    case MediaConstraintType::Volume:
        return "volume"_s;
    case MediaConstraintType::SampleRate:
        return "sampleRate"_s;
    case MediaConstraintType::SampleSize:
        return "sampleSize"_s;
    case MediaConstraintType::EchoCancellation:
        return "echoCancellation"_s;
    case MediaConstraintType::DeviceId:
        return "deviceId"_s;
    case MediaConstraintType::GroupId:
        return "groupId"_s;
    case MediaConstraintType::DisplaySurface:
        return "displaySurface"_s;
    case MediaConstraintType::LogicalSurface:
        return "logicalSurface"_s;
    case MediaConstraintType::FocusDistance:
        return "focusDistance"_s;
    case MediaConstraintType::WhiteBalanceMode:
        return "whiteBalanceMode"_s;
    case MediaConstraintType::Zoom:
        return "zoom"_s;
    case MediaConstraintType::Torch:
        return "torch"_s;
    }

    ASSERT_NOT_REACHED();
    return { };
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
