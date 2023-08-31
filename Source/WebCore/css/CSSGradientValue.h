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

// MARK: Gradient Repeat Definitions.

enum class CSSGradientRepeat : bool { NonRepeating, Repeating };

// MARK: Gradient Color Stop Definitions.

struct CSSGradientColorStop {
    RefPtr<CSSPrimitiveValue> color;
    RefPtr<CSSPrimitiveValue> position; // percentage or length
};

inline bool operator==(const CSSGradientColorStop& a, const CSSGradientColorStop& b)
{
    return compareCSSValuePtr(a.color, b.color) && compareCSSValuePtr(a.position, b.position);
}

using CSSGradientColorStopList = Vector<CSSGradientColorStop, 2>;

// MARK: Gradient Color Interpolation Definitions.

struct CSSGradientColorInterpolationMethod {
    enum class Default : bool { SRGB, OKLab };

    ColorInterpolationMethod method;
    Default defaultMethod;
    
    static CSSGradientColorInterpolationMethod legacyMethod(AlphaPremultiplication alphaPremultiplication)
    {
        return { { ColorInterpolationMethod::SRGB { }, alphaPremultiplication }, Default::SRGB };
    }

    friend bool operator==(const CSSGradientColorInterpolationMethod&, const CSSGradientColorInterpolationMethod&) = default;
};

// MARK: Gradient Definitions.

using CSSGradientPosition = std::pair<Ref<CSSValue>, Ref<CSSValue>>;

inline bool operator==(const CSSGradientPosition& a, const CSSGradientPosition& b)
{
    return compareCSSValue(a.first, b.first) && compareCSSValue(a.second, b.second);
}

// MARK: - Linear.

class CSSLinearGradientValue final : public CSSValue {
public:
    enum class Horizontal { Left, Right };
    enum class Vertical { Top, Bottom };
    struct Angle {
        Ref<CSSPrimitiveValue> value;
        friend bool operator==(const Angle&, const Angle&);
    };
    using GradientLine = std::variant<std::monostate, Angle, Horizontal, Vertical, std::pair<Horizontal, Vertical>>;

    struct Data {
        GradientLine gradientLine;
        friend bool operator==(const Data&, const Data&) = default;
    };

