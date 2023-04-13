/*
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSourceSettings.h"
#include <wtf/EnumTraits.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace WebCore {

class CapabilityValueOrRange {
public:

    enum Type {
        Undefined,
        Double,
        ULong,
        DoubleRange,
        ULongRange,
    };
    Type type() const { return m_type; }

    union ValueUnion {
        double asDouble;
        int asInt;
    };

    CapabilityValueOrRange()
        : m_type(Undefined)
    {
    }

    CapabilityValueOrRange(double value)
        : m_type(Double)
    {
        m_minOrValue.asDouble = value;
    }

    CapabilityValueOrRange(int value)
        : m_type(ULong)
    {
        m_minOrValue.asInt = value;
    }

    CapabilityValueOrRange(double min, double max)
        : m_type(DoubleRange)
    {
        m_minOrValue.asDouble = min;
        m_max.asDouble = max;
    }
    
    CapabilityValueOrRange(int min, int max)
        : m_type(ULongRange)
    {
        m_minOrValue.asInt = min;
        m_max.asInt = max;
    }

    const ValueUnion& rangeMin() const
    {
        ASSERT(m_type == DoubleRange || m_type == ULongRange);
        return m_minOrValue;
    }

    const ValueUnion& rangeMax() const
    {
        ASSERT(m_type == DoubleRange || m_type == ULongRange);
        return m_max;
    }

    const ValueUnion& value() const
    {
        ASSERT(m_type == Double || m_type == ULong);
        return m_minOrValue;
    }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static WARN_UNUSED_RETURN bool decode(Decoder&, CapabilityValueOrRange&);

private:
    ValueUnion m_minOrValue { };
    ValueUnion m_max { };
    Type m_type { Undefined };
};

template<class Encoder>
void CapabilityValueOrRange::encode(Encoder& encoder) const
{
    encoder.encodeObject(m_minOrValue);
    encoder.encodeObject(m_max);
    encoder << m_type;
}

template<class Decoder>
bool CapabilityValueOrRange::decode(Decoder& decoder, CapabilityValueOrRange& valueOrRange)
{
    auto minOrValue = decoder.template decodeObject<ValueUnion>();
    if (!minOrValue)
        return false;

    auto max = decoder.template decodeObject<ValueUnion>();
    if (!max)
        return false;

    auto type = decoder.template decode<Type>();
    if (!type)
        return false;

    valueOrRange.m_minOrValue = *minOrValue;
    valueOrRange.m_max = *max;
    valueOrRange.m_type = *type;
    return true;
}

class RealtimeMediaSourceCapabilities {
public:
    RealtimeMediaSourceCapabilities() = default;
    RealtimeMediaSourceCapabilities(const RealtimeMediaSourceSupportedConstraints& supportedConstraints)
        : m_supportedConstraints(supportedConstraints)
    {
    }
    
    enum class EchoCancellation : bool {
        ReadOnly = 0,
        ReadWrite = 1,
    };
    
    RealtimeMediaSourceCapabilities(CapabilityValueOrRange&& width, CapabilityValueOrRange&& height, CapabilityValueOrRange&& aspectRatio, CapabilityValueOrRange&& frameRate, Vector<VideoFacingMode>&& facingMode, CapabilityValueOrRange&& volume, CapabilityValueOrRange&& sampleRate, CapabilityValueOrRange&& sampleSize, EchoCancellation&& echoCancellation, AtomString&& deviceId, AtomString&& groupId, CapabilityValueOrRange&& focusDistance, CapabilityValueOrRange&& zoom, RealtimeMediaSourceSupportedConstraints&& supportedConstraints)
        : m_width(WTFMove(width))
        , m_height(WTFMove(height))
        , m_aspectRatio(WTFMove(aspectRatio))
        , m_frameRate(WTFMove(frameRate))
        , m_facingMode(WTFMove(facingMode))
        , m_volume(WTFMove(volume))
        , m_sampleRate(WTFMove(sampleRate))
        , m_sampleSize(WTFMove(sampleSize))
        , m_echoCancellation(WTFMove(echoCancellation))
        , m_deviceId(WTFMove(deviceId))
        , m_groupId(WTFMove(groupId))
        , m_focusDistance(WTFMove(focusDistance))
        , m_zoom(WTFMove(zoom))
        , m_supportedConstraints(WTFMove(supportedConstraints))
    {
    }

    ~RealtimeMediaSourceCapabilities() = default;

    static const RealtimeMediaSourceCapabilities& emptyCapabilities()
    {
        static NeverDestroyed<RealtimeMediaSourceCapabilities> emptyCapabilities;
        return emptyCapabilities;
    }

    bool supportsWidth() const { return m_supportedConstraints.supportsWidth(); }
    const CapabilityValueOrRange& width() const { return m_width; }
    void setWidth(const CapabilityValueOrRange& width) { m_width = width; }

    bool supportsHeight() const { return m_supportedConstraints.supportsHeight(); }
    const CapabilityValueOrRange& height() const { return m_height; }
    void setHeight(const CapabilityValueOrRange& height) { m_height = height; }

    bool supportsFrameRate() const { return m_supportedConstraints.supportsFrameRate(); }
    const CapabilityValueOrRange& frameRate() const { return m_frameRate; }
    void setFrameRate(const CapabilityValueOrRange& frameRate) { m_frameRate = frameRate; }

    bool supportsFacingMode() const { return m_supportedConstraints.supportsFacingMode(); }
    const Vector<VideoFacingMode>& facingMode() const { return m_facingMode; }
    void addFacingMode(VideoFacingMode mode) { m_facingMode.append(mode); }

    bool supportsAspectRatio() const { return m_supportedConstraints.supportsAspectRatio(); }
    const CapabilityValueOrRange& aspectRatio() const { return m_aspectRatio; }
    void setAspectRatio(const CapabilityValueOrRange& aspectRatio) { m_aspectRatio = aspectRatio; }

    bool supportsVolume() const { return m_supportedConstraints.supportsVolume(); }
    const CapabilityValueOrRange& volume() const { return m_volume; }
    void setVolume(const CapabilityValueOrRange& volume) { m_volume = volume; }

    bool supportsSampleRate() const { return m_supportedConstraints.supportsSampleRate(); }
    const CapabilityValueOrRange& sampleRate() const { return m_sampleRate; }
    void setSampleRate(const CapabilityValueOrRange& sampleRate) { m_sampleRate = sampleRate; }

    bool supportsSampleSize() const { return m_supportedConstraints.supportsSampleSize(); }
    const CapabilityValueOrRange& sampleSize() const { return m_sampleSize; }
    void setSampleSize(const CapabilityValueOrRange& sampleSize) { m_sampleSize = sampleSize; }

    bool supportsEchoCancellation() const { return m_supportedConstraints.supportsEchoCancellation(); }
    EchoCancellation echoCancellation() const { return m_echoCancellation; }
    void setEchoCancellation(EchoCancellation echoCancellation) { m_echoCancellation = echoCancellation; }

    bool supportsDeviceId() const { return m_supportedConstraints.supportsDeviceId(); }
    const AtomString& deviceId() const { return m_deviceId; }
    void setDeviceId(const AtomString& id)  { m_deviceId = id; }

    bool supportsGroupId() const { return m_supportedConstraints.supportsGroupId(); }
    const AtomString& groupId() const { return m_groupId; }
    void setGroupId(const AtomString& id)  { m_groupId = id; }

    bool supportsFocusDistance() const { return m_supportedConstraints.supportsFocusDistance(); }
    const CapabilityValueOrRange& focusDistance() const { return m_focusDistance; }
    void setFocusDistance(const CapabilityValueOrRange& focusDistance) { m_focusDistance = focusDistance; }

    bool supportsZoom() const { return m_supportedConstraints.supportsZoom(); }
    const CapabilityValueOrRange& zoom() const { return m_zoom; }
    void setZoom(const CapabilityValueOrRange& zoom) { m_zoom = zoom; }

    const RealtimeMediaSourceSupportedConstraints& supportedConstraints() const { return m_supportedConstraints; }
    void setSupportedConstraints(const RealtimeMediaSourceSupportedConstraints& constraints) { m_supportedConstraints = constraints; }

private:
    CapabilityValueOrRange m_width;
    CapabilityValueOrRange m_height;
    CapabilityValueOrRange m_aspectRatio;
    CapabilityValueOrRange m_frameRate;
    Vector<VideoFacingMode> m_facingMode;
    CapabilityValueOrRange m_volume;
    CapabilityValueOrRange m_sampleRate;
    CapabilityValueOrRange m_sampleSize;
    EchoCancellation m_echoCancellation { EchoCancellation::ReadOnly };
    AtomString m_deviceId;
    AtomString m_groupId;
    CapabilityValueOrRange m_focusDistance;
    CapabilityValueOrRange m_zoom;

    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::CapabilityValueOrRange::Type> {
    using values = EnumValues<
        WebCore::CapabilityValueOrRange::Type,
        WebCore::CapabilityValueOrRange::Type::Undefined,
        WebCore::CapabilityValueOrRange::Type::Double,
        WebCore::CapabilityValueOrRange::Type::ULong,
        WebCore::CapabilityValueOrRange::Type::DoubleRange,
        WebCore::CapabilityValueOrRange::Type::ULongRange
    >;
};

} // namespace WTF

#endif // ENABLE(MEDIA_STREAM)
