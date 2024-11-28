/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "CSSPropertyParserConsumer+Shapes.h"

#include "CSSBasicShapeValue.h"
#include "CSSParserTokenRange.h"
#include "CSSPathValue.h"
#include "CSSPrimitiveValue.h"
#include "CSSPropertyParserConsumer+AngleDefinitions.h"
#include "CSSPropertyParserConsumer+Background.h"
#include "CSSPropertyParserConsumer+Ident.h"
#include "CSSPropertyParserConsumer+Image.h"
#include "CSSPropertyParserConsumer+LengthPercentageDefinitions.h"
#include "CSSPropertyParserConsumer+MetaConsumer.h"
#include "CSSPropertyParserConsumer+Position.h"
#include "CSSPropertyParserConsumer+Primitives.h"
#include "CSSPropertyParserConsumer+URL.h"
#include "CSSPropertyParsing.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

template<CSSValueID Name, typename T> CSS::BasicShape toBasicShape(T&& parameters)
{
    return CSS::BasicShape { CSS::FunctionNotation<Name, T> { WTFMove(parameters) } };
}

template<CSSValueID Name, typename T> std::optional<CSS::BasicShape> toBasicShape(std::optional<T>&& parameters)
{
    if (!parameters)
        return { };
    return toBasicShape<Name>(WTFMove(*parameters));
}

static std::optional<CSS::FillRule> peekFillRule(CSSParserTokenRange& range)
{
    // <'fill-rule'> = nonzero | evenodd
    // https://svgwg.org/svg2-draft/painting.html#FillRuleProperty

    static constexpr std::pair<CSSValueID, CSS::FillRule> fillRuleMappings[] {
        { CSSValueNonzero, CSS::FillRule { CSS::Nonzero { } } },
        { CSSValueEvenodd, CSS::FillRule { CSS::Evenodd { } } },
    };
    static constexpr SortedArrayMap fillRuleMap { fillRuleMappings };

    return peekIdentUsingMapping(range, fillRuleMap);
}

static std::optional<CSS::FillRule> consumeFillRule(CSSParserTokenRange& range)
{
    auto result = peekFillRule(range);
    if (result)
        range.consumeIncludingWhitespace();
    return result;
}

template<typename Container> static std::optional<Container> consumePair(CSSParserTokenRange& range, const CSSParserContext& context, CSSPropertyParserOptions options)
{
    auto rangeCopy = range;

    auto p1 = MetaConsumer<typename Container::value_type>::consume(rangeCopy, context, { }, options);
    if (!p1)
        return { };
    auto p2 = MetaConsumer<typename Container::value_type>::consume(rangeCopy, context, { }, options);
    if (!p2)
        return { };

    range = rangeCopy;
    return Container { WTFMove(*p1), WTFMove(*p2) };
}

static std::optional<CSS::CoordinatePair> consumeCoordinatePair(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <coordinate-pair> = <length-percentage>{2}
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-coordinate-pair

    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    return consumePair<CSS::CoordinatePair>(range, context, options);
}

static std::optional<CSS::RelativeControlPoint> consumeRelativeControlPoint(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <relative-control-point> = [<coordinate-pair> [from [start | end | origin]]?]
    // Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    using Anchor = CSS::RelativeControlPoint::Anchor;

    static constexpr std::pair<CSSValueID, Anchor> anchorMappings[] {
        { CSSValueStart, Anchor { CSS::Start { } } },
        { CSSValueEnd, Anchor { CSS::End { } } },
        { CSSValueOrigin, Anchor { CSS::Origin { } } },
    };
    static constexpr SortedArrayMap anchorMap { anchorMappings };

    auto rangeCopy = range;

    auto offset = consumeCoordinatePair(rangeCopy, context);
    if (!offset)
        return { };

    std::optional<Anchor> anchor;
    if (consumeIdent<CSSValueFrom>(rangeCopy)) {
        anchor = consumeIdentUsingMapping(rangeCopy, anchorMap);
        if (!anchor)
            return { };
    }

    range = rangeCopy;

    return CSS::RelativeControlPoint {
        .offset = WTFMove(*offset),
        .anchor = WTFMove(anchor)
    };
}

