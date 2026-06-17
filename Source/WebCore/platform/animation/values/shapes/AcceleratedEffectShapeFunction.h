/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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

#if ENABLE(THREADED_ANIMATIONS)

#include <WebCore/FloatPoint.h>
#include <WebCore/FloatSize.h>
#include <WebCore/WindRule.h>
#include <optional>
#include <wtf/Variant.h>

namespace WebCore {

class FloatRect;
class Path;
struct AcceleratedEffectPathFunction;
struct BlendingContext;
template<typename> struct AcceleratedEffectInterpolation;

// shape() = shape( <'fill-rule'>? from <coordinate-pair>, <shape-command>#)
// https://drafts.csswg.org/css-shapes-2/#shape-function
struct AcceleratedEffectShapeFunction {
    enum class CommandAffinity : bool { By, To };
    enum class ArcSweep : bool { Cw, Ccw };
    enum class ArcSize : bool { Large, Small };
    enum class ControlPointAnchor : uint8_t { Start, End, Origin };

    // <to-position> = to <position>
    struct ToPosition {
        static constexpr auto affinity = CommandAffinity::To;

        FloatPoint offset;

        constexpr bool operator==(const ToPosition&) const = default;
    };

    // <by-coordinate-pair> = by <coordinate-pair>
    struct ByCoordinatePair {
        static constexpr auto affinity = CommandAffinity::By;

        FloatPoint offset;

        bool operator==(const ByCoordinatePair&) const = default;
    };

    // <relative-control-point> = [<coordinate-pair> [from [start | end | origin]]?]
    // Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
    struct RelativeControlPoint {
        using Anchor = ControlPointAnchor;
        static constexpr auto defaultAnchor = Anchor::Start;

        FloatPoint offset;
        std::optional<Anchor> anchor;

        bool operator==(const RelativeControlPoint&) const = default;
    };

    // <to-control-point> = [<position> | <relative-control-point>]
    // Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
    struct AbsoluteControlPoint {
        using Anchor = ControlPointAnchor;
        static constexpr auto defaultAnchor = Anchor::Origin;

        FloatPoint offset;
        std::optional<Anchor> anchor;

        bool operator==(const AbsoluteControlPoint&) const = default;
    };

    // MARK: - Move Command

    // <move-command> = move [to <position>] | [by <coordinate-pair>]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-move-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
    struct MoveCommand {
        using To = ToPosition;
        using By = ByCoordinatePair;
        using ToBy = Variant<To, By>;
        ToBy toBy;

        constexpr bool operator==(const MoveCommand&) const = default;
    };

    // MARK: - Line Command

    // <line-command> = line [to <position>] | [by <coordinate-pair>]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
    struct LineCommand {
        using To = ToPosition;
        using By = ByCoordinatePair;
        using ToBy = Variant<To, By>;
        ToBy toBy;

        constexpr bool operator==(const LineCommand&) const = default;
    };

    // MARK: - HLine Command

    // <horizontal-line-command> = hline [ to [ <length-percentage> | left | center | right | x-start | x-end ] | by <length-percentage> ]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2426552611
    struct HLineCommand {
        struct To {
            static constexpr auto affinity = CommandAffinity::To;

            float offset;

            constexpr bool operator==(const To&) const = default;
        };
        struct By {
            static constexpr auto affinity = CommandAffinity::By;

            float offset;

            constexpr  bool operator==(const By&) const = default;
        };
        using ToBy = Variant<To, By>;
        ToBy toBy;

        constexpr bool operator==(const HLineCommand&) const = default;
    };

    // MARK: - VLine Command

    // <vertical-line-command> = vline [ to [ <length-percentage> | top | center | bottom | y-start | y-end ] | by <length-percentage> ]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2426552611
    struct VLineCommand {
        struct To {
            static constexpr auto affinity = CommandAffinity::To;

            float offset;

            constexpr bool operator==(const To&) const = default;
        };
        struct By {
            static constexpr auto affinity = CommandAffinity::By;

            float offset;

            constexpr bool operator==(const By&) const = default;
        };
        using ToBy = Variant<To, By>;
        ToBy toBy;

