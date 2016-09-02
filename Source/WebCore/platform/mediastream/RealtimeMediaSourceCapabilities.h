/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef RealtimeMediaSourceCapabilities_h
#define RealtimeMediaSourceCapabilities_h

#if ENABLE(MEDIA_STREAM)

#include "RealtimeMediaSourceSettings.h"
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

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
        int asInt;
        double asDouble;
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

private:
    ValueUnion m_minOrValue;
    ValueUnion m_max;
    Type m_type;
};

class RealtimeMediaSourceCapabilities : public RefCounted<RealtimeMediaSourceCapabilities> {
public:
    static PassRefPtr<RealtimeMediaSourceCapabilities> create(const RealtimeMediaSourceSupportedConstraints& supportedConstraints)
    {
        return adoptRef(new RealtimeMediaSourceCapabilities(supportedConstraints));
    }

    ~RealtimeMediaSourceCapabilities() { }

    bool supportsWidth() const { return m_supportedConstraints.supportsWidth(); }
    const CapabilityValueOrRange& width() { return m_width; }
    void setWidth(const CapabilityValueOrRange& width) { m_width = width; }

    bool supportsHeight() const { return m_supportedConstraints.supportsHeight(); }
    const CapabilityValueOrRange& height() { return m_height; }
    void setHeight(const CapabilityValueOrRange& height) { m_height = height; }

    bool supportsFrameRate() const { return m_supportedConstraints.supportsFrameRate(); }
    const CapabilityValueOrRange& frameRate() { return m_frameRate; }
    void setFrameRate(const CapabilityValueOrRange& frameRate) { m_frameRate = frameRate; }

    bool supportsFacingMode() const { return m_supportedConstraints.supportsFacingMode(); }
    const Vector<RealtimeMediaSourceSettings::VideoFacingMode>& facingMode() { return m_facingMode; }
    void addFacingMode(RealtimeMediaSourceSettings::VideoFacingMode mode) { m_facingMode.append(mode); }

    bool supportsAspectRatio() const { return m_supportedConstraints.supportsAspectRatio(); }
    const CapabilityValueOrRange& aspectRatio() { return m_aspectRatio; }
    void setAspectRatio(const CapabilityValueOrRange& aspectRatio) { m_aspectRatio = aspectRatio; }

    bool supportsVolume() const { return m_supportedConstraints.supportsVolume(); }
    const CapabilityValueOrRange& volume() { return m_volume; }
    void setVolume(const CapabilityValueOrRange& volume) { m_volume = volume; }

    bool supportsSampleRate() const { return m_supportedConstraints.supportsSampleRate(); }
    const CapabilityValueOrRange& sampleRate() { return m_sampleRate; }
    void setSampleRate(const CapabilityValueOrRange& sampleRate) { m_sampleRate = sampleRate; }

    bool supportsSampleSize() const { return m_supportedConstraints.supportsSampleSize(); }
    const CapabilityValueOrRange& sampleSize() { return m_sampleSize; }
    void setSampleSize(const CapabilityValueOrRange& sampleSize) { m_sampleSize = sampleSize; }

    enum class EchoCancellation {
        ReadOnly = 0,
        ReadWrite = 1,
    };
    bool supportsEchoCancellation() const { return m_supportedConstraints.supportsEchoCancellation(); }
    EchoCancellation echoCancellation() { return m_echoCancellation; }
    void setEchoCancellation(EchoCancellation echoCancellation) { m_echoCancellation = echoCancellation; }

    bool supportsDeviceId() const { return m_supportedConstraints.supportsDeviceId(); }
    const AtomicString& deviceId() { return m_deviceId; }
    void setDeviceId(const AtomicString& id)  { m_deviceId = id; }

    bool supportsGroupId() const { return m_supportedConstraints.supportsGroupId(); }
    const AtomicString& groupId() { return m_groupId; }
    void setGroupId(const AtomicString& id)  { m_groupId = id; }
    
private:
    RealtimeMediaSourceCapabilities(const RealtimeMediaSourceSupportedConstraints& supportedConstraints)
        : m_supportedConstraints(supportedConstraints)
    {
    }

    CapabilityValueOrRange m_width;
    CapabilityValueOrRange m_height;
    CapabilityValueOrRange m_aspectRatio;
    CapabilityValueOrRange m_frameRate;
    Vector<RealtimeMediaSourceSettings::VideoFacingMode> m_facingMode;
    CapabilityValueOrRange m_volume;
    CapabilityValueOrRange m_sampleRate;
    CapabilityValueOrRange m_sampleSize;
    EchoCancellation m_echoCancellation;
    AtomicString m_deviceId;
    AtomicString m_groupId;

    const RealtimeMediaSourceSupportedConstraints& m_supportedConstraints;
};

} // namespace WebCore

#endif // RealtimeMediaSourceCapabilities_h

#endif