static std::optional<CSS::AbsoluteControlPoint> consumeAbsoluteControlPoint(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <to-control-point> = [<position> | <relative-control-point>]
    // Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    // FIXME: [<length-percentage> <length-percentage>] is valid for both <position> and <relative-control-point>.
    // We currently try <relative-control-point> first, but this ambiguity should probably be explicitly resolved in the spec.

    if (auto relativeControlPoint = consumeRelativeControlPoint(range, context)) {
        return CSS::AbsoluteControlPoint {
            .offset = CSS::Position {
                CSS::TwoComponentPosition {
                    { relativeControlPoint->offset.x() },
                    { relativeControlPoint->offset.y() }
                }
            },
            .anchor = relativeControlPoint->anchor
        };
    }
    if (auto position = consumePositionUnresolved(range, context)) {
        return CSS::AbsoluteControlPoint {
            .offset = WTFMove(*position),
            .anchor = std::nullopt
        };
    }
    return { };
}

static CSS::Circle::RadialSize consumeCircleRadialSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // NOTE: For circle(), it uses a modified <radial-size> as the two `length-percentage` values are invalid as expressed in the text.
    // <radial-size>    = <radial-extent> | <length [0,∞]>
    // <radial-extent>  = closest-corner | closest-side | farthest-corner | farthest-side
    // Default to `farthest-corner` if no radial-size is provided.
    // https://drafts.csswg.org/css-images-3/#typedef-radial-size

    // FIXME: The above text is what the spec says, but the current implementation uses the following grammar:
    // <radial-size>    = <radial-extent> | <length-percentage [0,∞]>
    // <radial-extent>  = closest-corner | closest-side | farthest-corner | farthest-side
    // Default to `closest-side` if no radial-size is provided.

    static constexpr std::pair<CSSValueID, CSS::Circle::Extent> extentMappings[] {
        { CSSValueClosestSide, CSS::Circle::Extent { CSS::ClosestSide { } } },
        { CSSValueClosestCorner, CSS::Circle::Extent { CSS::ClosestCorner { } } },
        { CSSValueFarthestSide, CSS::Circle::Extent { CSS::FarthestSide { } } },
        { CSSValueFarthestCorner, CSS::Circle::Extent { CSS::FarthestCorner { } } },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    // Default to `closest-side` if no radial-size is provided.
    // FIXME: The spec says that `farthest-corner` should be the default, but this does not match the tests.
    auto defaultValue = [] {
        return CSS::Circle::RadialSize { CSS::Circle::Extent { CSS::ClosestSide { } } };
    };

    if (range.peek().type() == IdentToken) {
        if (auto extent = consumeIdentUsingMapping(range, extentMap))
            return *extent;
        return defaultValue();
    }

    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    auto length = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, context, { }, options);
    if (!length)
        return defaultValue();

    return CSS::Circle::RadialSize { *length };
}

static std::optional<CSS::Circle> consumeBasicShapeCircleFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <circle()> = circle( <radial-size>? [ at <position> ]? )
    // https://drafts.csswg.org/css-shapes/#funcdef-basic-shape-circle

    auto radius = consumeCircleRadialSize(args, context);

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueAt>(args)) {
        position = consumePositionUnresolved(args, context);
        if (!position)
            return { };
    }

    return CSS::Circle {
        .radius = WTFMove(radius),
        .position = WTFMove(position)
    };
}

static std::optional<CSS::Ellipse::RadialSize> consumeEllipseRadialSize(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <radial-size>    = <radial-extent> | <length [0,∞]> | <length-percentage [0,∞]>{2}
    // <radial-extent>  = closest-corner | closest-side | farthest-corner | farthest-side
    // Default to `farthest-corner` if no radial-size is provided.
    // https://drafts.csswg.org/css-images-3/#typedef-radial-size

    // FIXME: The above text is what the spec says, but the current implementation uses the following grammar:
    // <radial-size>    = <radial-extent> | <length-percentage [0,∞]>
    // <radial-extent>  = closest-corner | closest-side | farthest-corner | farthest-side
    // Default to `closest-side` if no radial-size is provided.

    static constexpr std::pair<CSSValueID, CSS::Ellipse::Extent> extentMappings[] {
        { CSSValueClosestSide, CSS::Ellipse::Extent { CSS::ClosestSide { } } },
        { CSSValueClosestCorner, CSS::Ellipse::Extent { CSS::ClosestCorner { } } },
        { CSSValueFarthestSide, CSS::Ellipse::Extent { CSS::FarthestSide { } } },
        { CSSValueFarthestCorner, CSS::Ellipse::Extent { CSS::FarthestCorner { } } },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    if (range.peek().type() == IdentToken) {
        if (auto extent = consumeIdentUsingMapping(range, extentMap))
            return CSS::Ellipse::RadialSize { *extent };
        return std::nullopt;
    }

    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    auto length = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, context, { }, options);
    if (!length)
        return std::nullopt;

    return CSS::Ellipse::RadialSize { *length };
}