    static Ref<CSSLinearGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSLinearGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    CSSLinearGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSValue(LinearGradientClass)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSLinearGradientValue(const CSSLinearGradientValue& other)
        : CSSValue(LinearGradientClass)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSPrefixedLinearGradientValue final : public CSSValue {
public:
    enum class Horizontal { Left, Right };
    enum class Vertical { Top, Bottom };
    struct Angle {
        Ref<CSSPrimitiveValue> value;
        friend bool operator==(const Angle&, const Angle&);
    };
    using GradientLine = std::variant<std::monostate, Angle, Horizontal, Vertical, std::pair<Horizontal, Vertical>>;

    struct Data {
        GradientLine gradientLine;
        friend bool operator==(const Data&, const Data&) = default;
    };

    static Ref<CSSPrefixedLinearGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSPrefixedLinearGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSPrefixedLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    CSSPrefixedLinearGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSValue(PrefixedLinearGradientClass)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSPrefixedLinearGradientValue(const CSSPrefixedLinearGradientValue& other)
        : CSSValue(PrefixedLinearGradientClass)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSDeprecatedLinearGradientValue final : public CSSValue {
public:
    struct Data {
        Ref<CSSValue> firstX;
        Ref<CSSValue> firstY;
        Ref<CSSValue> secondX;
        Ref<CSSValue> secondY;
    };

    static Ref<CSSDeprecatedLinearGradientValue> create(Data data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSDeprecatedLinearGradientValue(WTFMove(data), colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSDeprecatedLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    CSSDeprecatedLinearGradientValue(Data&& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSValue(DeprecatedLinearGradientClass)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSDeprecatedLinearGradientValue(const CSSDeprecatedLinearGradientValue& other)
        : CSSValue(DeprecatedLinearGradientClass)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

bool operator==(const CSSDeprecatedLinearGradientValue::Data&, const CSSDeprecatedLinearGradientValue::Data&);

// MARK: - Radial.

class CSSRadialGradientValue final : public CSSValue {
public:
    enum class ShapeKeyword { Circle, Ellipse };
    enum class ExtentKeyword { ClosestCorner, ClosestSide, FarthestCorner, FarthestSide };
    struct Shape {
        ShapeKeyword shape;
        std::optional<CSSGradientPosition> position;
        friend bool operator==(const Shape&, const Shape&) = default;
    };
    struct Extent {
        ExtentKeyword extent;
        std::optional<CSSGradientPosition> position;
        friend bool operator==(const Extent&, const Extent&) = default;
    };
    struct Length {
        Ref<CSSPrimitiveValue> length; // <length [0,∞]>
        std::optional<CSSGradientPosition> position;

        friend bool operator==(const Length&, const Length&);
    };
    struct CircleOfLength {
        Ref<CSSPrimitiveValue> length; // <length [0,∞]>
        std::optional<CSSGradientPosition> position;
        friend bool operator==(const CircleOfLength&, const CircleOfLength&);
    };
    struct CircleOfExtent {
        ExtentKeyword extent;
        std::optional<CSSGradientPosition> position;
        friend bool operator==(const CircleOfExtent&, const CircleOfExtent&) = default;
    };
    struct Size {
        std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>> size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
        std::optional<CSSGradientPosition> position;
        friend bool operator==(const Size&, const Size&);
    };
    struct EllipseOfSize {
        std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>> size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
        std::optional<CSSGradientPosition> position;
        friend bool operator==(const EllipseOfSize&, const EllipseOfSize&);
    };
    struct EllipseOfExtent {
        ExtentKeyword extent;
        std::optional<CSSGradientPosition> position;
        friend bool operator==(const EllipseOfExtent&, const EllipseOfExtent&) = default;
    };
    using GradientBox = std::variant<std::monostate, Shape, Extent, Length, Size, CircleOfLength, CircleOfExtent, EllipseOfSize, EllipseOfExtent, CSSGradientPosition>;
    struct Data {
        GradientBox gradientBox;
        friend bool operator==(const Data&, const Data&) = default;
    };

    static Ref<CSSRadialGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSRadialGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    CSSRadialGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSValue(RadialGradientClass)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSRadialGradientValue(const CSSRadialGradientValue& other)
        : CSSValue(RadialGradientClass)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSPrefixedRadialGradientValue final : public CSSValue {
public:
    enum class ShapeKeyword { Circle, Ellipse };
    enum class ExtentKeyword { ClosestSide, ClosestCorner, FarthestSide, FarthestCorner, Contain, Cover };
    struct ShapeAndExtent {
        ShapeKeyword shape;
        ExtentKeyword extent;
        friend bool operator==(const ShapeAndExtent&, const ShapeAndExtent&) = default;
    };
    struct MeasuredSize {
        std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>> size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
        friend bool operator==(const MeasuredSize&, const MeasuredSize&);
    };

    using GradientBox = std::variant<std::monostate, ShapeKeyword, ExtentKeyword, ShapeAndExtent, MeasuredSize>;

    struct Data {
        GradientBox gradientBox;
        std::optional<CSSGradientPosition> position;

        friend bool operator==(const Data&, const Data&) = default;
    };

    static Ref<CSSPrefixedRadialGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSPrefixedRadialGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSPrefixedRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    CSSPrefixedRadialGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSValue(PrefixedRadialGradientClass)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSPrefixedRadialGradientValue(const CSSPrefixedRadialGradientValue& other)
        : CSSValue(PrefixedRadialGradientClass)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSDeprecatedRadialGradientValue final : public CSSValue {
public:
    struct Data {
        Ref<CSSValue> firstX;
        Ref<CSSValue> firstY;
        Ref<CSSValue> secondX;
        Ref<CSSValue> secondY;
        Ref<CSSPrimitiveValue> firstRadius;
        Ref<CSSPrimitiveValue> secondRadius;
    };

    static Ref<CSSDeprecatedRadialGradientValue> create(Data data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSDeprecatedRadialGradientValue(WTFMove(data), colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSDeprecatedRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    CSSDeprecatedRadialGradientValue(Data&& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSValue(DeprecatedRadialGradientClass)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSDeprecatedRadialGradientValue(const CSSDeprecatedRadialGradientValue& other)
        : CSSValue(DeprecatedRadialGradientClass)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

bool operator==(const CSSDeprecatedRadialGradientValue::Data&, const CSSDeprecatedRadialGradientValue::Data&);

// MARK: - Conic.

class CSSConicGradientValue final : public CSSValue {
public:
    struct Angle {
        RefPtr<CSSPrimitiveValue> value;
        friend bool operator==(const Angle&, const Angle&);
    };

    struct Data {
        Angle angle;
        std::optional<CSSGradientPosition> position;

        friend bool operator==(const Data&, const Data&) = default;
    };

    static Ref<CSSConicGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSConicGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSConicGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

private:
    explicit CSSConicGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
        : CSSValue(ConicGradientClass)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSConicGradientValue(const CSSConicGradientValue& other)
        : CSSValue(ConicGradientClass)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSLinearGradientValue, isLinearGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSRadialGradientValue, isRadialGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSConicGradientValue, isConicGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSDeprecatedLinearGradientValue, isDeprecatedLinearGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSDeprecatedRadialGradientValue, isDeprecatedRadialGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPrefixedLinearGradientValue, isPrefixedLinearGradientValue())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPrefixedRadialGradientValue, isPrefixedRadialGradientValue())
