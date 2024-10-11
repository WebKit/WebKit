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

#include "CSSGradientValue.h"
#include "ColorInterpolation.h"
#include "GeneratedImage.h"
#include "GeometryUtilities.h"
#include "GradientImage.h"
#include "NodeRenderStyle.h"
#include "RenderElement.h"
#include "RenderStyleInlines.h"

namespace WebCore {

template<typename Stops> static bool stopsAreCacheable(const Stops& stops)
{
    return std::ranges::none_of(stops, [](auto& stop) { return stop.color && stop.color->containsCurrentColor(); });
}

static bool stopsAreCacheable(const Style::Gradient& gradient)
{
    return WTF::switchOn(gradient, [](auto& gradient) { return stopsAreCacheable(gradient.stops); });
}

static Color resolveColorStopColor(const std::optional<StyleColor>& styleColor, const RenderStyle& style, bool hasColorFilter)
{
    if (!styleColor)
        return { };

    if (hasColorFilter)
        return style.colorWithColorFilter(*styleColor);
    return style.colorResolvingCurrentColor(*styleColor);
}

static std::optional<float> resolveColorStopPosition(const Style::GradientLinearColorStop::Position& position, float gradientLength)
{
    if (!position)
        return std::nullopt;

    return position->value.switchOn(
        [&](Style::Length length) -> std::optional<float> {
            if (gradientLength <= 0)
                return 0;
            return length.value / gradientLength;
        },
        [&](Style::Percentage percentage) -> std::optional<float> {
            return percentage.value / 100.0;
        },
        [&](const CalculationValue& calc) -> std::optional<float> {
            if (gradientLength <= 0)
                return 0;
            return calc.evaluate(gradientLength) / gradientLength;
        }
    );
}

static std::optional<float> resolveColorStopPosition(const Style::GradientAngularColorStop::Position& position, float)
{
    if (!position)
        return std::nullopt;

    return position->value.switchOn(
        [](Style::Angle angle) -> std::optional<float> {
            return angle.value / 360.0;
        },
        [](Style::Percentage percentage) -> std::optional<float> {
            return percentage.value / 100.0;
        },
        [](const CalculationValue& calc) -> std::optional<float> {
            return calc.evaluate(100) / 100.0;
        }
    );
}

static float resolveColorStopPosition(const Style::GradientDeprecatedColorStop::Position& position)
{
    return static_cast<float>(position.value);
}

StyleGradientImage::StyleGradientImage(Style::Gradient&& gradient)
    : StyleGeneratedImage { Type::GradientImage, StyleGradientImage::isFixedSize }
    , m_gradient { WTFMove(gradient) }
    , m_knownCacheableBarringFilter { stopsAreCacheable(m_gradient) }
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
    return m_gradient == other.m_gradient;
}

Ref<CSSValue> StyleGradientImage::computedStyleValue(const RenderStyle& style) const
{
    return WTF::switchOn(m_gradient,
        [&](const Style::LinearGradient& gradient) -> Ref<CSSValue> { return CSSLinearGradientValue::create(Style::toCSS(gradient, style)); },
        [&](const Style::PrefixedLinearGradient& gradient) -> Ref<CSSValue> { return CSSPrefixedLinearGradientValue::create(Style::toCSS(gradient, style)); },
        [&](const Style::DeprecatedLinearGradient& gradient) -> Ref<CSSValue> { return CSSDeprecatedLinearGradientValue::create(Style::toCSS(gradient, style)); },
        [&](const Style::RadialGradient& gradient) -> Ref<CSSValue> { return CSSRadialGradientValue::create(Style::toCSS(gradient, style)); },
        [&](const Style::PrefixedRadialGradient& gradient) -> Ref<CSSValue> { return CSSPrefixedRadialGradientValue::create(Style::toCSS(gradient, style)); },
        [&](const Style::DeprecatedRadialGradient& gradient) -> Ref<CSSValue> { return CSSDeprecatedRadialGradientValue::create(Style::toCSS(gradient, style)); },
        [&](const Style::ConicGradient& gradient) -> Ref<CSSValue> { return CSSConicGradientValue::create(Style::toCSS(gradient, style)); }
    );
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

    auto gradient = WTF::switchOn(m_gradient, [&](auto& gradient) -> Ref<Gradient> { return createGradient(gradient, size, style); });

    auto newImage = GradientImage::create(WTFMove(gradient), size);
    if (cacheable)
        const_cast<StyleGradientImage&>(*this).saveCachedImageForSize(size, newImage);
    return newImage;
}