static std::optional<CSS::Ellipse> consumeBasicShapeEllipseFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <ellipse()>      = ellipse( <radial-size>? [ at <position> ]? )
    // <radial-size>    = <radial-extent> | <length [0,∞]> | <length-percentage [0,∞]>{2}
    // <radial-extent>  = closest-corner | closest-side | farthest-corner | farthest-side
    // Default to `farthest-corner` if no radial-size is provided.
    // https://drafts.csswg.org/css-shapes/#funcdef-basic-shape-ellipse

    // FIXME: The above text is what the spec says, but the current implementation uses the following grammar:
    // <ellipse()>      = ellipse( [<shape-radius>{2}]? [at <position>]? )
    // <radial-size>    = <radial-extent> | <length-percentage [0,∞]>
    // <radial-extent>  = closest-corner | closest-side | farthest-corner | farthest-side
    // Default to `closest-side closest-side` if no radial-size is provided.

    auto consumeRadialSizePair = [&] -> std::optional<CSS::SpaceSeparatedPair<CSS::Ellipse::RadialSize>> {
        if (auto radiusX = consumeEllipseRadialSize(args, context)) {
            auto radiusY = consumeEllipseRadialSize(args, context);
            if (!radiusY)
                return std::nullopt;
            return CSS::SpaceSeparatedPair<CSS::Ellipse::RadialSize> { WTFMove(*radiusX), WTFMove(*radiusY) };
        }

        return CSS::SpaceSeparatedPair<CSS::Ellipse::RadialSize> {
            CSS::Ellipse::RadialSize { CSS::Ellipse::Extent { CSS::ClosestSide { } } },
            CSS::Ellipse::RadialSize { CSS::Ellipse::Extent { CSS::ClosestSide { } } }
        };
    };
    auto radii = consumeRadialSizePair();
    if (!radii)
        return std::nullopt;

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueAt>(args)) {
        position = consumePositionUnresolved(args, context);
        if (!position)
            return { };
    }

    return CSS::Ellipse {
        .radii = WTFMove(*radii),
        .position = WTFMove(position)
    };
}

static std::optional<CSS::Polygon> consumeBasicShapePolygonFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <polygon()> = polygon( <'fill-rule'>? [ round <length> ]? , [<length-percentage> <length-percentage>]# )
    // https://drafts.csswg.org/css-shapes/#funcdef-basic-shape-polygon

    // FIXME: The above text is what the spec says, but the current implementation does not support the "round" clause.

    auto fillRule = consumeFillRule(args);

    // FIXME: Consume optional `round` clause here.

    if (fillRule) {
        if (!consumeCommaIncludingWhitespace(args))
            return { };
    }

    const auto verticesOptions = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    CSS::Polygon::Vertices::Vector vertices;
    do {
        auto vertex = consumePair<CSS::Polygon::Vertex>(args, context, verticesOptions);
        if (!vertex)
            return { };
        vertices.append(WTFMove(*vertex));
    } while (consumeCommaIncludingWhitespace(args));

    return CSS::Polygon {
        .fillRule = WTFMove(fillRule),
        .vertices = { WTFMove(vertices) }
    };
}

static std::optional<CSS::Path> consumeBasicShapePathFunctionParameters(CSSParserTokenRange& args, const CSSParserContext&, OptionSet<PathParsingOption> options)
{
    // <path()> = path( <'fill-rule'>? , <string> )
    // https://drafts.csswg.org/css-shapes/#funcdef-basic-shape-path

    if (options.contains(PathParsingOption::RejectPath))
        return { };

    auto fillRule = peekFillRule(args);
    if (fillRule) {
        if (options.contains(PathParsingOption::RejectPathFillRule))
            return { };

        args.consumeIncludingWhitespace();
        if (!consumeCommaIncludingWhitespace(args))
            return { };
    }

    if (args.peek().type() != StringToken)
        return { };

    SVGPathByteStream byteStream;
    if (!buildSVGPathByteStreamFromString(args.consumeIncludingWhitespace().value(), byteStream, UnalteredParsing) || byteStream.isEmpty())
        return { };

    return CSS::Path {
        .fillRule = WTFMove(fillRule),
        .data = { .byteStream = WTFMove(byteStream) }
    };
}

