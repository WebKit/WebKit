/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Google Inc. nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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

#if ENABLE(MEDIA_STREAM)
#include "RealtimeMediaSourceSettings.h"

#include <wtf/NeverDestroyed.h>
#include <wtf/text/StringBuilder.h>

namespace WebCore {

RealtimeMediaSourceSettings RealtimeMediaSourceSettings::isolatedCopy() const
{
    return { m_width, m_height , m_frameRate, m_facingMode, m_volume , m_sampleRate, m_sampleSize, m_echoCancellation, m_deviceId.isolatedCopy(), m_groupId.isolatedCopy(), m_label.isolatedCopy(), m_displaySurface, m_logicalSurface, m_whiteBalanceMode, m_zoom, m_torch, RealtimeMediaSourceSupportedConstraints { m_supportedConstraints } };
}

VideoFacingMode RealtimeMediaSourceSettings::videoFacingModeEnum(const String& mode)
{
    if (mode == "user"_s)
        return VideoFacingMode::User;
    if (mode == "environment"_s)
        return VideoFacingMode::Environment;
    if (mode == "left"_s)
        return VideoFacingMode::Left;
    if (mode == "right"_s)
        return VideoFacingMode::Right;

    return VideoFacingMode::Unknown;
}

String RealtimeMediaSourceSettings::convertFlagsToString(const OptionSet<RealtimeMediaSourceSettings::Flag> flags)
{
    StringBuilder builder;

    builder.append("[ ");
    for (auto flag : flags) {
        if (!builder.isEmpty())
            builder.append(", ");

        switch (flag) {
        case RealtimeMediaSourceSettings::Width:
            builder.append("Width");
            break;
        case RealtimeMediaSourceSettings::Height:
            builder.append("Height");
            break;
        case RealtimeMediaSourceSettings::FrameRate:
            builder.append("FrameRate");
            break;
        case RealtimeMediaSourceSettings::FacingMode:
            builder.append("FacingMode");
            break;
        case RealtimeMediaSourceSettings::Volume:
            builder.append("Volume");
            break;
        case RealtimeMediaSourceSettings::SampleRate:
            builder.append("SampleRate");
            break;
        case RealtimeMediaSourceSettings::SampleSize:
            builder.append("SampleSize");
            break;
        case RealtimeMediaSourceSettings::EchoCancellation:
            builder.append("EchoCancellation");
            break;
        case RealtimeMediaSourceSettings::DeviceId:
            builder.append("DeviceId");
            break;
        case RealtimeMediaSourceSettings::GroupId:
            builder.append("GroupId");
            break;
        case RealtimeMediaSourceSettings::Label:
            builder.append("Label");
            break;
        case RealtimeMediaSourceSettings::DisplaySurface:
            builder.append("DisplaySurface");
            break;
        case RealtimeMediaSourceSettings::LogicalSurface:
            builder.append("LogicalSurface");
            break;
        case RealtimeMediaSourceSettings::WhiteBalanceMode:
            builder.append("WhiteBalanceMode");
            break;
        case RealtimeMediaSourceSettings::Zoom:
            builder.append("Zoom");
            break;
        case RealtimeMediaSourceSettings::Torch:
            builder.append("Torch");
            break;
        }
    }
    builder.append(" ]");

    return builder.toString();
}

OptionSet<RealtimeMediaSourceSettings::Flag> RealtimeMediaSourceSettings::difference(const RealtimeMediaSourceSettings& that) const
{
    OptionSet<RealtimeMediaSourceSettings::Flag> difference;

    if (width() != that.width())
        difference.add(RealtimeMediaSourceSettings::Width);
    if (height() != that.height())
        difference.add(RealtimeMediaSourceSettings::Height);
    if (frameRate() != that.frameRate())
        difference.add(RealtimeMediaSourceSettings::FrameRate);
    if (facingMode() != that.facingMode())
        difference.add(RealtimeMediaSourceSettings::FacingMode);
    if (volume() != that.volume())
        difference.add(RealtimeMediaSourceSettings::Volume);
    if (sampleRate() != that.sampleRate())
        difference.add(RealtimeMediaSourceSettings::SampleRate);
    if (sampleSize() != that.sampleSize())
        difference.add(RealtimeMediaSourceSettings::SampleSize);
    if (echoCancellation() != that.echoCancellation())
        difference.add(RealtimeMediaSourceSettings::EchoCancellation);
    if (deviceId() != that.deviceId())
        difference.add(RealtimeMediaSourceSettings::DeviceId);
    if (groupId() != that.groupId())
        difference.add(RealtimeMediaSourceSettings::GroupId);
    if (label() != that.label())
        difference.add(RealtimeMediaSourceSettings::Label);
    if (displaySurface() != that.displaySurface())
        difference.add(RealtimeMediaSourceSettings::DisplaySurface);
    if (logicalSurface() != that.logicalSurface())
        difference.add(RealtimeMediaSourceSettings::LogicalSurface);
    if (whiteBalanceMode() != that.whiteBalanceMode())
        difference.add(RealtimeMediaSourceSettings::WhiteBalanceMode);
    if (zoom() != that.zoom())
        difference.add(RealtimeMediaSourceSettings::Zoom);
    if (torch() != that.torch())
        difference.add(RealtimeMediaSourceSettings::Torch);

    return difference;
}

String convertEnumerationToString(VideoFacingMode enumerationValue)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("unknown"),
        MAKE_STATIC_STRING_IMPL("user"),
        MAKE_STATIC_STRING_IMPL("environment"),
        MAKE_STATIC_STRING_IMPL("left"),
        MAKE_STATIC_STRING_IMPL("right"),
    };
    static_assert(static_cast<size_t>(VideoFacingMode::Unknown) == 0, "VideoFacingMode::Unknown is not 0 as expected");
    static_assert(static_cast<size_t>(VideoFacingMode::User) == 1, "VideoFacingMode::User is not 1 as expected");
    static_assert(static_cast<size_t>(VideoFacingMode::Environment) == 2, "VideoFacingMode::Environment is not 2 as expected");
    static_assert(static_cast<size_t>(VideoFacingMode::Left) == 3, "VideoFacingMode::Left is not 3 as expected");
    static_assert(static_cast<size_t>(VideoFacingMode::Right) == 4, "VideoFacingMode::Right is not 4 as expected");
    ASSERT(static_cast<size_t>(enumerationValue) < std::size(values));
    return values[static_cast<size_t>(enumerationValue)];
}

