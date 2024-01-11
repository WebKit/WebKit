/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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

#include "MeteringMode.h"
#include "RealtimeMediaSourceSettings.h"
#include <wtf/ArgumentCoder.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CapabilityRange {
private:
    friend struct IPC::ArgumentCoder<CapabilityRange, void>;
public:
    struct LongRange {
        LongRange(int max, int min)
            : max(max)
            , min(min)
        {
            RELEASE_ASSERT(min <= max);
        }
        int max;
        int min;
    };

    struct DoubleRange {
        DoubleRange(double max, double min)
            : max(max)
            , min(min)
        {
            RELEASE_ASSERT(min <= max);
        }
        double max;
        double min;
    };

    enum class Type : uint8_t {
        Undefined,
        DoubleRange,
        LongRange,
    };
    Type type() const
    {
        if (m_doubleRange)
            return Type::DoubleRange;
        if (m_longRange)
            return Type::LongRange;
        return Type::Undefined;
    }

    CapabilityRange() = default;

    CapabilityRange(double value)
        : m_doubleRange({ value, value })
    {
    }

    CapabilityRange(int value)
        : m_longRange({ value, value })
    {
    }

    CapabilityRange(double min, double max)
        : m_doubleRange({ max, min })
    {
    }
    
    CapabilityRange(int min, int max)
        : m_longRange({ max, min })
    {
    }

    const DoubleRange& doubleRange() const
    {
        RELEASE_ASSERT(m_doubleRange);
        return *m_doubleRange;
    }

    const LongRange& longRange() const
    {
        RELEASE_ASSERT(m_longRange);
        return *m_longRange;
    }

private:
    std::optional<DoubleRange> m_doubleRange;
    std::optional<LongRange> m_longRange;
};

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
    
    RealtimeMediaSourceCapabilities(CapabilityRange width, CapabilityRange height, CapabilityRange aspectRatio, CapabilityRange frameRate, Vector<VideoFacingMode>&& facingMode, CapabilityRange volume, CapabilityRange sampleRate, CapabilityRange sampleSize, EchoCancellation echoCancellation, String&& deviceId, String&& groupId, CapabilityRange focusDistance, Vector<MeteringMode>&& whiteBalanceModes, CapabilityRange zoom, bool torch, RealtimeMediaSourceSupportedConstraints&& supportedConstraints)
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
        , m_whiteBalanceModes(whiteBalanceModes)
        , m_zoom(WTFMove(zoom))
        , m_torch(torch)
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
    const CapabilityRange& width() const { return m_width; }
    void setWidth(const CapabilityRange& width) { m_width = width; }

    bool supportsHeight() const { return m_supportedConstraints.supportsHeight(); }
    const CapabilityRange& height() const { return m_height; }
    void setHeight(const CapabilityRange& height) { m_height = height; }

    bool supportsFrameRate() const { return m_supportedConstraints.supportsFrameRate(); }
    const CapabilityRange& frameRate() const { return m_frameRate; }
    void setFrameRate(const CapabilityRange& frameRate) { m_frameRate = frameRate; }

    bool supportsFacingMode() const { return m_supportedConstraints.supportsFacingMode(); }
    const Vector<VideoFacingMode>& facingMode() const { return m_facingMode; }
    void addFacingMode(VideoFacingMode mode) { m_facingMode.append(mode); }

    bool supportsAspectRatio() const { return m_supportedConstraints.supportsAspectRatio(); }
    const CapabilityRange& aspectRatio() const { return m_aspectRatio; }
    void setAspectRatio(const CapabilityRange& aspectRatio) { m_aspectRatio = aspectRatio; }

    bool supportsVolume() const { return m_supportedConstraints.supportsVolume(); }
    const CapabilityRange& volume() const { return m_volume; }
    void setVolume(const CapabilityRange& volume) { m_volume = volume; }

    bool supportsSampleRate() const { return m_supportedConstraints.supportsSampleRate(); }
    const CapabilityRange& sampleRate() const { return m_sampleRate; }
    void setSampleRate(const CapabilityRange& sampleRate) { m_sampleRate = sampleRate; }

    bool supportsSampleSize() const { return m_supportedConstraints.supportsSampleSize(); }
    const CapabilityRange& sampleSize() const { return m_sampleSize; }
    void setSampleSize(const CapabilityRange& sampleSize) { m_sampleSize = sampleSize; }

    bool supportsEchoCancellation() const { return m_supportedConstraints.supportsEchoCancellation(); }
    EchoCancellation echoCancellation() const { return m_echoCancellation; }
    void setEchoCancellation(EchoCancellation echoCancellation) { m_echoCancellation = echoCancellation; }

    bool supportsDeviceId() const { return m_supportedConstraints.supportsDeviceId(); }
    const String& deviceId() const { return m_deviceId; }
    void setDeviceId(const String& id)  { m_deviceId = id; }

    bool supportsGroupId() const { return m_supportedConstraints.supportsGroupId(); }
    const String& groupId() const { return m_groupId; }
    void setGroupId(const String& id)  { m_groupId = id; }

    bool supportsFocusDistance() const { return m_supportedConstraints.supportsFocusDistance(); }
    const CapabilityRange& focusDistance() const { return m_focusDistance; }
    void setFocusDistance(const CapabilityRange& focusDistance) { m_focusDistance = focusDistance; }

    bool supportsWhiteBalanceMode() const { return m_supportedConstraints.supportsWhiteBalanceMode(); }
    const Vector<MeteringMode>& whiteBalanceModes() const { return m_whiteBalanceModes; }
    void setWhiteBalanceModes(Vector<MeteringMode>&& modes) { m_whiteBalanceModes = WTFMove(modes); }

    bool supportsZoom() const { return m_supportedConstraints.supportsZoom(); }
    const CapabilityRange& zoom() const { return m_zoom; }
    void setZoom(const CapabilityRange& zoom) { m_zoom = zoom; }

    bool supportsTorch() const { return m_supportedConstraints.supportsTorch(); }
    bool torch() const { return m_torch; }
    void setTorch(bool torch) { m_torch = torch; }

    const RealtimeMediaSourceSupportedConstraints& supportedConstraints() const { return m_supportedConstraints; }
    void setSupportedConstraints(const RealtimeMediaSourceSupportedConstraints& constraints) { m_supportedConstraints = constraints; }

    RealtimeMediaSourceCapabilities isolatedCopy() const { return { m_width, m_height, m_aspectRatio, m_frameRate, Vector<VideoFacingMode> { m_facingMode }, m_volume, m_sampleRate, m_sampleSize, m_echoCancellation, m_deviceId.isolatedCopy(), m_groupId.isolatedCopy(), m_focusDistance, Vector<MeteringMode> { m_whiteBalanceModes }, m_zoom, m_torch, RealtimeMediaSourceSupportedConstraints { m_supportedConstraints } }; }

private:
    CapabilityRange m_width;
    CapabilityRange m_height;
    CapabilityRange m_aspectRatio;
    CapabilityRange m_frameRate;
    Vector<VideoFacingMode> m_facingMode;
    CapabilityRange m_volume;
    CapabilityRange m_sampleRate;
    CapabilityRange m_sampleSize;
    EchoCancellation m_echoCancellation { EchoCancellation::ReadOnly };
    String m_deviceId;
    String m_groupId;
    CapabilityRange m_focusDistance;

    Vector<MeteringMode> m_whiteBalanceModes;
    CapabilityRange m_zoom;
    bool m_torch { false };

    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