static std::optional<CSS::CommandAffinity> consumeShapeCommandAffinity(CSSParserTokenRange& range, const CSSParserContext&)
{
    // <by-to> = by | to
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-by-to

    static constexpr std::pair<CSSValueID, CSS::CommandAffinity> affinityMappings[] {
        { CSSValueTo, CSS::CommandAffinity { CSS::To { } } },
        { CSSValueBy, CSS::CommandAffinity { CSS::By { } } },
    };
    static constexpr SortedArrayMap affinityMap { affinityMappings };

    return consumeIdentUsingMapping(range, affinityMap);
}

static std::optional<CSS::MoveCommand> consumeShapeMoveCommand(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <move-command> = move [to <position>] | [by <coordinate-pair>]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-move-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, context);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::To) -> std::optional<CSS::MoveCommand> {
            auto position = consumePositionUnresolved(range, context);
            if (!position)
                return std::nullopt;
            return CSS::MoveCommand {
                .toBy = CSS::MoveCommand::To { .offset = WTFMove(*position) }
            };
        },
        [&](CSS::By) -> std::optional<CSS::MoveCommand> {
            auto coordinatePair = consumeCoordinatePair(range, context);
            if (!coordinatePair)
                return std::nullopt;
            return CSS::MoveCommand {
                .toBy = CSS::MoveCommand::By { .offset = WTFMove(*coordinatePair) }
            };
        }
    );
}

static std::optional<CSS::LineCommand> consumeShapeLineCommand(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <line-command> = line [to <position>] | [by <coordinate-pair>]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, context);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::To) -> std::optional<CSS::LineCommand> {
            auto position = consumePositionUnresolved(range, context);
            if (!position)
                return { };
            return CSS::LineCommand {
                .toBy = CSS::LineCommand::To { .offset = WTFMove(*position) }
            };
        },
        [&](CSS::By) -> std::optional<CSS::LineCommand> {
            auto coordinatePair = consumeCoordinatePair(range, context);
            if (!coordinatePair)
                return { };
            return CSS::LineCommand {
                .toBy = CSS::LineCommand::By { .offset = WTFMove(*coordinatePair) }
            };
        }
    );
}

static std::optional<CSS::HLineCommand> consumeShapeHLineCommand(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <horizontal-line-command> = hline [ to [ <length-percentage> | left | center | right | x-start | x-end ] | by <length-percentage> ]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2426552611
    auto affinity = consumeShapeCommandAffinity(range, context);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::To) -> std::optional<CSS::HLineCommand> {
            auto offset = consumeTwoComponentPositionHorizontalUnresolved(range, context);
            if (!offset)
                return { };
            return CSS::HLineCommand {
                .toBy = CSS::HLineCommand::To { .offset = WTFMove(*offset) }
            };
        },
        [&](CSS::By) -> std::optional<CSS::HLineCommand> {
            const auto options = CSSPropertyParserOptions {
                .parserMode = context.mode,
                .unitlessZero = UnitlessZeroQuirk::Allow
            };
            auto offset = MetaConsumer<CSS::LengthPercentage<>>::consume(range, context, { }, options);
            if (!offset)
                return { };
            return CSS::HLineCommand {
                .toBy = CSS::HLineCommand::By { .offset = WTFMove(*offset) }
            };
        }
    );
}

static std::optional<CSS::VLineCommand> consumeShapeVLineCommand(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <hv-line-command> = [... | vline] <by-to> <length-percentage>
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command

    auto affinity = consumeShapeCommandAffinity(range, context);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::To) -> std::optional<CSS::VLineCommand> {
            auto offset = consumeTwoComponentPositionVerticalUnresolved(range, context);
            if (!offset)
                return { };
            return CSS::VLineCommand {
                .toBy = CSS::VLineCommand::To { .offset = WTFMove(*offset) }
            };
        },
        [&](CSS::By) -> std::optional<CSS::VLineCommand> {
            const auto options = CSSPropertyParserOptions {
                .parserMode = context.mode,
                .unitlessZero = UnitlessZeroQuirk::Allow
            };
            auto offset = MetaConsumer<CSS::LengthPercentage<>>::consume(range, context, { }, options);
            if (!offset)
                return { };
            return CSS::VLineCommand {
                .toBy = CSS::VLineCommand::By { .offset = WTFMove(*offset) }
            };
        }
    );
}

