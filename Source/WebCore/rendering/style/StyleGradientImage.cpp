/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "StyleGradientImage.h"

#include "CSSValuePair.h"
#include "CalculationValue.h"
#include "ColorInterpolation.h"
#include "ComputedStyleExtractor.h"
#include "GeneratedImage.h"
#include "GeometryUtilities.h"
#include "GradientImage.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "RenderStyleInlines.h"

namespace WebCore {

template<typename Stops> static bool stopsAreCacheable(const Stops& stops)
{
    for (auto& stop : stops) {
        if (stop.color && stop.color->containsCurrentColor())
            return false;
    }
    return true;
}

static bool stopsAreCacheable(const StyleGradientImage::Data& data)
{
    return WTF::switchOn(data, [](auto& data) { return stopsAreCacheable(data.stops); } );
}

static Color resolveColorStopColor(const std::optional<StyleColor>& styleColor, const RenderStyle& style, bool hasColorFilter)
{
    if (!styleColor)
        return { };

    if (hasColorFilter)
        return style.colorWithColorFilter(*styleColor);
    return style.colorResolvingCurrentColor(*styleColor);
}

static std::optional<float> resolveColorStopPosition(const StyleGradientImageLengthStop& stop, float gradientLength)
{
    if (!stop.position)
        return std::nullopt;

    if (stop.position->isPercent())
        return stop.position->percent() / 100.0;

    if (gradientLength <= 0)
        return 0;

    if (stop.position->isFixed())
        return stop.position->value() / gradientLength;

    if (stop.position->isCalculated())
        return stop.position->calculationValue().evaluate(gradientLength) / gradientLength;

    ASSERT_NOT_REACHED();
    return 0;
}

static std::optional<float> resolveColorStopPosition(const StyleGradientImageAngularStop& stop, float)
{
    return WTF::switchOn(stop.position,
        [](std::monostate) -> std::optional<float> {
            return std::nullopt;
        },
        [](AngleRaw angle) -> std::optional<float> {
            return CSSPrimitiveValue::computeDegrees(angle.type, angle.value) / 360.0;
        },
        [](PercentRaw percent) -> std::optional<float> {
            return percent.value / 100.0;
        }
    );
}

StyleGradientImage::StyleGradientImage(Data&& data, CSSGradientColorInterpolationMethod colorInterpolationMethod)
    : StyleGeneratedImage { Type::GradientImage, StyleGradientImage::isFixedSize }
    , m_data { WTFMove(data) }
    , m_colorInterpolationMethod { colorInterpolationMethod }
    , m_knownCacheableBarringFilter { stopsAreCacheable(m_data) }
{
}

StyleGradientImage::~StyleGradientImage() = default;

bool StyleGradientImage::operator==(const StyleImage& other) const
{
    auto* otherGradientImage = dynamicDowncast<StyleGradientImage>(other);
    return otherGradientImage && equals(*otherGradientImage);
}

bool StyleGradientImage::equals(const StyleGradientImage& other) const
{
    return m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

// MARK: Computed Style Extractor Helpers

static inline RefPtr<CSSPrimitiveValue> computedStyleValueForColorStopColor(const std::optional<StyleColor>& color, const RenderStyle& style)
{
    if (!color)
        return nullptr;
    return ComputedStyleExtractor::currentColorOrValidColor(style, *color);
}

static inline RefPtr<CSSPrimitiveValue> computedStyleValueForColorStopPosition(const StyleGradientImageLengthStop& stop, const RenderStyle& style)
{
    if (!stop.position)
        return nullptr;
    return ComputedStyleExtractor::zoomAdjustedPixelValueForLength(*stop.position, style);
}

static inline RefPtr<CSSPrimitiveValue> computedStyleValueForColorStopPositionDeprecated(const StyleGradientImageLengthStop& stop)
{
    if (!stop.position)
        return nullptr;
    return CSSPrimitiveValue::create(*stop.position);
}

static inline RefPtr<CSSPrimitiveValue> computedStyleValueForColorStopPosition(const StyleGradientImageAngularStop& stop, const RenderStyle&)
{
    return WTF::switchOn(stop.position,
        [](std::monostate) -> RefPtr<CSSPrimitiveValue> {
            return nullptr;
        },
        [](AngleRaw angle) -> RefPtr<CSSPrimitiveValue> {
            return CSSPrimitiveValue::create(angle.value, angle.type);
        },
        [](PercentRaw percent) -> RefPtr<CSSPrimitiveValue> {
            return CSSPrimitiveValue::create(percent.value, CSSUnitType::CSS_PERCENTAGE);
        }
    );
}

template<typename Stops> static CSSGradientColorStopList computeStyleStopsList(const RenderStyle& style, const Stops& stops)
{
    return stops.template map<CSSGradientColorStopList>([&](auto& stop) -> CSSGradientColorStop {
        return {
            computedStyleValueForColorStopColor(stop.color, style),
            computedStyleValueForColorStopPosition(stop, style)
        };
    });
}

template<typename Stops> static CSSGradientColorStopList computeStyleStopsListDeprecated(const RenderStyle& style, const Stops& stops)
{
    return stops.template map<CSSGradientColorStopList>([&](auto& stop) -> CSSGradientColorStop {
        return {
            computedStyleValueForColorStopColor(stop.color, style),
            computedStyleValueForColorStopPositionDeprecated(stop)
        };
    });
}

static Ref<CSSPrimitiveValue> computedStyleValue(const StyleGradientDeprecatedPoint::Coordinate& coordinate)
{
    return WTF::switchOn(coordinate.value,
        [](NumberRaw number) -> Ref<CSSPrimitiveValue> {
            return CSSPrimitiveValue::create(number.value, CSSUnitType::CSS_NUMBER);
        },
        [](PercentRaw percent) -> Ref<CSSPrimitiveValue> {
            return CSSPrimitiveValue::create(percent.value, CSSUnitType::CSS_PERCENTAGE);
        }
    );
}

static CSSGradientDeprecatedPoint computedStyleValue(const StyleGradientDeprecatedPoint& point)
{
    return {
        computedStyleValue(point.x),
        computedStyleValue(point.y)
    };
}

static Ref<CSSValue> computedStylePositionCoordinate(const StyleGradientPosition::Coordinate& coordinate, const RenderStyle& style)
{
    return ComputedStyleExtractor::zoomAdjustedPixelValueForLength(coordinate.length, style);
}

static CSSGradientPosition computedStyleValue(const StyleGradientPosition& position, const RenderStyle& style)
{
    return {
        computedStylePositionCoordinate(position.x, style),
        computedStylePositionCoordinate(position.y, style)
    };
}

static std::optional<CSSGradientPosition> computedStyleValue(const std::optional<StyleGradientPosition>& position, const RenderStyle& style)
{
    if (!position)
        return std::nullopt;
    return computedStyleValue(*position, style);
}

// MARK: Computed Style Extractors

static Ref<CSSValue> computedStyleValue(const StyleGradientImage::LinearData& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, const RenderStyle& style)
{
    auto gradientLine = WTF::switchOn(data.gradientLine,
        [](auto& value) -> CSSLinearGradientValue::GradientLine {
            return value;
        }
    );

    return CSSLinearGradientValue::create({
            WTFMove(gradientLine)
        },
        data.repeating,
        colorInterpolationMethod,
        computeStyleStopsList(style, data.stops)
    );
}

static Ref<CSSValue> computedStyleValue(const StyleGradientImage::PrefixedLinearData& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, const RenderStyle& style)
{
    auto gradientLine = WTF::switchOn(data.gradientLine,
        [](auto& value) -> CSSPrefixedLinearGradientValue::GradientLine {
            return value;
        }
    );

    return CSSPrefixedLinearGradientValue::create({
            WTFMove(gradientLine)
        },
        data.repeating,
        colorInterpolationMethod,
        computeStyleStopsList(style, data.stops)
    );
}

static Ref<CSSValue> computedStyleValue(const StyleGradientImage::DeprecatedLinearData& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, const RenderStyle& style)
{
    return CSSDeprecatedLinearGradientValue::create({
            computedStyleValue(data.first),
            computedStyleValue(data.second)
        },
        colorInterpolationMethod,
        computeStyleStopsListDeprecated(style, data.stops)
    );
}

static Ref<CSSValue> computedStyleValue(const StyleGradientImage::RadialData& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, const RenderStyle& style)
{
    auto gradientBox = WTF::switchOn(data.gradientBox,
        [&](std::monostate) -> CSSRadialGradientValue::GradientBox {
            return std::monostate { };
        },
        [&](const StyleGradientImage::RadialData::Shape& shape) -> CSSRadialGradientValue::GradientBox {
            return CSSRadialGradientValue::Shape {
                shape.shape,
                computedStyleValue(shape.position, style)
            };
        },
        [&](const StyleGradientImage::RadialData::Extent& extent) -> CSSRadialGradientValue::GradientBox {
            return CSSRadialGradientValue::Extent {
                extent.extent,
                computedStyleValue(extent.position, style)
            };
        },
        [&](const StyleGradientImage::RadialData::Length& length) -> CSSRadialGradientValue::GradientBox {
            return CSSRadialGradientValue::Length {
                ComputedStyleExtractor::zoomAdjustedPixelValueForLength(length.length, style),
                computedStyleValue(length.position, style)
            };
        },
        [&](const StyleGradientImage::RadialData::Size& size) -> CSSRadialGradientValue::GradientBox {
            return CSSRadialGradientValue::Size {
                {
                    ComputedStyleExtractor::zoomAdjustedPixelValueForLength(size.size.width, style),
                    ComputedStyleExtractor::zoomAdjustedPixelValueForLength(size.size.height, style)
                },
                computedStyleValue(size.position, style)
            };
        },
        [&](const StyleGradientImage::RadialData::CircleOfLength& circleOfLength) -> CSSRadialGradientValue::GradientBox {
            return CSSRadialGradientValue::CircleOfLength {
                ComputedStyleExtractor::zoomAdjustedPixelValueForLength(circleOfLength.length, style),
                computedStyleValue(circleOfLength.position, style)
            };
        },
        [&](const StyleGradientImage::RadialData::CircleOfExtent& circleOfExtent) -> CSSRadialGradientValue::GradientBox {
            return CSSRadialGradientValue::CircleOfExtent {
                circleOfExtent.extent,
                computedStyleValue(circleOfExtent.position, style)
            };
        },
        [&](const StyleGradientImage::RadialData::EllipseOfSize& ellipseOfSize) -> CSSRadialGradientValue::GradientBox {
            return CSSRadialGradientValue::EllipseOfSize {
                {
                    ComputedStyleExtractor::zoomAdjustedPixelValueForLength(ellipseOfSize.size.width, style),
                    ComputedStyleExtractor::zoomAdjustedPixelValueForLength(ellipseOfSize.size.height, style)
                },
                computedStyleValue(ellipseOfSize.position, style)
            };
        },
        [&](const StyleGradientImage::RadialData::EllipseOfExtent& ellipseOfExtent) -> CSSRadialGradientValue::GradientBox {
            return CSSRadialGradientValue::EllipseOfExtent {
                ellipseOfExtent.extent,
                computedStyleValue(ellipseOfExtent.position, style)
            };
        },
        [&](const StyleGradientPosition& position) -> CSSRadialGradientValue::GradientBox {
            return computedStyleValue(position, style);
        }
    );

    return CSSRadialGradientValue::create({
            WTFMove(gradientBox)
        },
        data.repeating,
        colorInterpolationMethod,
        computeStyleStopsList(style, data.stops)
    );
}

static Ref<CSSValue> computedStyleValue(const StyleGradientImage::PrefixedRadialData& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, const RenderStyle& style)
{
    auto gradientBox = WTF::switchOn(data.gradientBox,
        [&](std::monostate) -> CSSPrefixedRadialGradientValue::GradientBox {
            return std::monostate { };
        },
        [&](const StyleGradientImage::PrefixedRadialData::ShapeKeyword& shape) -> CSSPrefixedRadialGradientValue::GradientBox {
            return shape;
        },
        [&](const StyleGradientImage::PrefixedRadialData::ExtentKeyword& extent) -> CSSPrefixedRadialGradientValue::GradientBox {
            return extent;
        },
        [&](const StyleGradientImage::PrefixedRadialData::ShapeAndExtent& shapeAndExtent) -> CSSPrefixedRadialGradientValue::GradientBox {
            return shapeAndExtent;
        },
        [&](const StyleGradientImage::PrefixedRadialData::MeasuredSize& measuredSize) -> CSSPrefixedRadialGradientValue::GradientBox {
            return CSSPrefixedRadialGradientValue::MeasuredSize {
                {
                    ComputedStyleExtractor::zoomAdjustedPixelValueForLength(measuredSize.size.width, style),
                    ComputedStyleExtractor::zoomAdjustedPixelValueForLength(measuredSize.size.height, style)
                }
            };
        }
    );

    return CSSPrefixedRadialGradientValue::create({
            WTFMove(gradientBox),
            computedStyleValue(data.position, style)
        },
        data.repeating,
        colorInterpolationMethod,
        computeStyleStopsList(style, data.stops)
    );
}

static Ref<CSSValue> computedStyleValue(const StyleGradientImage::DeprecatedRadialData& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, const RenderStyle& style)
{
    return CSSDeprecatedRadialGradientValue::create({
            computedStyleValue(data.first),
            computedStyleValue(data.second),
            NumberRaw { data.firstRadius },
            NumberRaw { data.secondRadius }
        },
        colorInterpolationMethod,
        computeStyleStopsListDeprecated(style, data.stops)
    );
}

static Ref<CSSValue> computedStyleValue(const StyleGradientImage::ConicData& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, const RenderStyle& style)
{
    auto convertAngle = [](std::optional<AngleRaw> angle) -> CSSConicGradientValue::Angle {
        if (angle)
            return { *angle };
        return { std::monostate { } };
    };

    return CSSConicGradientValue::create({
            convertAngle(data.angle),
            computedStyleValue(data.position, style)
        },
        data.repeating,
        colorInterpolationMethod,
        computeStyleStopsList(style, data.stops)
    );
}

Ref<CSSValue> StyleGradientImage::computedStyleValue(const RenderStyle& style) const
{
    return WTF::switchOn(m_data, [&](const auto& data) -> Ref<CSSValue> { return WebCore::computedStyleValue(data, m_colorInterpolationMethod, style); } );
}

bool StyleGradientImage::isPending() const
{
    return false;
}

void StyleGradientImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

RefPtr<Image> StyleGradientImage::image(const RenderElement* renderer, const FloatSize& size, bool isForFirstLine) const
{
    if (!renderer)
        return &Image::nullImage();

    if (size.isEmpty())
        return nullptr;

    auto& style = isForFirstLine ? renderer->firstLineStyle() : renderer->style();

    bool cacheable = m_knownCacheableBarringFilter && !style.hasAppleColorFilter();
    if (cacheable) {
        if (!clients().contains(const_cast<RenderElement&>(*renderer)))
            return nullptr;
        if (auto* result = const_cast<StyleGradientImage&>(*this).cachedImageForSize(size))
            return result;
    }

    auto gradient = WTF::switchOn(m_data,
        [&](auto& data) -> Ref<Gradient> {
            return createGradient(data, size, style);
        }
    );

    auto newImage = GradientImage::create(WTFMove(gradient), size);
    if (cacheable)
        const_cast<StyleGradientImage&>(*this).saveCachedImageForSize(size, newImage);
    return newImage;
}

template<typename Stops> static bool knownToBeOpaque(const RenderElement& renderer, const Stops& stops)
{
    auto& style = renderer.style();
    bool hasColorFilter = style.hasAppleColorFilter();
    for (auto& stop : stops) {
        if (!resolveColorStopColor(stop.color, style, hasColorFilter).isOpaque())
            return false;
    }
    return true;
}

bool StyleGradientImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return WTF::switchOn(m_data, [&](auto& data) { return WebCore::knownToBeOpaque(renderer, data.stops); } );
}

FloatSize StyleGradientImage::fixedSize(const RenderElement&) const
{
    return { };
}

// MARK: Gradient creation.

namespace {

struct ResolvedGradientStop {
    Color color;
    std::optional<float> offset;

