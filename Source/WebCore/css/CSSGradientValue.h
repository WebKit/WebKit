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

#include "CSSPosition.h"
#include "CSSPrimitiveNumericTypes.h"
#include "CSSPrimitiveValue.h"
#include "CSSValueTypes.h"
#include "ColorInterpolationMethod.h"
#include "Gradient.h"
#include "StyleImage.h"

namespace WebCore {

namespace Style {
class BuilderState;
}

// MARK: Gradient Repeat Definitions.

enum class CSSGradientRepeat : bool { NonRepeating, Repeating };

// MARK: Gradient Color Stop Definitions.

template<typename T> struct CSSGradientColorStop {
    using Position = T;

    RefPtr<CSSPrimitiveValue> color;
    Position position;

    bool operator==(const CSSGradientColorStop<T>&) const;
};

template<typename T> inline bool CSSGradientColorStop<T>::operator==(const CSSGradientColorStop<Position>& other) const
{
    return compareCSSValuePtr(color, other.color) && position == other.position;
}

template<typename Stop> using CSSGradientColorStopList = Vector<Stop, 2>;

using CSSGradientAngularColorStopPosition = std::optional<CSS::AnglePercentage>;
using CSSGradientAngularColorStop = CSSGradientColorStop<CSSGradientAngularColorStopPosition>;
using CSSGradientAngularColorStopList = CSSGradientColorStopList<CSSGradientAngularColorStop>;

using CSSGradientLinearColorStopPosition = std::optional<CSS::LengthPercentage>;
using CSSGradientLinearColorStop = CSSGradientColorStop<CSSGradientLinearColorStopPosition>;
using CSSGradientLinearColorStopList = CSSGradientColorStopList<CSSGradientLinearColorStop>;

using CSSGradientDeprecatedColorStopPosition = CSS::PercentageOrNumber;
using CSSGradientDeprecatedColorStop = CSSGradientColorStop<CSSGradientDeprecatedColorStopPosition>;
using CSSGradientDeprecatedColorStopList = CSSGradientColorStopList<CSSGradientDeprecatedColorStop>;

// MARK: Gradient Color Interpolation Definitions.

struct CSSGradientColorInterpolationMethod {
    enum class Default : bool { SRGB, OKLab };

    ColorInterpolationMethod method;
    Default defaultMethod;
    
    static CSSGradientColorInterpolationMethod legacyMethod(AlphaPremultiplication alphaPremultiplication)
    {
        return { { ColorInterpolationMethod::SRGB { }, alphaPremultiplication }, Default::SRGB };
    }

    bool operator==(const CSSGradientColorInterpolationMethod&) const = default;
};

// MARK: Gradient Definitions.

using CSSDeprecatedGradientPosition = CSS::SpaceSeparatedTuple<CSS::PercentageOrNumber, CSS::PercentageOrNumber>;

// MARK: - Linear.

class CSSLinearGradientValue final : public CSSValue {
public:
    enum class Horizontal : bool { Left, Right };
    enum class Vertical : bool { Top, Bottom };
    using GradientLine = std::variant<std::monostate, CSS::Angle, Horizontal, Vertical, std::pair<Horizontal, Vertical>>;