static std::optional<CSS::CurveCommand> consumeShapeCurveCommand(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <curve-command> = curve [to <position> with <to-control-point> [/ <to-control-point>]?]
    //                       | [by <coordinate-pair> with <relative-control-point> [/ <relative-control-point>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-curve-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, context);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::To) -> std::optional<CSS::CurveCommand> {
            auto position = consumePositionUnresolved(range, context);
            if (!position)
                return { };

            if (!consumeIdent<CSSValueWith>(range))
                return { };

            auto controlPoint1 = consumeAbsoluteControlPoint(range, context);
            if (!controlPoint1)
                return { };

            if (consumeSlashIncludingWhitespace(range)) {
                auto controlPoint2 = consumeAbsoluteControlPoint(range, context);
                if (!controlPoint2)
                    return { };

                return CSS::CurveCommand {
                    .toBy = CSS::CurveCommand::To {
                        .offset = WTFMove(*position),
                        .controlPoint1 = WTFMove(*controlPoint1),
                        .controlPoint2 = WTFMove(*controlPoint2)
                    }
                };
            } else {
                return CSS::CurveCommand {
                    .toBy = CSS::CurveCommand::To {
                        .offset = WTFMove(*position),
                        .controlPoint1 = WTFMove(*controlPoint1),
                        .controlPoint2 = std::nullopt
                    }
                };
            }
        },
        [&](CSS::By) -> std::optional<CSS::CurveCommand> {
            auto coordinatePair = consumeCoordinatePair(range, context);
            if (!coordinatePair)
                return { };

            if (!consumeIdent<CSSValueWith>(range))
                return { };

            auto controlPoint1 = consumeRelativeControlPoint(range, context);
            if (!controlPoint1)
                return { };

            if (consumeSlashIncludingWhitespace(range)) {
                auto controlPoint2 = consumeRelativeControlPoint(range, context);
                if (!controlPoint2)
                    return { };

                return CSS::CurveCommand {
                    .toBy = CSS::CurveCommand::By {
                        .offset = WTFMove(*coordinatePair),
                        .controlPoint1 = WTFMove(*controlPoint1),
                        .controlPoint2 = WTFMove(*controlPoint2)
                    }
                };
            } else {
                return CSS::CurveCommand {
                    .toBy = CSS::CurveCommand::By {
                        .offset = WTFMove(*coordinatePair),
                        .controlPoint1 = WTFMove(*controlPoint1),
                        .controlPoint2 = std::nullopt
                    }
                };
            }
        }
    );
}

static std::optional<CSS::SmoothCommand> consumeShapeSmoothCommand(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <smooth-command> = smooth [to <position> [with <to-control-point>]?]
    //                         | [by <coordinate-pair> [with <relative-control-point>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-smooth-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, context);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::To) -> std::optional<CSS::SmoothCommand> {
            auto position = consumePositionUnresolved(range, context);
            if (!position)
                return { };

            if (consumeIdent<CSSValueWith>(range)) {
                auto controlPoint = consumeAbsoluteControlPoint(range, context);
                if (!controlPoint)
                    return { };

                return CSS::SmoothCommand {
                    .toBy = CSS::SmoothCommand::To {
                        .offset = WTFMove(*position),
                        .controlPoint = WTFMove(*controlPoint),
                    }
                };
            } else {
                return CSS::SmoothCommand {
                    .toBy = CSS::SmoothCommand::To {
                        .offset = WTFMove(*position),
                        .controlPoint = std::nullopt
                    }
                };
            }
        },
        [&](CSS::By) -> std::optional<CSS::SmoothCommand> {
            auto coordinatePair = consumeCoordinatePair(range, context);
            if (!coordinatePair)
                return { };

            if (consumeIdent<CSSValueWith>(range)) {
                auto controlPoint = consumeRelativeControlPoint(range, context);
                if (!controlPoint)
                    return { };

                return CSS::SmoothCommand {
                    .toBy = CSS::SmoothCommand::By {
                        .offset = WTFMove(*coordinatePair),
                        .controlPoint = WTFMove(*controlPoint),
                    }
                };
            } else {
                return CSS::SmoothCommand {
                    .toBy = CSS::SmoothCommand::By {
                        .offset = WTFMove(*coordinatePair),
                        .controlPoint = std::nullopt
                    }
                };
            }
        }
    );
}