        constexpr bool operator==(const VLineCommand&) const = default;
    };

    // MARK: - Curve Command

    // <curve-command> = curve [to <position> with <to-control-point> [/ <to-control-point>]?]
    //                       | [by <coordinate-pair> with <relative-control-point> [/ <relative-control-point>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-curve-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
    struct CurveCommand {
        struct To {
            static constexpr auto affinity = CommandAffinity::To;

            FloatPoint offset;
            AbsoluteControlPoint controlPoint1;
            std::optional<AbsoluteControlPoint> controlPoint2;

            constexpr bool operator==(const To&) const = default;
        };
        struct By {
            static constexpr auto affinity = CommandAffinity::By;

            FloatPoint offset;
            RelativeControlPoint controlPoint1;
            std::optional<RelativeControlPoint> controlPoint2;

            constexpr bool operator==(const By&) const = default;
        };
        using ToBy = Variant<To, By>;
        ToBy toBy;

        constexpr bool operator==(const CurveCommand&) const = default;
    };

    // MARK: - Smooth Command

    // <smooth-command> = smooth [to <position> [with <to-control-point>]?]
    //                         | [by <coordinate-pair> [with <relative-control-point>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-smooth-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
    struct SmoothCommand {
        struct To {
            static constexpr auto affinity = CommandAffinity::To;

            FloatPoint offset;
            std::optional<AbsoluteControlPoint> controlPoint;

            constexpr bool operator==(const To&) const = default;
        };
        struct By {
            static constexpr auto affinity = CommandAffinity::By;

            FloatPoint offset;
            std::optional<RelativeControlPoint> controlPoint;

            constexpr bool operator==(const By&) const = default;
        };
        using ToBy = Variant<To, By>;
        ToBy toBy;

        constexpr bool operator==(const SmoothCommand&) const = default;
    };

    // MARK: - Arc Command

    // <arc-command> = arc [to <position>] | [by <coordinate-pair>] of <length-percentage>{1,2} [<arc-sweep>? || <arc-size>? || [rotate <angle>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-arc-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773
    struct ArcCommand {
        using To = ToPosition;
        using By = ByCoordinatePair;
        using ToBy = Variant<To, By>;
        ToBy toBy;

        FloatSize size;

        ArcSweep arcSweep;
        ArcSize arcSize;
        double rotation;

        constexpr bool operator==(const ArcCommand&) const = default;
    };

    // MARK: - Close Command

    // <close> = close
    // https://drafts.csswg.org/css-shapes-2/#valdef-shape-close
    struct CloseCommand { constexpr bool operator==(const CloseCommand&) const = default; };

    // MARK: - Shape Command (variant)

    // <shape-command> = <move-command> | <line-command> | <hv-line-command> | <curve-command> | <smooth-command> | <arc-command> | close
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-command
    using Command = Variant<MoveCommand, LineCommand, HLineCommand, VLineCommand, CurveCommand, SmoothCommand, ArcCommand, CloseCommand>;

    std::optional<WindRule> fillRule;
    FloatPoint startingPoint;
    Vector<Command> commands;

    bool operator==(const AcceleratedEffectShapeFunction&) const = default;
};

// MARK: - Path Evaluation

Path path(const AcceleratedEffectShapeFunction&, const FloatRect&);

// MARK: - Blending

template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction> {
    auto canBlend(const AcceleratedEffectShapeFunction&, const AcceleratedEffectShapeFunction&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction&, const AcceleratedEffectShapeFunction&, const BlendingContext&) -> AcceleratedEffectShapeFunction;
};

// Returns whether the shape and path can be interpolated together
// according to the rules in https://drafts.csswg.org/css-shapes-2/#interpolating-shape.
bool canBlendShapeWithPath(const AcceleratedEffectShapeFunction&, const AcceleratedEffectPathFunction&);

// Makes a `Shape` representation of `Path`. Returns `std::nullopt` if the path cannot be parsed.
std::optional<AcceleratedEffectShapeFunction> makeShapeFromPath(const AcceleratedEffectPathFunction&);

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
