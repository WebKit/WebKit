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
#include "CSSPropertyParserState.h"
#include "CSSPropertyParsing.h"
#include "CSSValueKeywords.h"
#include "CSSValueList.h"
#include <wtf/SortedArrayMap.h>

namespace WebCore {
namespace CSSPropertyParserHelpers {

using namespace CSS::Literals;

template<CSSValueID Name, typename T> CSS::BasicShape toBasicShape(T&& parameters)
{
    return CSS::BasicShape { FunctionNotation<Name, T> { WTFMove(parameters) } };
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
        { CSSValueNonzero, CSS::FillRule { CSS::Keyword::Nonzero { } } },
        { CSSValueEvenodd, CSS::FillRule { CSS::Keyword::Evenodd { } } },
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

template<typename Container> static std::optional<Container> consumePair(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    auto rangeCopy = range;

    auto p1 = MetaConsumer<typename Container::value_type>::consume(rangeCopy, state);
    if (!p1)
        return { };
    auto p2 = MetaConsumer<typename Container::value_type>::consume(rangeCopy, state);
    if (!p2)
        return { };

    range = rangeCopy;
    return Container { WTFMove(*p1), WTFMove(*p2) };
}

static std::optional<CSS::CoordinatePair> consumeCoordinatePair(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <coordinate-pair> = <length-percentage>{2}
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-coordinate-pair

    return consumePair<CSS::CoordinatePair>(range, state);
}

static std::optional<CSS::RelativeControlPoint> consumeRelativeControlPoint(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <relative-control-point> = [<coordinate-pair> [from [start | end | origin]]?]
    // Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    using Anchor = CSS::RelativeControlPoint::Anchor;

    static constexpr std::pair<CSSValueID, Anchor> anchorMappings[] {
        { CSSValueStart, Anchor { CSS::Keyword::Start { } } },
        { CSSValueEnd, Anchor { CSS::Keyword::End { } } },
        { CSSValueOrigin, Anchor { CSS::Keyword::Origin { } } },
    };
    static constexpr SortedArrayMap anchorMap { anchorMappings };

    auto rangeCopy = range;

    auto offset = consumeCoordinatePair(rangeCopy, state);
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

static std::optional<CSS::AbsoluteControlPoint> consumeAbsoluteControlPoint(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <to-control-point> = [<position> | <relative-control-point>]
    // Specified https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    // FIXME: [<length-percentage> <length-percentage>] is valid for both <position> and <relative-control-point>.
    // We currently try <relative-control-point> first, but this ambiguity should probably be explicitly resolved in the spec.

    if (auto relativeControlPoint = consumeRelativeControlPoint(range, state)) {
        return CSS::AbsoluteControlPoint {
            .offset = CSS::Position {
                CSS::TwoComponentPositionHorizontalVertical {
                    { relativeControlPoint->offset.x() },
                    { relativeControlPoint->offset.y() }
                }
            },
            .anchor = relativeControlPoint->anchor
        };
    }
    if (auto position = consumePositionUnresolved(range, state)) {
        return CSS::AbsoluteControlPoint {
            .offset = WTFMove(*position),
            .anchor = std::nullopt
        };
    }
    return { };
}

static CSS::Circle::RadialSize consumeCircleRadialSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
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
        { CSSValueClosestSide, CSS::Circle::Extent { CSS::Keyword::ClosestSide { } } },
        { CSSValueClosestCorner, CSS::Circle::Extent { CSS::Keyword::ClosestCorner { } } },
        { CSSValueFarthestSide, CSS::Circle::Extent { CSS::Keyword::FarthestSide { } } },
        { CSSValueFarthestCorner, CSS::Circle::Extent { CSS::Keyword::FarthestCorner { } } },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    // Default to `closest-side` if no radial-size is provided.
    // FIXME: The spec says that `farthest-corner` should be the default, but this does not match the tests.
    auto defaultValue = [] {
        return CSS::Circle::RadialSize { CSS::Circle::Extent { CSS::Keyword::ClosestSide { } } };
    };

    if (range.peek().type() == IdentToken) {
        if (auto extent = consumeIdentUsingMapping(range, extentMap))
            return *extent;
        return defaultValue();
    }

    auto length = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state);
    if (!length)
        return defaultValue();

    return CSS::Circle::RadialSize { *length };
}

static std::optional<CSS::Circle> consumeBasicShapeCircleFunctionParameters(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <circle()> = circle( <radial-size>? [ at <position> ]? )
    // https://drafts.csswg.org/css-shapes/#funcdef-basic-shape-circle

    auto radius = consumeCircleRadialSize(args, state);

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueAt>(args)) {
        position = consumePositionUnresolved(args, state);
        if (!position)
            return { };
    }