String RealtimeMediaSourceSettings::displaySurface(DisplaySurfaceType surface)
{
    static const NeverDestroyed<String> values[] = {
        MAKE_STATIC_STRING_IMPL("monitor"),
        MAKE_STATIC_STRING_IMPL("window"),
        MAKE_STATIC_STRING_IMPL("application"),
        MAKE_STATIC_STRING_IMPL("browser"),
        MAKE_STATIC_STRING_IMPL("invalid"),
    };

    static_assert(static_cast<size_t>(DisplaySurfaceType::Monitor) == 0, "RealtimeMediaSourceSettings::DisplaySurface::Monitor is not 0 as expected");
    static_assert(static_cast<size_t>(DisplaySurfaceType::Window) == 1, "RealtimeMediaSourceSettings::DisplaySurface::Window is not 1 as expected");
    static_assert(static_cast<size_t>(DisplaySurfaceType::Application) == 2, "RealtimeMediaSourceSettings::DisplaySurface::Application is not 0 as expected");
    static_assert(static_cast<size_t>(DisplaySurfaceType::Browser) == 3, "RealtimeMediaSourceSettings::DisplaySurface::Browser is not 1 as expected");
    static_assert(static_cast<size_t>(DisplaySurfaceType::Invalid) == 4, "RealtimeMediaSourceSettings::DisplaySurface::Invalid is not 0 as expected");
    ASSERT(static_cast<size_t>(surface) < std::size(values));
    return values[static_cast<size_t>(surface)];
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
