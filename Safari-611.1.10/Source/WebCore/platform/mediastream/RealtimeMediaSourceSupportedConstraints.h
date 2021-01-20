/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include <wtf/text/WTFString.h>

namespace WebCore {

enum class MediaConstraintType : uint8_t {
    Unknown,
    Width,
    Height,
    AspectRatio,
    FrameRate,
    FacingMode,
    Volume,
    SampleRate,
    SampleSize,
    EchoCancellation,
    DeviceId,
    GroupId,
    DisplaySurface,
    LogicalSurface,
};

class RealtimeMediaSourceSupportedConstraints {
public:
    RealtimeMediaSourceSupportedConstraints()
    {
    }

    bool supportsWidth() const { return m_supportsWidth; }
    void setSupportsWidth(bool value) { m_supportsWidth = value; }

    bool supportsHeight() const { return m_supportsHeight; }
    void setSupportsHeight(bool value) { m_supportsHeight = value; }

    bool supportsAspectRatio() const { return m_supportsAspectRatio; }
    void setSupportsAspectRatio(bool value) { m_supportsAspectRatio = value; }

    bool supportsFrameRate() const { return m_supportsFrameRate; }
    void setSupportsFrameRate(bool value) { m_supportsFrameRate = value; }

    bool supportsFacingMode() const { return m_supportsFacingMode; }
    void setSupportsFacingMode(bool value) { m_supportsFacingMode = value; }

    bool supportsVolume() const { return m_supportsVolume; }
    void setSupportsVolume(bool value) { m_supportsVolume = value; }

    bool supportsSampleRate() const { return m_supportsSampleRate; }
    void setSupportsSampleRate(bool value) { m_supportsSampleRate = value; }

    bool supportsSampleSize() const { return m_supportsSampleSize; }
    void setSupportsSampleSize(bool value) { m_supportsSampleSize = value; }

    bool supportsEchoCancellation() const { return m_supportsEchoCancellation; }
    void setSupportsEchoCancellation(bool value) { m_supportsEchoCancellation = value; }

    bool supportsDeviceId() const { return m_supportsDeviceId; }
    void setSupportsDeviceId(bool value) { m_supportsDeviceId = value; }

    bool supportsGroupId() const { return m_supportsGroupId; }
    void setSupportsGroupId(bool value) { m_supportsGroupId = value; }

    bool supportsDisplaySurface() const { return m_supportsDisplaySurface; }
    void setSupportsDisplaySurface(bool value) { m_supportsDisplaySurface = value; }

    bool supportsLogicalSurface() const { return m_supportsLogicalSurface; }
    void setSupportsLogicalSurface(bool value) { m_supportsLogicalSurface = value; }

    bool supportsConstraint(MediaConstraintType) const;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, RealtimeMediaSourceSupportedConstraints&);

private:
    bool m_supportsWidth { false };
    bool m_supportsHeight { false };
    bool m_supportsAspectRatio { false };
    bool m_supportsFrameRate { false };
    bool m_supportsFacingMode { false };
    bool m_supportsVolume { false };
    bool m_supportsSampleRate { false };
    bool m_supportsSampleSize { false };
    bool m_supportsEchoCancellation { false };
    bool m_supportsDeviceId { false };
    bool m_supportsGroupId { false };
    bool m_supportsDisplaySurface { false };
    bool m_supportsLogicalSurface { false };
};

template<class Encoder>
void RealtimeMediaSourceSupportedConstraints::encode(Encoder& encoder) const
{
    encoder << m_supportsWidth
        << m_supportsHeight
        << m_supportsAspectRatio
        << m_supportsFrameRate
        << m_supportsFacingMode
        << m_supportsVolume
        << m_supportsSampleRate
        << m_supportsSampleSize
        << m_supportsEchoCancellation
        << m_supportsDeviceId
        << m_supportsGroupId
        << m_supportsDisplaySurface
        << m_supportsLogicalSurface;
}

template<class Decoder>
bool RealtimeMediaSourceSupportedConstraints::decode(Decoder& decoder, RealtimeMediaSourceSupportedConstraints& constraints)
{
    return decoder.decode(constraints.m_supportsWidth)
        && decoder.decode(constraints.m_supportsHeight)
        && decoder.decode(constraints.m_supportsAspectRatio)
        && decoder.decode(constraints.m_supportsFrameRate)
        && decoder.decode(constraints.m_supportsFacingMode)
        && decoder.decode(constraints.m_supportsVolume)
        && decoder.decode(constraints.m_supportsSampleRate)
        && decoder.decode(constraints.m_supportsSampleSize)
        && decoder.decode(constraints.m_supportsEchoCancellation)
        && decoder.decode(constraints.m_supportsDeviceId)
        && decoder.decode(constraints.m_supportsGroupId)
        && decoder.decode(constraints.m_supportsDisplaySurface)
        && decoder.decode(constraints.m_supportsLogicalSurface);
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::MediaConstraintType> {
    using values = EnumValues<
        WebCore::MediaConstraintType,
        WebCore::MediaConstraintType::Unknown,
        WebCore::MediaConstraintType::Width,
        WebCore::MediaConstraintType::Height,
        WebCore::MediaConstraintType::AspectRatio,
        WebCore::MediaConstraintType::FrameRate,
        WebCore::MediaConstraintType::FacingMode,
        WebCore::MediaConstraintType::Volume,
        WebCore::MediaConstraintType::SampleRate,
        WebCore::MediaConstraintType::SampleSize,
        WebCore::MediaConstraintType::EchoCancellation,
        WebCore::MediaConstraintType::DeviceId,
        WebCore::MediaConstraintType::GroupId,
        WebCore::MediaConstraintType::DisplaySurface,
        WebCore::MediaConstraintType::LogicalSurface
    >;
};

} // namespace WTF

#endif // ENABLE(MEDIA_STREAM)
