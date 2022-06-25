/*
 * Copyright (C) 2006-2021 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2008 Torch Mobile, Inc.
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

#include "Color.h"
#include "ColorInterpolationMethod.h"
#include "FloatPoint.h"
#include "GradientColorStops.h"
#include "GraphicsTypes.h"
#include <variant>
#include <wtf/Vector.h>

#if USE(CG)
#include "GradientRendererCG.h"
#endif

#if USE(CG)
typedef struct CGContext* CGContextRef;
#endif

#if USE(CAIRO)
typedef struct _cairo_pattern cairo_pattern_t;
#endif

namespace WTF {
class TextStream;
}

namespace WebCore {

class AffineTransform;
class FloatRect;
class GraphicsContext;

class Gradient : public RefCounted<Gradient> {
    friend WTF::TextStream& operator<<(WTF::TextStream&, const Gradient&);
public:
    struct LinearData {
        FloatPoint point0;
        FloatPoint point1;

        template<typename Encoder> void encode(Encoder&) const;
        template<typename Decoder> static std::optional<LinearData> decode(Decoder&);
    };

    struct RadialData {
        FloatPoint point0;
        FloatPoint point1;
        float startRadius;
        float endRadius;
        float aspectRatio; // For elliptical gradient, width / height.

        template<typename Encoder> void encode(Encoder&) const;
        template<typename Decoder> static std::optional<RadialData> decode(Decoder&);
    };

    struct ConicData {
        FloatPoint point0;
        float angleRadians;

        template<typename Encoder> void encode(Encoder&) const;
        template<typename Decoder> static std::optional<ConicData> decode(Decoder&);
    };

    using Data = std::variant<LinearData, RadialData, ConicData>;

    WEBCORE_EXPORT static Ref<Gradient> create(Data&&, ColorInterpolationMethod, GradientSpreadMethod = GradientSpreadMethod::Pad, GradientColorStops&& = { });

    bool isZeroSize() const;

    const Data& data() const { return m_data; }

    WEBCORE_EXPORT void addColorStop(GradientColorStop&&);

    const GradientColorStops& stops() const { return m_stops; }
    GradientSpreadMethod spreadMethod() const { return m_spreadMethod; }

    void fill(GraphicsContext&, const FloatRect&);
    void adjustParametersForTiledDrawing(FloatSize&, FloatRect&, const FloatSize& spacing);

    unsigned hash() const;

#if USE(CAIRO)
    RefPtr<cairo_pattern_t> createPattern(float globalAlpha, const AffineTransform&);
#endif

#if USE(CG)
    void paint(GraphicsContext&);
    void paint(CGContextRef);
#endif

    template<typename Encoder> void encode(Encoder&) const;
    template<typename Decoder> static std::optional<Ref<Gradient>> decode(Decoder&);

private:
    explicit Gradient(Data&&, ColorInterpolationMethod, GradientSpreadMethod, GradientColorStops&&);

    void stopsChanged();

    Data m_data;
    ColorInterpolationMethod m_colorInterpolationMethod;
    GradientSpreadMethod m_spreadMethod;
    GradientColorStops m_stops;
    mutable unsigned m_cachedHash { 0 };

#if USE(CG)
    std::optional<GradientRendererCG> m_platformRenderer;
#endif
};

template<typename Encoder> void Gradient::LinearData::encode(Encoder& encoder) const
{
    encoder << point0;
    encoder << point1;
}

template<typename Decoder> std::optional<Gradient::LinearData> Gradient::LinearData::decode(Decoder& decoder)
{
    std::optional<FloatPoint> point0;
    decoder >> point0;
    if (!point0)
        return std::nullopt;

    std::optional<FloatPoint> point1;
    decoder >> point1;
    if (!point1)
        return std::nullopt;

    return {{ *point0, *point1 }};
}

template<typename Encoder> void Gradient::RadialData::encode(Encoder& encoder) const
{
    encoder << point0;
    encoder << point1;
    encoder << startRadius;
    encoder << endRadius;
    encoder << aspectRatio;
}

template<typename Decoder> std::optional<Gradient::RadialData> Gradient::RadialData::decode(Decoder& decoder)
{
    std::optional<FloatPoint> point0;
    decoder >> point0;
    if (!point0)
        return std::nullopt;

    std::optional<FloatPoint> point1;
    decoder >> point1;
    if (!point1)
        return std::nullopt;

    std::optional<float> startRadius;
    decoder >> startRadius;
    if (!startRadius)
        return std::nullopt;

    std::optional<float> endRadius;
    decoder >> endRadius;
    if (!endRadius)
        return std::nullopt;

    std::optional<float> aspectRatio;
    decoder >> aspectRatio;
    if (!aspectRatio)
        return std::nullopt;

    return {{ *point0, *point1, *startRadius, *endRadius, *aspectRatio }};
}

template<typename Encoder> void Gradient::ConicData::encode(Encoder& encoder) const
{
    encoder << point0;
    encoder << angleRadians;
}

template<typename Decoder> std::optional<Gradient::ConicData> Gradient::ConicData::decode(Decoder& decoder)
{
    std::optional<FloatPoint> point0;
    decoder >> point0;
    if (!point0)
        return std::nullopt;

    std::optional<float> angleRadians;
    decoder >> angleRadians;
    if (!angleRadians)
        return std::nullopt;

    return {{ *point0, *angleRadians }};
}

template<typename Encoder> void Gradient::encode(Encoder& encoder) const
{
    encoder << m_data;
    encoder << m_colorInterpolationMethod;
    encoder << m_spreadMethod;
    encoder << m_stops;
}

template<typename Decoder> std::optional<Ref<Gradient>> Gradient::decode(Decoder& decoder)
{
    std::optional<Data> data;
    decoder >> data;
    if (!data)
        return std::nullopt;

    std::optional<ColorInterpolationMethod> colorInterpolationMethod;
    decoder >> colorInterpolationMethod;
    if (!colorInterpolationMethod)
        return std::nullopt;

    std::optional<GradientSpreadMethod> spreadMethod;
    decoder >> spreadMethod;
    if (!spreadMethod)
        return std::nullopt;

    std::optional<GradientColorStops> stops;
    decoder >> stops;
    if (!stops)
        return std::nullopt;

    return Gradient::create(WTFMove(*data), *colorInterpolationMethod, *spreadMethod, WTFMove(*stops));
}

} // namespace WebCore