template<typename Gradient> static bool knownToBeOpaque(const RenderElement& renderer, const Gradient& gradient)
{
    auto& style = renderer.style();
    bool hasColorFilter = style.hasAppleColorFilter();
    return std::ranges::all_of(gradient.stops, [&](auto& stop) { return resolveColorStopColor(stop.color, style, hasColorFilter).isOpaque(); });
}

bool StyleGradientImage::knownToBeOpaque(const RenderElement& renderer) const
{
    return WTF::switchOn(m_gradient, [&](auto& gradient) { return WebCore::knownToBeOpaque(renderer, gradient); } );
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
    WebCore::Gradient::LinearData& m_data;
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
    WebCore::Gradient::RadialData& m_data;
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

} // namespace (anonymous)

template<typename GradientAdapter, typename StyleGradient> GradientColorStops StyleGradientImage::computeStopsForDeprecatedVariants(GradientAdapter&, const StyleGradient& styleGradient, const RenderStyle& style) const
{
    bool hasColorFilter = style.hasAppleColorFilter();
    auto result = styleGradient.stops.value.template map<GradientColorStops::StopVector>([&](auto& stop) -> GradientColorStop {
        return {
            resolveColorStopPosition(stop.position),
            resolveColorStopColor(stop.color, style, hasColorFilter)
        };
    });

    std::ranges::stable_sort(result, [](const auto& a, const auto& b) {
        return a.offset < b.offset;
    });

    return GradientColorStops::Sorted { WTFMove(result) };
}

template<typename GradientAdapter, typename StyleGradient> GradientColorStops StyleGradientImage::computeStops(GradientAdapter& gradientAdapter, const StyleGradient& styleGradient, const RenderStyle& style, float maxLengthForRepeat) const
{
    bool hasColorFilter = style.hasAppleColorFilter();

    size_t numberOfStops = styleGradient.stops.size();
    Vector<ResolvedGradientStop> stops(numberOfStops);

    float gradientLength = gradientAdapter.gradientLength();

    for (size_t i = 0; i < numberOfStops; ++i) {
        auto& stop = styleGradient.stops[i];

        stops[i].color = resolveColorStopColor(stop.color, style, hasColorFilter);

        auto offset = resolveColorStopPosition(stop.position, gradientLength);
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
    // since it becomes small which increases the differentiation of luminance which hides the color stops.
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
            for (size_t y = 0; y < 6; ++y)
                newStops[y].offset = offset1 + (offset - offset1) * (7 + y) / 13;

            newStops[6].offset = offset;
            newStops[7].offset = offset + (offset2 - offset) / 3;
            newStops[8].offset = offset + (offset2 - offset) * 2 / 3;
        } else {
            newStops[0].offset = offset1 + (offset - offset1) / 3;
            newStops[1].offset = offset1 + (offset - offset1) * 2 / 3;
            newStops[2].offset = offset;

            for (size_t y = 1; y < 7; ++y)
                newStops[y + 2].offset = offset + (offset2 - offset) * y / 13;
        }
        // calculate colors
        for (size_t y = 0; y < 9; ++y) {
            float relativeOffset = (*newStops[y].offset - offset1) / (offset2 - offset1);
            float multiplier = std::pow(relativeOffset, std::log(.5f) / std::log(midpoint));
            newStops[y].color = interpolateColors(styleGradient.colorInterpolationMethod.method, color1, 1.0f - multiplier, color2, multiplier);
        }

        stops.remove(x);
        stops.insert(x, newStops, 9);
        x += 9;
    }

    numberOfStops = stops.size();

    // If the gradient is repeating, repeat the color stops.
    // We can't just push this logic down into the platform-specific Gradient code,
    // because we have to know the extent of the gradient, and possible move the end points.
    if (styleGradient.repeating == Style::GradientRepeat::Repeating && numberOfStops > 1) {
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
        gradientAdapter.normalizeStopsAndEndpointsOutsideRange(stops, styleGradient.colorInterpolationMethod.method);

    return GradientColorStops::Sorted {
        stops.template map<GradientColorStops::StopVector>([](auto& stop) -> GradientColorStop {
            return { *stop.offset, stop.color };
        })
    };
}

