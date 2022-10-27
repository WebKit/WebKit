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

static inline std::optional<StyleColor> computeStyleColor(const RefPtr<CSSPrimitiveValue>& color, Style::BuilderState& state)
{
    if (!color)
        return std::nullopt;

    // FIXME: This should call state.colorFromPrimitiveValue(*color) instead, but doing so is
    // blocked on fixing an issue where we don't respect ::first-line in StyleImage correctly.
    // See https://webkit.org/b/247127.
    return StyleColor { state.colorFromPrimitiveValueWithResolvedCurrentColor(*color) };
}

decltype(auto) CSSGradientValue::computeStops(Style::BuilderState& state) const
{
    return m_stops.map([&] (auto& stop) -> StyleGradientImageStop {
        return { computeStyleColor(stop.color, state), stop.position };
    });
}

RefPtr<StyleImage> CSSGradientValue::createStyleImage(Style::BuilderState& state) const
{
    if (is<CSSLinearGradientValue>(*this))
        return downcast<CSSLinearGradientValue>(*this).createStyleImage(state);
    if (is<CSSRadialGradientValue>(*this))
        return downcast<CSSRadialGradientValue>(*this).createStyleImage(state);
    return downcast<CSSConicGradientValue>(*this).createStyleImage(state);
}

RefPtr<StyleImage> CSSLinearGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return StyleGradientImage::create(
        // FIXME: The parameters to LinearData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::LinearData {
            firstX(),
            firstY(),
            secondX(),
            secondY(),
            m_angle.copyRef()
        },
        isRepeating() ? CSSGradientRepeat::Repeating : CSSGradientRepeat::NonRepeating,
        gradientType(),
        colorInterpolationMethod(),
        computeStops(state)
    );
}

RefPtr<StyleImage> CSSRadialGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return StyleGradientImage::create(
        // FIXME: The parameters to RadialData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::RadialData {
            firstX(),
            firstY(),
            secondX(),
            secondY(),
            m_firstRadius.copyRef(),
            m_secondRadius.copyRef(),
            m_shape.copyRef(),
            m_sizingBehavior.copyRef(),
            m_endHorizontalSize.copyRef(),
            m_endVerticalSize.copyRef()
        },
        isRepeating() ? CSSGradientRepeat::Repeating : CSSGradientRepeat::NonRepeating,
        gradientType(),
        colorInterpolationMethod(),
        computeStops(state)
    );
}

RefPtr<StyleImage> CSSConicGradientValue::createStyleImage(Style::BuilderState& state) const
{
    return StyleGradientImage::create(
        // FIXME: The parameters to ConicData should convert down to a non-CSS specific type here (e.g. Length, double, etc.).
        StyleGradientImage::ConicData {
            firstX(),
            firstY(),
            secondX(),
            secondY(),
            m_angle.copyRef()
        },
        isRepeating() ? CSSGradientRepeat::Repeating : CSSGradientRepeat::NonRepeating,
        gradientType(),
        colorInterpolationMethod(),
        computeStops(state)
    );
}

