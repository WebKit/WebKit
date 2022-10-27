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

#pragma once

#include "CSSPrimitiveValue.h"
#include "ColorInterpolationMethod.h"
#include "Gradient.h"

namespace WebCore {

struct StyleGradientImageStop;
class StyleImage;

namespace Style {
class BuilderState;
}

enum CSSGradientType {
    CSSDeprecatedLinearGradient,
    CSSDeprecatedRadialGradient,
    CSSPrefixedLinearGradient,
    CSSPrefixedRadialGradient,
    CSSLinearGradient,
    CSSRadialGradient,
    CSSConicGradient
};
enum CSSGradientRepeat { NonRepeating, Repeating };

struct CSSGradientColorStop {
    RefPtr<CSSPrimitiveValue> color;
    RefPtr<CSSPrimitiveValue> position; // percentage or length
};

inline bool operator==(const CSSGradientColorStop& a, const CSSGradientColorStop& b)
{
    return compareCSSValuePtr(a.color, b.color) && compareCSSValuePtr(a.position, b.position);
}

struct CSSGradientColorInterpolationMethod {
    enum class Default : bool { SRGB, OKLab };

    ColorInterpolationMethod method;
    Default defaultMethod;
    
    static CSSGradientColorInterpolationMethod legacyMethod(AlphaPremultiplication alphaPremultiplication)
    {
        return { { ColorInterpolationMethod::SRGB { }, alphaPremultiplication }, Default::SRGB };
    }
};

inline bool operator==(const CSSGradientColorInterpolationMethod& a, const CSSGradientColorInterpolationMethod& b)
{
    return a.method == b.method && a.defaultMethod == b.defaultMethod;
}

using CSSGradientColorStopList = Vector<CSSGradientColorStop, 2>;

class CSSGradientValue : public CSSValue {
public:
    void setFirstX(RefPtr<CSSPrimitiveValue>&& value) { m_firstX = WTFMove(value); }
    void setFirstY(RefPtr<CSSPrimitiveValue>&& value) { m_firstY = WTFMove(value); }
    void setSecondX(RefPtr<CSSPrimitiveValue>&& value) { m_secondX = WTFMove(value); }
    void setSecondY(RefPtr<CSSPrimitiveValue>&& value) { m_secondY = WTFMove(value); }

