/*
 * Copyright (C) 2008-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSGradientValue.h"

#include "CSSCalcSymbolTable.h"
#include "CSSCalcValue.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValueKeywords.h"
#include "CalculationValue.h"
#include "ColorInterpolation.h"
#include "StyleBuilderConverter.h"
#include "StyleBuilderState.h"
#include "StyleGradientImage.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

// FIXME: Can `styleImageIsUncacheable` be replaced via extending the `ComputedStyleDependencies` system?

static inline bool styleImageIsUncacheable(const CSSPrimitiveValue& value)
{
    return value.isFontRelativeLength() || value.isContainerPercentageLength();
}

static inline bool styleImageIsUncacheable(const CSSValue& value)
{
    if (const CSSPrimitiveValue* primitive = dynamicDowncast<CSSPrimitiveValue>(value))
        return styleImageIsUncacheable(*primitive);
    return false;
}

static bool styleImageIsUncacheable(const CSSGradientColorStopList& stops)
{
    for (auto& stop : stops) {
        if (stop.position && styleImageIsUncacheable(*stop.position))
            return true;
        if (stop.color && Style::BuilderState::isColorFromPrimitiveValueDerivedFromElement(*stop.color))
            return true;
    }
    return false;
}

static bool styleImageIsUncacheable(const CSSGradientPosition& position)
{
    auto styleImageIsUncacheableForCoordinate = [](CSSValue& coordinate) -> bool {
        if (coordinate.isPair())
            return styleImageIsUncacheable(coordinate.second());
        return styleImageIsUncacheable(coordinate);
    };

    return styleImageIsUncacheableForCoordinate(position.x) || styleImageIsUncacheableForCoordinate(position.y);
}

static bool styleImageIsUncacheable(const std::optional<CSSGradientPosition>& position)
{
    return position && styleImageIsUncacheable(*position);
}

static inline std::optional<StyleColor> computeStopColor(const RefPtr<CSSPrimitiveValue>& color, Style::BuilderState& state)
{
    if (!color)
        return std::nullopt;

    return state.colorFromPrimitiveValue(*color);
}

enum class StopPositionResolution { Standard, Deprecated };

template<StopPositionResolution resolution> static inline std::optional<Length> computeLengthStopPosition(const RefPtr<CSSPrimitiveValue>& position, Style::BuilderState& state)
{
    if (!position)
        return std::nullopt;

    if constexpr (resolution == StopPositionResolution::Deprecated) {
        if (position->isPercentage())
            return Length(position->floatValue(CSSUnitType::CSS_PERCENTAGE), LengthType::Percent);
        return Length(position->floatValue(CSSUnitType::CSS_NUMBER), LengthType::Fixed);
    } else
        return Style::BuilderConverter::convertLength(state, *position);
}

static inline std::variant<std::monostate, AngleRaw, PercentRaw> computeAngularStopPosition(const RefPtr<CSSPrimitiveValue>& position)
{
    if (!position)
        return std::monostate { };

    if (position->isPercentage())
        return { PercentRaw { position->doubleValue(CSSUnitType::CSS_PERCENTAGE) } };

    if (position->isAngle())
        return { AngleRaw { position->primitiveType(), position->doubleValue() } };

    ASSERT_NOT_REACHED();
    return std::monostate { };
}

template<StopPositionResolution resolution> static decltype(auto) computeLengthStops(const CSSGradientColorStopList& stops, Style::BuilderState& state)
{
    return stops.map([&](auto& stop) -> StyleGradientImageLengthStop {
        return { computeStopColor(stop.color, state), computeLengthStopPosition<resolution>(stop.position, state) };
    });
}

static decltype(auto) computeAngularStops(const CSSGradientColorStopList& stops, Style::BuilderState& state)
{
    return stops.map([&](auto& stop) -> StyleGradientImageAngularStop {
        return { computeStopColor(stop.color, state), computeAngularStopPosition(stop.position) };
    });
}

// MARK: StyleGradientDeprecatedPoint resolution.

static StyleGradientDeprecatedPoint::Coordinate resolvePointCoordinate(const Ref<CSSPrimitiveValue>& coordinate, Style::BuilderState&)
{
    if (coordinate->isPercentage())
        return { PercentRaw { coordinate->doubleValue(CSSUnitType::CSS_PERCENTAGE) } };
    return { NumberRaw { coordinate->doubleValue(CSSUnitType::CSS_NUMBER) } };
}

static StyleGradientDeprecatedPoint resolvePoint(const CSSGradientDeprecatedPoint& point, Style::BuilderState& state)
{
    return {
        resolvePointCoordinate(point.x, state),
        resolvePointCoordinate(point.y, state)
    };
}

// MARK: StyleGradientPosition resolution

static StyleGradientPosition::Coordinate resolvePositionCoordinateX(const Ref<CSSValue>& coordinate, Style::BuilderState& state)
{
    if (coordinate->isPair()) {
        if (coordinate->first().valueID() == CSSValueRight)
            return { convertTo100PercentMinusLength(Style::BuilderConverter::convertLength(state, coordinate->second())) };
        return { Style::BuilderConverter::convertLength(state, coordinate->second()) };
    }

    switch (coordinate->valueID()) {
    case CSSValueLeft:
        return { Length(0, LengthType::Percent) };
    case CSSValueCenter:
        return { Length(50, LengthType::Percent) };
    case CSSValueRight:
        return { Length(100, LengthType::Percent) };
    default:
        break;
    }

    return { Style::BuilderConverter::convertLength(state, coordinate) };
}

static StyleGradientPosition::Coordinate resolvePositionCoordinateY(const Ref<CSSValue>& coordinate, Style::BuilderState& state)
{
    if (coordinate->isPair()) {
        if (coordinate->first().valueID() == CSSValueBottom)
            return { convertTo100PercentMinusLength(Style::BuilderConverter::convertLength(state, coordinate->second())) };
        return { Style::BuilderConverter::convertLength(state, coordinate->second()) };
    }

    switch (coordinate->valueID()) {
    case CSSValueTop:
        return { Length(0, LengthType::Percent) };
    case CSSValueCenter:
        return { Length(50, LengthType::Percent) };
    case CSSValueBottom:
        return { Length(100, LengthType::Percent) };
    default:
        break;
    }

    return { Style::BuilderConverter::convertLength(state, coordinate) };
}

static inline StyleGradientPosition resolvePosition(const CSSGradientPosition& position, Style::BuilderState& state)
{
    return {
        resolvePositionCoordinateX(position.x, state),
        resolvePositionCoordinateY(position.y, state)
    };
}

static inline std::optional<StyleGradientPosition> resolvePosition(const std::optional<CSSGradientPosition>& position, Style::BuilderState& state)
{
    if (!position)
        return std::nullopt;
    return resolvePosition(*position, state);
}

// MARK: Serialization

static void serializationForCSS(StringBuilder& builder, const CSSGradientPosition& position)
{
    builder.append(position.x->cssText(), ' ', position.y->cssText());
}

static void serializationForCSS(StringBuilder& builder, const CSSGradientDeprecatedPoint& position)
{
    builder.append(position.x->cssText(), ' ', position.y->cssText());
}

// MARK: -

RefPtr<StyleImage> CSSLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto gradientLine = WTF::switchOn(m_data.gradientLine,
        [](const auto& value) -> StyleGradientImage::LinearData::GradientLine {
            return evaluateCalc(value, { });
        }
    );

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::LinearData {
            WTFMove(gradientLine),
            m_repeating,
            computeLengthStops<StopPositionResolution::Standard>(m_stops, state)
        },
        m_colorInterpolationMethod
    );
    if (!styleImageIsUncacheable())
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSPrefixedLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto gradientLine = WTF::switchOn(m_data.gradientLine,
        [](const auto& value) -> StyleGradientImage::PrefixedLinearData::GradientLine {
            return evaluateCalc(value, { });
        }
    );

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::PrefixedLinearData {
            WTFMove(gradientLine),
            m_repeating,
            computeLengthStops<StopPositionResolution::Standard>(m_stops, state)
        },
        m_colorInterpolationMethod
    );
    if (!styleImageIsUncacheable())
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSDeprecatedLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::DeprecatedLinearData {
            resolvePoint(m_data.first, state),
            resolvePoint(m_data.second, state),
            computeLengthStops<StopPositionResolution::Deprecated>(m_stops, state)
        },
        m_colorInterpolationMethod
    );
    if (!styleImageIsUncacheable())
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto gradientBox = WTF::switchOn(m_data.gradientBox,
        [&](std::monostate) -> StyleGradientImage::RadialData::GradientBox {
            return std::monostate { };
        },
        [&](const CSSRadialGradientValue::Shape& shape) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::Shape {
                shape.shape,
                resolvePosition(shape.position, state)
            };
        },
        [&](const CSSRadialGradientValue::Extent& extent) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::Extent {
                extent.extent,
                resolvePosition(extent.position, state)
            };
        },
        [&](const CSSRadialGradientValue::Length& length) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::Length {
                Style::BuilderConverter::convertLength(state, length.length),
                resolvePosition(length.position, state)
            };
        },
        [&](const CSSRadialGradientValue::Size& size) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::Size {
                LengthSize {
                    Style::BuilderConverter::convertLength(state, size.size.first),
                    Style::BuilderConverter::convertLength(state, size.size.second)
                },
                resolvePosition(size.position, state)
            };
        },
        [&](const CSSRadialGradientValue::CircleOfLength& circleOfLength) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::CircleOfLength {
                Style::BuilderConverter::convertLength(state, circleOfLength.length),
                resolvePosition(circleOfLength.position, state)
            };
        },
        [&](const CSSRadialGradientValue::CircleOfExtent& circleOfExtent) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::CircleOfExtent {
                circleOfExtent.extent,
                resolvePosition(circleOfExtent.position, state)
            };
        },
        [&](const CSSRadialGradientValue::EllipseOfSize& ellipseOfSize) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::EllipseOfSize {
                LengthSize {
                    Style::BuilderConverter::convertLength(state, ellipseOfSize.size.first),
                    Style::BuilderConverter::convertLength(state, ellipseOfSize.size.second)
                },
                resolvePosition(ellipseOfSize.position, state)
            };
        },
        [&](const CSSRadialGradientValue::EllipseOfExtent& ellipseOfExtent) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::EllipseOfExtent {
                ellipseOfExtent.extent,
                resolvePosition(ellipseOfExtent.position, state)
            };
        },
        [&](const CSSGradientPosition& position) -> StyleGradientImage::RadialData::GradientBox {
            return resolvePosition(position, state);
        }
    );

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::RadialData {
            WTFMove(gradientBox),
            m_repeating,
            computeLengthStops<StopPositionResolution::Standard>(m_stops, state)
        },
        m_colorInterpolationMethod
    );
    if (!styleImageIsUncacheable())
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSPrefixedRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto gradientBox = WTF::switchOn(m_data.gradientBox,
        [&](std::monostate) -> StyleGradientImage::PrefixedRadialData::GradientBox {
            return std::monostate { };
        },
        [&](const CSSPrefixedRadialGradientValue::ShapeKeyword& shape) -> StyleGradientImage::PrefixedRadialData::GradientBox {
            return shape;
        },
        [&](const CSSPrefixedRadialGradientValue::ExtentKeyword& extent) -> StyleGradientImage::PrefixedRadialData::GradientBox {
            return extent;
        },
        [&](const CSSPrefixedRadialGradientValue::ShapeAndExtent& shapeAndExtent) -> StyleGradientImage::PrefixedRadialData::GradientBox {
            return shapeAndExtent;
        },
        [&](const CSSPrefixedRadialGradientValue::MeasuredSize& measuredSize) -> StyleGradientImage::PrefixedRadialData::GradientBox {
            return StyleGradientImage::PrefixedRadialData::MeasuredSize {
                LengthSize {
                    Style::BuilderConverter::convertLength(state, measuredSize.size.first),
                    Style::BuilderConverter::convertLength(state, measuredSize.size.second)
                }
            };
        }
    );

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::PrefixedRadialData {
            WTFMove(gradientBox),
            resolvePosition(m_data.position, state),
            m_repeating,
            computeLengthStops<StopPositionResolution::Standard>(m_stops, state)
        },
        m_colorInterpolationMethod
    );
    if (!styleImageIsUncacheable())
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSDeprecatedRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto resolveRadius = [](const std::variant<NumberRaw, UnevaluatedCalc<NumberRaw>>& radius, Style::BuilderState& state) -> float {
        return evaluateCalc(radius, { }).value * state.cssToLengthConversionData().zoom();
    };

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::DeprecatedRadialData {
            resolvePoint(m_data.first, state),
            resolvePoint(m_data.second, state),
            resolveRadius(m_data.firstRadius, state),
            resolveRadius(m_data.secondRadius, state),
            computeLengthStops<StopPositionResolution::Deprecated>(m_stops, state)
        },
        m_colorInterpolationMethod
    );
    if (!styleImageIsUncacheable())
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSConicGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto resolveAngle = [&](const CSSConicGradientValue::Angle& angle) -> std::optional<AngleRaw> {
        return WTF::switchOn(angle,
            [](std::monostate) -> std::optional<AngleRaw> {
                return std::nullopt;
            },
            [](auto& value) -> std::optional<AngleRaw> {
                return evaluateCalc(value, { });
            }
        );
    };

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::ConicData {
            resolveAngle(m_data.angle),
            resolvePosition(m_data.position, state),
            m_repeating,
            computeAngularStops(m_stops, state)
        },
        m_colorInterpolationMethod
    );
    if (!styleImageIsUncacheable())
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

static bool appendColorInterpolationMethod(StringBuilder& builder, CSSGradientColorInterpolationMethod colorInterpolationMethod, bool needsLeadingSpace)
{
    return WTF::switchOn(colorInterpolationMethod.method.colorSpace,
        [&](const ColorInterpolationMethod::OKLab&) {
            if (colorInterpolationMethod.defaultMethod != CSSGradientColorInterpolationMethod::Default::OKLab) {
                builder.append(needsLeadingSpace ? " "_s : ""_s, "in oklab"_s);
                return true;
            }
            return false;
        },
        [&](const ColorInterpolationMethod::SRGB&) {
            if (colorInterpolationMethod.defaultMethod != CSSGradientColorInterpolationMethod::Default::SRGB) {
                builder.append(needsLeadingSpace ? " "_s : ""_s, "in srgb"_s);
                return true;
            }
            return false;
        },
        [&]<typename MethodColorSpace> (const MethodColorSpace& methodColorSpace) {
            builder.append(needsLeadingSpace ? " "_s : ""_s, "in "_s, serializationForCSS(methodColorSpace.interpolationColorSpace));
            if constexpr (hasHueInterpolationMethod<MethodColorSpace>)
                serializationForCSS(builder, methodColorSpace.hueInterpolationMethod);
            return true;
        }
    );
}

static void appendGradientStops(StringBuilder& builder, const Vector<CSSGradientColorStop, 2>& stops)
{
    for (auto& stop : stops) {
        double position = stop.position->doubleValue(CSSUnitType::CSS_NUMBER);
        if (!position)
            builder.append(", from("_s, stop.color->cssText(), ')');
        else if (position == 1)
            builder.append(", to("_s, stop.color->cssText(), ')');
        else
            builder.append(", color-stop("_s, position, ", "_s, stop.color->cssText(), ')');
    }
}

template<typename T, typename U> static void appendSpaceSeparatedOptionalCSSPtrText(StringBuilder& builder, const T& a, const U& b)
{
    if (a && b)
        builder.append(a->cssText(), ' ', b->cssText());
    else if (a)
        builder.append(a->cssText());
    else if (b)
        builder.append(b->cssText());
}

static void writeColorStop(StringBuilder& builder, const CSSGradientColorStop& stop)
{
    appendSpaceSeparatedOptionalCSSPtrText(builder, stop.color, stop.position);
}

// MARK: - Linear.

static ASCIILiteral cssText(CSSLinearGradientValue::Horizontal horizontal)
{
    switch (horizontal) {
    case CSSLinearGradientValue::Horizontal::Left:
        return "left"_s;
    case CSSLinearGradientValue::Horizontal::Right:
        return "right"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static ASCIILiteral cssText(CSSLinearGradientValue::Vertical vertical)
{
    switch (vertical) {
    case CSSLinearGradientValue::Vertical::Top:
        return "top"_s;
    case CSSLinearGradientValue::Vertical::Bottom:
        return "bottom"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

String CSSLinearGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "repeating-linear-gradient("_s : "linear-gradient("_s);
    bool wroteSomething = false;

    WTF::switchOn(m_data.gradientLine,
        [&](std::monostate) { },
        [&](const AngleRaw& angle) {
            if (CSSPrimitiveValue::computeDegrees(angle.type, angle.value) == 180)
                return;

            serializationForCSS(result, angle);
            wroteSomething = true;
        },
        [&](const UnevaluatedCalc<AngleRaw>& angleCalc) {
            serializationForCSS(result, angleCalc);
            wroteSomething = true;
        },
        [&](Horizontal horizontal) {
            result.append("to "_s, WebCore::cssText(horizontal));
            wroteSomething = true;
        },
        [&](Vertical vertical) {
            if (vertical == Vertical::Bottom)
                return;
    
            result.append("to "_s, WebCore::cssText(vertical));
            wroteSomething = true;
        },
        [&](const std::pair<Horizontal, Vertical>& pair) {
            result.append("to "_s, WebCore::cssText(pair.first), ' ', WebCore::cssText(pair.second));
            wroteSomething = true;
        }
    );

    if (appendColorInterpolationMethod(result, m_colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        result.append(", "_s);

    result.append(interleave(m_stops, writeColorStop, ", "_s), ')');

    return result.toString();
}

bool CSSLinearGradientValue::equals(const CSSLinearGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

bool CSSLinearGradientValue::styleImageIsUncacheable() const
{
    return WebCore::styleImageIsUncacheable(m_stops);
}

// MARK: - Prefixed Linear.

static ASCIILiteral cssText(CSSPrefixedLinearGradientValue::Horizontal horizontal)
{
    switch (horizontal) {
    case CSSPrefixedLinearGradientValue::Horizontal::Left:
        return "left"_s;
    case CSSPrefixedLinearGradientValue::Horizontal::Right:
        return "right"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static ASCIILiteral cssText(CSSPrefixedLinearGradientValue::Vertical vertical)
{
    switch (vertical) {
    case CSSPrefixedLinearGradientValue::Vertical::Top:
        return "top"_s;
    case CSSPrefixedLinearGradientValue::Vertical::Bottom:
        return "bottom"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

String CSSPrefixedLinearGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "-webkit-repeating-linear-gradient("_s : "-webkit-linear-gradient("_s);

    WTF::switchOn(m_data.gradientLine,
        [&](std::monostate) {
            result.append("top"_s);
        },
        [&](const AngleRaw& angle) {
            serializationForCSS(result, angle);
        },
        [&](const UnevaluatedCalc<AngleRaw>& angleCalc) {
            serializationForCSS(result, angleCalc);
        },
        [&](Horizontal horizontal) {
            result.append(WebCore::cssText(horizontal));
        },
        [&](Vertical vertical) {
            result.append(WebCore::cssText(vertical));
        },
        [&](const std::pair<Horizontal, Vertical>& pair) {
            result.append(WebCore::cssText(pair.first), ' ', WebCore::cssText(pair.second));
        }
    );

    for (auto& stop : m_stops) {
        result.append(", "_s);
        writeColorStop(result, stop);
    }

    result.append(')');
    return result.toString();
}

bool CSSPrefixedLinearGradientValue::equals(const CSSPrefixedLinearGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

bool CSSPrefixedLinearGradientValue::styleImageIsUncacheable() const
{
    return WebCore::styleImageIsUncacheable(m_stops);
}

// MARK: - Deprecated Linear.

String CSSDeprecatedLinearGradientValue::customCSSText() const
{
    StringBuilder result;
    result.append("-webkit-gradient(linear, "_s);
    serializationForCSS(result, m_data.first);
    result.append(", "_s);
    serializationForCSS(result, m_data.second);
    appendGradientStops(result, m_stops);
    result.append(')');
    return result.toString();
}

bool CSSDeprecatedLinearGradientValue::equals(const CSSDeprecatedLinearGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

bool CSSDeprecatedLinearGradientValue::styleImageIsUncacheable() const
{
    return WebCore::styleImageIsUncacheable(m_stops);
}

// MARK: - Radial.

static ASCIILiteral cssText(CSSRadialGradientValue::ExtentKeyword extent)
{
    switch (extent) {
    case CSSRadialGradientValue::ExtentKeyword::ClosestCorner:
        return "closest-corner"_s;
    case CSSRadialGradientValue::ExtentKeyword::ClosestSide:
        return "closest-side"_s;
    case CSSRadialGradientValue::ExtentKeyword::FarthestCorner:
        return "farthest-corner"_s;
    case CSSRadialGradientValue::ExtentKeyword::FarthestSide:
        return "farthest-side"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static bool isCenterPosition(const CSSValue& value)
{
    if (isValueID(value, CSSValueCenter))
        return true;
    auto* number = dynamicDowncast<CSSPrimitiveValue>(value);
    return number && number->doubleValue(CSSUnitType::CSS_PERCENTAGE) == 50;
}

static bool isCenterPosition(const CSSGradientPosition& position)
{
    return isCenterPosition(position.x) && isCenterPosition(position.y);
}

String CSSRadialGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "repeating-radial-gradient("_s : "radial-gradient("_s);

    bool wroteSomething = false;

    auto appendPosition = [&](const CSSGradientPosition& position) {
        if (!isCenterPosition(position)) {
            if (wroteSomething)
                result.append(' ');
            result.append("at "_s);
            serializationForCSS(result, position);
            wroteSomething = true;
        }
    };

    auto appendOptionalPosition = [&](const std::optional<CSSGradientPosition>& position) {
        if (!position)
            return;
        appendPosition(*position);
    };

    WTF::switchOn(m_data.gradientBox,
        [&](std::monostate) { },
        [&](const Shape& data) {
            if (data.shape != ShapeKeyword::Ellipse) {
                result.append("circle"_s);
                wroteSomething = true;
            }
            appendOptionalPosition(data.position);
        },
        [&](const Extent& data) {
            if (data.extent != ExtentKeyword::FarthestCorner) {
                result.append(WebCore::cssText(data.extent));
                wroteSomething = true;
            }

            appendOptionalPosition(data.position);
        },
        [&](const Length& data) {
            result.append(data.length->cssText());
            wroteSomething = true;

            appendOptionalPosition(data.position);
        },
        [&](const CircleOfLength& data) {
            result.append(data.length->cssText());
            wroteSomething = true;

            appendOptionalPosition(data.position);
        },
        [&](const CircleOfExtent& data) {
            if (data.extent != ExtentKeyword::FarthestCorner)
                result.append("circle "_s, WebCore::cssText(data.extent));
            else
                result.append("circle"_s);
            wroteSomething = true;
            appendOptionalPosition(data.position);
        },
        [&](const Size& data) {
            result.append(data.size.first->cssText(), ' ', data.size.second->cssText());
            wroteSomething = true;
            appendOptionalPosition(data.position);
        },
        [&](const EllipseOfSize& data) {
            result.append(data.size.first->cssText(), ' ', data.size.second->cssText());
            wroteSomething = true;
            appendOptionalPosition(data.position);
        },
        [&](const EllipseOfExtent& data) {
            if (data.extent != ExtentKeyword::FarthestCorner) {
                result.append(WebCore::cssText(data.extent));
                wroteSomething = true;
            }
            appendOptionalPosition(data.position);
        },
        [&](const CSSGradientPosition& data) {
            appendPosition(data);
        }
    );

    if (appendColorInterpolationMethod(result, m_colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        result.append(", "_s);

    result.append(interleave(m_stops, writeColorStop, ", "_s), ')');

    return result.toString();
}

bool CSSRadialGradientValue::Length::operator==(const CSSRadialGradientValue::Length& other) const
{
    return compareCSSValue(length, other.length)
        && position == other.position;
}

bool CSSRadialGradientValue::CircleOfLength::operator==(const CSSRadialGradientValue::CircleOfLength& other) const
{
    return compareCSSValue(length, other.length)
        && position == other.position;
}

bool CSSRadialGradientValue::Size::operator==(const CSSRadialGradientValue::Size& other) const
{
    return compareCSSValue(size.first, other.size.first)
        && compareCSSValue(size.second, other.size.second)
        && position == other.position;
}

bool CSSRadialGradientValue::EllipseOfSize::operator==(const CSSRadialGradientValue::EllipseOfSize& other) const
{
    return compareCSSValue(size.first, other.size.first)
        && compareCSSValue(size.second, other.size.second)
        && position == other.position;
}

bool CSSRadialGradientValue::equals(const CSSRadialGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

bool CSSRadialGradientValue::styleImageIsUncacheable() const
{
    if (WebCore::styleImageIsUncacheable(m_stops))
        return true;

    return WTF::switchOn(m_data.gradientBox,
        [&](std::monostate) {
            return false;
        },
        [&](const Shape& data) {
            return WebCore::styleImageIsUncacheable(data.position);
        },
        [&](const Extent& data) {
            return WebCore::styleImageIsUncacheable(data.position);
        },
        [&](const Length& data) {
            return WebCore::styleImageIsUncacheable(data.position) || WebCore::styleImageIsUncacheable(data.length);
        },
        [&](const CircleOfLength& data) {
            return WebCore::styleImageIsUncacheable(data.position) || WebCore::styleImageIsUncacheable(data.length);
        },
        [&](const CircleOfExtent& data) {
            return WebCore::styleImageIsUncacheable(data.position);
        },
        [&](const Size& data) {
            return WebCore::styleImageIsUncacheable(data.position) || WebCore::styleImageIsUncacheable(data.size.first) || WebCore::styleImageIsUncacheable(data.size.second);
        },
        [&](const EllipseOfSize& data) {
            return WebCore::styleImageIsUncacheable(data.position) || WebCore::styleImageIsUncacheable(data.size.first) || WebCore::styleImageIsUncacheable(data.size.second);
        },
        [&](const EllipseOfExtent& data) {
            return WebCore::styleImageIsUncacheable(data.position);
        },
        [&](const CSSGradientPosition& data) {
            return WebCore::styleImageIsUncacheable(data);
        }
    );
}

// MARK: Prefixed Radial.

static ASCIILiteral cssText(CSSPrefixedRadialGradientValue::ShapeKeyword shape)
{
    switch (shape) {
    case CSSPrefixedRadialGradientValue::ShapeKeyword::Circle:
        return "circle"_s;
    case CSSPrefixedRadialGradientValue::ShapeKeyword::Ellipse:
        return "ellipse"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

static ASCIILiteral cssText(CSSPrefixedRadialGradientValue::ExtentKeyword extent)
{
    switch (extent) {
    case CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestCorner:
        return "closest-corner"_s;
    case CSSPrefixedRadialGradientValue::ExtentKeyword::ClosestSide:
        return "closest-side"_s;
    case CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestCorner:
        return "farthest-corner"_s;
    case CSSPrefixedRadialGradientValue::ExtentKeyword::FarthestSide:
        return "farthest-side"_s;
    case CSSPrefixedRadialGradientValue::ExtentKeyword::Contain:
        return "contain"_s;
    case CSSPrefixedRadialGradientValue::ExtentKeyword::Cover:
        return "cover"_s;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

String CSSPrefixedRadialGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "-webkit-repeating-radial-gradient("_s : "-webkit-radial-gradient("_s);

    if (m_data.position)
        serializationForCSS(result, *m_data.position);
    else
        result.append("center"_s);

    WTF::switchOn(m_data.gradientBox,
        [&](std::monostate) { },
        [&](const ShapeKeyword& shape) {
            result.append(", "_s, WebCore::cssText(shape), " cover"_s);
        },
        [&](const ExtentKeyword& extent) {
            result.append(", ellipse "_s, WebCore::cssText(extent));
        },
        [&](const ShapeAndExtent& shapeAndExtent) {
            result.append(", "_s, WebCore::cssText(shapeAndExtent.shape), ' ', WebCore::cssText(shapeAndExtent.extent));
        },
        [&](const MeasuredSize& measuredSize) {
            result.append(", "_s, measuredSize.size.first->cssText(), ' ', measuredSize.size.second->cssText());
        }
    );

    for (auto& stop : m_stops) {
        result.append(", "_s);
        writeColorStop(result, stop);
    }

    result.append(')');

    return result.toString();
}

bool CSSPrefixedRadialGradientValue::MeasuredSize::operator==(const CSSPrefixedRadialGradientValue::MeasuredSize& other) const
{
    return compareCSSValue(size.first, other.size.first)
        && compareCSSValue(size.second, other.size.second);
}

bool CSSPrefixedRadialGradientValue::equals(const CSSPrefixedRadialGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

bool CSSPrefixedRadialGradientValue::styleImageIsUncacheable() const
{
    if (WebCore::styleImageIsUncacheable(m_stops))
        return true;

    if (WebCore::styleImageIsUncacheable(m_data.position))
        return true;

    return WTF::switchOn(m_data.gradientBox,
        [&](std::monostate) {
            return false;
        },
        [&](const ShapeKeyword&) {
            return false;
        },
        [&](const ExtentKeyword&) {
            return false;
        },
        [&](const ShapeAndExtent&) {
            return false;
        },
        [&](const MeasuredSize& measuredSize) {
            return WebCore::styleImageIsUncacheable(measuredSize.size.first) || WebCore::styleImageIsUncacheable(measuredSize.size.second);
        }
    );
}

// MARK: - Deprecated Radial.

String CSSDeprecatedRadialGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append("-webkit-gradient(radial, "_s);

    serializationForCSS(result, m_data.first);
    result.append(", "_s);
    serializationForCSS(result, m_data.firstRadius);
    result.append(", "_s);
    serializationForCSS(result, m_data.second);
    result.append(", "_s);
    serializationForCSS(result, m_data.secondRadius);

    appendGradientStops(result, m_stops);

    result.append(')');

    return result.toString();
}

bool CSSDeprecatedRadialGradientValue::equals(const CSSDeprecatedRadialGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

bool CSSDeprecatedRadialGradientValue::styleImageIsUncacheable() const
{
    return WebCore::styleImageIsUncacheable(m_stops);
}

// MARK: - Conic

String CSSConicGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "repeating-conic-gradient("_s : "conic-gradient("_s);

    bool wroteSomething = false;

    WTF::switchOn(m_data.angle,
        [&](std::monostate) { },
        [&](const AngleRaw& angle) {
            if (CSSPrimitiveValue::computeDegrees(angle.type, angle.value)) {
                result.append("from "_s);
                serializationForCSS(result, angle);
                wroteSomething = true;
            }
        },
        [&](const UnevaluatedCalc<AngleRaw>& angleCalc) {
            result.append("from "_s);
            serializationForCSS(result, angleCalc);
            wroteSomething = true;
        }
    );

    if (m_data.position && !isCenterPosition(*m_data.position)) {
        if (wroteSomething)
            result.append(' ');
        result.append("at "_s);
        serializationForCSS(result, *m_data.position);
        wroteSomething = true;
    }

    if (appendColorInterpolationMethod(result, m_colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        result.append(", "_s);

    result.append(interleave(m_stops, writeColorStop, ", "_s), ')');

    return result.toString();
}

bool CSSConicGradientValue::equals(const CSSConicGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

bool CSSConicGradientValue::styleImageIsUncacheable() const
{
    return WebCore::styleImageIsUncacheable(m_stops) || WebCore::styleImageIsUncacheable(m_data.position);
}

} // namespace WebCore