bool CSSGradientValue::equals(const CSSGradientValue& other) const
{
    return compareCSSValuePtr(m_firstX, other.m_firstX)
        && compareCSSValuePtr(m_firstY, other.m_firstY)
        && compareCSSValuePtr(m_secondX, other.m_secondX)
        && compareCSSValuePtr(m_secondY, other.m_secondY)
        && m_stops == other.m_stops
        && m_gradientType == other.m_gradientType
        && m_repeating == other.m_repeating
        && m_colorInterpolationMethod == other.m_colorInterpolationMethod;
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
    case HueInterpolationMethod::Specified:
        builder.append(" specified hue");
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

String CSSLinearGradientValue::customCSSText() const
{
    StringBuilder result;
    if (gradientType() == CSSDeprecatedLinearGradient) {
        result.append("-webkit-gradient(linear, ", firstX()->cssText(), ' ', firstY()->cssText(), ", ", secondX()->cssText(), ' ', secondY()->cssText());
        appendGradientStops(result, stops());
    } else if (gradientType() == CSSPrefixedLinearGradient) {
        if (isRepeating())
            result.append("-webkit-repeating-linear-gradient(");
        else
            result.append("-webkit-linear-gradient(");

        if (m_angle)
            result.append(m_angle->cssText());
        else
            appendSpaceSeparatedOptionalCSSPtrText(result, firstX(), firstY());

        for (auto& stop : stops()) {
            result.append(", ");
            writeColorStop(result, stop);
        }
    } else {
        if (isRepeating())
            result.append("repeating-linear-gradient(");
        else
            result.append("linear-gradient(");

        bool wroteSomething = false;

        if (m_angle && m_angle->computeDegrees() != 180) {
            result.append(m_angle->cssText());
            wroteSomething = true;
        } else if (firstX() || (firstY() && firstY()->valueID() != CSSValueBottom)) {
            result.append("to ");
            appendSpaceSeparatedOptionalCSSPtrText(result, firstX(), firstY());
            wroteSomething = true;
        }

        if (appendColorInterpolationMethod(result, colorInterpolationMethod(), wroteSomething))
            wroteSomething = true;

        for (auto& stop : stops()) {
            if (wroteSomething)
                result.append(", ");
            wroteSomething = true;
            writeColorStop(result, stop);
        }
    }

    result.append(')');
    return result.toString();
}

bool CSSLinearGradientValue::equals(const CSSLinearGradientValue& other) const
{
    return CSSGradientValue::equals(other) && compareCSSValuePtr(m_angle, other.m_angle);
}

String CSSRadialGradientValue::customCSSText() const
{
    StringBuilder result;

    if (gradientType() == CSSDeprecatedRadialGradient) {
        result.append("-webkit-gradient(radial, ", firstX()->cssText(), ' ', firstY()->cssText(), ", ", m_firstRadius->cssText(),
            ", ", secondX()->cssText(), ' ', secondY()->cssText(), ", ", m_secondRadius->cssText());
        appendGradientStops(result, stops());
    } else if (gradientType() == CSSPrefixedRadialGradient) {
        if (isRepeating())
            result.append("-webkit-repeating-radial-gradient(");
        else
            result.append("-webkit-radial-gradient(");

        if (firstX() || firstY())
            appendSpaceSeparatedOptionalCSSPtrText(result, firstX(), firstY());
        else
            result.append("center");

        if (m_shape || m_sizingBehavior) {
            result.append(", ");
            if (m_shape)
                result.append(m_shape->cssText(), ' ');
            else
                result.append("ellipse ");
            if (m_sizingBehavior)
                result.append(m_sizingBehavior->cssText());
            else
                result.append("cover");
        } else if (m_endHorizontalSize && m_endVerticalSize)
            result.append(", ", m_endHorizontalSize->cssText(), ' ', m_endVerticalSize->cssText());

        for (auto& stop : stops()) {
            result.append(", ");
            writeColorStop(result, stop);
        }
    } else {
        if (isRepeating())
            result.append("repeating-radial-gradient(");
        else
            result.append("radial-gradient(");

        bool wroteSomething = false;

        // The only ambiguous case that needs an explicit shape to be provided
        // is when a sizing keyword is used (or all sizing is omitted).
        if (m_shape && m_shape->valueID() != CSSValueEllipse && (m_sizingBehavior || (!m_sizingBehavior && !m_endHorizontalSize))) {
            result.append("circle");
            wroteSomething = true;
        }

        if (m_sizingBehavior && m_sizingBehavior->valueID() != CSSValueFarthestCorner) {
            if (wroteSomething)
                result.append(' ');
            result.append(m_sizingBehavior->cssText());
            wroteSomething = true;
        } else if (m_endHorizontalSize) {
            if (wroteSomething)
                result.append(' ');
            result.append(m_endHorizontalSize->cssText());
            if (m_endVerticalSize)
                result.append(' ', m_endVerticalSize->cssText());
            wroteSomething = true;
        }

        if ((firstX() && !firstX()->isCenterPosition()) || (firstY() && !firstY()->isCenterPosition())) {
            if (wroteSomething)
                result.append(' ');
            result.append("at ");
            appendSpaceSeparatedOptionalCSSPtrText(result, firstX(), firstY());
            wroteSomething = true;
        }

        if (appendColorInterpolationMethod(result, colorInterpolationMethod(), wroteSomething))
            wroteSomething = true;

        if (wroteSomething)
            result.append(", ");

        bool wroteFirstStop = false;
        for (auto& stop : stops()) {
            if (wroteFirstStop)
                result.append(", ");
            wroteFirstStop = true;
            writeColorStop(result, stop);
        }
    }

    result.append(')');
    return result.toString();
}

bool CSSRadialGradientValue::equals(const CSSRadialGradientValue& other) const
{
    return CSSGradientValue::equals(other)
        && compareCSSValuePtr(m_shape, other.m_shape)
        && compareCSSValuePtr(m_sizingBehavior, other.m_sizingBehavior)
        && compareCSSValuePtr(m_endHorizontalSize, other.m_endHorizontalSize)
        && compareCSSValuePtr(m_endVerticalSize, other.m_endVerticalSize);
}

String CSSConicGradientValue::customCSSText() const
{
    StringBuilder result;

    result.append(isRepeating() ? "repeating-conic-gradient(" : "conic-gradient(");

    bool wroteSomething = false;

    if (m_angle && m_angle->computeDegrees()) {
        result.append("from ", m_angle->cssText());
        wroteSomething = true;
    }

    if ((firstX() && !firstX()->isCenterPosition()) || (firstY() && !firstY()->isCenterPosition())) {
        if (wroteSomething)
            result.append(' ');
        result.append("at ");
        appendSpaceSeparatedOptionalCSSPtrText(result, firstX(), firstY());
        wroteSomething = true;
    }

    if (appendColorInterpolationMethod(result, colorInterpolationMethod(), wroteSomething))
        wroteSomething = true;

    if (wroteSomething)
        result.append(", ");

    bool wroteFirstStop = false;
    for (auto& stop : stops()) {
        if (wroteFirstStop)
            result.append(", ");
        wroteFirstStop = true;
        writeColorStop(result, stop);
    }

    result.append(')');
    return result.toString();
}

bool CSSConicGradientValue::equals(const CSSConicGradientValue& other) const
{
    return CSSGradientValue::equals(other) && compareCSSValuePtr(m_angle, other.m_angle);
}

} // namespace WebCore