    CSSGradientType gradientType() const { return m_gradientType; }

    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

protected:
    CSSGradientValue(ClassType classType, CSSGradientRepeat repeat, CSSGradientType gradientType, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSValue(classType)
        , m_stops(WTFMove(stops))
        , m_gradientType(gradientType)
        , m_repeating(repeat == Repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSGradientValue(const CSSGradientValue& other, ClassType classType)
        : CSSValue(classType)
        , m_firstX(other.m_firstX)
        , m_firstY(other.m_firstY)
        , m_secondX(other.m_secondX)
        , m_secondY(other.m_secondY)
        , m_stops(other.m_stops)
        , m_gradientType(other.m_gradientType)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
    {
    }

    decltype(auto) computeStops(Style::BuilderState&) const;

    auto firstX() const { return m_firstX.get(); }
    auto firstY() const { return m_firstY.get(); }
    auto secondX() const { return m_secondX.get(); }
    auto secondY() const { return m_secondY.get(); }
    auto& stops() const { return m_stops; }
    bool isRepeating() const { return m_repeating; }
    auto colorInterpolationMethod() const { return m_colorInterpolationMethod; }

    bool equals(const CSSGradientValue&) const;

private:
    RefPtr<CSSPrimitiveValue> m_firstX;
    RefPtr<CSSPrimitiveValue> m_firstY;
    RefPtr<CSSPrimitiveValue> m_secondX;
    RefPtr<CSSPrimitiveValue> m_secondY;
    CSSGradientColorStopList m_stops;
    CSSGradientType m_gradientType;
    bool m_repeating { false };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
};

class CSSLinearGradientValue final : public CSSGradientValue {
public:
    static Ref<CSSLinearGradientValue> create(CSSGradientRepeat repeat, CSSGradientType gradientType, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSLinearGradientValue(repeat, gradientType, colorInterpolationMethod, WTFMove(stops)));
    }

    void setAngle(RefPtr<CSSPrimitiveValue>&& value) { m_angle = WTFMove(value); }

    String customCSSText() const;

    bool equals(const CSSLinearGradientValue&) const;

    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    CSSLinearGradientValue(CSSGradientRepeat repeat, CSSGradientType gradientType, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSGradientValue(LinearGradientClass, repeat, gradientType, colorInterpolationMethod, WTFMove(stops))
    {
    }

    CSSLinearGradientValue(const CSSLinearGradientValue& other)
        : CSSGradientValue(other, LinearGradientClass)
        , m_angle(other.m_angle)
    {
    }

    RefPtr<CSSPrimitiveValue> m_angle; // may be null.
};

class CSSRadialGradientValue final : public CSSGradientValue {
public:
    static Ref<CSSRadialGradientValue> create(CSSGradientRepeat repeat, CSSGradientType gradientType, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSRadialGradientValue(repeat, gradientType, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;

    void setFirstRadius(RefPtr<CSSPrimitiveValue>&& value) { m_firstRadius = WTFMove(value); }
    void setSecondRadius(RefPtr<CSSPrimitiveValue>&& value) { m_secondRadius = WTFMove(value); }

    void setShape(RefPtr<CSSPrimitiveValue>&& value) { m_shape = WTFMove(value); }
    void setSizingBehavior(RefPtr<CSSPrimitiveValue>&& value) { m_sizingBehavior = WTFMove(value); }

    void setEndHorizontalSize(RefPtr<CSSPrimitiveValue>&& value) { m_endHorizontalSize = WTFMove(value); }
    void setEndVerticalSize(RefPtr<CSSPrimitiveValue>&& value) { m_endVerticalSize = WTFMove(value); }

    bool equals(const CSSRadialGradientValue&) const;

    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    CSSRadialGradientValue(CSSGradientRepeat repeat, CSSGradientType gradientType, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSGradientValue(RadialGradientClass, repeat, gradientType, colorInterpolationMethod, WTFMove(stops))
    {
    }

    CSSRadialGradientValue(const CSSRadialGradientValue& other)
        : CSSGradientValue(other, RadialGradientClass)
        , m_firstRadius(other.m_firstRadius)
        , m_secondRadius(other.m_secondRadius)
        , m_shape(other.m_shape)
        , m_sizingBehavior(other.m_sizingBehavior)
        , m_endHorizontalSize(other.m_endHorizontalSize)
        , m_endVerticalSize(other.m_endVerticalSize)
    {
    }

    // These may be null for non-deprecated gradients.
    RefPtr<CSSPrimitiveValue> m_firstRadius;
    RefPtr<CSSPrimitiveValue> m_secondRadius;

    // The below are only used for non-deprecated gradients. Any of them may be null.
    RefPtr<CSSPrimitiveValue> m_shape;
    RefPtr<CSSPrimitiveValue> m_sizingBehavior;

    RefPtr<CSSPrimitiveValue> m_endHorizontalSize;
    RefPtr<CSSPrimitiveValue> m_endVerticalSize;
};

class CSSConicGradientValue final : public CSSGradientValue {
public:
    static Ref<CSSConicGradientValue> create(CSSGradientRepeat repeat, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSConicGradientValue(repeat, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;

    void setAngle(RefPtr<CSSPrimitiveValue>&& value) { m_angle = WTFMove(value); }

    bool equals(const CSSConicGradientValue&) const;

    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    explicit CSSConicGradientValue(CSSGradientRepeat repeat, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSGradientValue(ConicGradientClass, repeat, CSSConicGradient, colorInterpolationMethod, WTFMove(stops))
    {
    }

    CSSConicGradientValue(const CSSConicGradientValue& other)
        : CSSGradientValue(other, ConicGradientClass)
        , m_angle(other.m_angle)
    {
    }

    RefPtr<CSSPrimitiveValue> m_angle; // may be null.
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSGradientValue, isGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSLinearGradientValue, isLinearGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSRadialGradientValue, isRadialGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSConicGradientValue, isConicGradientValue())
