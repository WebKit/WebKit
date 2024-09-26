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
#include "CSSPrimitiveNumericTypes+Serialization.h"
#include "CSSPrimitiveValueMappings.h"
#include "CSSValueKeywords.h"
#include "CalculationValue.h"
#include "ColorInterpolation.h"
#include "StyleBuilderConverter.h"
#include "StyleGradientImage.h"
#include "StylePosition.h"
#include "StylePrimitiveNumericTypes+Conversions.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

template<CSS::RawNumeric T> static inline bool styleImageIsUncacheable(const T&);
template<typename T> static bool styleImageIsUncacheable(const std::optional<T>&);
template<typename T> static bool styleImageIsUncacheable(const CSS::UnevaluatedCalc<T>&);
template<typename T> static bool styleImageIsUncacheable(const CSS::PrimitiveNumeric<T>&);
template<typename T1, typename T2> static bool styleImageIsUncacheable(const std::pair<T1, T2>&);
template<typename... Ts> static inline bool styleImageIsUncacheable(const CSS::SpaceSeparatedTuple<Ts...>&);
template<typename... Ts> static inline bool styleImageIsUncacheable(const std::variant<Ts...>&);

static inline bool styleImageIsUncacheable(CSSUnitType unit)
{
    return conversionToCanonicalUnitRequiresConversionData(unit);
}

static inline bool styleImageIsUncacheable(const CSSPrimitiveValue& value)
{
    if (auto* calc = value.cssCalcValue())
        return calc->requiresConversionData();
    return styleImageIsUncacheable(value.primitiveType());
}

static inline bool styleImageIsUncacheable(const CSSValue& value)
{
    if (const CSSPrimitiveValue* primitive = dynamicDowncast<CSSPrimitiveValue>(value))
        return styleImageIsUncacheable(*primitive);
    return false;
}

constexpr bool styleImageIsUncacheable(const CSS::Left&)
{
    return false;
}

constexpr bool styleImageIsUncacheable(const CSS::Right&)
{
    return false;
}

constexpr bool styleImageIsUncacheable(const CSS::Top&)
{
    return false;
}

constexpr bool styleImageIsUncacheable(const CSS::Bottom&)
{
    return false;
}

constexpr bool styleImageIsUncacheable(const CSS::Center&)
{
    return false;
}

static inline bool styleImageIsUncacheable(const CSS::Position& position)
{
    return styleImageIsUncacheable(position.value);
}

template<CSS::RawNumeric T> static inline bool styleImageIsUncacheable(const T& raw)
{
    return styleImageIsUncacheable(raw.type);
}

template<typename T> static bool styleImageIsUncacheable(const std::optional<T>& optional)
{
    return optional && styleImageIsUncacheable(*optional);
}

template<typename T> static bool styleImageIsUncacheable(const CSS::UnevaluatedCalc<T>& calc)
{
    return calc.calc->requiresConversionData();
}

template<typename T> static bool styleImageIsUncacheable(const CSS::PrimitiveNumeric<T>& numeric)
{
    return styleImageIsUncacheable(numeric.value);
}

template<typename T1, typename T2> static bool styleImageIsUncacheable(const std::pair<T1, T2>& pair)
{
    return styleImageIsUncacheable(pair.first) || styleImageIsUncacheable(pair.second);
}

template<typename... Ts> static inline bool styleImageIsUncacheable(const CSS::SpaceSeparatedTuple<Ts...>& tuple)
{
    return WTF::apply([&](const auto& ...x) { return (styleImageIsUncacheable(x) || ...); }, tuple);
}

template<typename... Ts> static inline bool styleImageIsUncacheable(const std::variant<Ts...>& variant)
{
    return WTF::switchOn(variant, [](const auto& value) { return styleImageIsUncacheable(value); });
}