static std::optional<CSS::ArcCommand> consumeShapeArcCommand(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <arc-command> = arc [to <position>] | [by <coordinate-pair>] of <length-percentage>{1,2} [<arc-sweep>? || <arc-size>? || [rotate <angle>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-arc-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, context);
    if (!affinity)
        return { };

    using ToBy = std::variant<CSS::ArcCommand::To, CSS::ArcCommand::By>;
    auto toBy = WTF::switchOn(*affinity,
        [&](CSS::To) -> std::optional<ToBy> {
            auto position = consumePositionUnresolved(range, context);
            if (!position)
                return { };
            return ToBy { CSS::ArcCommand::To { WTFMove(*position) } };
        },
        [&](CSS::By) -> std::optional<ToBy> {
            auto coordinatePair = consumeCoordinatePair(range, context);
            if (!coordinatePair)
                return { };
            return ToBy { CSS::ArcCommand::By { WTFMove(*coordinatePair) } };
        }
    );
    if (!toBy)
        return { };

    if (!consumeIdent<CSSValueOf>(range))
        return { };

    const auto lengthOptions = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    auto length1 = MetaConsumer<CSS::LengthPercentage<>>::consume(range, context, { }, lengthOptions);
    if (!length1)
        return { };
    auto length2 = MetaConsumer<CSS::LengthPercentage<>>::consume(range, context, { }, lengthOptions);
    if (!length2)
        length2 = length1; // Copy `length1` to `length2` if there is only one length consumed.

    std::optional<CSS::ArcSweep> arcSweep;
    std::optional<CSS::ArcSize> arcSize;
    std::optional<CSS::Angle<>> angle;

    auto atEndOfCommand = [&] {
        return range.atEnd() || range.peek().type() == CommaToken;
    };

    while (!atEndOfCommand()) {
        auto ident = consumeIdent<CSSValueCw, CSSValueCcw, CSSValueLarge, CSSValueSmall, CSSValueRotate>(range);
        if (!ident)
            return { };

        switch (ident->valueID()) {
        case CSSValueCw:
            if (arcSweep)
                return { };
            arcSweep = CSS::Cw { };
            break;
        case CSSValueCcw:
            if (arcSweep)
                return { };
            arcSweep = CSS::Ccw { };
            break;

        case CSSValueLarge:
            if (arcSize)
                return { };
            arcSize = CSS::Large { };
            break;

        case CSSValueSmall:
            if (arcSize)
                return { };
            arcSize = CSS::Small { };
            break;

        case CSSValueRotate:
            if (angle)
                return { };

            angle = MetaConsumer<CSS::Angle<>>::consume(range, context, { }, { .parserMode = context.mode });
            if (!angle)
                return { };
            break;

        default:
            break;
        }
    }

    return CSS::ArcCommand {
        .toBy = WTFMove(*toBy),
        .size = { WTFMove(*length1), WTFMove(*length2) },
        .arcSweep = arcSweep.value_or(CSS::ArcSweep { CSS::Ccw { } }),
        .arcSize = arcSize.value_or(CSS::ArcSize { CSS::Small { } }),
        .rotation = angle.value_or(CSS::Angle<> { CSS::AngleRaw<> { CSSUnitType::CSS_DEG, 0 } })
    };
}

static std::optional<CSS::ShapeCommand> consumeShapeCommand(CSSParserTokenRange& range, const CSSParserContext& context)
{
    if (range.peek().type() != IdentToken)
        return { };

    auto id = range.consumeIncludingWhitespace().id();
    switch (id) {
    case CSSValueMove:
        if (auto command = consumeShapeMoveCommand(range, context))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueLine:
        if (auto command = consumeShapeLineCommand(range, context))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueHline:
        if (auto command = consumeShapeHLineCommand(range, context))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueVline:
        if (auto command = consumeShapeVLineCommand(range, context))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueCurve:
        if (auto command = consumeShapeCurveCommand(range, context))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueSmooth:
        if (auto command = consumeShapeSmoothCommand(range, context))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueArc:
        if (auto command = consumeShapeArcCommand(range, context))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueClose:
        return CSS::ShapeCommand { CSS::CloseCommand { } };

    default:
        break;
    }

    return { };
}

static std::optional<CSS::Shape> consumeBasicShapeShapeFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // shape() = shape( <'fill-rule'>? from <coordinate-pair>, <shape-command># )
    // https://drafts.csswg.org/css-shapes-2/#shape-function

    if (!context.cssShapeFunctionEnabled)
        return { };

    auto fillRule = consumeFillRule(args);

    if (!consumeIdent<CSSValueFrom>(args))
        return { };

    // FIXME: The spec says this should be a <coordinate-pair>, but the tests and some comments indicate it has changed to position.
    auto startingPoint = consumePositionUnresolved(args, context);
    if (!startingPoint)
        return { };

    if (!consumeCommaIncludingWhitespace(args))
        return { };

    CSS::Shape::Commands::Vector commands;
    do {
        auto command = consumeShapeCommand(args, context);
        if (!command)
            return { };

        commands.append(WTFMove(*command));
    } while (consumeCommaIncludingWhitespace(args));

    return CSS::Shape {
        .fillRule = WTFMove(fillRule),
        .startingPoint = WTFMove(*startingPoint),
        .commands = { WTFMove(commands) }
    };
}

