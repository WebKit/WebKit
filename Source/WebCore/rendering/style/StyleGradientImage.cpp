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

#include "CSSCalcValue.h"
#include "CSSToLengthConversionData.h"
#include "ColorInterpolation.h"
#include "ComputedStyleExtractor.h"
#include "GeneratedImage.h"
#include "GeometryUtilities.h"
#include "GradientImage.h"
#include "NodeRenderStyle.h"
#include "Pair.h"
#include "RenderElement.h"
#include "StyleBuilderState.h"

namespace WebCore {

static inline bool operator==(const StyleGradientImageStop& a, const StyleGradientImageStop& b)
{
    return a.color == b.color
        && compareCSSValuePtr(a.position, b.position);
}

static inline bool operator==(const StyleGradientImage::LinearData& a, const StyleGradientImage::LinearData& b)
{
    return a.repeating == b.repeating
        && a.data == b.data;
}

static inline bool operator==(const StyleGradientImage::PrefixedLinearData& a, const StyleGradientImage::PrefixedLinearData& b)
{
    return a.repeating == b.repeating
        && a.data == b.data;
}

static inline bool operator==(const StyleGradientImage::DeprecatedLinearData& a, const StyleGradientImage::DeprecatedLinearData& b)
{
    return a.data == b.data;
}

static inline bool operator==(const StyleGradientImage::RadialData& a, const StyleGradientImage::RadialData& b)
{
    return a.repeating == b.repeating
        && a.data == b.data;
}

static inline bool operator==(const StyleGradientImage::PrefixedRadialData& a, const StyleGradientImage::PrefixedRadialData& b)
{
    return a.repeating == b.repeating
        && a.data == b.data;
}

static inline bool operator==(const StyleGradientImage::DeprecatedRadialData& a, const StyleGradientImage::DeprecatedRadialData& b)
{
    return a.data == b.data;
}

static inline bool operator==(const StyleGradientImage::ConicData& a, const StyleGradientImage::ConicData& b)
{
    return a.repeating == b.repeating
        && a.data == b.data;
}

static bool stopsAreCacheable(const Vector<StyleGradientImage::Stop>& stops)
{
    for (auto& stop : stops) {
        if (stop.position && stop.position->isFontRelativeLength())
            return false;
        if (stop.color && stop.color->isCurrentColor())
            return false;
    }
    return true;
}

static Color resolveColorStopColor(const std::optional<StyleColor>& styleColor, const RenderStyle& style, bool hasColorFilter)
{
    if (!styleColor)
        return { };

    if (hasColorFilter)
        return style.colorWithColorFilter(*styleColor);
    return style.colorResolvingCurrentColor(*styleColor);
}

StyleGradientImage::StyleGradientImage(Data&& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, Vector<StyleGradientImageStop>&& stops)
    : StyleGeneratedImage { Type::GradientImage, StyleGradientImage::isFixedSize }
    , m_data { WTFMove(data) }
    , m_colorInterpolationMethod { colorInterpolationMethod }
    , m_stops { WTFMove(stops) }
    , m_knownCacheableBarringFilter { stopsAreCacheable(stops) }
{
}

StyleGradientImage::~StyleGradientImage() = default;

bool StyleGradientImage::operator==(const StyleImage& other) const
{
    return is<StyleGradientImage>(other) && equals(downcast<StyleGradientImage>(other));
}

bool StyleGradientImage::equals(const StyleGradientImage& other) const
{
    return m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data
        && m_stops == other.m_stops;
}

static inline RefPtr<CSSPrimitiveValue> computedStyleValueForColorStopColor(const std::optional<StyleColor>& color, const RenderStyle& style)
{
    if (!color)
        return nullptr;
    return ComputedStyleExtractor::currentColorOrValidColor(style, *color);
}

Ref<CSSValue> StyleGradientImage::computedStyleValue(const RenderStyle& style) const
{
    auto cssStopList = m_stops.map<CSSGradientColorStopList>([&](auto& stop) -> CSSGradientColorStop {
        return { computedStyleValueForColorStopColor(stop.color, style), stop.position };
    });

    return WTF::switchOn(m_data,
        [&] (const LinearData& data) -> Ref<CSSValue> {
            return CSSLinearGradientValue::create(data.data, data.repeating, m_colorInterpolationMethod, WTFMove(cssStopList));
        },
        [&] (const PrefixedLinearData& data) -> Ref<CSSValue> {
            return CSSPrefixedLinearGradientValue::create(data.data, data.repeating, m_colorInterpolationMethod, WTFMove(cssStopList));
        },
        [&] (const DeprecatedLinearData& data) -> Ref<CSSValue> {
            return CSSDeprecatedLinearGradientValue::create(data.data, m_colorInterpolationMethod, WTFMove(cssStopList));
        },
        [&] (const RadialData& data) -> Ref<CSSValue> {
            return CSSRadialGradientValue::create(data.data, data.repeating, m_colorInterpolationMethod, WTFMove(cssStopList));
        },
        [&] (const PrefixedRadialData& data) -> Ref<CSSValue> {
            return CSSPrefixedRadialGradientValue::create(data.data, data.repeating, m_colorInterpolationMethod, WTFMove(cssStopList));
        },
        [&] (const DeprecatedRadialData& data) -> Ref<CSSValue> {
            return CSSDeprecatedRadialGradientValue::create(data.data, m_colorInterpolationMethod, WTFMove(cssStopList) );
        },
        [&] (const ConicData& data) -> Ref<CSSValue> {
            return CSSConicGradientValue::create(data.data, data.repeating, m_colorInterpolationMethod, WTFMove(cssStopList));
        }
    );
}

bool StyleGradientImage::isPending() const
{
    return false;
}

void StyleGradientImage::load(CachedResourceLoader&, const ResourceLoaderOptions&)
{
}

RefPtr<Image> StyleGradientImage::image(const RenderElement* renderer, const FloatSize& size) const
{
    if (!renderer)
        return &Image::nullImage();

    if (size.isEmpty())
        return nullptr;

    bool cacheable = m_knownCacheableBarringFilter && !renderer->style().hasAppleColorFilter();
    if (cacheable) {
        if (!clients().contains(const_cast<RenderElement*>(renderer)))
            return nullptr;
        if (auto* result = const_cast<StyleGradientImage&>(*this).cachedImageForSize(size))
            return result;
    }

    auto gradient = WTF::switchOn(m_data,
        [&] (auto& data) -> Ref<Gradient> {
            return createGradient(data, *renderer, size);
        }
    );

    auto newImage = GradientImage::create(WTFMove(gradient), size);
    if (cacheable)
        const_cast<StyleGradientImage&>(*this).saveCachedImageForSize(size, newImage);
    return newImage;
}

bool StyleGradientImage::knownToBeOpaque(const RenderElement& renderer) const
{
    auto& style = renderer.style();
    bool hasColorFilter = style.hasAppleColorFilter();
    for (auto& stop : m_stops) {
        if (!resolveColorStopColor(stop.color, style, hasColorFilter).isOpaque())
            return false;
    }
    return true;
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

template<typename GradientAdapter>
GradientColorStops StyleGradientImage::computeStopsForDeprecatedVariants(GradientAdapter&, const CSSToLengthConversionData&, const RenderStyle& style) const
{
    bool hasColorFilter = style.hasAppleColorFilter();
    auto result = m_stops.map<GradientColorStops::StopVector>([&] (auto& stop) -> GradientColorStop {
        return {
            // FIXME: Use doubleValueDividingBy100IfPercentage? Or float version?
            stop.position->isPercentage() ? stop.position->floatValue(CSSUnitType::CSS_PERCENTAGE) / 100 : stop.position->floatValue(CSSUnitType::CSS_NUMBER),
            resolveColorStopColor(stop.color, style, hasColorFilter)
        };
    });

    std::stable_sort(result.begin(), result.end(), [] (const auto& a, const auto& b) {
        return a.offset < b.offset;
    });

    return GradientColorStops::Sorted { WTFMove(result) };
}

template<typename GradientAdapter>
GradientColorStops StyleGradientImage::computeStops(GradientAdapter& gradientAdapter, const CSSToLengthConversionData& conversionData, const RenderStyle& style, float maxLengthForRepeat, CSSGradientRepeat repeating) const
{
    bool hasColorFilter = style.hasAppleColorFilter();

    size_t numberOfStops = m_stops.size();
    Vector<ResolvedGradientStop> stops(numberOfStops);

    float gradientLength = gradientAdapter.gradientLength();

    for (size_t i = 0; i < numberOfStops; ++i) {
        auto& stop = m_stops[i];

        stops[i].color = resolveColorStopColor(stop.color, style, hasColorFilter);

        if (stop.position) {
            auto& positionValue = *stop.position;
            if (positionValue.isPercentage())
                stops[i].offset = positionValue.floatValue(CSSUnitType::CSS_PERCENTAGE) / 100;
            else if (positionValue.isLength() || positionValue.isViewportPercentageLength() || positionValue.isCalculatedPercentageWithLength()) {
                float length;
                if (positionValue.isLength())
                    length = positionValue.computeLength<float>(conversionData);
                else {
                    Ref<CalculationValue> calculationValue { positionValue.cssCalcValue()->createCalculationValue(conversionData) };
                    length = calculationValue->evaluate(gradientLength);
                }
                stops[i].offset = (gradientLength > 0) ? length / gradientLength : 0;
            } else if (positionValue.isAngle())
                stops[i].offset = positionValue.floatValue(CSSUnitType::CSS_DEG) / 360;
            else {
                ASSERT_NOT_REACHED();
                stops[i].offset = 0;
            }
        } else {
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
        // If the difference in the positions of the first and last color-stops is 0,
        // the gradient defines a solid-color image with the color of the last color-stop in the rule.
        float gradientRange = *stops.last().offset - *stops.first().offset;
        if (!gradientRange) {
            stops.first().offset = 0;
            stops.first().color = stops.last().color;
            stops.shrink(1);
            numberOfStops = 1;
        } else {
            float maxExtent = gradientAdapter.maxExtent(maxLengthForRepeat, gradientLength);

            size_t originalNumberOfStops = numberOfStops;
            size_t originalFirstStopIndex = 0;

            // Work backwards from the first, adding stops until we get one before 0.
            float firstOffset = *stops[0].offset;
            if (firstOffset > 0) {
                float currOffset = firstOffset;
                size_t srcStopOrdinal = originalNumberOfStops - 1;

                while (true) {
                    auto newStop = stops[originalFirstStopIndex + srcStopOrdinal];
                    newStop.offset = currOffset;
                    stops.insert(0, newStop);
                    ++originalFirstStopIndex;
                    if (currOffset < 0)
                        break;

                    if (srcStopOrdinal)
                        currOffset -= *stops[originalFirstStopIndex + srcStopOrdinal].offset - *stops[originalFirstStopIndex + srcStopOrdinal - 1].offset;
                    srcStopOrdinal = (srcStopOrdinal + originalNumberOfStops - 1) % originalNumberOfStops;
                }
            }

            // Work forwards from the end, adding stops until we get one after 1.
            float lastOffset = *stops[stops.size() - 1].offset;
            if (lastOffset < maxExtent) {
                float currOffset = lastOffset;
                size_t srcStopOrdinal = 0;

                while (true) {
                    size_t srcStopIndex = originalFirstStopIndex + srcStopOrdinal;
                    auto newStop = stops[srcStopIndex];
                    newStop.offset = currOffset;
                    stops.append(newStop);
                    if (currOffset > maxExtent)
                        break;
                    if (srcStopOrdinal < originalNumberOfStops - 1)
                        currOffset += *stops[srcStopIndex + 1].offset - *stops[srcStopIndex].offset;
                    srcStopOrdinal = (srcStopOrdinal + 1) % originalNumberOfStops;
                }
            }
        }
    }

    // If the gradient goes outside the 0-1 range, normalize it by moving the endpoints, and adjusting the stops.
    if (stops.size() > 1 && (*stops.first().offset < 0 || *stops.last().offset > 1))
        gradientAdapter.normalizeStopsAndEndpointsOutsideRange(stops, m_colorInterpolationMethod.method);
    
    return GradientColorStops::Sorted {
        stops.map<GradientColorStops::StopVector>([] (auto& stop) -> GradientColorStop {
            return { *stop.offset, stop.color };
        })
    };
}

static float positionFromValue(const CSSPrimitiveValue& initialValue, const CSSToLengthConversionData& conversionData, const FloatSize& size, bool isHorizontal)
{
    float origin = 0;
    float sign = 1;
    float edgeDistance = isHorizontal ? size.width() : size.height();

    const CSSPrimitiveValue* value;

    // In this case the center of the gradient is given relative to an edge in the
    // form of: [ top | bottom | right | left ] [ <percentage> | <length> ].
    if (initialValue.isPair()) {
        auto originID = initialValue.pairValue()->first()->valueID();
        if (originID == CSSValueRight || originID == CSSValueBottom) {
            // For right/bottom, the offset is relative to the far edge.
            origin = edgeDistance;
            sign = -1;
        }
        value = initialValue.pairValue()->second();
    } else {
        value = &initialValue;
    }

    if (value->isNumber())
        return origin + sign * value->floatValue() * conversionData.zoom();

    if (value->isPercentage())
        return origin + sign * value->floatValue() / 100 * edgeDistance;

    if (value->isCalculatedPercentageWithLength())
        return origin + sign * value->cssCalcValue()->createCalculationValue(conversionData)->evaluate(edgeDistance);

    switch (value->valueID()) {
    case CSSValueTop:
        ASSERT(!isHorizontal);
        return 0;
    case CSSValueLeft:
        ASSERT(isHorizontal);
        return 0;
    case CSSValueBottom:
        ASSERT(!isHorizontal);
        return edgeDistance;
    case CSSValueRight:
        ASSERT(isHorizontal);
        return edgeDistance;
    case CSSValueCenter:
        return origin + sign * .5f * edgeDistance;
    default:
        break;
    }

    return origin + sign * value->computeLength<float>(conversionData);
}

// Resolve points/radii to front end values.
static inline FloatPoint computeEndPoint(const CSSPrimitiveValue& horizontal, const CSSPrimitiveValue& vertical, const CSSToLengthConversionData& conversionData, const FloatSize& size)
{
    return { positionFromValue(horizontal, conversionData, size, true), positionFromValue(vertical, conversionData, size, false) };
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
    return endPointsFromAngle(angleDeg = 90 - angleDeg, size);
}

static float resolveRadius(CSSPrimitiveValue& radius, const CSSToLengthConversionData& conversionData, float widthOrHeight)
{
    if (radius.isNumber())
        return radius.floatValue() * conversionData.zoom();
    
    if (radius.isPercentage())
        return widthOrHeight * radius.floatValue() / 100;
    
    if (radius.isCalculatedPercentageWithLength())
        return radius.cssCalcValue()->createCalculationValue(conversionData)->evaluate(widthOrHeight);

    return radius.computeLength<float>(conversionData);
}

static float resolveRadius(CSSPrimitiveValue& radius, const CSSToLengthConversionData& conversionData)
{
    if (radius.isNumber())
        return radius.floatValue() * conversionData.zoom();
    return radius.computeLength<float>(conversionData);
}

struct DistanceToCorner {
    float distance;
    FloatPoint corner;
};

static DistanceToCorner distanceToClosestCorner(const FloatPoint& p, const FloatSize& size)
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

static DistanceToCorner distanceToFarthestCorner(const FloatPoint& p, const FloatSize& size)
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

Ref<Gradient> StyleGradientImage::createGradient(const LinearData& linear, const RenderElement& renderer, const FloatSize& size) const
{
    ASSERT(!size.isEmpty());

    const RenderStyle* rootStyle = nullptr;
    if (auto* documentElement = renderer.document().documentElement())
        rootStyle = documentElement->renderStyle();

    CSSToLengthConversionData conversionData(renderer.style(), rootStyle, renderer.parentStyle(), &renderer.view(), renderer.generatingElement());

    auto [firstPoint, secondPoint] = WTF::switchOn(linear.data.gradientLine,
        [&] (std::monostate) -> std::pair<FloatPoint, FloatPoint> {
            return { FloatPoint { 0, 0 }, FloatPoint { 0, size.height() } };
        },
        [&] (const CSSLinearGradientValue::Angle& angle) -> std::pair<FloatPoint, FloatPoint> {
            return endPointsFromAngle(angle.value->floatValue(CSSUnitType::CSS_DEG), size);
        },
        [&] (CSSLinearGradientValue::Horizontal horizontal) -> std::pair<FloatPoint, FloatPoint> {
            switch (horizontal) {
            case CSSLinearGradientValue::Horizontal::Left:
                return { FloatPoint { size.width(), 0 }, FloatPoint { 0, 0 } };
            case CSSLinearGradientValue::Horizontal::Right:
                return { FloatPoint { 0, 0 }, FloatPoint { size.width(), 0 } };
            }
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&] (CSSLinearGradientValue::Vertical vertical) -> std::pair<FloatPoint, FloatPoint> {
            switch (vertical) {
            case CSSLinearGradientValue::Vertical::Top:
                return { FloatPoint { 0, size.height() }, FloatPoint { 0, 0 } };
            case CSSLinearGradientValue::Vertical::Bottom:
                return { FloatPoint { 0, 0 }, FloatPoint { 0, size.height() } };
            }
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&] (const std::pair<CSSLinearGradientValue::Horizontal, CSSLinearGradientValue::Vertical>& pair) -> std::pair<FloatPoint, FloatPoint> {
            float rise = size.width();
            float run = size.height();
            if (pair.first == CSSLinearGradientValue::Horizontal::Left)
                run *= -1;
            if (pair.second == CSSLinearGradientValue::Vertical::Bottom)
                rise *= -1;
            // Compute angle, and flip it back to "bearing angle" degrees.
            float angle = 90 - rad2deg(atan2(rise, run));
            return endPointsFromAngle(angle, size);
        }
    );

    Gradient::LinearData data { firstPoint, secondPoint };
    LinearGradientAdapter adapter { data };
    auto stops = computeStops(adapter, conversionData, renderer.style(), 1, linear.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Prefixed Linear create.

Ref<Gradient> StyleGradientImage::createGradient(const PrefixedLinearData& linear, const RenderElement& renderer, const FloatSize& size) const
{
    ASSERT(!size.isEmpty());

    const RenderStyle* rootStyle = nullptr;
    if (auto* documentElement = renderer.document().documentElement())
        rootStyle = documentElement->renderStyle();

    CSSToLengthConversionData conversionData(renderer.style(), rootStyle, renderer.parentStyle(), &renderer.view(), renderer.generatingElement());

    auto [firstPoint, secondPoint] = WTF::switchOn(linear.data.gradientLine,
        [&] (std::monostate) -> std::pair<FloatPoint, FloatPoint> {
            return { FloatPoint { 0, 0 }, FloatPoint { 0, size.height() } };
        },
        [&] (const CSSPrefixedLinearGradientValue::Angle& angle) -> std::pair<FloatPoint, FloatPoint> {
            return endPointsFromAngleForPrefixedVariants(angle.value->floatValue(CSSUnitType::CSS_DEG), size);
        },
        [&] (CSSPrefixedLinearGradientValue::Horizontal horizontal) -> std::pair<FloatPoint, FloatPoint> {
            switch (horizontal) {
            case CSSPrefixedLinearGradientValue::Horizontal::Left:
                return { FloatPoint { 0, 0 }, FloatPoint { size.width(), 0 } };
            case CSSPrefixedLinearGradientValue::Horizontal::Right:
                return { FloatPoint { size.width(), 0 }, FloatPoint { 0, 0 } };
            }
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&] (CSSPrefixedLinearGradientValue::Vertical vertical) -> std::pair<FloatPoint, FloatPoint> {
            switch (vertical) {
            case CSSPrefixedLinearGradientValue::Vertical::Top:
                return { FloatPoint { 0, 0 }, FloatPoint { 0, size.height() } };
            case CSSPrefixedLinearGradientValue::Vertical::Bottom:
                return { FloatPoint { 0, size.height() }, FloatPoint { 0, 0 } };
            }
            RELEASE_ASSERT_NOT_REACHED();
        },
        [&] (const std::pair<CSSPrefixedLinearGradientValue::Horizontal, CSSPrefixedLinearGradientValue::Vertical>& pair) -> std::pair<FloatPoint, FloatPoint> {
            switch (pair.first) {
            case CSSPrefixedLinearGradientValue::Horizontal::Left:
                switch (pair.second) {
                case CSSPrefixedLinearGradientValue::Vertical::Top:
                    return { FloatPoint { 0, 0 }, FloatPoint { size.width(), size.height() } };
                case CSSPrefixedLinearGradientValue::Vertical::Bottom:
                    return { FloatPoint { 0, size.height() }, FloatPoint { size.width(), 0 } };
                }
                RELEASE_ASSERT_NOT_REACHED();

            case CSSPrefixedLinearGradientValue::Horizontal::Right:
                switch (pair.second) {
                case CSSPrefixedLinearGradientValue::Vertical::Top:
                    return { FloatPoint { size.width(), 0 }, FloatPoint { 0, size.height() } };
                case CSSPrefixedLinearGradientValue::Vertical::Bottom:
                    return { FloatPoint { size.width(), size.height() }, FloatPoint { 0, 0 } };
                }
                RELEASE_ASSERT_NOT_REACHED();
            }
            RELEASE_ASSERT_NOT_REACHED();
        }
    );

    Gradient::LinearData data { firstPoint, secondPoint };
    LinearGradientAdapter adapter { data };
    auto stops = computeStops(adapter, conversionData, renderer.style(), 1, linear.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Deprecated Linear create.

Ref<Gradient> StyleGradientImage::createGradient(const DeprecatedLinearData& linear, const RenderElement& renderer, const FloatSize& size) const
{
    ASSERT(!size.isEmpty());

    const RenderStyle* rootStyle = nullptr;
    if (auto* documentElement = renderer.document().documentElement())
        rootStyle = documentElement->renderStyle();

    CSSToLengthConversionData conversionData(renderer.style(), rootStyle, renderer.parentStyle(), &renderer.view(), renderer.generatingElement());

    auto firstPoint = computeEndPoint(linear.data.firstX, linear.data.firstY, conversionData, size);
    auto secondPoint = computeEndPoint(linear.data.secondX, linear.data.secondY, conversionData, size);

    Gradient::LinearData data { firstPoint, secondPoint };
    LinearGradientAdapter adapter { data };
    auto stops = computeStopsForDeprecatedVariants(adapter, conversionData, renderer.style());

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Radial create.

Ref<Gradient> StyleGradientImage::createGradient(const RadialData& radial, const RenderElement& renderer, const FloatSize& size) const
{
    ASSERT(!size.isEmpty());

    const RenderStyle* rootStyle = nullptr;
    if (auto* documentElement = renderer.document().documentElement())
        rootStyle = documentElement->renderStyle();

    CSSToLengthConversionData conversionData(renderer.style(), rootStyle, renderer.parentStyle(), &renderer.view(), renderer.generatingElement());

    auto computeCenterPoint = [&](const CSSGradientPosition& position) -> FloatPoint {
        return computeEndPoint(position.first, position.second, conversionData, size);
    };
    auto computeCenterPointOptional = [&](const std::optional<CSSGradientPosition>& position) -> FloatPoint {
        return position ? computeCenterPoint(*position) : FloatPoint { size.width() / 2, size.height() / 2 };
    };

    auto computeCircleRadius = [&](CSSRadialGradientValue::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (extent) {
        case CSSRadialGradientValue::ExtentKeyword::ClosestSide:
            return { std::min({ centerPoint.x(), size.width() - centerPoint.x(), centerPoint.y(), size.height() - centerPoint.y() }), 1 };

        case CSSRadialGradientValue::ExtentKeyword::FarthestSide:
            return { std::max({ centerPoint.x(), size.width() - centerPoint.x(), centerPoint.y(), size.height() - centerPoint.y() }), 1 };

        case CSSRadialGradientValue::ExtentKeyword::ClosestCorner:
            return { distanceToClosestCorner(centerPoint, size).distance, 1 };

        case CSSRadialGradientValue::ExtentKeyword::FarthestCorner:
            return { distanceToFarthestCorner(centerPoint, size).distance, 1 };
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto computeEllipseRadii = [&](CSSRadialGradientValue::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (extent) {
        case CSSRadialGradientValue::ExtentKeyword::ClosestSide: {
            float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
            return { xDist, xDist / yDist };
        }

        case CSSRadialGradientValue::ExtentKeyword::FarthestSide: {
            float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
            return { xDist, xDist / yDist };
        }

        case CSSRadialGradientValue::ExtentKeyword::ClosestCorner: {
            auto [distance, corner] = distanceToClosestCorner(centerPoint, size);
            // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
            // that it would if closest-side or farthest-side were specified, as appropriate.
            float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
            return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
        }

        case CSSRadialGradientValue::ExtentKeyword::FarthestCorner: {
            auto [distance, corner] = distanceToFarthestCorner(centerPoint, size);
            // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
            // that it would if closest-side or farthest-side were specified, as appropriate.
            float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
            return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto computeRadii = [&] (CSSRadialGradientValue::ShapeKeyword shape, CSSRadialGradientValue::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (shape) {
        case CSSRadialGradientValue::ShapeKeyword::Circle:
            return computeCircleRadius(extent, centerPoint);
        case CSSRadialGradientValue::ShapeKeyword::Ellipse:
            return computeEllipseRadii(extent, centerPoint);
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto data = WTF::switchOn(radial.data.gradientBox,
        [&] (std::monostate) -> Gradient::RadialData {
            auto centerPoint = FloatPoint { size.width() / 2, size.height() / 2 };
            auto [endRadius, aspectRatio] = computeRadii(CSSRadialGradientValue::ShapeKeyword::Ellipse, CSSRadialGradientValue::ExtentKeyword::FarthestCorner, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&] (const CSSRadialGradientValue::Shape& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto [endRadius, aspectRatio] = computeRadii(data.shape, CSSRadialGradientValue::ExtentKeyword::FarthestCorner, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&] (const CSSRadialGradientValue::Extent& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto [endRadius, aspectRatio] = computeRadii(CSSRadialGradientValue::ShapeKeyword::Ellipse, data.extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&] (const CSSRadialGradientValue::Length& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto endRadius = resolveRadius(data.length, conversionData, size.width());

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, 1 };
        },
        [&] (const CSSRadialGradientValue::CircleOfLength& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto endRadius = resolveRadius(data.length, conversionData, size.width());

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, 1 };
        },
        [&] (const CSSRadialGradientValue::CircleOfExtent& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto [endRadius, aspectRatio] = computeRadii(CSSRadialGradientValue::ShapeKeyword::Circle, data.extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, 1 };
        },
        [&] (const CSSRadialGradientValue::Size& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto endRadius = resolveRadius(data.size.first, conversionData, size.width());
            auto aspectRatio = endRadius / resolveRadius(data.size.second, conversionData, size.height());

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&] (const CSSRadialGradientValue::EllipseOfSize& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto endRadius = resolveRadius(data.size.first, conversionData, size.width());
            auto aspectRatio = endRadius / resolveRadius(data.size.second, conversionData, size.height());

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&] (const CSSRadialGradientValue::EllipseOfExtent& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPointOptional(data.position);
            auto [endRadius, aspectRatio] = computeRadii(CSSRadialGradientValue::ShapeKeyword::Ellipse, data.extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, 1 };
        },
        [&] (const CSSGradientPosition& data) -> Gradient::RadialData {
            auto centerPoint = computeCenterPoint(data);
            auto [radius, aspectRatio] = computeRadii(CSSRadialGradientValue::ShapeKeyword::Ellipse, CSSRadialGradientValue::ExtentKeyword::FarthestCorner, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, radius, aspectRatio };
        }
    );

    // computeStops() only uses maxExtent for repeating gradients.
    float maxExtent = radial.repeating == CSSGradientRepeat::Repeating ? distanceToFarthestCorner(data.point1, size).distance : 0;

    RadialGradientAdapter adapter { data };
    auto stops = computeStops(adapter, conversionData, renderer.style(), maxExtent, radial.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Prefixed Radial create.

Ref<Gradient> StyleGradientImage::createGradient(const PrefixedRadialData& radial, const RenderElement& renderer, const FloatSize& size) const
{
    ASSERT(!size.isEmpty());

    const RenderStyle* rootStyle = nullptr;
    if (auto* documentElement = renderer.document().documentElement())
        rootStyle = documentElement->renderStyle();

    CSSToLengthConversionData conversionData(renderer.style(), rootStyle, renderer.parentStyle(), &renderer.view(), renderer.generatingElement());

    auto computeCircleRadius = [&](CSSPrefixedRadialGradientValue::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (extent) {
        case CSSPrefixedRadialGradientValue::ExtentKeyword::Contain:
        case CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestSide:
            return { std::min({ centerPoint.x(), size.width() - centerPoint.x(), centerPoint.y(), size.height() - centerPoint.y() }), 1 };

        case CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestSide:
            return { std::max({ centerPoint.x(), size.width() - centerPoint.x(), centerPoint.y(), size.height() - centerPoint.y() }), 1 };

        case CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestCorner:
            return { distanceToClosestCorner(centerPoint, size).distance, 1 };

        case CSSPrefixedRadialGradientValue::ExtentKeyword::Cover:
        case CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestCorner:
            return { distanceToFarthestCorner(centerPoint, size).distance, 1 };
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto computeEllipseRadii = [&](CSSPrefixedRadialGradientValue::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (extent) {
        case CSSPrefixedRadialGradientValue::ExtentKeyword::Contain:
        case CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestSide: {
            float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
            return { xDist, xDist / yDist };
        }

        case CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestSide: {
            float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
            return { xDist, xDist / yDist };
        }

        case CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestCorner: {
            auto [distance, corner] = distanceToClosestCorner(centerPoint, size);
            // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
            // that it would if closest-side or farthest-side were specified, as appropriate.
            float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
            return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
        }

        case CSSPrefixedRadialGradientValue::ExtentKeyword::Cover:
        case CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestCorner: {
            auto [distance, corner] = distanceToFarthestCorner(centerPoint, size);
            // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
            // that it would if closest-side or farthest-side were specified, as appropriate.
            float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
            float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
            return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
        }
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto computeRadii = [&] (CSSPrefixedRadialGradientValue::ShapeKeyword shape, CSSPrefixedRadialGradientValue::ExtentKeyword extent, FloatPoint centerPoint) -> std::pair<float, float> {
        switch (shape) {
        case CSSPrefixedRadialGradientValue::ShapeKeyword::Circle:
            return computeCircleRadius(extent, centerPoint);
        case CSSPrefixedRadialGradientValue::ShapeKeyword::Ellipse:
            return computeEllipseRadii(extent, centerPoint);
        }
        RELEASE_ASSERT_NOT_REACHED();
    };

    auto centerPoint = radial.data.position ? computeEndPoint(radial.data.position->first, radial.data.position->second, conversionData, size) : FloatPoint { size.width() / 2, size.height() / 2 };

    auto data = WTF::switchOn(radial.data.gradientBox,
        [&] (std::monostate) -> Gradient::RadialData {
            auto [endRadius, aspectRatio] = computeRadii(CSSPrefixedRadialGradientValue::ShapeKeyword::Ellipse, CSSPrefixedRadialGradientValue::ExtentKeyword::Cover, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&] (const CSSPrefixedRadialGradientValue::ShapeKeyword& shape) -> Gradient::RadialData {
            auto [endRadius, aspectRatio] = computeRadii(shape, CSSPrefixedRadialGradientValue::ExtentKeyword::Cover, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&] (const CSSPrefixedRadialGradientValue::ExtentKeyword& extent) -> Gradient::RadialData {
            auto [endRadius, aspectRatio] = computeRadii(CSSPrefixedRadialGradientValue::ShapeKeyword::Ellipse, extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&] (const CSSPrefixedRadialGradientValue::ShapeAndExtent& shapeAndExtent) -> Gradient::RadialData {
            auto [endRadius, aspectRatio] = computeRadii(shapeAndExtent.shape, shapeAndExtent.extent, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&] (const CSSPrefixedRadialGradientValue::MeasuredSize& measuredSize) -> Gradient::RadialData {
            auto endRadius = resolveRadius(measuredSize.size.first, conversionData, size.width());
            auto aspectRatio = endRadius / resolveRadius(measuredSize.size.second, conversionData, size.height());
        
            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        }
    );

    // computeStops() only uses maxExtent for repeating gradients.
    float maxExtent = radial.repeating == CSSGradientRepeat::Repeating ? distanceToFarthestCorner(data.point1, size).distance : 0;

    RadialGradientAdapter adapter { data };
    auto stops = computeStops(adapter, conversionData, renderer.style(), maxExtent, radial.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Deprecated Radial create.

Ref<Gradient> StyleGradientImage::createGradient(const DeprecatedRadialData& radial, const RenderElement& renderer, const FloatSize& size) const
{
    ASSERT(!size.isEmpty());

    const RenderStyle* rootStyle = nullptr;
    if (auto* documentElement = renderer.document().documentElement())
        rootStyle = documentElement->renderStyle();

    CSSToLengthConversionData conversionData(renderer.style(), rootStyle, renderer.parentStyle(), &renderer.view(), renderer.generatingElement());

    auto firstPoint = computeEndPoint(radial.data.firstX, radial.data.firstY, conversionData, size);
    auto secondPoint = computeEndPoint(radial.data.secondX, radial.data.secondY, conversionData, size);

    auto firstRadius = resolveRadius(radial.data.firstRadius, conversionData);
    auto secondRadius = resolveRadius(radial.data.secondRadius, conversionData);
    auto aspectRatio = 1.0f;

    Gradient::RadialData data { firstPoint, secondPoint, firstRadius, secondRadius, aspectRatio };
    RadialGradientAdapter adapter { data };
    auto stops = computeStopsForDeprecatedVariants(adapter, conversionData, renderer.style());

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Conic create.

Ref<Gradient> StyleGradientImage::createGradient(const ConicData& conic, const RenderElement& renderer, const FloatSize& size) const
{
    ASSERT(!size.isEmpty());

    const RenderStyle* rootStyle = nullptr;
    if (auto* documentElement = renderer.document().documentElement())
        rootStyle = documentElement->renderStyle();

    CSSToLengthConversionData conversionData(renderer.style(), rootStyle, renderer.parentStyle(), &renderer.view(), renderer.generatingElement());

    auto centerPoint = conic.data.position ? computeEndPoint(conic.data.position->first, conic.data.position->second, conversionData, size) : FloatPoint { size.width() / 2, size.height() / 2 };
    auto angleRadians = conic.data.angle.value ? conic.data.angle.value->floatValue(CSSUnitType::CSS_RAD) : 0;

    Gradient::ConicData data { centerPoint, angleRadians };
    ConicGradientAdapter adapter;
    auto stops = computeStops(adapter, conversionData, renderer.style(), 1, conic.repeating);

    return Gradient::create(WTFMove(data), m_colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

} // namespace WebCore