template<typename Stop> static bool styleImageIsUncacheable(const CSSGradientColorStopList<Stop>& stops)
{
    for (auto& stop : stops) {
        if (styleImageIsUncacheable(stop.position))
            return true;
        if (stop.color && Style::BuilderState::isColorFromPrimitiveValueDerivedFromElement(*stop.color))
            return true;
    }
    return false;
}

static inline std::optional<StyleColor> computeStopColor(const RefPtr<CSSPrimitiveValue>& color, Style::BuilderState& state)
{
    if (!color)
        return std::nullopt;
    return state.colorFromPrimitiveValue(*color);
}

static inline std::optional<Style::LengthPercentage> computeStopPosition(const CSSGradientLinearColorStop::Position& position, Style::BuilderState& state)
{
    return CSS::toStyle(position, state);
}

static inline std::optional<Style::AnglePercentage> computeStopPosition(const CSSGradientAngularColorStop::Position& position, Style::BuilderState& state)
{
    return CSS::toStyle(position, state);
}

static inline Style::Number computeStopPosition(const CSSGradientDeprecatedColorStop::Position& position, Style::BuilderState& state)
{
    return WTF::switchOn(position,
        [&](const CSS::Number& number) {
            return CSS::toStyle(number, state);
        },
        [&](const CSS::Percentage& percentage) {
            return Style::Number { CSS::toStyle(percentage, state).value / 100.0f };
        }
    );
}

static decltype(auto) toStyle(const CSSGradientLinearColorStopList& stops, Style::BuilderState& state)
{
    return stops.map([&](auto& stop) -> StyleGradientImageLinearColorStop {
        return { computeStopColor(stop.color, state), computeStopPosition(stop.position, state) };
    });
}

static decltype(auto) toStyle(const CSSGradientAngularColorStopList& stops, Style::BuilderState& state)
{
    return stops.map([&](auto& stop) -> StyleGradientImageAngularColorStop {
        return { computeStopColor(stop.color, state), computeStopPosition(stop.position, state) };
    });
}

static decltype(auto) toStyle(const CSSGradientDeprecatedColorStopList& stops, Style::BuilderState& state)
{
    return stops.map([&](auto& stop) -> StyleGradientImageDeprecatedColorStop {
        return { computeStopColor(stop.color, state), computeStopPosition(stop.position, state) };
    });
}

// MARK: -