// MARK: - <rect()>

static std::optional<CSS::Rect::Edge> consumeBasicShapeRectEdge(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <rect-edge> = [ <length-percentage> | auto ]

    if (args.peek().type() == IdentToken) {
        if (args.peek().id() == CSSValueAuto) {
            args.consumeIncludingWhitespace();
            return { CSS::Auto { } };
        }
        return { };
    }

    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    if (auto edge = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, options))
        return { WTFMove(*edge) };

    return { };
}

static std::optional<RectEdges<CSS::Rect::Edge>> consumeBasicShapeRectEdges(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <rect-edges> = <rect-edge>{4}

    auto top = consumeBasicShapeRectEdge(args, context);
    if (!top)
        return { };
    auto right = consumeBasicShapeRectEdge(args, context);
    if (!right)
        return { };
    auto bottom = consumeBasicShapeRectEdge(args, context);
    if (!bottom)
        return { };
    auto left = consumeBasicShapeRectEdge(args, context);
    if (!left)
        return { };
    return RectEdges<CSS::Rect::Edge> { WTFMove(*top), WTFMove(*right), WTFMove(*bottom), WTFMove(*left) };
}

static std::optional<CSS::Rect> consumeBasicShapeRectFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <rect()> = rect( [ <length-percentage> | auto ]{4} [ round <'border-radius'> ]? )
    // https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-rect

    auto edges = consumeBasicShapeRectEdges(args, context);
    if (!edges)
        return { };

    std::optional<CSS::BorderRadius> radii;
    if (consumeIdent<CSSValueRound>(args)) {
        radii = consumeUnresolvedBorderRadius(args, context);
        if (!radii)
            return { };
    }

    return CSS::Rect {
        .edges = WTFMove(*edges),
        .radii = radii.value_or(CSS::BorderRadius::defaultValue())
    };
}

// MARK: - <xywh()>

static std::optional<CSS::Xywh> consumeBasicShapeXywhFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <xywh()> = xywh( <length-percentage>{2} <length-percentage [0,∞]>{2} [ round <'border-radius'> ]? )
    // https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-xywh

    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };

    auto location = consumePair<CSS::Xywh::Location>(args, context, options);
    if (!location)
        return { };
    auto size = consumePair<CSS::Xywh::Size>(args, context, options);
    if (!size)
        return { };

    std::optional<CSS::BorderRadius> radii;
    if (consumeIdent<CSSValueRound>(args)) {
        radii = consumeUnresolvedBorderRadius(args, context);
        if (!radii)
            return { };
    }

    return CSS::Xywh {
        .location = WTFMove(*location),
        .size = WTFMove(*size),
        .radii = radii.value_or(CSS::BorderRadius::defaultValue())
    };
}

// MARK: - <inset()>

static std::optional<CSS::Inset::Insets> consumeBasicShapeInsetInsets(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <insets> = <length-percentage>{1,4}

    const auto options = CSSPropertyParserOptions {
        .parserMode = context.mode,
        .unitlessZero = UnitlessZeroQuirk::Allow
    };
    auto inset1 = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, options);
    if (!inset1)
        return { };

    auto inset2 = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, options);
    if (!inset2)
        return completeQuad<CSS::Inset::Insets>(WTFMove(*inset1));

    auto inset3 = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, options);
    if (!inset3)
        return completeQuad<CSS::Inset::Insets>(WTFMove(*inset1), WTFMove(*inset2));

    auto inset4 = MetaConsumer<CSS::LengthPercentage<>>::consume(args, context, { }, options);
    if (!inset4)
        return completeQuad<CSS::Inset::Insets>(WTFMove(*inset1), WTFMove(*inset2), WTFMove(*inset3));

    return CSS::Inset::Insets { WTFMove(*inset1), WTFMove(*inset2), WTFMove(*inset3), WTFMove(*inset4) };
}