static inline float positionFromValue(const Style::LengthPercentage& coordinate, float widthOrHeight)
{
    return Style::evaluate(coordinate, widthOrHeight);
}

static inline float positionFromValue(const Style::PercentageOrNumber& coordinate, float widthOrHeight)
{
    return WTF::switchOn(coordinate,
        [&](Style::Number number) -> float { return number.value; },
        [&](Style::Percentage percentage) -> float { return percentage.value / 100.0f * widthOrHeight; }
    );
}

template<typename Position> static inline FloatPoint computeEndPoint(const Position& value, const FloatSize& size)
{
    return {
        positionFromValue(get<0>(value), size.width()),
        positionFromValue(get<1>(value), size.height())
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

static float resolveRadius(const Style::LengthPercentage& radius, float widthOrHeight)
{
    return Style::evaluate(radius, widthOrHeight);
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

Ref<WebCore::Gradient> StyleGradientImage::createGradient(const Style::LinearGradient& linear, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto [point0, point1] = WTF::switchOn(linear.gradientLine,
        [&](const Style::Angle& angle) -> std::pair<FloatPoint, FloatPoint> {
            return endPointsFromAngle(angle.value, size);
        },
        [&](const Style::Horizontal& horizontal) -> std::pair<FloatPoint, FloatPoint> {
            return WTF::switchOn(horizontal,
                [&](Style::Left) -> std::pair<FloatPoint, FloatPoint> {
                    return { { size.width(), 0 }, { 0, 0 } };
                },
                [&](Style::Right) -> std::pair<FloatPoint, FloatPoint> {
                    return { { 0, 0 }, { size.width(), 0 } };
                }
            );
        },
        [&](const Style::Vertical& vertical) -> std::pair<FloatPoint, FloatPoint> {
            return WTF::switchOn(vertical,
                [&](Style::Top) -> std::pair<FloatPoint, FloatPoint> {
                    return { { 0, size.height() }, { 0, 0 } };
                },
                [&](Style::Bottom) -> std::pair<FloatPoint, FloatPoint> {
                    return { { 0, 0 }, { 0, size.height() } };
                }
            );
        },
        [&](const Style::SpaceSeparatedTuple<Style::Horizontal, Style::Vertical>& pair) -> std::pair<FloatPoint, FloatPoint> {
            float rise = size.width();
            float run = size.height();
            if (std::holds_alternative<Style::Left>(get<0>(pair)))
                run *= -1;
            if (std::holds_alternative<Style::Bottom>(get<1>(pair)))
                rise *= -1;
            // Compute angle, and flip it back to "bearing angle" degrees.
            float angle = 90 - rad2deg(atan2(rise, run));
            return endPointsFromAngle(angle, size);
        }
    );

    WebCore::Gradient::LinearData data { point0, point1 };
    LinearGradientAdapter adapter { data };
    auto stops = computeStops(adapter, linear, style, 1);

    return WebCore::Gradient::create(WTFMove(data), linear.colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Prefixed Linear create.

Ref<WebCore::Gradient> StyleGradientImage::createGradient(const Style::PrefixedLinearGradient& linear, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto [point0, point1] = WTF::switchOn(linear.gradientLine,
        [&](const Style::Angle& angle) -> std::pair<FloatPoint, FloatPoint> {
            return endPointsFromAngleForPrefixedVariants(angle.value, size);
        },
        [&](const Style::Horizontal& horizontal) -> std::pair<FloatPoint, FloatPoint> {
            return WTF::switchOn(horizontal,
                [&](Style::Left) -> std::pair<FloatPoint, FloatPoint> {
                    return { { 0, 0 }, { size.width(), 0 } };
                },
                [&](Style::Right) -> std::pair<FloatPoint, FloatPoint> {
                    return { { size.width(), 0 }, { 0, 0 } };
                }
            );
        },
        [&](const Style::Vertical vertical) -> std::pair<FloatPoint, FloatPoint> {
            return WTF::switchOn(vertical,
                [&](Style::Top) -> std::pair<FloatPoint, FloatPoint> {
                    return { { 0, 0 }, { 0, size.height() } };
                },
                [&](Style::Bottom) -> std::pair<FloatPoint, FloatPoint> {
                    return { { 0, size.height() }, { 0, 0 } };
                }
            );
        },
        [&](const Style::SpaceSeparatedTuple<Style::Horizontal, Style::Vertical>& pair) -> std::pair<FloatPoint, FloatPoint> {
            return std::visit(WTF::makeVisitor(
                [&](Style::Left, Style::Top) -> std::pair<FloatPoint, FloatPoint> {
                    return { { 0, 0 }, { size.width(), size.height() } };
                },
                [&](Style::Left, Style::Bottom) -> std::pair<FloatPoint, FloatPoint> {
                    return { { 0, size.height() }, { size.width(), 0 } };
                },
                [&](Style::Right, Style::Top) -> std::pair<FloatPoint, FloatPoint> {
                    return { { size.width(), 0 }, { 0, size.height() } };
                },
                [&](Style::Right, Style::Bottom) -> std::pair<FloatPoint, FloatPoint> {
                    return { { size.width(), size.height() }, { 0, 0 } };
                }
            ), get<0>(pair), get<1>(pair));
        }
    );

    WebCore::Gradient::LinearData data { point0, point1 };
    LinearGradientAdapter adapter { data };
    auto stops = computeStops(adapter, linear, style, 1);

    return WebCore::Gradient::create(WTFMove(data), linear.colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Deprecated Linear create.

Ref<WebCore::Gradient> StyleGradientImage::createGradient(const Style::DeprecatedLinearGradient& linear, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto point0 = computeEndPoint(get<0>(linear.gradientLine), size);
    auto point1 = computeEndPoint(get<1>(linear.gradientLine), size);

    WebCore::Gradient::LinearData data { point0, point1 };
    LinearGradientAdapter adapter { data };
    auto stops = computeStopsForDeprecatedVariants(adapter, linear, style);

    return WebCore::Gradient::create(WTFMove(data), linear.colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Radial create.

Ref<WebCore::Gradient> StyleGradientImage::createGradient(const Style::RadialGradient& radial, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto computeCenterPoint = [&](const std::optional<Style::Position>& position) -> FloatPoint {
        return position ? computeEndPoint(*position, size) : FloatPoint { size.width() / 2, size.height() / 2 };
    };

    auto computeCircleRadius = [&](const std::variant<Style::RadialGradient::Circle::Length, Style::RadialGradient::Extent>& circleLengthOrExtent, FloatPoint centerPoint) -> std::pair<float, float> {
        return WTF::switchOn(circleLengthOrExtent,
            [&](const Style::RadialGradient::Circle::Length& circleLength) -> std::pair<float, float> {
                return { circleLength.value, 1 };
            },
            [&](const Style::RadialGradient::Extent& extent) -> std::pair<float, float> {
                return WTF::switchOn(extent,
                    [&](Style::ClosestSide) -> std::pair<float, float> {
                        return { distanceToClosestSide(centerPoint, size), 1 };
                    },
                    [&](Style::FarthestSide) -> std::pair<float, float> {
                        return { distanceToFarthestSide(centerPoint, size), 1 };
                    },
                    [&](Style::ClosestCorner) -> std::pair<float, float> {
                        return { distanceToClosestCorner(centerPoint, size), 1 };
                    },
                    [&](Style::FarthestCorner) -> std::pair<float, float> {
                        return { distanceToFarthestCorner(centerPoint, size), 1 };
                    }
                );
            }
        );
    };

    auto computeEllipseRadii = [&](const std::variant<Style::RadialGradient::Ellipse::Size, Style::RadialGradient::Extent>& ellipseSizeOrExtent, FloatPoint centerPoint) -> std::pair<float, float> {
        return WTF::switchOn(ellipseSizeOrExtent,
            [&](const Style::RadialGradient::Ellipse::Size& ellipseSize) -> std::pair<float, float> {
                auto xDist = resolveRadius(get<0>(ellipseSize), size.width());
                auto yDist = resolveRadius(get<1>(ellipseSize), size.height());
                return { xDist, xDist / yDist };
            },
            [&](const Style::RadialGradient::Extent& extent) -> std::pair<float, float> {
                return WTF::switchOn(extent,
                    [&](Style::ClosestSide) -> std::pair<float, float> {
                        float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
                        return { xDist, xDist / yDist };
                    },
                    [&](Style::FarthestSide) -> std::pair<float, float> {
                        float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
                        return { xDist, xDist / yDist };
                    },
                    [&](Style::ClosestCorner) -> std::pair<float, float> {
                        auto [distance, corner] = findDistanceToClosestCorner(centerPoint, size);
                        // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
                        // that it would if closest-side or farthest-side were specified, as appropriate.
                        float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
                        return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
                    },
                    [&](Style::FarthestCorner) -> std::pair<float, float> {
                        auto [distance, corner] = findDistanceToFarthestCorner(centerPoint, size);
                        // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
                        // that it would if closest-side or farthest-side were specified, as appropriate.
                        float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
                        return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
                    }
                );
            }
        );
    };

    auto data = WTF::switchOn(radial.gradientBox,
        [&](const Style::RadialGradient::Ellipse& ellipse) -> Gradient::RadialData {
            auto centerPoint = computeCenterPoint(ellipse.position);
            auto [endRadius, aspectRatio] = computeEllipseRadii(ellipse.size, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const Style::RadialGradient::Circle& circle) -> Gradient::RadialData {
            auto centerPoint = computeCenterPoint(circle.position);
            auto [endRadius, aspectRatio] = computeCircleRadius(circle.size, centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        }
    );

    // computeStops() only uses maxExtent for repeating gradients.
    float maxExtent = radial.repeating == Style::GradientRepeat::Repeating ? distanceToFarthestCorner(data.point1, size) : 0;

    RadialGradientAdapter adapter { data };
    auto stops = computeStops(adapter, radial, style, maxExtent);

    return WebCore::Gradient::create(WTFMove(data), radial.colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Prefixed Radial create.

Ref<WebCore::Gradient> StyleGradientImage::createGradient(const Style::PrefixedRadialGradient& radial, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto computeCenterPoint = [&](const std::optional<Style::Position>& position) -> FloatPoint {
        return position ? computeEndPoint(*position, size) : FloatPoint { size.width() / 2, size.height() / 2 };
    };

    auto computeEllipseRadii = [&](const std::variant<Style::PrefixedRadialGradient::Ellipse::Size, Style::PrefixedRadialGradient::Extent>& ellipseSizeOrExtent, FloatPoint centerPoint) -> std::pair<float, float> {
        return WTF::switchOn(ellipseSizeOrExtent,
            [&](const Style::PrefixedRadialGradient::Ellipse::Size& ellipseSize) -> std::pair<float, float> {
                auto xDist = resolveRadius(get<0>(ellipseSize), size.width());
                auto yDist = resolveRadius(get<1>(ellipseSize), size.height());
                return { xDist, xDist / yDist };
            },
            [&](const Style::PrefixedRadialGradient::Extent& extent) -> std::pair<float, float> {
                return WTF::switchOn(extent,
                    [&](Style::ClosestSide) -> std::pair<float, float> {
                        float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
                        return { xDist, xDist / yDist };
                    },
                    [&](Style::Contain) -> std::pair<float, float> {
                        float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
                        return { xDist, xDist / yDist };
                    },
                    [&](Style::FarthestSide) -> std::pair<float, float> {
                        float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
                        return { xDist, xDist / yDist };
                    },
                    [&](Style::ClosestCorner) -> std::pair<float, float> {
                        auto [distance, corner] = findDistanceToClosestCorner(centerPoint, size);
                        // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
                        // that it would if closest-side or farthest-side were specified, as appropriate.
                        float xDist = std::min(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::min(centerPoint.y(), size.height() - centerPoint.y());
                        return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
                    },
                    [&](Style::FarthestCorner) -> std::pair<float, float> {
                        auto [distance, corner] = findDistanceToFarthestCorner(centerPoint, size);
                        // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
                        // that it would if closest-side or farthest-side were specified, as appropriate.
                        float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
                        return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
                    },
                    [&](Style::Cover) -> std::pair<float, float> {
                        auto [distance, corner] = findDistanceToFarthestCorner(centerPoint, size);
                        // If <shape> is ellipse, the gradient-shape has the same ratio of width to height
                        // that it would if closest-side or farthest-side were specified, as appropriate.
                        float xDist = std::max(centerPoint.x(), size.width() - centerPoint.x());
                        float yDist = std::max(centerPoint.y(), size.height() - centerPoint.y());
                        return { horizontalEllipseRadius(corner - centerPoint, xDist / yDist), xDist / yDist };
                    }
                );
            }
        );
    };

    auto computeCircleRadius = [&](const Style::PrefixedRadialGradient::Extent& extent, FloatPoint centerPoint) -> std::pair<float, float> {
        return WTF::switchOn(extent,
            [&](Style::ClosestSide) -> std::pair<float, float> {
                return { std::min({ centerPoint.x(), size.width() - centerPoint.x(), centerPoint.y(), size.height() - centerPoint.y() }), 1 };
            },
            [&](Style::Contain) -> std::pair<float, float> {
                return { std::min({ centerPoint.x(), size.width() - centerPoint.x(), centerPoint.y(), size.height() - centerPoint.y() }), 1 };
            },
            [&](Style::FarthestSide) -> std::pair<float, float> {
                return { std::max({ centerPoint.x(), size.width() - centerPoint.x(), centerPoint.y(), size.height() - centerPoint.y() }), 1 };
            },
            [&](Style::ClosestCorner) -> std::pair<float, float> {
                return { distanceToClosestCorner(centerPoint, size), 1 };
            },
            [&](Style::FarthestCorner) -> std::pair<float, float> {
                return { distanceToFarthestCorner(centerPoint, size), 1 };
            },
            [&](Style::Cover) -> std::pair<float, float> {
                return { distanceToFarthestCorner(centerPoint, size), 1 };
            }
        );
    };

    auto data = WTF::switchOn(radial.gradientBox,
        [&](const Style::PrefixedRadialGradient::Ellipse& ellipse) -> Gradient::RadialData {
            auto centerPoint = computeCenterPoint(ellipse.position);
            auto [endRadius, aspectRatio] = computeEllipseRadii(ellipse.size.value_or(Style::PrefixedRadialGradient::Extent { CSS::Cover { } }), centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        },
        [&](const Style::PrefixedRadialGradient::Circle& circle) -> Gradient::RadialData {
            auto centerPoint = computeCenterPoint(circle.position);
            auto [endRadius, aspectRatio] = computeCircleRadius(circle.size.value_or(Style::PrefixedRadialGradient::Extent { CSS::Cover { } }), centerPoint);

            return Gradient::RadialData { centerPoint, centerPoint, 0, endRadius, aspectRatio };
        }
    );

    // computeStops() only uses maxExtent for repeating gradients.
    float maxExtent = radial.repeating == Style::GradientRepeat::Repeating ? distanceToFarthestCorner(data.point1, size) : 0;

    RadialGradientAdapter adapter { data };
    auto stops = computeStops(adapter, radial, style, maxExtent);

    return WebCore::Gradient::create(WTFMove(data), radial.colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Deprecated Radial create.

Ref<WebCore::Gradient> StyleGradientImage::createGradient(const Style::DeprecatedRadialGradient& radial, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto firstPoint = computeEndPoint(radial.gradientBox.first, size);
    auto secondPoint = computeEndPoint(radial.gradientBox.second, size);

    auto firstRadius = radial.gradientBox.firstRadius.value;
    auto secondRadius = radial.gradientBox.secondRadius.value;
    auto aspectRatio = 1.0f;

    WebCore::Gradient::RadialData data { firstPoint, secondPoint, firstRadius, secondRadius, aspectRatio };
    RadialGradientAdapter adapter { data };
    auto stops = computeStopsForDeprecatedVariants(adapter, radial, style);

    return WebCore::Gradient::create(WTFMove(data), radial.colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

// MARK: - Conic create.

Ref<WebCore::Gradient> StyleGradientImage::createGradient(const Style::ConicGradient& conic, const FloatSize& size, const RenderStyle& style) const
{
    ASSERT(!size.isEmpty());

    auto computeCenterPoint = [&](const std::optional<Style::Position>& position) -> FloatPoint {
        return position ? computeEndPoint(*position, size) : FloatPoint { size.width() / 2, size.height() / 2 };
    };

    auto centerPoint = computeCenterPoint(conic.gradientBox.position);
    float angleRadians = conic.gradientBox.angle ? CSSPrimitiveValue::computeRadians(conic.gradientBox.angle->unit, conic.gradientBox.angle->value) : 0;

    WebCore::Gradient::ConicData data { centerPoint, angleRadians };
    ConicGradientAdapter adapter;
    auto stops = computeStops(adapter, conic, style, 1);

    return WebCore::Gradient::create(WTFMove(data), conic.colorInterpolationMethod.method, GradientSpreadMethod::Pad, WTFMove(stops));
}

} // namespace WebCore
