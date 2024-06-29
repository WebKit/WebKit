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

#include "CSSCalcValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+RawTypes.h"
#include "CSSPropertyParserConsumer+UnevaluatedCalc.h"
#include "ColorInterpolationMethod.h"
#include "Gradient.h"

namespace WebCore {

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

    bool operator==(const CSSGradientColorStop&) const;
};

inline bool CSSGradientColorStop::operator==(const CSSGradientColorStop& other) const
{
    return compareCSSValuePtr(color, other.color) && compareCSSValuePtr(position, other.position);
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

    bool operator==(const CSSGradientColorInterpolationMethod&) const = default;
};

// MARK: Gradient Definitions.

struct CSSGradientPosition {
    Ref<CSSValue> x;
    Ref<CSSValue> y;

    bool operator==(const CSSGradientPosition& other) const
    {
        return compareCSSValue(x, other.x) && compareCSSValue(y, other.y);
    }
};

struct CSSGradientDeprecatedPoint {
    Ref<CSSPrimitiveValue> x;
    Ref<CSSPrimitiveValue> y;

    bool operator==(const CSSGradientDeprecatedPoint& other) const
    {
        return compareCSSValue(x, other.x) && compareCSSValue(y, other.y);
    }
};

// MARK: - Linear.

class CSSLinearGradientValue final : public CSSValue {
public:
    enum class Horizontal : bool { Left, Right };
    enum class Vertical : bool { Top, Bottom };
    using GradientLine = std::variant<std::monostate, AngleRaw, UnevaluatedCalc<AngleRaw>, Horizontal, Vertical, std::pair<Horizontal, Vertical>>;