    bool isSpecified() const { return offset.has_value(); }
    bool isMidpoint() const { return !color.isValid(); }
};

class LinearGradientAdapter {
public:
    explicit LinearGradientAdapter(Gradient::LinearData& data)
        : m_data(data)
    {
    }

    float gradientLength() const
    {
        auto gradientSize = m_data.point0 - m_data.point1;
        return gradientSize.diagonalLength();
    }

    static constexpr float maxExtent(float, float)   { return 1; }

    void normalizeStopsAndEndpointsOutsideRange(Vector<ResolvedGradientStop>& stops, ColorInterpolationMethod)
    {
        float firstOffset = *stops.first().offset;
        float lastOffset = *stops.last().offset;
        if (firstOffset != lastOffset) {
            float scale = lastOffset - firstOffset;

            for (auto& stop : stops)
                stop.offset = (*stop.offset - firstOffset) / scale;

            auto p0 = m_data.point0;
            auto p1 = m_data.point1;
            m_data.point0 = { p0.x() + firstOffset * (p1.x() - p0.x()), p0.y() + firstOffset * (p1.y() - p0.y()) };
            m_data.point1 = { p1.x() + (lastOffset - 1) * (p1.x() - p0.x()), p1.y() + (lastOffset - 1) * (p1.y() - p0.y()) };
        } else {
            // There's a single position that is outside the scale, clamp the positions to 1.
            for (auto& stop : stops)
                stop.offset = 1;
        }
    }

private:
    Gradient::LinearData& m_data;
};

class RadialGradientAdapter {
public:
    explicit RadialGradientAdapter(Gradient::RadialData& data)
        : m_data(data)
    {
    }

