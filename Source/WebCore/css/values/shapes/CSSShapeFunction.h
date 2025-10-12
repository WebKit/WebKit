/*
 * Copyright (C) 2024 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/CSSFillRule.h>
#include <WebCore/CSSPosition.h>
#include <WebCore/CSSPrimitiveNumericTypes.h>

namespace WebCore {
namespace CSS {

// <coordinate-pair> = <length-percentage>{2}
using CoordinatePair = SpaceSeparatedPoint<LengthPercentage<>>;

// <by-to> = by | to
// https://drafts.csswg.org/css-shapes-2/#typedef-shape-by-to
// Indicates if a command is relative or absolute.
using CommandAffinity = Variant<Keyword::By, Keyword::To>;

// <arc-sweep> = cw | ccw
using ArcSweep        = Variant<Keyword::Cw, Keyword::Ccw>;

// <arc-size> = large | small
using ArcSize         = Variant<Keyword::Large, Keyword::Small>;

// <control-point-anchor> = start | end | origin
using ControlPointAnchor = Variant<Keyword::Start, Keyword::End, Keyword::Origin>;

// <to-position> = to <position>
struct ToPosition {
    static constexpr auto affinity = Keyword::To { };

    Position offset;

    bool operator==(const ToPosition&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ToPosition, offset);

template<> struct Serialize<ToPosition> { void operator()(StringBuilder&, const SerializationContext&, const ToPosition&); };

// <by-coordinate-pair> = by <coordinate-pair>
struct ByCoordinatePair {
    static constexpr auto affinity = Keyword::By { };

    CoordinatePair offset;

    bool operator==(const ByCoordinatePair&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(ByCoordinatePair, offset);

template<> struct Serialize<ByCoordinatePair> { void operator()(StringBuilder&, const SerializationContext&, const ByCoordinatePair&); };

// <relative-control-point> = [<coordinate-pair> [from [start | end | origin]]?]
// Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
struct RelativeControlPoint {
    using Anchor = ControlPointAnchor;

    CoordinatePair offset;
    std::optional<Anchor> anchor;

    bool operator==(const RelativeControlPoint&) const = default;
};
template<size_t I> const auto& get(const RelativeControlPoint& value)
{
    if constexpr (!I)
        return value.offset;
    if constexpr (I == 1)
        return value.anchor;
}
template<> struct Serialize<RelativeControlPoint> { void operator()(StringBuilder&, const SerializationContext&, const RelativeControlPoint&); };

// <to-control-point> = [<position> | <relative-control-point>]
// Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
struct AbsoluteControlPoint {
    using Anchor = ControlPointAnchor;

    Position offset;
    std::optional<Anchor> anchor;

    bool operator==(const AbsoluteControlPoint&) const = default;
};
template<size_t I> const auto& get(const AbsoluteControlPoint& value)
{
    if constexpr (!I)
        return value.offset;
    if constexpr (I == 1)
        return value.anchor;
}
template<> struct Serialize<AbsoluteControlPoint> { void operator()(StringBuilder&, const SerializationContext&, const AbsoluteControlPoint&); };

// <move-command> = move [to <position>] | [by <coordinate-pair>]
// https://drafts.csswg.org/css-shapes-2/#typedef-shape-move-command
// Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
struct MoveCommand {
    static constexpr auto name = CSSValueMove;
    using To = ToPosition;
    using By = ByCoordinatePair;
    Variant<To, By> toBy;

    bool operator==(const MoveCommand&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(MoveCommand, toBy);

template<> struct Serialize<MoveCommand> { void operator()(StringBuilder&, const SerializationContext&, const MoveCommand&); };

// <line-command> = line [to <position>] | [by <coordinate-pair>]
// https://drafts.csswg.org/css-shapes-2/#typedef-shape-line-command
// Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
struct LineCommand {
    static constexpr auto name = CSSValueLine;
    using To = ToPosition;
    using By = ByCoordinatePair;
    Variant<To, By> toBy;

    bool operator==(const LineCommand&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(LineCommand, toBy);

template<> struct Serialize<LineCommand> { void operator()(StringBuilder&, const SerializationContext&, const LineCommand&); };

// <horizontal-line-command> = hline [ to [ <length-percentage> | left | center | right | x-start | x-end ] | by <length-percentage> ]
// https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command
// Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2426552611
struct HLineCommand {
    static constexpr auto name = CSSValueHline;

    struct To {
        static constexpr auto affinity = Keyword::To { };

        TwoComponentPositionHorizontal offset;

        bool operator==(const To&) const = default;
    };
    struct By {
        static constexpr auto affinity = Keyword::By { };

        LengthPercentage<> offset;

        bool operator==(const By&) const = default;
    };
    Variant<To, By> toBy;

    bool operator==(const HLineCommand&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(HLineCommand::By, offset);
DEFINE_TYPE_WRAPPER_GET(HLineCommand::To, offset);
DEFINE_TYPE_WRAPPER_GET(HLineCommand, toBy);

template<> struct Serialize<HLineCommand::To> { void operator()(StringBuilder&, const SerializationContext&, const HLineCommand::To&); };
template<> struct Serialize<HLineCommand::By> { void operator()(StringBuilder&, const SerializationContext&, const HLineCommand::By&); };
template<> struct Serialize<HLineCommand> { void operator()(StringBuilder&, const SerializationContext&, const HLineCommand&); };

// <vertical-line-command> = vline [ to [ <length-percentage> | top | center | bottom | y-start | y-end ] | by <length-percentage> ]
// https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command
// Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2426552611
struct VLineCommand {
    static constexpr auto name = CSSValueVline;
    struct To {
        static constexpr auto affinity = Keyword::To { };

        TwoComponentPositionVertical offset;

        bool operator==(const To&) const = default;
    };
    struct By {
        static constexpr auto affinity = Keyword::By { };

        LengthPercentage<> offset;

        bool operator==(const By&) const = default;
    };
    Variant<To, By> toBy;

    bool operator==(const VLineCommand&) const = default;
};
DEFINE_TYPE_WRAPPER_GET(VLineCommand::By, offset);
DEFINE_TYPE_WRAPPER_GET(VLineCommand::To, offset);
DEFINE_TYPE_WRAPPER_GET(VLineCommand, toBy);

template<> struct Serialize<VLineCommand::To> { void operator()(StringBuilder&, const SerializationContext&, const VLineCommand::To&); };
template<> struct Serialize<VLineCommand::By> { void operator()(StringBuilder&, const SerializationContext&, const VLineCommand::By&); };
template<> struct Serialize<VLineCommand> { void operator()(StringBuilder&, const SerializationContext&, const VLineCommand&); };

// <curve-command> = curve [to <position> with <to-control-point> [/ <to-control-point>]?]
//                       | [by <coordinate-pair> with <relative-control-point> [/ <relative-control-point>]?]
// https://drafts.csswg.org/css-shapes-2/#typedef-shape-curve-command
// Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
struct CurveCommand {
    static constexpr auto name = CSSValueCurve;
    struct To {
        static constexpr auto affinity = Keyword::To { };

        Position offset;
        AbsoluteControlPoint controlPoint1;
        std::optional<AbsoluteControlPoint> controlPoint2;

        bool operator==(const To&) const = default;
    };
    struct By {
        static constexpr auto affinity = Keyword::By { };

        CoordinatePair offset;
        RelativeControlPoint controlPoint1;
        std::optional<RelativeControlPoint> controlPoint2;

        bool operator==(const By&) const = default;
    };
    Variant<To, By> toBy;

    bool operator==(const CurveCommand&) const = default;
};
template<size_t I> const auto& get(const CurveCommand::To& value)
{
    if constexpr (!I)
        return value.offset;
    if constexpr (I == 1)
        return value.controlPoint1;
    if constexpr (I == 2)
        return value.controlPoint2;
}
template<size_t I> const auto& get(const CurveCommand::By& value)
{
    if constexpr (!I)
        return value.offset;
    if constexpr (I == 1)
        return value.controlPoint1;
    if constexpr (I == 2)
        return value.controlPoint2;
}
DEFINE_TYPE_WRAPPER_GET(CurveCommand, toBy);

template<> struct Serialize<CurveCommand::To> { void operator()(StringBuilder&, const SerializationContext&, const CurveCommand::To&); };
template<> struct Serialize<CurveCommand::By> { void operator()(StringBuilder&, const SerializationContext&, const CurveCommand::By&); };
template<> struct Serialize<CurveCommand> { void operator()(StringBuilder&, const SerializationContext&, const CurveCommand&); };

// <smooth-command> = smooth [to <position> [with <to-control-point>]?]
//                         | [by <coordinate-pair> [with <relative-control-point>]?]
// https://drafts.csswg.org/css-shapes-2/#typedef-shape-smooth-command
// Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
struct SmoothCommand {
    static constexpr auto name = CSSValueSmooth;
    struct To {
        static constexpr auto affinity = Keyword::To { };

        Position offset;
        std::optional<AbsoluteControlPoint> controlPoint;

        bool operator==(const To&) const = default;
    };
    struct By {
        static constexpr auto affinity = Keyword::By { };

        CoordinatePair offset;
        std::optional<RelativeControlPoint> controlPoint;

        bool operator==(const By&) const = default;
    };
    Variant<To, By> toBy;

    bool operator==(const SmoothCommand&) const = default;
};
template<size_t I> const auto& get(const SmoothCommand::To& value)
{
    if constexpr (!I)
        return value.offset;
    if constexpr (I == 1)
        return value.controlPoint;
}
template<size_t I> const auto& get(const SmoothCommand::By& value)
{
    if constexpr (!I)
        return value.offset;
    if constexpr (I == 1)
        return value.controlPoint;
}
DEFINE_TYPE_WRAPPER_GET(SmoothCommand, toBy);

template<> struct Serialize<SmoothCommand::To> { void operator()(StringBuilder&, const SerializationContext&, const SmoothCommand::To&); };
template<> struct Serialize<SmoothCommand::By> { void operator()(StringBuilder&, const SerializationContext&, const SmoothCommand::By&); };
template<> struct Serialize<SmoothCommand> { void operator()(StringBuilder&, const SerializationContext&, const SmoothCommand&); };

// <arc-command> = arc [to <position>] | [by <coordinate-pair>] of <length-percentage>{1,2} [<arc-sweep>? || <arc-size>? || [rotate <angle>]?]
// https://drafts.csswg.org/css-shapes-2/#typedef-shape-arc-command
// Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
struct ArcCommand {
    static constexpr auto name = CSSValueArc;
    using To = ToPosition;
    using By = ByCoordinatePair;
    Variant<To, By> toBy;

    using SizeOfEllipse = MinimallySerializingSpaceSeparatedSize<LengthPercentage<>>;
    SizeOfEllipse size;

    ArcSweep arcSweep;
    ArcSize arcSize;
    Angle<> rotation;

    bool operator==(const ArcCommand&) const = default;
};
template<size_t I> const auto& get(const ArcCommand& value)
{
    if constexpr (!I)
        return value.toBy;
    if constexpr (I == 1)
        return value.size;
    if constexpr (I == 2)
        return value.arcSweep;
    if constexpr (I == 3)
        return value.arcSize;
    if constexpr (I == 4)
        return value.rotation;
}
template<> struct Serialize<ArcCommand> { void operator()(StringBuilder&, const SerializationContext&, const ArcCommand&); };

// <close> = close
// https://drafts.csswg.org/css-shapes-2/#valdef-shape-close
using CloseCommand = Keyword::Close;

// <shape-command> = <move-command> | <line-command> | <hv-line-command> | <curve-command> | <smooth-command> | <arc-command> | close
// https://drafts.csswg.org/css-shapes-2/#typedef-shape-command
using ShapeCommand = Variant<MoveCommand, LineCommand, HLineCommand, VLineCommand, CurveCommand, SmoothCommand, ArcCommand, CloseCommand>;

// shape() = shape( <'fill-rule'>? from <coordinate-pair>, <shape-command>#)
// https://drafts.csswg.org/css-shapes-2/#shape-function
struct Shape {
    using Commands = CommaSeparatedVector<ShapeCommand>;

    std::optional<FillRule> fillRule;
    // FIXME: The spec says this should be a <coordinate-pair>, but the tests and some comments indicate it has changed to position.
    Position startingPoint;
    Commands commands;

    bool operator==(const Shape&) const = default;
};
using ShapeFunction = FunctionNotation<CSSValueShape, Shape>;

template<size_t I> const auto& get(const Shape& value)
{
    if constexpr (!I)
        return value.fillRule;
    else if constexpr (I == 1)
        return value.startingPoint;
    else if constexpr (I == 2)
        return value.commands;
}

template<> struct Serialize<Shape> { void operator()(StringBuilder&, const SerializationContext&, const Shape&); };

} // namespace CSS
} // namespace WebCore

DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ToPosition, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ByCoordinatePair, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::RelativeControlPoint, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::AbsoluteControlPoint, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::MoveCommand, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::LineCommand, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::HLineCommand::To, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::HLineCommand::By, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::HLineCommand, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::VLineCommand::To, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::VLineCommand::By, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::VLineCommand, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::CurveCommand::To, 3)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::CurveCommand::By, 3)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::CurveCommand, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::SmoothCommand::To, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::SmoothCommand::By, 2)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::SmoothCommand, 1)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::ArcCommand, 5)
DEFINE_TUPLE_LIKE_CONFORMANCE(WebCore::CSS::Shape, 3)