static std::optional<CSS::Inset> consumeBasicShapeInsetFunctionParameters(CSSParserTokenRange& args, const CSSParserContext& context)
{
    // <inset()> = inset( <length-percentage>{1,4} [ round <'border-radius'> ]? )
    // https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-inset

    auto insets = consumeBasicShapeInsetInsets(args, context);
    if (!insets)
        return { };

    std::optional<CSS::BorderRadius> radii;
    if (consumeIdent<CSSValueRound>(args)) {
        radii = consumeUnresolvedBorderRadius(args, context);
        if (!radii)
            return { };
    }

    return CSS::Inset {
        .insets = WTFMove(*insets),
        .radii = radii.value_or(CSS::BorderRadius::defaultValue())
    };
}

// MARK: - <basic-shape>

RefPtr<CSSValue> consumeBasicShape(CSSParserTokenRange& range, const CSSParserContext& context, OptionSet<PathParsingOption> options)
{
    // <basic-shape> = <circle()> | <ellipse() | <inset()> | <path()> | <polygon()> | <rect()> | <shape()> | <xywh()>
    // https://drafts.csswg.org/css-shapes/#typedef-basic-shape

    auto rangeCopy = range;

    if (rangeCopy.peek().type() != FunctionToken)
        return { };

    auto id = rangeCopy.peek().functionId();
    auto args = consumeFunction(rangeCopy);

    std::optional<CSS::BasicShape> result;
    if (id == CSSValueCircle)
        result = toBasicShape<CSSValueCircle>(consumeBasicShapeCircleFunctionParameters(args, context));
    else if (id == CSSValueEllipse)
        result = toBasicShape<CSSValueEllipse>(consumeBasicShapeEllipseFunctionParameters(args, context));
    else if (id == CSSValuePolygon)
        result = toBasicShape<CSSValuePolygon>(consumeBasicShapePolygonFunctionParameters(args, context));
    else if (id == CSSValueInset)
        result = toBasicShape<CSSValueInset>(consumeBasicShapeInsetFunctionParameters(args, context));
    else if (id == CSSValueRect)
        result = toBasicShape<CSSValueRect>(consumeBasicShapeRectFunctionParameters(args, context));
    else if (id == CSSValueXywh)
        result = toBasicShape<CSSValueXywh>(consumeBasicShapeXywhFunctionParameters(args, context));
    else if (id == CSSValuePath)
        result = toBasicShape<CSSValuePath>(consumeBasicShapePathFunctionParameters(args, context, options));
    else if (id == CSSValueShape)
        result = toBasicShape<CSSValueShape>(consumeBasicShapeShapeFunctionParameters(args, context));

    if (!result || !args.atEnd())
        return { };

    range = rangeCopy;
    return CSSBasicShapeValue::create(WTFMove(*result));
}

RefPtr<CSSValue> consumePath(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <path()> = path( <'fill-rule'>? , <string> )
    // https://drafts.csswg.org/css-shapes/#funcdef-basic-shape-path

    if (range.peek().type() != FunctionToken)
        return nullptr;
    if (range.peek().functionId() != CSSValuePath)
        return nullptr;

    auto args = consumeFunction(range);
    auto result = consumeBasicShapePathFunctionParameters(args, context, { });
    if (!result || !args.atEnd())
        return nullptr;

    return CSSPathValue::create(
        CSS::PathFunction {
            .parameters = WTFMove(*result)
        }
    );
}

RefPtr<CSSValue> consumeShapeOutside(CSSParserTokenRange& range, const CSSParserContext& context)
{
    // <'shape-outside'> = none | [ <basic-shape> || <shape-box> ] | <image>
    // https://drafts.csswg.org/css-shapes-1/#propdef-shape-outside

    if (auto imageOrNoneValue = consumeImageOrNone(range, context))
        return imageOrNoneValue;

    CSSValueListBuilder list;
    auto boxValue = CSSPropertyParsing::consumeShapeBox(range);
    bool hasShapeValue = false;

    // FIXME: The spec says we should allows `path()` functions.
    if (RefPtr basicShape = consumeBasicShape(range, context, PathParsingOption::RejectPath)) {
        list.append(basicShape.releaseNonNull());
        hasShapeValue = true;
    }
    if (!boxValue)
        boxValue = CSSPropertyParsing::consumeShapeBox(range);

    // margin-box is the default.
    if (boxValue && (boxValue->valueID() != CSSValueMarginBox || !hasShapeValue))
        list.append(boxValue.releaseNonNull());

    if (list.isEmpty())
        return nullptr;

    return CSSValueList::createSpaceSeparated(WTFMove(list));
}

} // namespace CSSPropertyParserHelpers
} // namespace WebCore