RefPtr<StyleImage> CSSLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto gradientLine = WTF::switchOn(m_data.gradientLine,
        [&](const CSS::Angle& value) -> StyleGradientImage::LinearData::GradientLine {
            return CSS::toStyle(value, state);
        },
        [&](const auto& value) -> StyleGradientImage::LinearData::GradientLine {
            return value;
        }
    );

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::LinearData {
            WTFMove(gradientLine),
            m_repeating,
            toStyle(m_stops, state)
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
        [&](const CSS::Angle& value) -> StyleGradientImage::PrefixedLinearData::GradientLine {
            return CSS::toStyle(value, state);
        },
        [&](const auto& value) -> StyleGradientImage::PrefixedLinearData::GradientLine {
            return value;
        }
    );

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::PrefixedLinearData {
            WTFMove(gradientLine),
            m_repeating,
            toStyle(m_stops, state)
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
            CSS::toStyle(m_data.first, state),
            CSS::toStyle(m_data.second, state),
            toStyle(m_stops, state)
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
                CSS::toStyle(shape.position, state)
            };
        },
        [&](const CSSRadialGradientValue::Extent& extent) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::Extent {
                extent.extent,
                CSS::toStyle(extent.position, state)
            };
        },
        [&](const CSSRadialGradientValue::Length& length) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::Length {
                CSS::toStyle(length.length, state),
                CSS::toStyle(length.position, state)
            };
        },
        [&](const CSSRadialGradientValue::Size& size) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::Size {
                CSS::toStyle(size.size, state),
                CSS::toStyle(size.position, state)
            };
        },
        [&](const CSSRadialGradientValue::CircleOfLength& circleOfLength) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::CircleOfLength {
                CSS::toStyle(circleOfLength.length, state),
                CSS::toStyle(circleOfLength.position, state)
            };
        },
        [&](const CSSRadialGradientValue::CircleOfExtent& circleOfExtent) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::CircleOfExtent {
                circleOfExtent.extent,
                CSS::toStyle(circleOfExtent.position, state)
            };
        },
        [&](const CSSRadialGradientValue::EllipseOfSize& ellipseOfSize) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::EllipseOfSize {
                CSS::toStyle(ellipseOfSize.size, state),
                CSS::toStyle(ellipseOfSize.position, state)
            };
        },
        [&](const CSSRadialGradientValue::EllipseOfExtent& ellipseOfExtent) -> StyleGradientImage::RadialData::GradientBox {
            return StyleGradientImage::RadialData::EllipseOfExtent {
                ellipseOfExtent.extent,
                CSS::toStyle(ellipseOfExtent.position, state)
            };
        },
        [&](const CSS::Position& position) -> StyleGradientImage::RadialData::GradientBox {
            return CSS::toStyle(position, state);
        }
    );

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::RadialData {
            WTFMove(gradientBox),
            m_repeating,
            toStyle(m_stops, state)
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
                CSS::toStyle(measuredSize.size, state)
            };
        }
    );

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::PrefixedRadialData {
            WTFMove(gradientBox),
            CSS::toStyle(m_data.position, state),
            m_repeating,
            toStyle(m_stops, state)
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

    auto toStyleRadius = [](const CSS::Number& radius, Style::BuilderState& state) -> Style::Number {
        return { CSS::toStyle(radius, state).value * state.cssToLengthConversionData().zoom() };
    };

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::DeprecatedRadialData {
            CSS::toStyle(m_data.first, state),
            CSS::toStyle(m_data.second, state),
            toStyleRadius(m_data.firstRadius, state),
            toStyleRadius(m_data.secondRadius, state),
            toStyle(m_stops, state)
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

    auto styleImage = StyleGradientImage::create(
        StyleGradientImage::ConicData {
            CSS::toStyle(m_data.angle, state),
            CSS::toStyle(m_data.position, state),
            m_repeating,
            toStyle(m_stops, state)
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

static void appendColorStop(StringBuilder& builder, const CSSGradientDeprecatedColorStop& stop)
{
    auto appendRaw = [&](const auto& color, CSS::NumberRaw raw) {
        if (!raw.value)
            builder.append("from("_s, color->cssText(), ')');
        else if (raw.value == 1)
            builder.append("to("_s, color->cssText(), ')');
        else {
            builder.append("color-stop("_s);
            serializationForCSS(builder, raw);
            builder.append(", "_s, color->cssText(), ')');
        }
    };

    auto appendCalc = [&](const auto& calc) {
        builder.append("color-stop("_s, calc.calc->cssText(), ", "_s, stop.color->cssText(), ')');
    };

    WTF::switchOn(stop.position,
        [&](const CSS::Number& number) {
            return WTF::switchOn(number.value,
                [&](CSS::NumberRaw raw) {
                    appendRaw(stop.color, raw);
                },
                [&](const CSS::UnevaluatedCalc<CSS::NumberRaw>& calc) {
                    appendCalc(calc);
                }
            );
        },
        [&](const CSS::Percentage& percentage) {
            return WTF::switchOn(percentage.value,
                [&](CSS::PercentageRaw raw) {
                    appendRaw(stop.color, { raw.value / 100.0 });
                },
                [&](const CSS::UnevaluatedCalc<CSS::PercentageRaw>& calc) {
                    appendCalc(calc);
                }
            );
        }
    );
}

template<typename T> static void appendColorStop(StringBuilder& builder, const CSSGradientColorStop<T>& stop)
{
    if (stop.color && stop.position) {
        builder.append(stop.color->cssText(), ' ');
        serializationForCSS(builder, *stop.position);
    } else if (stop.color)
        builder.append(stop.color->cssText());
    else if (stop.position)
        serializationForCSS(builder, *stop.position);
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
        [&](const CSS::Angle& angle) {
            WTF::switchOn(angle.value,
                [&](const CSS::AngleRaw& angleRaw) {
                    if (CSSPrimitiveValue::computeDegrees(angleRaw.type, angleRaw.value) == 180)
                        return;

                    serializationForCSS(result, angleRaw);
                    wroteSomething = true;
                },
                [&](const CSS::UnevaluatedCalc<CSS::AngleRaw>& angleCalc) {
                    serializationForCSS(result, angleCalc);
                    wroteSomething = true;
                }
            );
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

    result.append(interleave(m_stops, appendColorStop, ", "_s), ')');

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
        [&](const CSS::Angle& angle) {
            serializationForCSS(result, angle);
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

    result.append(", "_s, interleave(m_stops, appendColorStop, ", "_s), ')');

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
    if (!m_stops.isEmpty())
        result.append(", "_s, interleave(m_stops, appendColorStop, ", "_s));
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

String CSSRadialGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "repeating-radial-gradient("_s : "radial-gradient("_s);

    bool wroteSomething = false;

    auto appendPosition = [&](const CSS::Position& position) {
        if (!CSS::isCenterPosition(position)) {
            if (wroteSomething)
                result.append(' ');
            result.append("at "_s);
            serializationForCSS(result, position);
            wroteSomething = true;
        }
    };

    auto appendOptionalPosition = [&](const std::optional<CSS::Position>& position) {
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
            serializationForCSS(result, data.length);
            wroteSomething = true;

            appendOptionalPosition(data.position);
        },
        [&](const CircleOfLength& data) {
            serializationForCSS(result, data.length);
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
            serializationForCSS(result, data.size);
            wroteSomething = true;

            appendOptionalPosition(data.position);
        },
        [&](const EllipseOfSize& data) {
            serializationForCSS(result, data.size);
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
        [&](const CSS::Position& data) {
            appendPosition(data);
        }
    );

    if (appendColorInterpolationMethod(result, m_colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        result.append(", "_s);

    result.append(interleave(m_stops, appendColorStop, ", "_s), ')');

    return result.toString();
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
            return WebCore::styleImageIsUncacheable(data.position) || WebCore::styleImageIsUncacheable(data.size);
        },
        [&](const EllipseOfSize& data) {
            return WebCore::styleImageIsUncacheable(data.position) || WebCore::styleImageIsUncacheable(data.size);
        },
        [&](const EllipseOfExtent& data) {
            return WebCore::styleImageIsUncacheable(data.position);
        },
        [&](const CSS::Position& data) {
            return WebCore::styleImageIsUncacheable(data.value);
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
            result.append(", "_s);
            serializationForCSS(result, measuredSize.size);
        }
    );

    result.append(", "_s, interleave(m_stops, appendColorStop, ", "_s), ')');

    return result.toString();
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
            return WebCore::styleImageIsUncacheable(measuredSize.size);
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
    if (!m_stops.isEmpty())
        result.append(", "_s, interleave(m_stops, appendColorStop, ", "_s));
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

    if (m_data.angle) {
        WTF::switchOn(m_data.angle->value,
            [&](const CSS::AngleRaw& angleRaw) {
                if (angleRaw.value) {
                    result.append("from "_s);
                    serializationForCSS(result, angleRaw);
                    wroteSomething = true;
                }
            },
            [&](const CSS::UnevaluatedCalc<CSS::AngleRaw>& angleCalc) {
                result.append("from "_s);
                serializationForCSS(result, angleCalc);
                wroteSomething = true;
            }
        );
    }

    if (m_data.position && !CSS::isCenterPosition(*m_data.position)) {
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

    result.append(interleave(m_stops, appendColorStop, ", "_s), ')');

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