    struct Data {
        GradientLine gradientLine;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSLinearGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientLinearColorStopList stops)
    {
        return adoptRef(*new CSSLinearGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (CSS::visitCSSValueChildren(m_data.gradientLine, func) == IterationStatus::Done)
            return IterationStatus::Done;
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (CSS::visitCSSValueChildren(stop.position, func) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

private:
    CSSLinearGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientLinearColorStopList stops)
        : CSSValue(ClassType::LinearGradient)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSLinearGradientValue(const CSSLinearGradientValue& other)
        : CSSValue(ClassType::LinearGradient)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientLinearColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSPrefixedLinearGradientValue final : public CSSValue {
public:
    enum class Horizontal : bool { Left, Right };
    enum class Vertical : bool { Top, Bottom };
    using GradientLine = std::variant<std::monostate, CSS::Angle, Horizontal, Vertical, std::pair<Horizontal, Vertical>>;

    struct Data {
        GradientLine gradientLine;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSPrefixedLinearGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientLinearColorStopList stops)
    {
        return adoptRef(*new CSSPrefixedLinearGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSPrefixedLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (CSS::visitCSSValueChildren(m_data.gradientLine, func) == IterationStatus::Done)
            return IterationStatus::Done;
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (CSS::visitCSSValueChildren(stop.position, func) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

private:
    CSSPrefixedLinearGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientLinearColorStopList stops)
        : CSSValue(ClassType::PrefixedLinearGradient)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSPrefixedLinearGradientValue(const CSSPrefixedLinearGradientValue& other)
        : CSSValue(ClassType::PrefixedLinearGradient)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientLinearColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSDeprecatedLinearGradientValue final : public CSSValue {
public:
    struct Data {
        CSSDeprecatedGradientPosition first;
        CSSDeprecatedGradientPosition second;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSDeprecatedLinearGradientValue> create(Data data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientDeprecatedColorStopList stops)
    {
        return adoptRef(*new CSSDeprecatedLinearGradientValue(WTFMove(data), colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSDeprecatedLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (CSS::visitCSSValueChildren(m_data.first, func) == IterationStatus::Done)
            return IterationStatus::Done;
        if (CSS::visitCSSValueChildren(m_data.second, func) == IterationStatus::Done)
            return IterationStatus::Done;
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (CSS::visitCSSValueChildren(stop.position, func) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

private:
    CSSDeprecatedLinearGradientValue(Data&& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientDeprecatedColorStopList stops)
        : CSSValue(ClassType::DeprecatedLinearGradient)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSDeprecatedLinearGradientValue(const CSSDeprecatedLinearGradientValue& other)
        : CSSValue(ClassType::DeprecatedLinearGradient)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientDeprecatedColorStopList m_stops;
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

// MARK: - Radial.

class CSSRadialGradientValue final : public CSSValue {
public:
    enum class ShapeKeyword : bool { Circle, Ellipse };
    enum class ExtentKeyword : uint8_t { ClosestCorner, ClosestSide, FarthestCorner, FarthestSide };
    struct Shape {
        ShapeKeyword shape;
        std::optional<CSS::Position> position;
        bool operator==(const Shape&) const = default;
    };
    struct Extent {
        ExtentKeyword extent;
        std::optional<CSS::Position> position;
        bool operator==(const Extent&) const = default;
    };
    struct Length {
        CSS::Length length; // <length [0,∞]>
        std::optional<CSS::Position> position;
        bool operator==(const Length&) const = default;
    };
    struct CircleOfLength {
        CSS::Length length; // <length [0,∞]>
        std::optional<CSS::Position> position;
        bool operator==(const CircleOfLength&) const = default;
    };
    struct CircleOfExtent {
        ExtentKeyword extent;
        std::optional<CSS::Position> position;
        bool operator==(const CircleOfExtent&) const = default;
    };
    struct Size {
        CSS::SpaceSeparatedTuple<CSS::LengthPercentage, CSS::LengthPercentage> size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
        std::optional<CSS::Position> position;
        bool operator==(const Size&) const = default;
    };
    struct EllipseOfSize {
        CSS::SpaceSeparatedTuple<CSS::LengthPercentage, CSS::LengthPercentage> size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
        std::optional<CSS::Position> position;
        bool operator==(const EllipseOfSize&) const = default;
    };
    struct EllipseOfExtent {
        ExtentKeyword extent;
        std::optional<CSS::Position> position;
        bool operator==(const EllipseOfExtent&) const = default;
    };
    using GradientBox = std::variant<std::monostate, Shape, Extent, Length, Size, CircleOfLength, CircleOfExtent, EllipseOfSize, EllipseOfExtent, CSS::Position>;

    struct Data {
        GradientBox gradientBox;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSRadialGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientLinearColorStopList stops)
    {
        return adoptRef(*new CSSRadialGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        {
            auto result = WTF::switchOn(m_data.gradientBox,
                [&](const Shape& data) {
                    return CSS::visitCSSValueChildren(data.position, func);
                },
                [&](const Extent& data) {
                    return CSS::visitCSSValueChildren(data.position, func);
                },
                [&](const Length& data) {
                    if (CSS::visitCSSValueChildren(data.length, func) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (CSS::visitCSSValueChildren(data.position, func) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const Size& data) {
                    if (CSS::visitCSSValueChildren(data.size, func) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (CSS::visitCSSValueChildren(data.position, func) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const CircleOfLength& data) {
                    if (CSS::visitCSSValueChildren(data.length, func) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (CSS::visitCSSValueChildren(data.position, func) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const CircleOfExtent& data) {
                    return CSS::visitCSSValueChildren(data.position, func);
                },
                [&](const EllipseOfSize& data) {
                    if (CSS::visitCSSValueChildren(data.size, func) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (CSS::visitCSSValueChildren(data.position, func) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const EllipseOfExtent& data) {
                    return CSS::visitCSSValueChildren(data.position, func);
                },
                [&](const CSS::Position& data) {
                    return CSS::visitCSSValueChildren(data, func);
                },
                [&](const auto&) {
                    return IterationStatus::Continue;
                }
            );
            if (result == IterationStatus::Done)
                return IterationStatus::Done;
        }
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (CSS::visitCSSValueChildren(stop.position, func) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

private:
    CSSRadialGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientLinearColorStopList stops)
        : CSSValue(ClassType::RadialGradient)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSRadialGradientValue(const CSSRadialGradientValue& other)
        : CSSValue(ClassType::RadialGradient)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientLinearColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSPrefixedRadialGradientValue final : public CSSValue {
public:
    enum class ShapeKeyword : bool { Circle, Ellipse };
    enum class ExtentKeyword : uint8_t { ClosestSide, ClosestCorner, FarthestSide, FarthestCorner, Contain, Cover };
    struct ShapeAndExtent {
        ShapeKeyword shape;
        ExtentKeyword extent;
        bool operator==(const ShapeAndExtent&) const = default;
    };
    struct MeasuredSize {
        CSS::SpaceSeparatedTuple<CSS::LengthPercentage, CSS::LengthPercentage> size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
        bool operator==(const MeasuredSize&) const = default;
    };

    using GradientBox = std::variant<std::monostate, ShapeKeyword, ExtentKeyword, ShapeAndExtent, MeasuredSize>;

    struct Data {
        GradientBox gradientBox;
        std::optional<CSS::Position> position;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSPrefixedRadialGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientLinearColorStopList stops)
    {
        return adoptRef(*new CSSPrefixedRadialGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSPrefixedRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        {
            auto result = WTF::switchOn(m_data.gradientBox,
                [&](const MeasuredSize& data) {
                    return CSS::visitCSSValueChildren(data.size, func);
                },
                [&](const auto&) {
                    return IterationStatus::Continue;
                }
            );
            if (result == IterationStatus::Done)
                return IterationStatus::Done;
        }
        if (CSS::visitCSSValueChildren(m_data.position, func) == IterationStatus::Done)
            return IterationStatus::Done;
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (CSS::visitCSSValueChildren(stop.position, func) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

private:
    CSSPrefixedRadialGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientLinearColorStopList stops)
        : CSSValue(ClassType::PrefixedRadialGradient)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSPrefixedRadialGradientValue(const CSSPrefixedRadialGradientValue& other)
        : CSSValue(ClassType::PrefixedRadialGradient)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientLinearColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSDeprecatedRadialGradientValue final : public CSSValue {
public:
    struct Data {
        CSSDeprecatedGradientPosition first;
        CSSDeprecatedGradientPosition second;
        CSS::Number firstRadius; // <number [0,∞]>
        CSS::Number secondRadius; // <number [0,∞]>
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSDeprecatedRadialGradientValue> create(Data data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientDeprecatedColorStopList stops)
    {
        return adoptRef(*new CSSDeprecatedRadialGradientValue(WTFMove(data), colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSDeprecatedRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (CSS::visitCSSValueChildren(m_data.first, func) == IterationStatus::Done)
            return IterationStatus::Done;
        if (CSS::visitCSSValueChildren(m_data.second, func) == IterationStatus::Done)
            return IterationStatus::Done;
        if (CSS::visitCSSValueChildren(m_data.firstRadius, func) == IterationStatus::Done)
            return IterationStatus::Done;
        if (CSS::visitCSSValueChildren(m_data.secondRadius, func) == IterationStatus::Done)
            return IterationStatus::Done;

        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (CSS::visitCSSValueChildren(stop.position, func) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

private:
    CSSDeprecatedRadialGradientValue(Data&& data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientDeprecatedColorStopList stops)
        : CSSValue(ClassType::DeprecatedRadialGradient)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSDeprecatedRadialGradientValue(const CSSDeprecatedRadialGradientValue& other)
        : CSSValue(ClassType::DeprecatedRadialGradient)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientDeprecatedColorStopList m_stops;
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

// MARK: - Conic.

class CSSConicGradientValue final : public CSSValue {
public:
    struct Data {
        std::optional<CSS::Angle> angle;
        std::optional<CSS::Position> position;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSConicGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientAngularColorStopList stops)
    {
        return adoptRef(*new CSSConicGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSConicGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (CSS::visitCSSValueChildren(m_data.angle, func) == IterationStatus::Done)
            return IterationStatus::Done;
        if (CSS::visitCSSValueChildren(m_data.position, func) == IterationStatus::Done)
            return IterationStatus::Done;
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (CSS::visitCSSValueChildren(stop.position, func) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        return IterationStatus::Continue;
    }

private:
    explicit CSSConicGradientValue(Data&& data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientAngularColorStopList stops)
        : CSSValue(ClassType::ConicGradient)
        , m_data(WTFMove(data))
        , m_stops(WTFMove(stops))
        , m_repeating(repeating)
        , m_colorInterpolationMethod(colorInterpolationMethod)
    {
    }

    CSSConicGradientValue(const CSSConicGradientValue& other)
        : CSSValue(ClassType::ConicGradient)
        , m_data(other.m_data)
        , m_stops(other.m_stops)
        , m_repeating(other.m_repeating)
        , m_colorInterpolationMethod(other.m_colorInterpolationMethod)
        , m_cachedStyleImage(other.m_cachedStyleImage)
    {
    }

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientAngularColorStopList m_stops;
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