    return CSS::Circle {
        .radius = WTFMove(radius),
        .position = WTFMove(position)
    };
}

static std::optional<CSS::Ellipse::RadialSize> consumeEllipseRadialSize(CSSParserTokenRange& range, CSS::PropertyParserState& state)
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
        { CSSValueClosestSide, CSS::Ellipse::Extent { CSS::Keyword::ClosestSide { } } },
        { CSSValueClosestCorner, CSS::Ellipse::Extent { CSS::Keyword::ClosestCorner { } } },
        { CSSValueFarthestSide, CSS::Ellipse::Extent { CSS::Keyword::FarthestSide { } } },
        { CSSValueFarthestCorner, CSS::Ellipse::Extent { CSS::Keyword::FarthestCorner { } } },
    };
    static constexpr SortedArrayMap extentMap { extentMappings };

    if (range.peek().type() == IdentToken) {
        if (auto extent = consumeIdentUsingMapping(range, extentMap))
            return CSS::Ellipse::RadialSize { *extent };
        return std::nullopt;
    }

    auto length = MetaConsumer<CSS::LengthPercentage<CSS::Nonnegative>>::consume(range, state);
    if (!length)
        return std::nullopt;

    return CSS::Ellipse::RadialSize { *length };
}

static std::optional<CSS::Ellipse> consumeBasicShapeEllipseFunctionParameters(CSSParserTokenRange& args, CSS::PropertyParserState& state)
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

    auto consumeRadialSizePair = [&] -> std::optional<SpaceSeparatedPair<CSS::Ellipse::RadialSize>> {
        if (auto radiusX = consumeEllipseRadialSize(args, state)) {
            auto radiusY = consumeEllipseRadialSize(args, state);
            if (!radiusY)
                return std::nullopt;
            return SpaceSeparatedPair<CSS::Ellipse::RadialSize> { WTFMove(*radiusX), WTFMove(*radiusY) };
        }

        return SpaceSeparatedPair<CSS::Ellipse::RadialSize> {
            CSS::Ellipse::RadialSize { CSS::Ellipse::Extent { CSS::Keyword::ClosestSide { } } },
            CSS::Ellipse::RadialSize { CSS::Ellipse::Extent { CSS::Keyword::ClosestSide { } } }
        };
    };
    auto radii = consumeRadialSizePair();
    if (!radii)
        return std::nullopt;

    std::optional<CSS::Position> position;
    if (consumeIdent<CSSValueAt>(args)) {
        position = consumePositionUnresolved(args, state);
        if (!position)
            return { };
    }

    return CSS::Ellipse {
        .radii = WTFMove(*radii),
        .position = WTFMove(position)
    };
}

static std::optional<CSS::Polygon> consumeBasicShapePolygonFunctionParameters(CSSParserTokenRange& args, CSS::PropertyParserState& state)
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

    CSS::Polygon::Vertices::Container vertices;
    do {
        auto vertex = consumePair<CSS::Polygon::Vertex>(args, state);
        if (!vertex)
            return { };
        vertices.append(WTFMove(*vertex));
    } while (consumeCommaIncludingWhitespace(args));

    return CSS::Polygon {
        .fillRule = WTFMove(fillRule),
        .vertices = { WTFMove(vertices) }
    };
}

static std::optional<CSS::Path> consumeBasicShapePathFunctionParameters(CSSParserTokenRange& args, CSS::PropertyParserState&, OptionSet<PathParsingOption> options)
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

static std::optional<CSS::CommandAffinity> consumeShapeCommandAffinity(CSSParserTokenRange& range, CSS::PropertyParserState&)
{
    // <by-to> = by | to
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-by-to

    static constexpr std::pair<CSSValueID, CSS::CommandAffinity> affinityMappings[] {
        { CSSValueTo, CSS::CommandAffinity { CSS::Keyword::To { } } },
        { CSSValueBy, CSS::CommandAffinity { CSS::Keyword::By { } } },
    };
    static constexpr SortedArrayMap affinityMap { affinityMappings };

    return consumeIdentUsingMapping(range, affinityMap);
}