    float gradientLength() const { return m_data.endRadius; }

    // Radial gradients may need to extend further than the endpoints, because they have
    // to repeat out to the corners of the box.
    float maxExtent(float maxLengthForRepeat, float gradientLength) const
    {
        if (maxLengthForRepeat > gradientLength)
            return gradientLength > 0 ? maxLengthForRepeat / gradientLength : 0;
        return 1;
    }

    void normalizeStopsAndEndpointsOutsideRange(Vector<ResolvedGradientStop>& stops, ColorInterpolationMethod colorInterpolationMethod)
    {
        auto numberOfStops = stops.size();

        // Rather than scaling the points < 0, we truncate them, so only scale according to the largest point.
        float firstOffset = 0;
        float lastOffset = *stops.last().offset;
        float scale = lastOffset - firstOffset;

        // Reset points below 0 to the first visible color.
        size_t firstZeroOrGreaterIndex = numberOfStops;
        for (size_t i = 0; i < numberOfStops; ++i) {
            if (*stops[i].offset >= 0) {
                firstZeroOrGreaterIndex = i;
                break;
            }
        }

        if (firstZeroOrGreaterIndex > 0) {
            if (firstZeroOrGreaterIndex < numberOfStops && *stops[firstZeroOrGreaterIndex].offset > 0) {
                float prevOffset = *stops[firstZeroOrGreaterIndex - 1].offset;
                float nextOffset = *stops[firstZeroOrGreaterIndex].offset;

                float interStopProportion = -prevOffset / (nextOffset - prevOffset);
                auto blendedColor = interpolateColors(colorInterpolationMethod, stops[firstZeroOrGreaterIndex - 1].color, 1.0f - interStopProportion, stops[firstZeroOrGreaterIndex].color, interStopProportion);

                // Clamp the positions to 0 and set the color.
                for (size_t i = 0; i < firstZeroOrGreaterIndex; ++i) {
                    stops[i].offset = 0;
                    stops[i].color = blendedColor;
                }
            } else {
                // All stops are below 0; just clamp them.
                for (size_t i = 0; i < firstZeroOrGreaterIndex; ++i)
                    stops[i].offset = 0;
            }
        }

        for (auto& stop : stops)
            *stop.offset /= scale;

        m_data.startRadius *= scale;
        m_data.endRadius *= scale;
    }

private:
    Gradient::RadialData& m_data;
};

class ConicGradientAdapter {
public:
    static constexpr float gradientLength() { return 1; }
    static constexpr float maxExtent(float, float) { return 1; }

