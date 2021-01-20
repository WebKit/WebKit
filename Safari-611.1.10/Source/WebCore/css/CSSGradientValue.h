/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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

#include "CSSImageGeneratorValue.h"
#include "CSSPrimitiveValue.h"
#include "Gradient.h"

namespace WebCore {

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
    Color resolvedColor;
};

inline bool operator==(const CSSGradientColorStop& a, const CSSGradientColorStop& b)
{
    return compareCSSValuePtr(a.color, b.color) && compareCSSValuePtr(a.position, b.position);
}

class CSSGradientValue : public CSSImageGeneratorValue {
public:
    void setFirstX(RefPtr<CSSPrimitiveValue>&& value) { m_firstX = WTFMove(value); }
    void setFirstY(RefPtr<CSSPrimitiveValue>&& value) { m_firstY = WTFMove(value); }
    void setSecondX(RefPtr<CSSPrimitiveValue>&& value) { m_secondX = WTFMove(value); }
    void setSecondY(RefPtr<CSSPrimitiveValue>&& value) { m_secondY = WTFMove(value); }
    void addStop(CSSGradientColorStop&& stop) { m_stops.append(WTFMove(stop)); }
    void doneAddingStops() { m_stops.shrinkToFit(); }
    bool hasAtLeastTwoStops() const { return m_stops.size() >= 2; }
    void resolveRGBColors();

    CSSGradientType gradientType() const { return m_gradientType; }

    RefPtr<Image> image(RenderElement&, const FloatSize&);
    bool knownToBeOpaque(const RenderElement&) const;

    static constexpr bool isFixedSize() { return false; }
    static FloatSize fixedSize(const RenderElement&) { return FloatSize(); }
    static constexpr bool isPending() { return false; }
    static void loadSubimages(CachedResourceLoader&, const ResourceLoaderOptions&) { }

    Ref<CSSGradientValue> gradientWithStylesResolved(Style::BuilderState&);

protected:
    CSSGradientValue(ClassType classType, CSSGradientRepeat repeat, CSSGradientType gradientType)
        : CSSImageGeneratorValue(classType)
        , m_gradientType(gradientType)
        , m_repeating(repeat == Repeating)
    {
    }

    CSSGradientValue(const CSSGradientValue& other, ClassType classType)
        : CSSImageGeneratorValue(classType)
        , m_firstX(other.m_firstX)
        , m_firstY(other.m_firstY)
        , m_secondX(other.m_secondX)
        , m_secondY(other.m_secondY)
        , m_stops(other.m_stops)
        , m_gradientType(other.m_gradientType)
        , m_repeating(other.m_repeating)
        , m_hasColorDerivedFromElement(other.m_hasColorDerivedFromElement)
    {
    }

    template<typename GradientAdapter> Gradient::ColorStopVector computeStops(GradientAdapter&, const CSSToLengthConversionData&, const RenderStyle&, float maxLengthForRepeat);

    auto firstX() const { return m_firstX.get(); }
    auto firstY() const { return m_firstY.get(); }
    auto secondX() const { return m_secondX.get(); }
    auto secondY() const { return m_secondY.get(); }
    auto& stops() const { return m_stops; }
    bool isRepeating() const { return m_repeating; }

    bool equals(const CSSGradientValue&) const;

private:
    bool hasColorDerivedFromElement() const;
    bool isCacheable() const;

    RefPtr<CSSPrimitiveValue> m_firstX;
    RefPtr<CSSPrimitiveValue> m_firstY;
    RefPtr<CSSPrimitiveValue> m_secondX;
    RefPtr<CSSPrimitiveValue> m_secondY;
    Vector<CSSGradientColorStop, 2> m_stops;
    CSSGradientType m_gradientType;
    bool m_repeating { false };

    mutable Optional<bool> m_hasColorDerivedFromElement;
};

class CSSLinearGradientValue final : public CSSGradientValue {
public:
    static Ref<CSSLinearGradientValue> create(CSSGradientRepeat repeat, CSSGradientType gradientType = CSSLinearGradient)
    {
        return adoptRef(*new CSSLinearGradientValue(repeat, gradientType));
    }

    void setAngle(Ref<CSSPrimitiveValue>&& value) { m_angle = WTFMove(value); }

    String customCSSText() const;

    // Create the gradient for a given size.
    Ref<Gradient> createGradient(RenderElement&, const FloatSize&);

    Ref<CSSLinearGradientValue> clone() const
    {
        return adoptRef(*new CSSLinearGradientValue(*this));
    }

    bool equals(const CSSLinearGradientValue&) const;

private:
    CSSLinearGradientValue(CSSGradientRepeat repeat, CSSGradientType gradientType = CSSLinearGradient)
        : CSSGradientValue(LinearGradientClass, repeat, gradientType)
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
    static Ref<CSSRadialGradientValue> create(CSSGradientRepeat repeat, CSSGradientType gradientType = CSSRadialGradient)
    {
        return adoptRef(*new CSSRadialGradientValue(repeat, gradientType));
    }

    Ref<CSSRadialGradientValue> clone() const
    {
        return adoptRef(*new CSSRadialGradientValue(*this));
    }

    String customCSSText() const;

    void setFirstRadius(RefPtr<CSSPrimitiveValue>&& value) { m_firstRadius = WTFMove(value); }
    void setSecondRadius(RefPtr<CSSPrimitiveValue>&& value) { m_secondRadius = WTFMove(value); }

    void setShape(RefPtr<CSSPrimitiveValue>&& value) { m_shape = WTFMove(value); }
    void setSizingBehavior(RefPtr<CSSPrimitiveValue>&& value) { m_sizingBehavior = WTFMove(value); }

    void setEndHorizontalSize(RefPtr<CSSPrimitiveValue>&& value) { m_endHorizontalSize = WTFMove(value); }
    void setEndVerticalSize(RefPtr<CSSPrimitiveValue>&& value) { m_endVerticalSize = WTFMove(value); }

    // Create the gradient for a given size.
    Ref<Gradient> createGradient(RenderElement&, const FloatSize&);

    bool equals(const CSSRadialGradientValue&) const;

private:
    CSSRadialGradientValue(CSSGradientRepeat repeat, CSSGradientType gradientType = CSSRadialGradient)
        : CSSGradientValue(RadialGradientClass, repeat, gradientType)
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

    // Resolve points/radii to front end values.
    float resolveRadius(CSSPrimitiveValue&, const CSSToLengthConversionData&, float* widthOrHeight = 0);

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
    static Ref<CSSConicGradientValue> create(CSSGradientRepeat repeat)
    {
        return adoptRef(*new CSSConicGradientValue(repeat));
    }

    Ref<CSSConicGradientValue> clone() const
    {
        return adoptRef(*new CSSConicGradientValue(*this));
    }

    String customCSSText() const;

    void setAngle(RefPtr<CSSPrimitiveValue>&& value) { m_angle = WTFMove(value); }

    // Create the gradient for a given size.
    Ref<Gradient> createGradient(RenderElement&, const FloatSize&);

    bool equals(const CSSConicGradientValue&) const;

private:
    explicit CSSConicGradientValue(CSSGradientRepeat repeat)
        : CSSGradientValue(ConicGradientClass, repeat, CSSConicGradient)
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