static std::optional<CSS::MoveCommand> consumeShapeMoveCommand(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <move-command> = move [to <position>] | [by <coordinate-pair>]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-move-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, state);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::Keyword::To) -> std::optional<CSS::MoveCommand> {
            auto position = consumePositionUnresolved(range, state);
            if (!position)
                return std::nullopt;
            return CSS::MoveCommand {
                .toBy = CSS::MoveCommand::To { .offset = WTFMove(*position) }
            };
        },
        [&](CSS::Keyword::By) -> std::optional<CSS::MoveCommand> {
            auto coordinatePair = consumeCoordinatePair(range, state);
            if (!coordinatePair)
                return std::nullopt;
            return CSS::MoveCommand {
                .toBy = CSS::MoveCommand::By { .offset = WTFMove(*coordinatePair) }
            };
        }
    );
}

static std::optional<CSS::LineCommand> consumeShapeLineCommand(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <line-command> = line [to <position>] | [by <coordinate-pair>]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, state);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::Keyword::To) -> std::optional<CSS::LineCommand> {
            auto position = consumePositionUnresolved(range, state);
            if (!position)
                return { };
            return CSS::LineCommand {
                .toBy = CSS::LineCommand::To { .offset = WTFMove(*position) }
            };
        },
        [&](CSS::Keyword::By) -> std::optional<CSS::LineCommand> {
            auto coordinatePair = consumeCoordinatePair(range, state);
            if (!coordinatePair)
                return { };
            return CSS::LineCommand {
                .toBy = CSS::LineCommand::By { .offset = WTFMove(*coordinatePair) }
            };
        }
    );
}

static std::optional<CSS::HLineCommand> consumeShapeHLineCommand(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <horizontal-line-command> = hline [ to [ <length-percentage> | left | center | right | x-start | x-end ] | by <length-percentage> ]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2426552611
    auto affinity = consumeShapeCommandAffinity(range, state);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::Keyword::To) -> std::optional<CSS::HLineCommand> {
            auto offset = consumeTwoComponentPositionHorizontalUnresolved(range, state);
            if (!offset)
                return { };
            return CSS::HLineCommand {
                .toBy = CSS::HLineCommand::To { .offset = WTFMove(*offset) }
            };
        },
        [&](CSS::Keyword::By) -> std::optional<CSS::HLineCommand> {
            auto offset = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state);
            if (!offset)
                return { };
            return CSS::HLineCommand {
                .toBy = CSS::HLineCommand::By { .offset = WTFMove(*offset) }
            };
        }
    );
}

static std::optional<CSS::VLineCommand> consumeShapeVLineCommand(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <hv-line-command> = [... | vline] <by-to> <length-percentage>
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-hv-line-command

    auto affinity = consumeShapeCommandAffinity(range, state);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::Keyword::To) -> std::optional<CSS::VLineCommand> {
            auto offset = consumeTwoComponentPositionVerticalUnresolved(range, state);
            if (!offset)
                return { };
            return CSS::VLineCommand {
                .toBy = CSS::VLineCommand::To { .offset = WTFMove(*offset) }
            };
        },
        [&](CSS::Keyword::By) -> std::optional<CSS::VLineCommand> {
            auto offset = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state);
            if (!offset)
                return { };
            return CSS::VLineCommand {
                .toBy = CSS::VLineCommand::By { .offset = WTFMove(*offset) }
            };
        }
    );
}

