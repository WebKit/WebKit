/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "GStreamerCommon.h"
#include "VideoEncoderScalabilityMode.h"
#include <wtf/TZoneMalloc.h>

#define WEBKIT_TYPE_VIDEO_ENCODER (webkit_video_encoder_get_type())
#define WEBKIT_VIDEO_ENCODER(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_TYPE_VIDEO_ENCODER, WebKitVideoEncoder))
#define WEBKIT_VIDEO_ENCODER_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), WEBKIT_TYPE_VIDEO_ENCODER, WebKitVideoEncoderClass))
#define WEBKIT_IS_VIDEO_ENCODER(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_TYPE_VIDEO_ENCODER))
#define WEBKIT_IS_VIDEO_ENCODER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), WEBKIT_TYPE_VIDEO_ENCODER))

typedef struct _WebKitVideoEncoder WebKitVideoEncoder;
typedef struct _WebKitVideoEncoderClass WebKitVideoEncoderClass;
typedef struct _WebKitVideoEncoderPrivate WebKitVideoEncoderPrivate;

struct _WebKitVideoEncoder {
    GstBin parent;

    WebKitVideoEncoderPrivate* priv;
};

struct _WebKitVideoEncoderClass {
    GstBinClass parentClass;
};

GType webkit_video_encoder_get_type(void);

class WebKitVideoEncoderBitRateAllocation : public RefCounted<WebKitVideoEncoderBitRateAllocation> {
    WTF_MAKE_TZONE_ALLOCATED(WebKitVideoEncoderBitRateAllocation);
    WTF_MAKE_NONCOPYABLE(WebKitVideoEncoderBitRateAllocation);

public:
    static Ref<WebKitVideoEncoderBitRateAllocation> create(WebCore::VideoEncoderScalabilityMode scalabilityMode)
    {
        return adoptRef(*new WebKitVideoEncoderBitRateAllocation(scalabilityMode));
    }

    static const unsigned MaxSpatialLayers = 5;
    static const unsigned MaxTemporalLayers = 4;

    void setBitRate(unsigned spatialLayerIndex, unsigned temporalLayerIndex, uint32_t bitRate)
    {
        RELEASE_ASSERT(spatialLayerIndex < MaxSpatialLayers);
        RELEASE_ASSERT(temporalLayerIndex < MaxTemporalLayers);
        m_bitRates[spatialLayerIndex][temporalLayerIndex].emplace(bitRate);
    }

    std::optional<uint32_t> getBitRate(unsigned spatialLayerIndex, unsigned temporalLayerIndex) const
    {
        if (UNLIKELY(spatialLayerIndex >= MaxSpatialLayers))
            return std::nullopt;
        if (UNLIKELY(temporalLayerIndex >= MaxTemporalLayers))
            return std::nullopt;
        return m_bitRates[spatialLayerIndex][temporalLayerIndex];
    }

    WebCore::VideoEncoderScalabilityMode scalabilityMode() const { return m_scalabilityMode; }

private:
    WebKitVideoEncoderBitRateAllocation(WebCore::VideoEncoderScalabilityMode scalabilityMode)
        : m_scalabilityMode(scalabilityMode)
    { }

    WebCore::VideoEncoderScalabilityMode m_scalabilityMode;
    std::optional<uint32_t> m_bitRates[MaxSpatialLayers][MaxTemporalLayers];
};

bool videoEncoderSupportsCodec(WebKitVideoEncoder*, const String&);
bool videoEncoderSetCodec(WebKitVideoEncoder*, const String&, std::optional<WebCore::IntSize> = std::nullopt, std::optional<double> frameRate = std::nullopt);
void videoEncoderSetBitRateAllocation(WebKitVideoEncoder*, RefPtr<WebKitVideoEncoderBitRateAllocation>&&);
void teardownVideoEncoderSingleton();
