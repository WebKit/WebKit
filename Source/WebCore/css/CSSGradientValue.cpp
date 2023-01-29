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

#include "CSSCalcValue.h"
#include "CSSToLengthConversionData.h"
#include "CSSValueKeywords.h"
#include "ColorInterpolation.h"
#include "StyleBuilderState.h"
#include "StyleGradientImage.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

static bool styleImageIsCacheable(const CSSGradientColorStopList& stops)
{
    for (auto& stop : stops) {
        if (stop.color && Style::BuilderState::isColorFromPrimitiveValueDerivedFromElement(*stop.color))
            return false;
    }
    return true;
}

static inline std::optional<StyleColor> computeStyleColor(const RefPtr<CSSPrimitiveValue>& color, Style::BuilderState& state)
{
    if (!color)
        return std::nullopt;

    // FIXME: This should call state.colorFromPrimitiveValue(*color) instead, but doing so is
    // blocked on fixing an issue where we don't respect ::first-line in StyleImage correctly.
    // See https://webkit.org/b/247127.
    return StyleColor { state.colorFromPrimitiveValueWithResolvedCurrentColor(*color) };
}

static decltype(auto) computeStops(const CSSGradientColorStopList& stops, Style::BuilderState& state)
{
    return stops.map([&] (auto& stop) -> StyleGradientImageStop {
        return { computeStyleColor(stop.color, state), stop.position };
    });
}

RefPtr<StyleImage> CSSLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        // FIXME: The parameters to LinearData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::LinearData { m_data, m_repeating },
        m_colorInterpolationMethod,
        computeStops(m_stops, state)
    );
    if (styleImageIsCacheable(m_stops))
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSPrefixedLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        // FIXME: The parameters to LinearData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::PrefixedLinearData { m_data, m_repeating },
        m_colorInterpolationMethod,
        computeStops(m_stops, state)
    );
    if (styleImageIsCacheable(m_stops))
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSDeprecatedLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        // FIXME: The parameters to LinearData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::DeprecatedLinearData { m_data },
        m_colorInterpolationMethod,
        computeStops(m_stops, state)
    );
    if (styleImageIsCacheable(m_stops))
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        // FIXME: The parameters to RadialData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::RadialData { m_data, m_repeating },
        m_colorInterpolationMethod,
        computeStops(m_stops, state)
    );
    if (styleImageIsCacheable(m_stops))
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSPrefixedRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        // FIXME: The parameters to RadialData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::PrefixedRadialData { m_data, m_repeating },
        m_colorInterpolationMethod,
        computeStops(m_stops, state)
    );
    if (styleImageIsCacheable(m_stops))
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSDeprecatedRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        // FIXME: The parameters to RadialData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::DeprecatedRadialData { m_data },
        m_colorInterpolationMethod,
        computeStops(m_stops, state)
    );
    if (styleImageIsCacheable(m_stops))
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

RefPtr<StyleImage> CSSConicGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (m_cachedStyleImage)
        return m_cachedStyleImage;

    auto styleImage = StyleGradientImage::create(
        // FIXME: The parameters to ConicData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::ConicData { m_data, m_repeating },
        m_colorInterpolationMethod,
        computeStops(m_stops, state)
    );
    if (styleImageIsCacheable(m_stops))
        m_cachedStyleImage = styleImage.ptr();

    return styleImage;
}

static void appendHueInterpolationMethod(StringBuilder& builder, HueInterpolationMethod hueInterpolationMethod)
{
    switch (hueInterpolationMethod) {
    case HueInterpolationMethod::Shorter:
        break;
    case HueInterpolationMethod::Longer:
        builder.append(" longer hue");
        break;
    case HueInterpolationMethod::Increasing:
        builder.append(" increasing hue");
        break;
    case HueInterpolationMethod::Decreasing:
        builder.append(" decreasing hue");
        break;
    }
}