static std::optional<CSS::CurveCommand> consumeShapeCurveCommand(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <curve-command> = curve [to <position> with <to-control-point> [/ <to-control-point>]?]
    //                       | [by <coordinate-pair> with <relative-control-point> [/ <relative-control-point>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-curve-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, state);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::Keyword::To) -> std::optional<CSS::CurveCommand> {
            auto position = consumePositionUnresolved(range, state);
            if (!position)
                return { };

            if (!consumeIdent<CSSValueWith>(range))
                return { };

            auto controlPoint1 = consumeAbsoluteControlPoint(range, state);
            if (!controlPoint1)
                return { };

            if (consumeSlashIncludingWhitespace(range)) {
                auto controlPoint2 = consumeAbsoluteControlPoint(range, state);
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
        [&](CSS::Keyword::By) -> std::optional<CSS::CurveCommand> {
            auto coordinatePair = consumeCoordinatePair(range, state);
            if (!coordinatePair)
                return { };

            if (!consumeIdent<CSSValueWith>(range))
                return { };

            auto controlPoint1 = consumeRelativeControlPoint(range, state);
            if (!controlPoint1)
                return { };

            if (consumeSlashIncludingWhitespace(range)) {
                auto controlPoint2 = consumeRelativeControlPoint(range, state);
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

static std::optional<CSS::SmoothCommand> consumeShapeSmoothCommand(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <smooth-command> = smooth [to <position> [with <to-control-point>]?]
    //                         | [by <coordinate-pair> [with <relative-control-point>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-smooth-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, state);
    if (!affinity)
        return { };

    return WTF::switchOn(*affinity,
        [&](CSS::Keyword::To) -> std::optional<CSS::SmoothCommand> {
            auto position = consumePositionUnresolved(range, state);
            if (!position)
                return { };

            if (consumeIdent<CSSValueWith>(range)) {
                auto controlPoint = consumeAbsoluteControlPoint(range, state);
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
        [&](CSS::Keyword::By) -> std::optional<CSS::SmoothCommand> {
            auto coordinatePair = consumeCoordinatePair(range, state);
            if (!coordinatePair)
                return { };

            if (consumeIdent<CSSValueWith>(range)) {
                auto controlPoint = consumeRelativeControlPoint(range, state);
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

static std::optional<CSS::ArcCommand> consumeShapeArcCommand(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <arc-command> = arc [to <position>] | [by <coordinate-pair>] of <length-percentage>{1,2} [<arc-sweep>? || <arc-size>? || [rotate <angle>]?]
    // https://drafts.csswg.org/css-shapes-2/#typedef-shape-arc-command
    // Modified by https://github.com/w3c/csswg-drafts/issues/10649#issuecomment-2412816773

    auto affinity = consumeShapeCommandAffinity(range, state);
    if (!affinity)
        return { };

    using ToBy = Variant<CSS::ArcCommand::To, CSS::ArcCommand::By>;
    auto toBy = WTF::switchOn(*affinity,
        [&](CSS::Keyword::To) -> std::optional<ToBy> {
            auto position = consumePositionUnresolved(range, state);
            if (!position)
                return { };
            return ToBy { CSS::ArcCommand::To { WTFMove(*position) } };
        },
        [&](CSS::Keyword::By) -> std::optional<ToBy> {
            auto coordinatePair = consumeCoordinatePair(range, state);
            if (!coordinatePair)
                return { };
            return ToBy { CSS::ArcCommand::By { WTFMove(*coordinatePair) } };
        }
    );
    if (!toBy)
        return { };

    if (!consumeIdent<CSSValueOf>(range))
        return { };

    auto length1 = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state);
    if (!length1)
        return { };
    auto length2 = MetaConsumer<CSS::LengthPercentage<>>::consume(range, state);
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
            arcSweep = CSS::Keyword::Cw { };
            break;
        case CSSValueCcw:
            if (arcSweep)
                return { };
            arcSweep = CSS::Keyword::Ccw { };
            break;

        case CSSValueLarge:
            if (arcSize)
                return { };
            arcSize = CSS::Keyword::Large { };
            break;

        case CSSValueSmall:
            if (arcSize)
                return { };
            arcSize = CSS::Keyword::Small { };
            break;

        case CSSValueRotate:
            if (angle)
                return { };

            angle = MetaConsumer<CSS::Angle<>>::consume(range, state);
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
        .arcSweep = arcSweep.value_or(CSS::ArcSweep { CSS::Keyword::Ccw { } }),
        .arcSize = arcSize.value_or(CSS::ArcSize { CSS::Keyword::Small { } }),
        .rotation = angle.value_or(0_css_deg)
    };
}

static std::optional<CSS::ShapeCommand> consumeShapeCommand(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    if (range.peek().type() != IdentToken)
        return { };

    auto id = range.consumeIncludingWhitespace().id();
    switch (id) {
    case CSSValueMove:
        if (auto command = consumeShapeMoveCommand(range, state))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueLine:
        if (auto command = consumeShapeLineCommand(range, state))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueHline:
        if (auto command = consumeShapeHLineCommand(range, state))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueVline:
        if (auto command = consumeShapeVLineCommand(range, state))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueCurve:
        if (auto command = consumeShapeCurveCommand(range, state))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueSmooth:
        if (auto command = consumeShapeSmoothCommand(range, state))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueArc:
        if (auto command = consumeShapeArcCommand(range, state))
            return CSS::ShapeCommand { WTFMove(*command) };
        break;

    case CSSValueClose:
        return CSS::ShapeCommand { CSS::CloseCommand { } };

    default:
        break;
    }

    return { };
}

static std::optional<CSS::Shape> consumeBasicShapeShapeFunctionParameters(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // shape() = shape( <'fill-rule'>? from <coordinate-pair>, <shape-command># )
    // https://drafts.csswg.org/css-shapes-2/#shape-function

    if (!state.context.cssShapeFunctionEnabled)
        return { };

    auto fillRule = consumeFillRule(args);

    if (!consumeIdent<CSSValueFrom>(args))
        return { };

    // FIXME: The spec says this should be a <coordinate-pair>, but the tests and some comments indicate it has changed to position.
    auto startingPoint = consumePositionUnresolved(args, state);
    if (!startingPoint)
        return { };

    if (!consumeCommaIncludingWhitespace(args))
        return { };

    CSS::Shape::Commands::Container commands;
    do {
        auto command = consumeShapeCommand(args, state);
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

static std::optional<CSS::Rect::Edge> consumeBasicShapeRectEdge(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <rect-edge> = [ <length-percentage> | auto ]

    if (args.peek().type() == IdentToken) {
        if (args.peek().id() == CSSValueAuto) {
            args.consumeIncludingWhitespace();
            return { CSS::Keyword::Auto { } };
        }
        return { };
    }

    if (auto edge = MetaConsumer<CSS::LengthPercentage<>>::consume(args, state))
        return { WTFMove(*edge) };

    return { };
}

static std::optional<SpaceSeparatedRectEdges<CSS::Rect::Edge>> consumeBasicShapeRectEdges(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <rect-edges> = <rect-edge>{4}

    auto top = consumeBasicShapeRectEdge(args, state);
    if (!top)
        return { };
    auto right = consumeBasicShapeRectEdge(args, state);
    if (!right)
        return { };
    auto bottom = consumeBasicShapeRectEdge(args, state);
    if (!bottom)
        return { };
    auto left = consumeBasicShapeRectEdge(args, state);
    if (!left)
        return { };
    return SpaceSeparatedRectEdges<CSS::Rect::Edge> { WTFMove(*top), WTFMove(*right), WTFMove(*bottom), WTFMove(*left) };
}

static std::optional<CSS::Rect> consumeBasicShapeRectFunctionParameters(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <rect()> = rect( [ <length-percentage> | auto ]{4} [ round <'border-radius'> ]? )
    // https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-rect

    auto edges = consumeBasicShapeRectEdges(args, state);
    if (!edges)
        return { };

    std::optional<CSS::BorderRadius> radii;
    if (consumeIdent<CSSValueRound>(args)) {
        radii = consumeUnresolvedBorderRadius(args, state);
        if (!radii)
            return { };
    }

    return CSS::Rect {
        .edges = WTFMove(*edges),
        .radii = radii.value_or(CSS::BorderRadius::defaultValue())
    };
}

// MARK: - <xywh()>

static std::optional<CSS::Xywh> consumeBasicShapeXywhFunctionParameters(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <xywh()> = xywh( <length-percentage>{2} <length-percentage [0,∞]>{2} [ round <'border-radius'> ]? )
    // https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-xywh

    auto location = consumePair<CSS::Xywh::Location>(args, state);
    if (!location)
        return { };
    auto size = consumePair<CSS::Xywh::Size>(args, state);
    if (!size)
        return { };

    std::optional<CSS::BorderRadius> radii;
    if (consumeIdent<CSSValueRound>(args)) {
        radii = consumeUnresolvedBorderRadius(args, state);
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

static std::optional<CSS::Inset::Insets> consumeBasicShapeInsetInsets(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <insets> = <length-percentage>{1,4}

    auto inset1 = MetaConsumer<CSS::LengthPercentage<>>::consume(args, state);
    if (!inset1)
        return { };

    auto inset2 = MetaConsumer<CSS::LengthPercentage<>>::consume(args, state);
    if (!inset2)
        return completeQuad<CSS::Inset::Insets>(WTFMove(*inset1));

    auto inset3 = MetaConsumer<CSS::LengthPercentage<>>::consume(args, state);
    if (!inset3)
        return completeQuad<CSS::Inset::Insets>(WTFMove(*inset1), WTFMove(*inset2));

    auto inset4 = MetaConsumer<CSS::LengthPercentage<>>::consume(args, state);
    if (!inset4)
        return completeQuad<CSS::Inset::Insets>(WTFMove(*inset1), WTFMove(*inset2), WTFMove(*inset3));

    return CSS::Inset::Insets { WTFMove(*inset1), WTFMove(*inset2), WTFMove(*inset3), WTFMove(*inset4) };
}

static std::optional<CSS::Inset> consumeBasicShapeInsetFunctionParameters(CSSParserTokenRange& args, CSS::PropertyParserState& state)
{
    // <inset()> = inset( <length-percentage>{1,4} [ round <'border-radius'> ]? )
    // https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-inset

    auto insets = consumeBasicShapeInsetInsets(args, state);
    if (!insets)
        return { };

    std::optional<CSS::BorderRadius> radii;
    if (consumeIdent<CSSValueRound>(args)) {
        radii = consumeUnresolvedBorderRadius(args, state);
        if (!radii)
            return { };
    }

    return CSS::Inset {
        .insets = WTFMove(*insets),
        .radii = radii.value_or(CSS::BorderRadius::defaultValue())
    };
}

// MARK: - <basic-shape>

RefPtr<CSSValue> consumeBasicShape(CSSParserTokenRange& range, CSS::PropertyParserState& state, OptionSet<PathParsingOption> options)
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
        result = toBasicShape<CSSValueCircle>(consumeBasicShapeCircleFunctionParameters(args, state));
    else if (id == CSSValueEllipse)
        result = toBasicShape<CSSValueEllipse>(consumeBasicShapeEllipseFunctionParameters(args, state));
    else if (id == CSSValuePolygon)
        result = toBasicShape<CSSValuePolygon>(consumeBasicShapePolygonFunctionParameters(args, state));
    else if (id == CSSValueInset)
        result = toBasicShape<CSSValueInset>(consumeBasicShapeInsetFunctionParameters(args, state));
    else if (id == CSSValueRect)
        result = toBasicShape<CSSValueRect>(consumeBasicShapeRectFunctionParameters(args, state));
    else if (id == CSSValueXywh)
        result = toBasicShape<CSSValueXywh>(consumeBasicShapeXywhFunctionParameters(args, state));
    else if (id == CSSValuePath)
        result = toBasicShape<CSSValuePath>(consumeBasicShapePathFunctionParameters(args, state, options));
    else if (id == CSSValueShape)
        result = toBasicShape<CSSValueShape>(consumeBasicShapeShapeFunctionParameters(args, state));

    if (!result || !args.atEnd())
        return { };

    range = rangeCopy;
    return CSSBasicShapeValue::create(WTFMove(*result));
}

RefPtr<CSSValue> consumePath(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <path()> = path( <'fill-rule'>? , <string> )
    // https://drafts.csswg.org/css-shapes/#funcdef-basic-shape-path

    if (range.peek().type() != FunctionToken)
        return nullptr;
    if (range.peek().functionId() != CSSValuePath)
        return nullptr;

    auto args = consumeFunction(range);
    auto result = consumeBasicShapePathFunctionParameters(args, state, { });
    if (!result || !args.atEnd())
        return nullptr;

    return CSSPathValue::create(
        CSS::PathFunction {
            .parameters = WTFMove(*result)
        }
    );
}

RefPtr<CSSValue> consumeShapeOutside(CSSParserTokenRange& range, CSS::PropertyParserState& state)
{
    // <'shape-outside'> = none | [ <basic-shape> || <shape-box> ] | <image>
    // https://drafts.csswg.org/css-shapes-1/#propdef-shape-outside

    if (auto imageOrNoneValue = consumeImageOrNone(range, state))
        return imageOrNoneValue;

    CSSValueListBuilder list;
    auto boxValue = CSSPropertyParsing::consumeShapeBox(range);
    bool hasShapeValue = false;

    // FIXME: The spec says we should allows `path()` functions.
    if (RefPtr basicShape = consumeBasicShape(range, state, PathParsingOption::RejectPath)) {
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
