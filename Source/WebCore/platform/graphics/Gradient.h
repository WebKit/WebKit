/*
 * Copyright (C) 2006, 2007, 2008, 2011, 2012, 2013 Apple Inc. All rights reserved.
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

#include "AffineTransform.h"
#include "Color.h"
#include "FloatPoint.h"
#include "GraphicsTypes.h"
#include <wtf/EnumTraits.h>
#include <wtf/RefCounted.h>
#include <wtf/Variant.h>
#include <wtf/Vector.h>

#if USE(CG)
typedef struct CGContext* CGContextRef;
typedef struct CGGradient* CGGradientRef;
typedef CGGradientRef PlatformGradient;
#elif USE(DIRECT2D)
interface ID2D1Brush;
interface ID2D1RenderTarget;
typedef ID2D1Brush* PlatformGradient;
#elif USE(CAIRO)
typedef struct _cairo_pattern cairo_pattern_t;
typedef cairo_pattern_t* PlatformGradient;
#else
typedef void* PlatformGradient;
#endif

namespace WebCore {

class Color;
class FloatRect;
class GraphicsContext;

class Gradient : public RefCounted<Gradient> {
public:
    // FIXME: ExtendedColor - A color stop needs a notion of color space.
    struct ColorStop {
        float offset { 0 };
        Color color;

        ColorStop() = default;
        ColorStop(float offset, const Color& color)
            : offset(offset)
            , color(color)
        {
        }

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<ColorStop> decode(Decoder&);
    };

    using ColorStopVector = Vector<ColorStop, 2>;

    struct LinearData {
        FloatPoint point0;
        FloatPoint point1;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<LinearData> decode(Decoder&);
    };

    struct RadialData {
        FloatPoint point0;
        FloatPoint point1;
        float startRadius;
        float endRadius;
        float aspectRatio; // For elliptical gradient, width / height.

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<RadialData> decode(Decoder&);
    };
    
    struct ConicData {
        FloatPoint point0;
        float angleRadians;

        template<class Encoder> void encode(Encoder&) const;
        template<class Decoder> static Optional<ConicData> decode(Decoder&);
    };

    using Data = Variant<LinearData, RadialData, ConicData>;

    enum class Type { Linear, Radial, Conic };

    WEBCORE_EXPORT static Ref<Gradient> create(LinearData&&);
    WEBCORE_EXPORT static Ref<Gradient> create(RadialData&&);
    WEBCORE_EXPORT static Ref<Gradient> create(ConicData&&);

    WEBCORE_EXPORT ~Gradient();

    WEBCORE_EXPORT Type type() const;

    bool hasAlpha() const;
    bool isZeroSize() const;

    const Data& data() const { return m_data; }

    WEBCORE_EXPORT void addColorStop(const ColorStop&);
    WEBCORE_EXPORT void addColorStop(float, const Color&);
    WEBCORE_EXPORT void setSortedColorStops(ColorStopVector&&);

    const ColorStopVector& stops() const { return m_stops; }

    WEBCORE_EXPORT void setSpreadMethod(GradientSpreadMethod);
    GradientSpreadMethod spreadMethod() const { return m_spreadMethod; }

    // CG needs to transform the gradient at draw time.
    WEBCORE_EXPORT void setGradientSpaceTransform(const AffineTransform& gradientSpaceTransformation);
    const AffineTransform& gradientSpaceTransform() const { return m_gradientSpaceTransformation; }

    void fill(GraphicsContext&, const FloatRect&);
    void adjustParametersForTiledDrawing(FloatSize&, FloatRect&, const FloatSize& spacing);

    unsigned hash() const;
    void invalidateHash() { m_cachedHash = 0; }

#if USE(CG)
    void paint(GraphicsContext&);
    void paint(CGContextRef);
#elif USE(DIRECT2D)
    PlatformGradient createPlatformGradientIfNecessary(ID2D1RenderTarget*);
#elif USE(CAIRO)
    PlatformGradient createPlatformGradient(float globalAlpha);
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<Ref<Gradient>> decode(Decoder&);

private:
    Gradient(LinearData&&);
    Gradient(RadialData&&);
    Gradient(ConicData&&);

    PlatformGradient platformGradient();
    void platformInit() { m_gradient = nullptr; }
    void platformDestroy();

    void sortStopsIfNecessary();

#if USE(DIRECT2D)
    void generateGradient(ID2D1RenderTarget*);
#endif

    Data m_data;

    mutable ColorStopVector m_stops;
    mutable bool m_stopsSorted { false };
    GradientSpreadMethod m_spreadMethod { SpreadMethodPad };
    AffineTransform m_gradientSpaceTransformation;

    mutable unsigned m_cachedHash { 0 };

    PlatformGradient m_gradient;
};

template<class Encoder>
void Gradient::ColorStop::encode(Encoder& encoder) const
{
    encoder << offset;
    encoder << color;
}

template<class Decoder>
Optional<Gradient::ColorStop> Gradient::ColorStop::decode(Decoder& decoder)
{
    Optional<float> offset;
    decoder >> offset;
    if (!offset)
        return WTF::nullopt;

    Optional<Color> color;
    decoder >> color;
    if (!color)
        return WTF::nullopt;

    return {{ *offset, *color }};
}

template<class Encoder>
void Gradient::LinearData::encode(Encoder& encoder) const
{
    encoder << point0;
    encoder << point1;
}

template<class Decoder>
Optional<Gradient::LinearData> Gradient::LinearData::decode(Decoder& decoder)
{
    Optional<FloatPoint> point0;
    decoder >> point0;
    if (!point0)
        return WTF::nullopt;

    Optional<FloatPoint> point1;
    decoder >> point1;
    if (!point1)
        return WTF::nullopt;

    return {{ *point0, *point1 }};
}

template<class Encoder>
void Gradient::RadialData::encode(Encoder& encoder) const
{
    encoder << point0;
    encoder << point1;
    encoder << startRadius;
    encoder << endRadius;
    encoder << aspectRatio;
}

template<class Decoder>
Optional<Gradient::RadialData> Gradient::RadialData::decode(Decoder& decoder)
{
    Optional<FloatPoint> point0;
    decoder >> point0;
    if (!point0)
        return WTF::nullopt;

    Optional<FloatPoint> point1;
    decoder >> point1;
    if (!point1)
        return WTF::nullopt;

    Optional<float> startRadius;
    decoder >> startRadius;
    if (!startRadius)
        return WTF::nullopt;

    Optional<float> endRadius;
    decoder >> endRadius;
    if (!endRadius)
        return WTF::nullopt;

    Optional<float> aspectRatio;
    decoder >> aspectRatio;
    if (!aspectRatio)
        return WTF::nullopt;

    return {{ *point0, *point1, *startRadius, *endRadius, *aspectRatio }};
}

template<class Encoder>
void Gradient::ConicData::encode(Encoder& encoder) const
{
    encoder << point0;
    encoder << angleRadians;
}

template<class Decoder>
Optional<Gradient::ConicData> Gradient::ConicData::decode(Decoder& decoder)
{
    Optional<FloatPoint> point0;
    decoder >> point0;
    if (!point0)
        return WTF::nullopt;

    Optional<float> angleRadians;
    decoder >> angleRadians;
    if (!angleRadians)
        return WTF::nullopt;

    return {{ *point0, *angleRadians }};
}

template<class Encoder>
void Gradient::encode(Encoder& encoder) const
{
    auto type = this->type();
    encoder << type;
    switch (type) {
    case Type::Linear:
        encoder << WTF::get<Gradient::LinearData>(m_data);
        break;
    case Type::Radial:
        encoder << WTF::get<Gradient::RadialData>(m_data);
        break;
    case Type::Conic:
        encoder << WTF::get<Gradient::ConicData>(m_data);
        break;
    }
    encoder << m_stops;
    encoder << m_stopsSorted;
    encoder.encodeEnum(m_spreadMethod);
    encoder << m_gradientSpaceTransformation;
}

template<class Decoder>
Optional<Ref<Gradient>> Gradient::decode(Decoder& decoder)
{
    Optional<Gradient::Type> type;
    decoder >> type;
    if (!type)
        return WTF::nullopt;

    RefPtr<Gradient> gradient;
    switch (*type) {
    case Type::Linear: {
        Optional<LinearData> linearData;
        decoder >> linearData;
        if (!linearData)
            return WTF::nullopt;

        gradient = Gradient::create(WTFMove(*linearData));
        break;
    }
    case Type::Radial: {
        Optional<RadialData> radialData;
        decoder >> radialData;
        if (!radialData)
            return WTF::nullopt;

        gradient = Gradient::create(WTFMove(*radialData));
        break;
    }
    case Type::Conic: {
        Optional<ConicData> conicData;
        decoder >> conicData;
        if (!conicData)
            return WTF::nullopt;

        gradient = Gradient::create(WTFMove(*conicData));
        break;
    }
    }

    if (!gradient) {
        ASSERT_NOT_REACHED();
        return WTF::nullopt;
    }

    Optional<ColorStopVector> stops;
    decoder >> stops;
    if (!stops)
        return WTF::nullopt;

    Optional<bool> stopsSorted;
    decoder >> stopsSorted;
    if (!stopsSorted.hasValue())
        return WTF::nullopt;

    if (stopsSorted.value())
        gradient->setSortedColorStops(WTFMove(*stops));
    else {
        for (auto& stop : *stops)
            gradient->addColorStop(stop);
    }

    GradientSpreadMethod spreadMethod;
    if (!decoder.decodeEnum(spreadMethod))
        return WTF::nullopt;

    gradient->setSpreadMethod(spreadMethod);

    Optional<AffineTransform> gradientSpaceTransformation;
    decoder >> gradientSpaceTransformation;
    if (!gradientSpaceTransformation)
        return WTF::nullopt;

    gradient->setGradientSpaceTransform(WTFMove(*gradientSpaceTransformation));
    return gradient.releaseNonNull();
}

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::Gradient::Type> {
    using values = EnumValues<
    WebCore::Gradient::Type,
    WebCore::Gradient::Type::Linear,
    WebCore::Gradient::Type::Radial,
    WebCore::Gradient::Type::Conic
    >;
};

} // namespace WTF