static bool appendColorInterpolationMethod(StringBuilder& builder, CSSGradientColorInterpolationMethod colorInterpolationMethod, bool needsLeadingSpace)
{
    return WTF::switchOn(colorInterpolationMethod.method.colorSpace,
        [&] (const ColorInterpolationMethod::HSL& hsl) {
            builder.append(needsLeadingSpace ? " " : "", "in hsl");
            appendHueInterpolationMethod(builder, hsl.hueInterpolationMethod);
            return true;
        },
        [&] (const ColorInterpolationMethod::HWB& hwb) {
            builder.append(needsLeadingSpace ? " " : "", "in hwb");
            appendHueInterpolationMethod(builder, hwb.hueInterpolationMethod);
            return true;
        },
        [&] (const ColorInterpolationMethod::LCH& lch) {
            builder.append(needsLeadingSpace ? " " : "", "in lch");
            appendHueInterpolationMethod(builder, lch.hueInterpolationMethod);
            return true;
        },
        [&] (const ColorInterpolationMethod::Lab&) {
            builder.append(needsLeadingSpace ? " " : "", "in lab");
            return true;
        },
        [&] (const ColorInterpolationMethod::OKLCH& oklch) {
            builder.append(needsLeadingSpace ? " " : "", "in oklch");
            appendHueInterpolationMethod(builder, oklch.hueInterpolationMethod);
            return true;
        },
        [&] (const ColorInterpolationMethod::OKLab&) {
            if (colorInterpolationMethod.defaultMethod != CSSGradientColorInterpolationMethod::Default::OKLab) {
                builder.append(needsLeadingSpace ? " " : "", "in oklab");
                return true;
            }
            return false;
        },
        [&] (const ColorInterpolationMethod::SRGB&) {
            if (colorInterpolationMethod.defaultMethod != CSSGradientColorInterpolationMethod::Default::SRGB) {
                builder.append(needsLeadingSpace ? " " : "", "in srgb");
                return true;
            }
            return false;
        },
        [&] (const ColorInterpolationMethod::SRGBLinear&) {
            builder.append(needsLeadingSpace ? " " : "", "in srgb-linear");
            return true;
        },
        [&] (const ColorInterpolationMethod::XYZD50&) {
            builder.append(needsLeadingSpace ? " " : "", "in xyz-d50");
            return true;
        },
        [&] (const ColorInterpolationMethod::XYZD65&) {
            builder.append(needsLeadingSpace ? " " : "", "in xyz-d65");
            return true;
        }
    );
}