    struct Data {
        GradientLine gradientLine;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSLinearGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSLinearGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        {
            auto result = WTF::switchOn(m_data.gradientLine,
                [&](UnevaluatedCalc<AngleRaw>& data) {
                    if (func(data.calc.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const auto&) {
                    return IterationStatus::Continue;
                });
            if (result == IterationStatus::Done)
                return IterationStatus::Done;
        }
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (stop.position) {
                if (func(*stop.position) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
        }
        return IterationStatus::Continue;
    }

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

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSPrefixedLinearGradientValue final : public CSSValue {
public:
    enum class Horizontal : bool { Left, Right };
    enum class Vertical : bool { Top, Bottom };
    using GradientLine = std::variant<std::monostate, AngleRaw, UnevaluatedCalc<AngleRaw>, Horizontal, Vertical, std::pair<Horizontal, Vertical>>;

    struct Data {
        GradientLine gradientLine;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSPrefixedLinearGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSPrefixedLinearGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSPrefixedLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        {
            auto result = WTF::switchOn(m_data.gradientLine,
                [&](UnevaluatedCalc<AngleRaw>& data) {
                    if (func(data.calc.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const auto&) {
                    return IterationStatus::Continue;
                });
            if (result == IterationStatus::Done)
                return IterationStatus::Done;
        }
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (stop.position) {
                if (func(*stop.position) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
        }
        return IterationStatus::Continue;
    }

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

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSDeprecatedLinearGradientValue final : public CSSValue {
public:
    struct Data {
        CSSGradientDeprecatedPoint first;
        CSSGradientDeprecatedPoint second;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSDeprecatedLinearGradientValue> create(Data data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSDeprecatedLinearGradientValue(WTFMove(data), colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSDeprecatedLinearGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (func(m_data.first.x.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (func(m_data.first.y.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (func(m_data.second.x.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (func(m_data.second.y.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (stop.position) {
                if (func(*stop.position) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
        }
        return IterationStatus::Continue;
    }

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

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientColorStopList m_stops;
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
        std::optional<CSSGradientPosition> position;
        bool operator==(const Shape&) const = default;
    };
    struct Extent {
        ExtentKeyword extent;
        std::optional<CSSGradientPosition> position;
        bool operator==(const Extent&) const = default;
    };
    struct Length {
        Ref<CSSPrimitiveValue> length; // <length [0,∞]>
        std::optional<CSSGradientPosition> position;
        bool operator==(const Length&) const;
    };
    struct CircleOfLength {
        Ref<CSSPrimitiveValue> length; // <length [0,∞]>
        std::optional<CSSGradientPosition> position;
        bool operator==(const CircleOfLength&) const;
    };
    struct CircleOfExtent {
        ExtentKeyword extent;
        std::optional<CSSGradientPosition> position;
        bool operator==(const CircleOfExtent&) const = default;
    };
    struct Size {
        std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>> size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
        std::optional<CSSGradientPosition> position;
        bool operator==(const Size&) const;
    };
    struct EllipseOfSize {
        std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>> size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
        std::optional<CSSGradientPosition> position;
        bool operator==(const EllipseOfSize&) const;
    };
    struct EllipseOfExtent {
        ExtentKeyword extent;
        std::optional<CSSGradientPosition> position;
        bool operator==(const EllipseOfExtent&) const = default;
    };
    using GradientBox = std::variant<std::monostate, Shape, Extent, Length, Size, CircleOfLength, CircleOfExtent, EllipseOfSize, EllipseOfExtent, CSSGradientPosition>;

    struct Data {
        GradientBox gradientBox;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSRadialGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
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
                    if (data.position) {
                        if (func(data.position->x.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                        if (func(data.position->y.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                    }
                    return IterationStatus::Continue;
                },
                [&](const Extent& data) {
                    if (data.position) {
                        if (func(data.position->x.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                        if (func(data.position->y.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                    }
                    return IterationStatus::Continue;
                },
                [&](const Length& data) {
                    if (func(data.length.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (data.position) {
                        if (func(data.position->x.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                        if (func(data.position->y.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                    }
                    return IterationStatus::Continue;
                },
                [&](const Size& data) {
                    if (func(data.size.first.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (func(data.size.second.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (data.position) {
                        if (func(data.position->x.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                        if (func(data.position->y.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                    }
                    return IterationStatus::Continue;
                },
                [&](const CircleOfLength& data) {
                    if (func(data.length.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (data.position) {
                        if (func(data.position->x.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                        if (func(data.position->y.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                    }
                    return IterationStatus::Continue;
                },
                [&](const CircleOfExtent& data) {
                    if (data.position) {
                        if (func(data.position->x.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                        if (func(data.position->y.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                    }
                    return IterationStatus::Continue;
                },
                [&](const EllipseOfSize& data) {
                    if (func(data.size.first.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (func(data.size.second.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (data.position) {
                        if (func(data.position->x.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                        if (func(data.position->y.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                    }
                    return IterationStatus::Continue;
                },
                [&](const EllipseOfExtent& data) {
                    if (data.position) {
                        if (func(data.position->x.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                        if (func(data.position->y.get()) == IterationStatus::Done)
                            return IterationStatus::Done;
                    }
                    return IterationStatus::Continue;
                },
                [&](const CSSGradientPosition& data) {
                    if (func(data.x.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (func(data.y.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const auto&) {
                    return IterationStatus::Continue;
                });
            if (result == IterationStatus::Done)
                return IterationStatus::Done;
        }
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (stop.position) {
                if (func(*stop.position) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
        }
        return IterationStatus::Continue;
    }

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

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientColorStopList m_stops;
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
        std::pair<Ref<CSSPrimitiveValue>, Ref<CSSPrimitiveValue>> size; // <length-percentage [0,∞]>, <length-percentage [0,∞]>
        bool operator==(const MeasuredSize&) const;
    };

    using GradientBox = std::variant<std::monostate, ShapeKeyword, ExtentKeyword, ShapeAndExtent, MeasuredSize>;

    struct Data {
        GradientBox gradientBox;
        std::optional<CSSGradientPosition> position;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSPrefixedRadialGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
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
                    if (func(data.size.first.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    if (func(data.size.second.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const auto&) {
                    return IterationStatus::Continue;
                });
            if (result == IterationStatus::Done)
                return IterationStatus::Done;
        }
        if (m_data.position) {
            if (func(m_data.position->x.get()) == IterationStatus::Done)
                return IterationStatus::Done;
            if (func(m_data.position->y.get()) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (stop.position) {
                if (func(*stop.position) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
        }
        return IterationStatus::Continue;
    }

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

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientRepeat m_repeating { CSSGradientRepeat::NonRepeating };
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

class CSSDeprecatedRadialGradientValue final : public CSSValue {
public:
    struct Data {
        CSSGradientDeprecatedPoint first;
        CSSGradientDeprecatedPoint second;
        std::variant<NumberRaw, UnevaluatedCalc<NumberRaw>> firstRadius; // <number [0,∞]>
        std::variant<NumberRaw, UnevaluatedCalc<NumberRaw>> secondRadius; // <number [0,∞]>
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSDeprecatedRadialGradientValue> create(Data data, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSDeprecatedRadialGradientValue(WTFMove(data), colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSDeprecatedRadialGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        if (func(m_data.first.x.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (func(m_data.first.y.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (func(m_data.second.x.get()) == IterationStatus::Done)
            return IterationStatus::Done;
        if (func(m_data.second.y.get()) == IterationStatus::Done)
            return IterationStatus::Done;

        {
            auto result = WTF::switchOn(m_data.firstRadius,
                [&](const UnevaluatedCalc<NumberRaw>& data) {
                    if (func(data.calc.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const auto&) {
                    return IterationStatus::Continue;
                });
            if (result == IterationStatus::Done)
                return IterationStatus::Done;
        }

        {
            auto result = WTF::switchOn(m_data.secondRadius,
                [&](const UnevaluatedCalc<NumberRaw>& data) {
                    if (func(data.calc.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const auto&) {
                    return IterationStatus::Continue;
                });
            if (result == IterationStatus::Done)
                return IterationStatus::Done;
        }

        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (stop.position) {
                if (func(*stop.position) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
        }
        return IterationStatus::Continue;
    }

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

    bool styleImageIsUncacheable() const;

    Data m_data;
    CSSGradientColorStopList m_stops;
    CSSGradientColorInterpolationMethod m_colorInterpolationMethod;
    mutable RefPtr<StyleImage> m_cachedStyleImage;
};

// MARK: - Conic.

class CSSConicGradientValue final : public CSSValue {
public:
    using Angle = std::variant<std::monostate, AngleRaw, UnevaluatedCalc<AngleRaw>>;

    struct Data {
        Angle angle;
        std::optional<CSSGradientPosition> position;
        bool operator==(const Data&) const = default;
    };

    static Ref<CSSConicGradientValue> create(Data data, CSSGradientRepeat repeating, CSSGradientColorInterpolationMethod colorInterpolationMethod, CSSGradientColorStopList stops)
    {
        return adoptRef(*new CSSConicGradientValue(WTFMove(data), repeating, colorInterpolationMethod, WTFMove(stops)));
    }

    String customCSSText() const;
    bool equals(const CSSConicGradientValue&) const;
    RefPtr<StyleImage> createStyleImage(Style::BuilderState&) const;

    IterationStatus customVisitChildren(const Function<IterationStatus(CSSValue&)>& func) const
    {
        {
            auto result = WTF::switchOn(m_data.angle,
                [&](UnevaluatedCalc<AngleRaw>& data) {
                    if (func(data.calc.get()) == IterationStatus::Done)
                        return IterationStatus::Done;
                    return IterationStatus::Continue;
                },
                [&](const auto&) {
                    return IterationStatus::Continue;
                });
            if (result == IterationStatus::Done)
                return IterationStatus::Done;
        }
        if (m_data.position) {
            if (func(m_data.position->x.get()) == IterationStatus::Done)
                return IterationStatus::Done;
            if (func(m_data.position->y.get()) == IterationStatus::Done)
                return IterationStatus::Done;
        }
        for (auto& stop : m_stops) {
            if (stop.color) {
                if (func(*stop.color) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
            if (stop.position) {
                if (func(*stop.position) == IterationStatus::Done)
                    return IterationStatus::Done;
            }
        }
        return IterationStatus::Continue;
    }

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

    bool styleImageIsUncacheable() const;

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