    void normalizeStopsAndEndpointsOutsideRange(Vector<ResolvedGradientStop>& stops, ColorInterpolationMethod colorInterpolationMethod)
    {
        size_t numberOfStops = stops.size();
        size_t lastStopIndex = numberOfStops - 1;

        std::optional<size_t> firstZeroOrGreaterIndex;
        for (size_t i = 0; i < numberOfStops; ++i) {
            if (*stops[i].offset >= 0) {
                firstZeroOrGreaterIndex = i;
                break;
            }
        }

        if (firstZeroOrGreaterIndex) {
            size_t index = *firstZeroOrGreaterIndex;
            if (index > 0) {
                float previousOffset = *stops[index - 1].offset;
                float nextOffset = *stops[index].offset;

                float interStopProportion = -previousOffset / (nextOffset - previousOffset);
                auto blendedColor = interpolateColors(colorInterpolationMethod, stops[index - 1].color, 1.0f - interStopProportion, stops[index].color, interStopProportion);

                // Clamp the positions to 0 and set the color.
                for (size_t i = 0; i < index; ++i) {
                    stops[i].offset = 0;
                    stops[i].color = blendedColor;
                }
            }
        } else {
            // All stop offsets below 0, clamp them.
            for (auto& stop : stops)
                stop.offset = 0;
        }

        std::optional<size_t> lastOneOrLessIndex;
        for (int i = lastStopIndex; i >= 0; --i) {
            if (*stops[i].offset <= 1) {
                lastOneOrLessIndex = i;
                break;
            }
        }

        if (lastOneOrLessIndex) {
            size_t index = *lastOneOrLessIndex;
            if (index < lastStopIndex) {
                float previousOffset = *stops[index].offset;
                float nextOffset = *stops[index + 1].offset;

                float interStopProportion = (1 - previousOffset) / (nextOffset - previousOffset);
                auto blendedColor = interpolateColors(colorInterpolationMethod, stops[index].color, 1.0f - interStopProportion, stops[index + 1].color, interStopProportion);

                // Clamp the positions to 1 and set the color.
                for (size_t i = index + 1; i < numberOfStops; ++i) {
                    stops[i].offset = 1;
                    stops[i].color = blendedColor;
                }
            }
        } else {
            // All stop offsets above 1, clamp them.
            for (auto& stop : stops)
                stop.offset = 1;
        }
    }
};

} // anonymous namespace

template<typename GradientAdapter, typename Stops> GradientColorStops StyleGradientImage::computeStopsForDeprecatedVariants(GradientAdapter&, const Stops& styleStops, const RenderStyle& style) const
{
    bool hasColorFilter = style.hasAppleColorFilter();
    auto result = styleStops.template map<GradientColorStops::StopVector>([&](auto& stop) -> GradientColorStop {
        return {
            stop.position->isPercent() ? stop.position->percent() / 100.0f : stop.position->value(),
            resolveColorStopColor(stop.color, style, hasColorFilter)
        };
    });

    std::ranges::stable_sort(result, [](const auto& a, const auto& b) {
        return a.offset < b.offset;
    });

    return GradientColorStops::Sorted { WTFMove(result) };
}

template<typename GradientAdapter, typename Stops> GradientColorStops StyleGradientImage::computeStops(GradientAdapter& gradientAdapter, const Stops& styleStops, const RenderStyle& style, float maxLengthForRepeat, CSSGradientRepeat repeating) const
{
    bool hasColorFilter = style.hasAppleColorFilter();

    size_t numberOfStops = styleStops.size();
    Vector<ResolvedGradientStop> stops(numberOfStops);

    float gradientLength = gradientAdapter.gradientLength();

    for (size_t i = 0; i < numberOfStops; ++i) {
        auto& stop = styleStops[i];

        stops[i].color = resolveColorStopColor(stop.color, style, hasColorFilter);

        auto offset = resolveColorStopPosition(stop, gradientLength);
        if (offset)
            stops[i].offset = *offset;
        else {
            // If the first color-stop does not have a position, its position defaults to 0%.
            // If the last color-stop does not have a position, its position defaults to 100%.
            if (!i)
                stops[i].offset = 0;
            else if (numberOfStops > 1 && i == numberOfStops - 1)
                stops[i].offset = 1;
        }

        // If a color-stop has a position that is less than the specified position of any
        // color-stop before it in the list, its position is changed to be equal to the
        // largest specified position of any color-stop before it.
        if (stops[i].isSpecified() && i > 0) {
            size_t prevSpecifiedIndex;
            for (prevSpecifiedIndex = i - 1; prevSpecifiedIndex; --prevSpecifiedIndex) {
                if (stops[prevSpecifiedIndex].isSpecified())
                    break;
            }

            if (*stops[i].offset < *stops[prevSpecifiedIndex].offset)
                stops[i].offset = stops[prevSpecifiedIndex].offset;
        }
    }

    ASSERT(stops[0].isSpecified() && stops[numberOfStops - 1].isSpecified());

    // If any color-stop still does not have a position, then, for each run of adjacent
    // color-stops without positions, set their positions so that they are evenly spaced
    // between the preceding and following color-stops with positions.
    if (numberOfStops > 2) {
        size_t unspecifiedRunStart = 0;
        bool inUnspecifiedRun = false;

        for (size_t i = 0; i < numberOfStops; ++i) {
            if (!stops[i].isSpecified() && !inUnspecifiedRun) {
                unspecifiedRunStart = i;
                inUnspecifiedRun = true;
            } else if (stops[i].isSpecified() && inUnspecifiedRun) {
                size_t unspecifiedRunEnd = i;

                if (unspecifiedRunStart < unspecifiedRunEnd) {
                    float lastSpecifiedOffset = *stops[unspecifiedRunStart - 1].offset;
                    float nextSpecifiedOffset = *stops[unspecifiedRunEnd].offset;
                    float delta = (nextSpecifiedOffset - lastSpecifiedOffset) / (unspecifiedRunEnd - unspecifiedRunStart + 1);

                    for (size_t j = unspecifiedRunStart; j < unspecifiedRunEnd; ++j)
                        stops[j].offset = lastSpecifiedOffset + (j - unspecifiedRunStart + 1) * delta;
                }

                inUnspecifiedRun = false;
            }
        }
    }

    // Walk over the color stops, look for midpoints and add stops as needed.
    // If mid < 50%, add 2 stops to the left and 6 to the right
    // else add 6 stops to the left and 2 to the right.
    // Stops on the side with the most stops start midway because the curve approximates
    // a line in that region. We then add 5 more color stops on that side to minimize the change
    // how the luminance changes at each of the color stops. We don't have to add as many on the other side
    // since it becomes small which increases the differentation of luminance which hides the color stops.
    // Even with 4 extra color stops, it *is* possible to discern the steps when the gradient is large and has
    // large luminance differences between midpoint and color stop. If this becomes an issue, we can consider
    // making this algorithm a bit smarter.

    // Midpoints that coincide with color stops are treated specially since they don't require
    // extra stops and generate hard lines.
    for (size_t x = 1; x < stops.size() - 1;) {
        if (!stops[x].isMidpoint()) {
            ++x;
            continue;
        }

        // Find previous and next color so we know what to interpolate between.
        // We already know they have a color since we checked for that earlier.
        Color color1 = stops[x - 1].color;
        Color color2 = stops[x + 1].color;
        // Likewise find the position of previous and next color stop.
        float offset1 = *stops[x - 1].offset;
        float offset2 = *stops[x + 1].offset;
        float offset = *stops[x].offset;

        // Check if everything coincides or the midpoint is exactly in the middle.
        // If so, ignore the midpoint.
        if (offset - offset1 == offset2 - offset) {
            stops.remove(x);
            continue;
        }

        // Check if we coincide with the left color stop.
        if (offset1 == offset) {
            // Morph the midpoint to a regular stop with the color of the next color stop.
            stops[x].color = color2;
            continue;
        }

        // Check if we coincide with the right color stop.
        if (offset2 == offset) {
            // Morph the midpoint to a regular stop with the color of the previous color stop.
            stops[x].color = color1;
            continue;
        }

        float midpoint = (offset - offset1) / (offset2 - offset1);
        ResolvedGradientStop newStops[9];
        if (midpoint > .5f) {
            for (size_t y = 0; y < 7; ++y)
                newStops[y].offset = offset1 + (offset - offset1) * (7 + y) / 13;

            newStops[7].offset = offset + (offset2 - offset) / 3;
            newStops[8].offset = offset + (offset2 - offset) * 2 / 3;
        } else {
            newStops[0].offset = offset1 + (offset - offset1) / 3;
            newStops[1].offset = offset1 + (offset - offset1) * 2 / 3;

            for (size_t y = 0; y < 7; ++y)
                newStops[y + 2].offset = offset + (offset2 - offset) * y / 13;
        }
        // calculate colors
        for (size_t y = 0; y < 9; ++y) {
            float relativeOffset = (*newStops[y].offset - offset1) / (offset2 - offset1);
            float multiplier = std::pow(relativeOffset, std::log(.5f) / std::log(midpoint));
            newStops[y].color = interpolateColors(m_colorInterpolationMethod.method, color1, 1.0f - multiplier, color2, multiplier);
        }

        stops.remove(x);
        stops.insert(x, newStops, 9);
        x += 9;
    }

    numberOfStops = stops.size();

    // If the gradient is repeating, repeat the color stops.
    // We can't just push this logic down into the platform-specific Gradient code,
    // because we have to know the extent of the gradient, and possible move the end points.
    if (repeating == CSSGradientRepeat::Repeating && numberOfStops > 1) {
        float maxExtent = gradientAdapter.maxExtent(maxLengthForRepeat, gradientLength);
        // If the difference in the positions of the first and last color-stops is 0,
        // the gradient defines a solid-color image with the color of the last color-stop in the rule.
        float gradientRange = *stops.last().offset - *stops.first().offset;
        if (maxExtent > 1)
            gradientRange /= maxExtent;
        if (!gradientRange) {
            stops.first().offset = 0;
            stops.first().color = stops.last().color;
            stops.shrink(1);
            numberOfStops = 1;
        } else if (std::abs(gradientRange) < (float(1) / (2 << 15))) {
            // If the gradient range is too small, the subsequent replication of stops
            // across the complete [0, maxExtent] range can challenging to complete both
            // because of potentially-expensive initial traversal across the [0, first-offset]
            // and [last-offset, maxExtent] ranges as well as likely exorbitant memory consumption
            // needed for all such generated stops. In case of such a gradient range the initial
            // Vector of stops remains unchanged, and additional stops for the purpose of the
            // repeating nature of the gradient are not computed.
        } else {
            // Since the gradient range is deemed big enough, the amount of necessary stops is
            // calculated for both the [0, first-offset] and the [last-offset, maxExtent] ranges.
            CheckedSize numberOfGeneratedStopsBeforeFirst;
            CheckedSize numberOfGeneratedStopsAfterLast;

            if (*stops.first().offset > 0) {
                float currOffset = *stops.first().offset;
                size_t srcStopOrdinal = numberOfStops - 1;

                while (true) {
                    ++numberOfGeneratedStopsBeforeFirst;
                    if (currOffset < 0)
                        break;

                    if (srcStopOrdinal)
                        currOffset -= *stops[srcStopOrdinal].offset - *stops[srcStopOrdinal - 1].offset;
                    srcStopOrdinal = (srcStopOrdinal + numberOfStops - 1) % numberOfStops;
                }
            }

            if (*stops.last().offset < maxExtent) {
                float currOffset = *stops.last().offset;
                size_t srcStopOrdinal = 0;

                while (true) {
                    ++numberOfGeneratedStopsAfterLast;
                    if (currOffset > maxExtent)
                        break;

                    if (srcStopOrdinal < numberOfStops - 1)
                        currOffset += *stops[srcStopOrdinal + 1].offset - *stops[srcStopOrdinal].offset;
                    srcStopOrdinal = (srcStopOrdinal + 1) % numberOfStops;
                }
            }

            // With the number of stops necessary for the repeating gradient now known, we can impose
            // some reasonable limit to prevent generation of memory-expensive amounts of gradient stops.
            CheckedSize checkedNumberOfGeneratedStops = CheckedSize(numberOfStops) + numberOfGeneratedStopsBeforeFirst + numberOfGeneratedStopsAfterLast;
            if (checkedNumberOfGeneratedStops.hasOverflowed() || checkedNumberOfGeneratedStops.value() > (2 << 15)) {
                // More than 65536 gradient stops are expected. Let's fall back to the initially-provided
                // Vector of stops, effectively meaning the repetition of stops is not applied.
            } else {
                // An affordable amount of gradient stops is determined. A separate Vector object is constructed
                // accordingly, first generating the repeated stops in the [0, first-offset] range, then adding
                // the original stops, and finally generating the repeated stops in the [last-offset, maxExtent]
                // range. The resulting Vector is then moved in to replace the original stops.
                Vector<ResolvedGradientStop> generatedStops;
                generatedStops.reserveInitialCapacity(checkedNumberOfGeneratedStops.value());

                if (numberOfGeneratedStopsBeforeFirst > 0) {
                    float currOffset = *stops.first().offset;
                    size_t srcStopOrdinal = numberOfStops - 1;

                    for (size_t i = 0; i < numberOfGeneratedStopsBeforeFirst; ++i) {
                        auto newStop = stops[srcStopOrdinal];
                        newStop.offset = currOffset;
                        generatedStops.append(newStop);

                        if (srcStopOrdinal)
                            currOffset -= *stops[srcStopOrdinal].offset - *stops[srcStopOrdinal - 1].offset;
                        srcStopOrdinal = (srcStopOrdinal + numberOfStops - 1) % numberOfStops;
                    }

                    generatedStops.reverse();
                }

                generatedStops.appendVector(stops);

                if (numberOfGeneratedStopsAfterLast > 0) {
                    float currOffset = *stops.last().offset;
                    size_t srcStopOrdinal = 0;

                    for (size_t i = 0; i < numberOfGeneratedStopsAfterLast; ++i) {
                        auto newStop = stops[srcStopOrdinal];
                        newStop.offset = currOffset;
                        generatedStops.append(newStop);

                        if (srcStopOrdinal < numberOfStops - 1)
                            currOffset += *stops[srcStopOrdinal + 1].offset - *stops[srcStopOrdinal].offset;
                        srcStopOrdinal = (srcStopOrdinal + 1) % numberOfStops;
                    }
                }

                stops = WTFMove(generatedStops);
            }
        }
    }

    // If the gradient goes outside the 0-1 range, normalize it by moving the endpoints, and adjusting the stops.
    if (stops.size() > 1 && (*stops.first().offset < 0 || *stops.last().offset > 1))
        gradientAdapter.normalizeStopsAndEndpointsOutsideRange(stops, m_colorInterpolationMethod.method);
    
    return GradientColorStops::Sorted {
        stops.template map<GradientColorStops::StopVector>([](auto& stop) -> GradientColorStop {
            return { *stop.offset, stop.color };
        })
    };
}

static inline float resolveLengthPercentage(const Length& length, float widthOrHeight)
{
    if (length.isFixed())
        return length.value();

    if (length.isPercent())
        return length.percent() / 100.0f * widthOrHeight;

    if (length.isCalculated())
        return length.calculationValue().evaluate(widthOrHeight);

    ASSERT_NOT_REACHED();
    return 0.0f;
}

static inline float positionFromValue(const StyleGradientPosition::Coordinate& coordinate, float widthOrHeight)
{
    return resolveLengthPercentage(coordinate.length, widthOrHeight);
}

static inline FloatPoint computeEndPoint(const StyleGradientPosition& value, const FloatSize& size)
{
    return {
        positionFromValue(value.x, size.width()),
        positionFromValue(value.y, size.height())
    };
}

static float positionFromValue(const StyleGradientDeprecatedPoint::Coordinate& coordinate, float edgeDistance)
{
    return WTF::switchOn(coordinate.value,
        [&](NumberRaw number) -> float {
            return number.value;
        },
        [&](PercentRaw percent) -> float {
            return percent.value / 100.0f * edgeDistance;
        }
    );
}

static inline FloatPoint computeEndPoint(const StyleGradientDeprecatedPoint& point, const FloatSize& size)
{
    return {
        positionFromValue(point.x, size.width()),
        positionFromValue(point.y, size.height())
    };
}

// Compute the endpoints so that a gradient of the given angle covers a box of the given size.
static std::pair<FloatPoint, FloatPoint> endPointsFromAngle(float angleDeg, const FloatSize& size)
{
    angleDeg = toPositiveAngle(angleDeg);

    if (!angleDeg)
        return { { 0, size.height() }, { 0, 0 } };

    if (angleDeg == 90)
        return { { 0, 0 }, { size.width(), 0 } };

    if (angleDeg == 180)
        return { { 0, 0 }, { 0, size.height() } };

    if (angleDeg == 270)
        return { { size.width(), 0 }, { 0, 0 } };

    // angleDeg is a "bearing angle" (0deg = N, 90deg = E),
    // but tan expects 0deg = E, 90deg = N.
    float slope = std::tan(deg2rad(90 - angleDeg));

    // We find the endpoint by computing the intersection of the line formed by the slope,
    // and a line perpendicular to it that intersects the corner.
    float perpendicularSlope = -1 / slope;

    // Compute start corner relative to center, in Cartesian space (+y = up).
    float halfHeight = size.height() / 2;
    float halfWidth = size.width() / 2;
    FloatPoint endCorner;
    if (angleDeg < 90)
        endCorner.set(halfWidth, halfHeight);
    else if (angleDeg < 180)
        endCorner.set(halfWidth, -halfHeight);
    else if (angleDeg < 270)
        endCorner.set(-halfWidth, -halfHeight);
    else
        endCorner.set(-halfWidth, halfHeight);

    // Compute c (of y = mx + c) using the corner point.
    float c = endCorner.y() - perpendicularSlope * endCorner.x();
    float endX = c / (slope - perpendicularSlope);
    float endY = perpendicularSlope * endX + c;

    // We computed the end point, so set the second point,
    // taking into account the moved origin and the fact
    // that we're in drawing space (+y = down). Reflect
    // around the center for the start point.
    
    return { FloatPoint(halfWidth - endX, halfHeight + endY), FloatPoint(halfWidth + endX, halfHeight - endY) };
}

static std::pair<FloatPoint, FloatPoint> endPointsFromAngleForPrefixedVariants(float angleDeg, const FloatSize& size)
{
    // Prefixed gradients use "polar coordinate" angles, rather than "bearing" angles.
    return endPointsFromAngle(90 - angleDeg, size);
}

static float resolveRadius(const Length& radius, float widthOrHeight)
{
    return resolveLengthPercentage(radius, widthOrHeight);
}

struct DistanceToCorner {
    float distance;
    FloatPoint corner;
};

static DistanceToCorner findDistanceToClosestCorner(const FloatPoint& p, const FloatSize& size)
{
    FloatPoint topLeft;
    float topLeftDistance = FloatSize(p - topLeft).diagonalLength();

    FloatPoint topRight(size.width(), 0);
    float topRightDistance = FloatSize(p - topRight).diagonalLength();

    FloatPoint bottomLeft(0, size.height());
    float bottomLeftDistance = FloatSize(p - bottomLeft).diagonalLength();

    FloatPoint bottomRight(size.width(), size.height());
    float bottomRightDistance = FloatSize(p - bottomRight).diagonalLength();

    FloatPoint corner = topLeft;
    float minDistance = topLeftDistance;

    if (topRightDistance < minDistance) {
        minDistance = topRightDistance;
        corner = topRight;
    }

    if (bottomLeftDistance < minDistance) {
        minDistance = bottomLeftDistance;
        corner = bottomLeft;
    }

    if (bottomRightDistance < minDistance) {
        minDistance = bottomRightDistance;
        corner = bottomRight;
    }
    return { minDistance, corner };
}

static DistanceToCorner findDistanceToFarthestCorner(const FloatPoint& p, const FloatSize& size)
{
    FloatPoint topLeft;
    float topLeftDistance = FloatSize(p - topLeft).diagonalLength();

    FloatPoint topRight(size.width(), 0);
    float topRightDistance = FloatSize(p - topRight).diagonalLength();

    FloatPoint bottomLeft(0, size.height());
    float bottomLeftDistance = FloatSize(p - bottomLeft).diagonalLength();

    FloatPoint bottomRight(size.width(), size.height());
    float bottomRightDistance = FloatSize(p - bottomRight).diagonalLength();

    FloatPoint corner = topLeft;
    float maxDistance = topLeftDistance;

    if (topRightDistance > maxDistance) {
        maxDistance = topRightDistance;
        corner = topRight;
    }

    if (bottomLeftDistance > maxDistance) {
        maxDistance = bottomLeftDistance;
        corner = bottomLeft;
    }

    if (bottomRightDistance > maxDistance) {
        maxDistance = bottomRightDistance;
        corner = bottomRight;
    }

    return { maxDistance, corner };
}

// Compute horizontal radius of ellipse with center at 0,0 which passes through p, and has
// width/height given by aspectRatio.
static inline float horizontalEllipseRadius(const FloatSize& p, float aspectRatio)
{
    // x^2/a^2 + y^2/b^2 = 1
    // a/b = aspectRatio, b = a/aspectRatio
    // a = sqrt(x^2 + y^2/(1/r^2))
    return std::hypot(p.width(), p.height() * aspectRatio);
}

// MARK: - Linear create.

Ref<Gradient> StyleGradientImage::createGradient(const LinearData& linear, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto [firstPoint, secondPoint] = WTF::switchOn(linear.gradientLine,
        [&](std::monostate) -> std::pair<FloatPoint, FloatPoint> {
            return { FloatPoint { 0, 0 }, FloatPoint { 0, size.height() } };
        },
        [&](const AngleRaw& angle) -> std::pair<FloatPoint, FloatPoint> {
            return endPointsFromAngle(CSSPrimitiveValue::computeDegrees(angle.type, angle.value), size);
        },
        [&](LinearData::Horizontal horizontal) -> std::pair<FloatPoint, FloatPoint> {
            switch (horizontal) {
            case LinearData::Horizontal::Left:
                return { FloatPoint { size.width(), 0 }, FloatPoint { 0, 0 } };
            case LinearData::Horizontal::Right:
                return { FloatPoint { 0, 0 }, FloatPoint { size.width(), 0 } };
            }
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](LinearData::Vertical vertical) -> std::pair<FloatPoint, FloatPoint> {
            switch (vertical) {
            case LinearData::Vertical::Top:
                return { FloatPoint { 0, size.height() }, FloatPoint { 0, 0 } };
            case LinearData::Vertical::Bottom:
                return { FloatPoint { 0, 0 }, FloatPoint { 0, size.height() } };
            }
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const std::pair<LinearData::Horizontal, LinearData::Vertical>& pair) -> std::pair<FloatPoint, FloatPoint> {
            float rise = size.width();
            float run = size.height();
            if (pair.first == LinearData::Horizontal::Left)
                run *= -1;
            if (pair.second == LinearData::Vertical::Bottom)
                rise *= -1;
            // Compute angle, and flip it back to "bearing angle" degrees.
            float angle = 90 - rad2deg(atan2(rise, run));
            return endPointsFromAngle(angle, size);
        }
    );

    Gradient::LinearData data { firstPoint, secondPoint };
    LinearGradientAdapter adapter { data };
    auto stops = computeStops(adapter, linear.stops, style, 1, linear.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Prefixed Linear create.

Ref<Gradient> StyleGradientImage::createGradient(const PrefixedLinearData& linear, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto [firstPoint, secondPoint] = WTF::switchOn(linear.gradientLine,
        [&](std::monostate) -> std::pair<FloatPoint, FloatPoint> {
            return { FloatPoint { 0, 0 }, FloatPoint { 0, size.height() } };
        },
        [&](const AngleRaw& angle) -> std::pair<FloatPoint, FloatPoint> {
            return endPointsFromAngleForPrefixedVariants(CSSPrimitiveValue::computeDegrees(angle.type, angle.value), size);
        },
        [&](PrefixedLinearData::Horizontal horizontal) -> std::pair<FloatPoint, FloatPoint> {
            switch (horizontal) {
            case PrefixedLinearData::Horizontal::Left:
                return { FloatPoint { 0, 0 }, FloatPoint { size.width(), 0 } };
            case PrefixedLinearData::Horizontal::Right:
                return { FloatPoint { size.width(), 0 }, FloatPoint { 0, 0 } };
            }
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](PrefixedLinearData::Vertical vertical) -> std::pair<FloatPoint, FloatPoint> {
            switch (vertical) {
            case PrefixedLinearData::Vertical::Top:
                return { FloatPoint { 0, 0 }, FloatPoint { 0, size.height() } };
            case PrefixedLinearData::Vertical::Bottom:
                return { FloatPoint { 0, size.height() }, FloatPoint { 0, 0 } };
            }
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&](const std::pair<PrefixedLinearData::Horizontal, PrefixedLinearData::Vertical>& pair) -> std::pair<FloatPoint, FloatPoint> {
            switch (pair.first) {
            case PrefixedLinearData::Horizontal::Left:
                switch (pair.second) {
                case PrefixedLinearData::Vertical::Top:
                    return { FloatPoint { 0, 0 }, FloatPoint { size.width(), size.height() } };
                case PrefixedLinearData::Vertical::Bottom:
                    return { FloatPoint { 0, size.height() }, FloatPoint { size.width(), 0 } };
                }
                RELEASE_ASSERT_NOT_REACHED();

            case PrefixedLinearData::Horizontal::Right:
                switch (pair.second) {
                case PrefixedLinearData::Vertical::Top:
                    return { FloatPoint { size.width(), 0 }, FloatPoint { 0, size.height() } };
                case PrefixedLinearData::Vertical::Bottom:
                    return { FloatPoint { size.width(), size.height() }, FloatPoint { 0, 0 } };
                }
                RELEASE_ASSERT_NOT_REACHED();
            }
            RELEASE_ASSERT_NOT_REACHED();
        }
    );

    Gradient::LinearData data { firstPoint, secondPoint };
    LinearGradientAdapter adapter { data };
    auto stops = computeStops(adapter, linear.stops, style, 1, linear.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Deprecated Linear create.

Ref<Gradient> StyleGradientImage::createGradient(const DeprecatedLinearData& linear, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto firstPoint = computeEndPoint(linear.first, size);
    auto secondPoint = computeEndPoint(linear.second, size);

    Gradient::LinearData data { firstPoint, secondPoint };
    LinearGradientAdapter adapter { data };
    auto stops = computeStopsForDeprecatedVariants(adapter, linear.stops, style);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Radial create.

Ref<Gradient> StyleGradientImage::createGradient(const RadialData& radial, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto computeCenterPoint = [&](const StyleGradientPosition& position) -> FloatPoint {
        return computeEndPoint(position, size);
    };

    auto computeCenterPointOptional = [&](const std::optional<StyleGradientPosition>& position) -> FloatPoint {
        return position ? computeCenterPoint(*position) : FloatPoint { size.width() / 2, size.height() / 2 };
    };

    auto computeCircleRadius = [&](RadialData::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (extent) {
        case RadialData::ExtentKeyword::ClosestSide:
            return { distanceToClosestSide(centerPoint, size), 1 };

        case RadialData::ExtentKeyword::FarthestSide:
            return { distanceToFarthestSide(centerPoint, size), 1 };

        case RadialData::ExtentKeyword::ClosestCorner:
            return { distanceToClosestCorner(centerPoint, size), 1 };

        case RadialData::ExtentKeyword::FarthestCorner:
            return { distanceToFarthestCorner(centerPoint, size), 1 };
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto computeEllipseRadii = [&](RadialData::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (extent) {
        case RadialData::ExtentKeyword::ClosestSide: {
            float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
            return { xDist, xDist / yDist };
        }

        case RadialData::ExtentKeyword::FarthestSide: {
            float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
            return { xDist, xDist / yDist };
        }

        case RadialData::ExtentKeyword::ClosestCorner: {
            auto [distance, corner] = findDistanceToClosestCorner(centerPoint, size);
            // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
            // that it would if closest-side or farthest-side were specified, as appropriate.
            float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
            return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
        }

        case RadialData::ExtentKeyword::FarthestCorner: {
            auto [distance, corner] = findDistanceToFarthestCorner(centerPoint, size);
            // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
            // that it would if closest-side or farthest-side were specified, as appropriate.
            float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
            return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto computeRadii = [&](RadialData::ShapeKeyword shape, RadialData::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (shape) {
        case RadialData::ShapeKeyword::Circle:
            return computeCircleRadius(extent, centerPoint);
        case RadialData::ShapeKeyword::Ellipse:
            return computeEllipseRadii(extent, centerPoint);
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto data = WTF::switchOn(radial.gradientBox,
        [&](std::monostate) -> Gradient::RadialData {
            auto centerPoint = FloatPoint { size.width() / 2, size.height() / 2 };
            auto [endRadius, aspectRatio] = computeRadii(RadialData::ShapeKeyword::Ellipse, RadialData::ExtentKeyword::FarthestCorner, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const RadialData::Shape& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto [endRadius, aspectRatio] = computeRadii(data.shape, RadialData::ExtentKeyword::FarthestCorner, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const RadialData::Extent& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto [endRadius, aspectRatio] = computeRadii(RadialData::ShapeKeyword::Ellipse, data.extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const RadialData::Length& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto endRadius = resolveRadius(data.length, size.width());

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, 1 };
        },
        [&](const RadialData::CircleOfLength& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto endRadius = resolveRadius(data.length, size.width());

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, 1 };
        },
        [&](const RadialData::CircleOfExtent& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto [endRadius, aspectRatio] = computeRadii(RadialData::ShapeKeyword::Circle, data.extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, 1 };
        },
        [&](const RadialData::Size& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto endRadius = resolveRadius(data.size.width, size.width());
            auto aspectRatio = endRadius / resolveRadius(data.size.height, size.height());

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const RadialData::EllipseOfSize& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto endRadius = resolveRadius(data.size.width, size.width());
            auto aspectRatio = endRadius / resolveRadius(data.size.height, size.height());

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const RadialData::EllipseOfExtent& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto [endRadius, aspectRatio] = computeRadii(RadialData::ShapeKeyword::Ellipse, data.extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, 1 };
        },
        [&](const StyleGradientPosition& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPoint(data);
            auto [radius, aspectRatio] = computeRadii(RadialData::ShapeKeyword::Ellipse, RadialData::ExtentKeyword::FarthestCorner, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, radius, aspectRatio };
        }
    );

    // computeStops() only uses maxExtent for repeating gradients.
    float maxExtent = radial.repeating == CSSGradientRepeat::Repeating ? distanceToFarthestCorner(data.point1, size) : 0;

    RadialGradientAdapter adapter { data };
    auto stops = computeStops(adapter, radial.stops, style, maxExtent, radial.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Prefixed Radial create.

Ref<Gradient> StyleGradientImage::createGradient(const PrefixedRadialData& radial, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto computeCircleRadius = [&](PrefixedRadialData::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (extent) {
        case PrefixedRadialData::ExtentKeyword::Contain:
        case PrefixedRadialData::ExtentKeyword::ClosestSide:
            return { std::min({ centerPoint.x(), size.width() - centerPoint.x(), centerPoint.y(), size.height() - centerPoint.y() }), 1 };

        case PrefixedRadialData::ExtentKeyword::FarthestSide:
            return { std::max({ centerPoint.x(), size.width() - centerPoint.x(), centerPoint.y(), size.height() - centerPoint.y() }), 1 };

        case PrefixedRadialData::ExtentKeyword::ClosestCorner:
            return { distanceToClosestCorner(centerPoint, size), 1 };

        case PrefixedRadialData::ExtentKeyword::Cover:
        case PrefixedRadialData::ExtentKeyword::FarthestCorner:
            return { distanceToFarthestCorner(centerPoint, size), 1 };
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto computeEllipseRadii = [&](PrefixedRadialData::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (extent) {
        case PrefixedRadialData::ExtentKeyword::Contain:
        case PrefixedRadialData::ExtentKeyword::ClosestSide: {
            float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
            return { xDist, xDist / yDist };
        }

        case PrefixedRadialData::ExtentKeyword::FarthestSide: {
            float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
            return { xDist, xDist / yDist };
        }

        case PrefixedRadialData::ExtentKeyword::ClosestCorner: {
            auto [distance, corner] = findDistanceToClosestCorner(centerPoint, size);
            // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
            // that it would if closest-side or farthest-side were specified, as appropriate.
            float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
            return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
        }

        case PrefixedRadialData::ExtentKeyword::Cover:
        case PrefixedRadialData::ExtentKeyword::FarthestCorner: {
            auto [distance, corner] = findDistanceToFarthestCorner(centerPoint, size);
            // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
            // that it would if closest-side or farthest-side were specified, as appropriate.
            float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
            return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto computeRadii = [&](PrefixedRadialData::ShapeKeyword shape, PrefixedRadialData::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (shape) {
        case PrefixedRadialData::ShapeKeyword::Circle:
            return computeCircleRadius(extent, centerPoint);
        case PrefixedRadialData::ShapeKeyword::Ellipse:
            return computeEllipseRadii(extent, centerPoint);
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto computeCenterPoint = [&](const StyleGradientPosition& position) -> FloatPoint {
        return computeEndPoint(position, size);
    };

    auto computeCenterPointOptional = [&](const std::optional<StyleGradientPosition>& position) -> FloatPoint {
        return position ? computeCenterPoint(*position) : FloatPoint { size.width() / 2, size.height() / 2 };
    };

    auto centerPoint = computeCenterPointOptional(radial.position);

    auto data = WTF::switchOn(radial.gradientBox,
        [&](std::monostate) -> Gradient::RadialData {
            auto [endRadius, aspectRatio] = computeRadii(PrefixedRadialData::ShapeKeyword::Ellipse, PrefixedRadialData::ExtentKeyword::Cover, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const PrefixedRadialData::ShapeKeyword& shape) -> Gradient::RadialData {
            auto [endRadius, aspectRatio] = computeRadii(shape, PrefixedRadialData::ExtentKeyword::Cover, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const PrefixedRadialData::ExtentKeyword& extent) -> Gradient::RadialData {
            auto [endRadius, aspectRatio] = computeRadii(PrefixedRadialData::ShapeKeyword::Ellipse, extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const PrefixedRadialData::ShapeAndExtent& shapeAndExtent) -> Gradient::RadialData {
            auto [endRadius, aspectRatio] = computeRadii(shapeAndExtent.shape, shapeAndExtent.extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const PrefixedRadialData::MeasuredSize& measuredSize) -> Gradient::RadialData {
            auto endRadius = resolveRadius(measuredSize.size.width, size.width());
            auto aspectRatio = endRadius / resolveRadius(measuredSize.size.height, size.height());

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        }
    );

    // computeStops() only uses maxExtent for repeating gradients.
    float maxExtent = radial.repeating == CSSGradientRepeat::Repeating ? distanceToFarthestCorner(data.point1, size) : 0;

    RadialGradientAdapter adapter { data };
    auto stops = computeStops(adapter, radial.stops, style, maxExtent, radial.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Deprecated Radial create.

Ref<Gradient> StyleGradientImage::createGradient(const DeprecatedRadialData& radial, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto firstPoint = computeEndPoint(radial.first, size);
    auto secondPoint = computeEndPoint(radial.second, size);

    auto firstRadius = radial.firstRadius;
    auto secondRadius = radial.secondRadius;
    auto aspectRatio = 1.0f;

    Gradient::RadialData data { firstPoint, secondPoint, firstRadius, secondRadius, aspectRatio };
    RadialGradientAdapter adapter { data };
    auto stops = computeStopsForDeprecatedVariants(adapter, radial.stops, style);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Conic create.

Ref<Gradient> StyleGradientImage::createGradient(const ConicData& conic, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto computeCenterPoint = [&](const StyleGradientPosition& position) -> FloatPoint {
        return computeEndPoint(position, size);
    };

    auto computeCenterPointOptional = [&](const std::optional<StyleGradientPosition>& position) -> FloatPoint {
        return position ? computeCenterPoint(*position) : FloatPoint { size.width() / 2, size.height() / 2 };
    };

    auto centerPoint = computeCenterPointOptional(conic.position);
    float angleRadians = conic.angle ? CSSPrimitiveValue::computeRadians(conic.angle->type, conic.angle->value) : 0;

    Gradient::ConicData data { centerPoint, angleRadians };
    ConicGradientAdapter adapter;
    auto stops = computeStops(adapter, conic.stops, style, 1, conic.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

} // namespace WebCore