static void appendGradientStops(StringBuilder& builder, const Vector<CSSGradientColorStop, 2>& stops)
{
    for (auto& stop : stops) {
        double position = stop.position->doubleValue(CSSUnitType::CSS_NUMBER);
        if (!position)
            builder.append(", from(", stop.color->cssText(), ')');
        else if (position == 1)
            builder.append(", to(", stop.color->cssText(), ')');
        else
            builder.append(", color-stop(", position, ", ", stop.color->cssText(), ')');
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

static bool operator==(const CSSGradientPosition& a, const CSSGradientPosition& b)
{
    return compareCSSValue(a.first, b.first)
        && compareCSSValue(a.second, b.second);
}

static bool operator==(const std::optional<CSSGradientPosition>& a, const std::optional<CSSGradientPosition>& b)
{
    return (!a && !b) || (a && b && *a == *b);
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

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "repeating-linear-gradient(" : "linear-gradient(");
    bool wroteSomething = false;

    WTF::switchOn(m_data.gradientLine,
        [&] (std::monostate) { },
        [&] (const Angle& angle) {
            if (angle.value->computeDegrees() == 180)
                return;

            result.append(angle.value->cssText());
            wroteSomething = true;
        },
        [&] (Horizontal horizontal) {
            result.append("to ", WebCore::cssText(horizontal));
            wroteSomething = true;
        },
        [&] (Vertical vertical) {
            if (vertical == Vertical::Bottom)
                return;
    
            result.append("to ", WebCore::cssText(vertical));
            wroteSomething = true;
        },
        [&] (const std::pair<Horizontal, Vertical>& pair) {
            result.append("to ", WebCore::cssText(pair.first), ' ', WebCore::cssText(pair.second));
            wroteSomething = true;
        }
    );

    if (appendColorInterpolationMethod(result, m_colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    for (auto& stop : m_stops) {
        if (wroteSomething)
            result.append(", ");
        wroteSomething = true;
        writeColorStop(result, stop);
    }

    result.append(')');
    return result.toString();
}

static bool operator==(const CSSLinearGradientValue::Angle& a, const CSSLinearGradientValue::Angle& b)
{
    return compareCSSValue(a.value, b.value);
}

bool operator==(const CSSLinearGradientValue::Data& a, const CSSLinearGradientValue::Data& b)
{
    return a.gradientLine == b.gradientLine;
}

bool CSSLinearGradientValue::equals(const CSSLinearGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
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

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "-webkit-repeating-linear-gradient(" : "-webkit-linear-gradient(");

    WTF::switchOn(m_data.gradientLine,
        [&] (std::monostate) {
            result.append("top");
        },
        [&] (const Angle& angle) {
            result.append(angle.value->cssText());
        },
        [&] (Horizontal horizontal) {
            result.append(WebCore::cssText(horizontal));
        },
        [&] (Vertical vertical) {
            result.append(WebCore::cssText(vertical));
        },
        [&] (const std::pair<Horizontal, Vertical>& pair) {
            result.append(WebCore::cssText(pair.first), ' ', WebCore::cssText(pair.second));
        }
    );

    for (auto& stop : m_stops) {
        result.append(", ");
        writeColorStop(result, stop);
    }

    result.append(')');
    return result.toString();
}

static bool operator==(const CSSPrefixedLinearGradientValue::Angle& a, const CSSPrefixedLinearGradientValue::Angle& b)
{
    return compareCSSValue(a.value, b.value);
}

bool operator==(const CSSPrefixedLinearGradientValue::Data& a, const CSSPrefixedLinearGradientValue::Data& b)
{
    return a.gradientLine == b.gradientLine;
}

bool CSSPrefixedLinearGradientValue::equals(const CSSPrefixedLinearGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

// MARK: - Deprecated Linear.

String CSSDeprecatedLinearGradientValue::customCSSText() const
{
    StringBuilder result;
    result.append("-webkit-gradient(linear, ", m_data.firstX->cssText(), ' ', m_data.firstY->cssText(), ", ", m_data.secondX->cssText(), ' ', m_data.secondY->cssText());
    appendGradientStops(result, m_stops);
    result.append(')');
    return result.toString();
}

bool operator==(const CSSDeprecatedLinearGradientValue::Data& a, const CSSDeprecatedLinearGradientValue::Data& b)
{
    return compareCSSValue(a.firstX, b.firstX)
        && compareCSSValue(a.firstY, b.firstY)
        && compareCSSValue(a.secondX, b.secondX)
        && compareCSSValue(a.secondY, b.secondY);
}

bool CSSDeprecatedLinearGradientValue::equals(const CSSDeprecatedLinearGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
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

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "repeating-radial-gradient(" : "radial-gradient(");

    bool wroteSomething = false;

    auto appendPosition = [&](const CSSGradientPosition& position) {
        if (!position.first->isCenterPosition() || !position.second->isCenterPosition()) {
            if (wroteSomething)
                result.append(' ');
            result.append("at ");
            appendSpaceSeparatedOptionalCSSPtrText(result, position.first.ptr(), position.second.ptr());
            wroteSomething = true;
        }
    };

    auto appendOptionalPosition = [&](const std::optional<CSSGradientPosition>& position) {
        if (!position)
            return;
        appendPosition(*position);
    };

    WTF::switchOn(m_data.gradientBox,
        [&] (std::monostate) { },
        [&] (const Shape& data) {
            if (data.shape != ShapeKeyword::Ellipse) {
                result.append("circle");
                wroteSomething = true;
            }
            appendOptionalPosition(data.position);
        },
        [&] (const Extent& data) {
            if (data.extent != ExtentKeyword::FarthestCorner) {
                result.append(WebCore::cssText(data.extent));
                wroteSomething = true;
            }

            appendOptionalPosition(data.position);
        },
        [&] (const Length& data) {
            result.append(data.length->cssText());
            wroteSomething = true;

            appendOptionalPosition(data.position);
        },
        [&] (const CircleOfLength& data) {
            result.append(data.length->cssText());
            wroteSomething = true;

            appendOptionalPosition(data.position);
        },
        [&] (const CircleOfExtent& data) {
            if (data.extent != ExtentKeyword::FarthestCorner)
                result.append("circle ", WebCore::cssText(data.extent));
            else
                result.append("circle");
            wroteSomething = true;
            appendOptionalPosition(data.position);
        },
        [&] (const Size& data) {
            result.append(data.size.first->cssText(), ' ', data.size.second->cssText());
            wroteSomething = true;
            appendOptionalPosition(data.position);
        },
        [&] (const EllipseOfSize& data) {
            result.append(data.size.first->cssText(), ' ', data.size.second->cssText());
            wroteSomething = true;
            appendOptionalPosition(data.position);
        },
        [&] (const EllipseOfExtent& data) {
            if (data.extent != ExtentKeyword::FarthestCorner) {
                result.append(WebCore::cssText(data.extent));
                wroteSomething = true;
            }
            appendOptionalPosition(data.position);
        },
        [&] (const CSSGradientPosition& data) {
            appendPosition(data);
        }
    );

    if (appendColorInterpolationMethod(result, m_colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        result.append(", ");

    bool wroteFirstStop = false;
    for (auto& stop : m_stops) {
        if (wroteFirstStop)
            result.append(", ");
        wroteFirstStop = true;
        writeColorStop(result, stop);
    }

    result.append(')');

    return result.toString();
}

static bool operator==(const CSSRadialGradientValue::Shape& a, const CSSRadialGradientValue::Shape& b)
{
    return a.shape == b.shape
        && a.position == b.position;
}

static bool operator==(const CSSRadialGradientValue::Extent& a, const CSSRadialGradientValue::Extent& b)
{
    return a.extent == b.extent
        && a.position == b.position;
}

static bool operator==(const CSSRadialGradientValue::Length& a, const CSSRadialGradientValue::Length& b)
{
    return compareCSSValue(a.length, b.length)
        && a.position == b.position;
}

static bool operator==(const CSSRadialGradientValue::CircleOfLength& a, const CSSRadialGradientValue::CircleOfLength& b)
{
    return compareCSSValue(a.length, b.length)
        && a.position == b.position;
}

static bool operator==(const CSSRadialGradientValue::CircleOfExtent& a, const CSSRadialGradientValue::CircleOfExtent& b)
{
    return a.extent == b.extent
        && a.position == b.position;
}

static bool operator==(const CSSRadialGradientValue::Size& a, const CSSRadialGradientValue::Size& b)
{
    return compareCSSValue(a.size.first, b.size.first)
        && compareCSSValue(a.size.second, b.size.second)
        && a.position == b.position;
}

static bool operator==(const CSSRadialGradientValue::EllipseOfSize& a, const CSSRadialGradientValue::EllipseOfSize& b)
{
    return compareCSSValue(a.size.first, b.size.first)
        && compareCSSValue(a.size.second, b.size.second)
        && a.position == b.position;
}

static bool operator==(const CSSRadialGradientValue::EllipseOfExtent& a, const CSSRadialGradientValue::EllipseOfExtent& b)
{
    return a.extent == b.extent
        && a.position == b.position;
}

bool operator==(const CSSRadialGradientValue::Data& a, const CSSRadialGradientValue::Data& b)
{
    return a.gradientBox == b.gradientBox;
}

bool CSSRadialGradientValue::equals(const CSSRadialGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
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

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "-webkit-repeating-radial-gradient(" : "-webkit-radial-gradient(");

    if (m_data.position)
        result.append(m_data.position->first->cssText(), ' ', m_data.position->second->cssText());
    else
        result.append("center");

    WTF::switchOn(m_data.gradientBox,
        [&] (std::monostate) { },
        [&] (const ShapeKeyword& shape) {
            result.append(", ", WebCore::cssText(shape), " cover");
        },
        [&] (const ExtentKeyword& extent) {
            result.append(", ellipse ", WebCore::cssText(extent));
        },
        [&] (const ShapeAndExtent& shapeAndExtent) {
            result.append(", ", WebCore::cssText(shapeAndExtent.shape), ' ', WebCore::cssText(shapeAndExtent.extent));
        },
        [&] (const MeasuredSize& measuredSize) {
            result.append(", ", measuredSize.size.first->cssText(), ' ', measuredSize.size.second->cssText());
        }
    );

    for (auto& stop : m_stops) {
        result.append(", ");
        writeColorStop(result, stop);
    }

    result.append(')');

    return result.toString();
}

static bool operator==(const CSSPrefixedRadialGradientValue::ShapeAndExtent& a, const CSSPrefixedRadialGradientValue::ShapeAndExtent& b)
{
    return a.shape == b.shape
        && a.extent == b.extent;
}
static bool operator==(const CSSPrefixedRadialGradientValue::MeasuredSize& a, const CSSPrefixedRadialGradientValue::MeasuredSize& b)
{
    return compareCSSValue(a.size.first, b.size.first)
        && compareCSSValue(a.size.second, b.size.second);
}

bool operator==(const CSSPrefixedRadialGradientValue::Data& a, const CSSPrefixedRadialGradientValue::Data& b)
{
    return a.gradientBox == b.gradientBox
        && a.position == b.position;
}

bool CSSPrefixedRadialGradientValue::equals(const CSSPrefixedRadialGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

// MARK: - Deprecated Radial.

String CSSDeprecatedRadialGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append("-webkit-gradient(radial, ",
        m_data.firstX->cssText(), ' ', m_data.firstY->cssText(), ", ", m_data.firstRadius->cssText(), ", ",
        m_data.secondX->cssText(), ' ', m_data.secondY->cssText(), ", ", m_data.secondRadius->cssText());

    appendGradientStops(result, m_stops);

    result.append(')');

    return result.toString();
}

bool operator==(const CSSDeprecatedRadialGradientValue::Data& a, const CSSDeprecatedRadialGradientValue::Data& b)
{
    return compareCSSValue(a.firstX, b.firstX)
        && compareCSSValue(a.firstY, b.firstY)
        && compareCSSValue(a.secondX, b.secondX)
        && compareCSSValue(a.secondY, b.secondY)
        && compareCSSValue(a.firstRadius, b.firstRadius)
        && compareCSSValue(a.secondRadius, b.secondRadius);
}

bool CSSDeprecatedRadialGradientValue::equals(const CSSDeprecatedRadialGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

// MARK: - Conic

String CSSConicGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append(m_repeating == CSSGradientRepeat::Repeating ? "repeating-conic-gradient(" : "conic-gradient(");

    bool wroteSomething = false;

    if (m_data.angle.value && m_data.angle.value->computeDegrees()) {
        result.append("from ", m_data.angle.value->cssText());
        wroteSomething = true;
    }

    if (m_data.position) {
        if (!m_data.position->first->isCenterPosition() || !m_data.position->second->isCenterPosition()) {
            if (wroteSomething)
                result.append(' ');
            result.append("at ", m_data.position->first->cssText(), ' ', m_data.position->second->cssText());
            wroteSomething = true;
        }
    }

    if (appendColorInterpolationMethod(result, m_colorInterpolationMethod, wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        result.append(", ");

    bool wroteFirstStop = false;
    for (auto& stop : m_stops) {
        if (wroteFirstStop)
            result.append(", ");
        wroteFirstStop = true;
        writeColorStop(result, stop);
    }

    result.append(')');
    return result.toString();
}

static bool operator==(const CSSConicGradientValue::Angle& a, const CSSConicGradientValue::Angle& b)
{
    return compareCSSValuePtr(a.value, b.value);
}

bool operator==(const CSSConicGradientValue::Data& a, const CSSConicGradientValue::Data& b)
{
    return a.angle == b.angle
        && a.position == b.position;
}

bool CSSConicGradientValue::equals(const CSSConicGradientValue& other) const
{
    return m_stops == other.m_stops
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod
        && m_data == other.m_data;
}

} // namespace WebCore
