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

#include "config.h"
#include "StyleShapeFunction.h"

#include "FloatConversion.h"
#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "SVGPathBuilder.h"
#include "SVGPathByteStreamSource.h"
#include "SVGPathParser.h"
#include "SVGPathSource.h"
#include "StylePathFunction.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"

namespace WebCore {
namespace Style {

// MARK: - Offset Point Evaluation

static FloatPoint evaluate(const ByCoordinatePair& value, const FloatSize& boxSize)
{
    return evaluate(value.offset, boxSize);
}

static FloatPoint evaluate(const ToPosition& value, const FloatSize& boxSize)
{
    return evaluate(value.offset, boxSize);
}

static FloatPoint evaluate(const std::variant<ToPosition, ByCoordinatePair>& value, const FloatSize& boxSize)
{
    return WTF::switchOn(value, [&](const auto& value) -> FloatPoint { return evaluate(value, boxSize); });
}

static float evaluate(const std::variant<HLineCommand::To, HLineCommand::By>& value, float width)
{
    return WTF::switchOn(value, [&](const auto& value) -> float { return evaluate(value.offset, width); });
}

static float evaluate(const std::variant<VLineCommand::To, VLineCommand::By>& value, float height)
{
    return WTF::switchOn(value, [&](const auto& value) -> float { return evaluate(value.offset, height); });
}

// MARK: - Control Point Evaluation

template<typename ControlPoint> static ControlPointAnchor evaluateControlPointAnchoring(const ControlPoint& value, ControlPointAnchor defaultValue)
{
    if (value.anchor)
        return *value.anchor;
    return defaultValue;
}

template<typename ControlPoint> static FloatPoint evaluateControlPointOffset(const ControlPoint& value, const FloatSize& boxSize)
{
    return evaluate(value.offset, boxSize);
}

template<typename ControlPoint> static FloatPoint resolveControlPoint(CommandAffinity affinity, FloatPoint currentPosition, FloatPoint segmentOffset, const ControlPoint& controlPoint, const FloatSize& boxSize)
{
    auto controlPointOffset = evaluateControlPointOffset(controlPoint, boxSize);

    auto defaultAnchor = (std::holds_alternative<By>(affinity)) ? RelativeControlPoint::defaultAnchor : AbsoluteControlPoint::defaultAnchor;
    auto controlPointAnchoring = evaluateControlPointAnchoring(controlPoint, defaultAnchor);

    auto absoluteControlPoint = WTF::switchOn(controlPointAnchoring,
        [&](Style::Start) {
            auto absoluteStartPoint = currentPosition;
            return absoluteStartPoint + controlPointOffset;
        },
        [&](Style::End) {
            auto absoluteEndPoint = (std::holds_alternative<By>(affinity)) ? currentPosition + toFloatSize(segmentOffset) : segmentOffset;
            return absoluteEndPoint + controlPointOffset;
        },
        [&](Style::Origin) {
            return controlPointOffset;
        }
    );

    if (std::holds_alternative<By>(affinity))
        return absoluteControlPoint - toFloatSize(currentPosition);
    return absoluteControlPoint;
}

// MARK: - ShapeSVGPathSource

class ShapeSVGPathSource final : public SVGPathSource {
public:
    explicit ShapeSVGPathSource(const Position& startPoint, const Shape& shape, const FloatSize& boxSize)
        : m_start(startPoint)
        , m_shape(shape)
        , m_boxSize(boxSize)
        , m_endIndex(shape.commands.size())
    {
    }

private:
    bool hasMoreData() const override
    {
        return m_nextIndex < m_endIndex;
    }

    bool moveToNextToken() override { return true; }

    SVGPathSegType nextCommand(SVGPathSegType) override
    {
        auto type = segmentTypeAtIndex(m_nextIndex);
        ++m_nextIndex;
        return type;
    }

    std::optional<SVGPathSegType> parseSVGSegmentType() override
    {
        // This represents the initial move to to set the "from" position.
        ASSERT(!m_nextIndex);
        return SVGPathSegType::MoveToAbs;
    }

    std::optional<MoveToSegment> parseMoveToSegment(FloatPoint) override
    {
        if (!m_nextIndex)
            return MoveToSegment { evaluate(m_start, m_boxSize) };

        auto& moveCommand = currentValue<MoveCommand>();

        return MoveToSegment { evaluate(moveCommand.toBy, m_boxSize) };
    }

    std::optional<LineToSegment> parseLineToSegment(FloatPoint) override
    {
        auto& lineCommand = currentValue<LineCommand>();

        return LineToSegment { evaluate(lineCommand.toBy, m_boxSize) };
    }

    std::optional<LineToHorizontalSegment> parseLineToHorizontalSegment(FloatPoint) override
    {
        auto& lineCommand = currentValue<HLineCommand>();

        return LineToHorizontalSegment { evaluate(lineCommand.toBy, m_boxSize.width()) };
    }

    std::optional<LineToVerticalSegment> parseLineToVerticalSegment(FloatPoint) override
    {
        auto& lineCommand = currentValue<VLineCommand>();

        return LineToVerticalSegment { evaluate(lineCommand.toBy, m_boxSize.height()) };
    }

    std::optional<CurveToCubicSegment> parseCurveToCubicSegment(FloatPoint currentPosition) override
    {
        auto& curveCommand = currentValue<CurveCommand>();

        return WTF::switchOn(curveCommand.toBy,
            [&](const auto& value) {
                auto offset = evaluate(value.offset, m_boxSize);
                return CurveToCubicSegment {
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint1, m_boxSize),
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint2.value(), m_boxSize),
                    offset
                };
            }
        );
    }

    std::optional<CurveToQuadraticSegment> parseCurveToQuadraticSegment(FloatPoint currentPosition) override
    {
        auto& curveCommand = currentValue<CurveCommand>();

        return WTF::switchOn(curveCommand.toBy,
            [&](const auto& value) {
                auto offset = evaluate(value.offset, m_boxSize);
                return CurveToQuadraticSegment {
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint1, m_boxSize),
                    offset
                };
            }
        );
    }

    std::optional<CurveToCubicSmoothSegment> parseCurveToCubicSmoothSegment(FloatPoint currentPosition) override
    {
        auto& smoothCommand = currentValue<SmoothCommand>();

        return WTF::switchOn(smoothCommand.toBy,
            [&](const auto& value) {
                ASSERT(value.controlPoint);
                auto offset = evaluate(value.offset, m_boxSize);
                return CurveToCubicSmoothSegment {
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint.value(), m_boxSize),
                    offset
                };
            }
        );
    }

    std::optional<CurveToQuadraticSmoothSegment> parseCurveToQuadraticSmoothSegment(FloatPoint) override
    {
        auto& smoothCommand = currentValue<SmoothCommand>();

        return WTF::switchOn(smoothCommand.toBy,
            [&](const auto& value) {
                return CurveToQuadraticSmoothSegment {
                    evaluate(value.offset, m_boxSize)
                };
            }
        );
    }

    std::optional<ArcToSegment> parseArcToSegment(FloatPoint) override
    {
        auto& arcCommand = currentValue<ArcCommand>();

        auto radius = evaluate(arcCommand.size, m_boxSize);
        return ArcToSegment {
            .rx = radius.width(),
            .ry = radius.height(),
            .angle = narrowPrecisionToFloat(arcCommand.rotation.value),
            .largeArc = std::holds_alternative<Large>(arcCommand.arcSize),
            .sweep = std::holds_alternative<Cw>(arcCommand.arcSweep),
            .targetPoint = evaluate(arcCommand.toBy, m_boxSize)
        };
    }

    SVGPathSegType segmentTypeAtIndex(size_t index) const
    {
        if (index >= m_shape.commands.size())
            return SVGPathSegType::Unknown;

        return WTF::switchOn(m_shape.commands[index],
            [&](const MoveCommand& command) {
                return std::holds_alternative<MoveCommand::To>(command.toBy) ? SVGPathSegType::MoveToAbs : SVGPathSegType::MoveToRel;
            },
            [&](const LineCommand& command) {
                return std::holds_alternative<LineCommand::To>(command.toBy) ? SVGPathSegType::LineToAbs : SVGPathSegType::LineToRel;
            },
            [&](const HLineCommand& command) {
                return std::holds_alternative<HLineCommand::To>(command.toBy) ? SVGPathSegType::LineToHorizontalAbs : SVGPathSegType::LineToHorizontalRel;
            },
            [&](const VLineCommand& command) {
                return std::holds_alternative<VLineCommand::To>(command.toBy) ? SVGPathSegType::LineToVerticalAbs : SVGPathSegType::LineToVerticalRel;
            },
            [&](const CurveCommand& command) {
                return WTF::switchOn(command.toBy,
                    [](const auto& value) {
                        if (value.controlPoint2)
                            return std::holds_alternative<To>(value.affinity) ? SVGPathSegType::CurveToCubicAbs : SVGPathSegType::CurveToCubicRel;
                        return std::holds_alternative<To>(value.affinity) ? SVGPathSegType::CurveToQuadraticAbs : SVGPathSegType::CurveToQuadraticRel;
                    }
                );
            },
            [&](const SmoothCommand& command) {
                return WTF::switchOn(command.toBy,
                    [](const auto& value) {
                        if (value.controlPoint)
                            return std::holds_alternative<To>(value.affinity) ? SVGPathSegType::CurveToCubicSmoothAbs : SVGPathSegType::CurveToCubicSmoothRel;
                        return std::holds_alternative<To>(value.affinity) ? SVGPathSegType::CurveToQuadraticSmoothAbs : SVGPathSegType::CurveToQuadraticSmoothRel;
                    }
                );
            },
            [&](const ArcCommand& command) {
                return std::holds_alternative<ArcCommand::To>(command.toBy) ? SVGPathSegType::ArcAbs : SVGPathSegType::ArcRel;
            },
            [&](const CloseCommand&) {
                return SVGPathSegType::ClosePath;
            }
        );
    }

    template<typename T>
    const T& currentValue() const
    {
        ASSERT(m_nextIndex);
        ASSERT(m_nextIndex <= m_shape.commands.size());
        return std::get<T>(m_shape.commands[m_nextIndex - 1]);
    }

    Position m_start;
    const Shape& m_shape;
    FloatSize m_boxSize;
    size_t m_endIndex { 0 };
    size_t m_nextIndex { 0 };
};

// MARK: - ShapeConversionPathConsumer

class ShapeConversionPathConsumer final : public SVGPathConsumer {
public:
    ShapeConversionPathConsumer(Vector<ShapeCommand>& commands)
        : m_commands(commands)
    {
    }

    const std::optional<Position>& initialMove() const { return m_initialMove; }

private:
    static Position toPosition(FloatPoint p)
    {
        return { p };
    }

    static CoordinatePair toCoordinatePair(FloatPoint p)
    {
        return toPosition(p).value;
    }

    static Position absoluteOffsetPoint(FloatPoint p)
    {
        return toPosition(p);
    }

    static CoordinatePair relativeOffsetPoint(FloatPoint p)
    {
        return toCoordinatePair(p);
    }

    static std::variant<ToPosition, ByCoordinatePair> fromOffsetPoint(const FloatPoint& offsetPoint, PathCoordinateMode mode)
    {
        switch (mode) {
        case AbsoluteCoordinates:
            return ToPosition { absoluteOffsetPoint(offsetPoint) };
        case RelativeCoordinates:
            return ByCoordinatePair { relativeOffsetPoint(offsetPoint) };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template<typename Command> static std::variant<typename Command::To, typename Command::By> fromOffsetLength(float offset, PathCoordinateMode mode)
    {
        switch (mode) {
        case AbsoluteCoordinates:
            return typename Command::To { .offset = { LengthPercentage<> { Length<> { offset } } } };
        case RelativeCoordinates:
            return typename Command::By { .offset = LengthPercentage<> { Length<> { offset } } };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    static AbsoluteControlPoint absoluteControlPoint(const FloatPoint& controlPoint)
    {
        return { toPosition(controlPoint), std::nullopt };
    }

    static RelativeControlPoint relativeControlPoint(const FloatPoint& controlPoint)
    {
        return { toCoordinatePair(controlPoint), std::nullopt };
    }

    void incrementPathSegmentCount() override
    {
    }

    bool continueConsuming() override
    {
        return true;
    }

    void moveTo(const FloatPoint& offsetPoint, bool, PathCoordinateMode mode) override
    {
        if (m_commands.isEmpty() && mode == PathCoordinateMode::AbsoluteCoordinates && !m_initialMove) {
            m_initialMove = toPosition(offsetPoint);
            return;
        }

        m_commands.append(
            MoveCommand {
                .toBy = fromOffsetPoint(offsetPoint, mode)
            }
        );
    }

    void lineTo(const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        m_commands.append(
            LineCommand {
                .toBy = fromOffsetPoint(offsetPoint, mode)
            }
        );
    }

    void lineToHorizontal(float length, PathCoordinateMode mode) override
    {
        m_commands.append(
            HLineCommand {
                .toBy = fromOffsetLength<HLineCommand>(length, mode)
            }
        );
    }

    void lineToVertical(float length, PathCoordinateMode mode) override
    {
        m_commands.append(
            VLineCommand {
                .toBy = fromOffsetLength<VLineCommand>(length, mode)
            }
        );
    }

    void curveToCubic(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        switch (mode) {
        case AbsoluteCoordinates:
            m_commands.append(
                CurveCommand {
                    .toBy = CurveCommand::To {
                        .offset = absoluteOffsetPoint(offsetPoint),
                        .controlPoint1 = absoluteControlPoint(controlPoint1),
                        .controlPoint2 = absoluteControlPoint(controlPoint2),
                    }
                }
            );
            break;
        case RelativeCoordinates:
            m_commands.append(
                CurveCommand {
                    .toBy = CurveCommand::By {
                        .offset = relativeOffsetPoint(offsetPoint),
                        .controlPoint1 = relativeControlPoint(controlPoint1),
                        .controlPoint2 = relativeControlPoint(controlPoint2),
                    }
                }
            );
            break;
        }
    }

    void curveToQuadratic(const FloatPoint& controlPoint, const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        switch (mode) {
        case AbsoluteCoordinates:
            m_commands.append(
                CurveCommand {
                    .toBy = CurveCommand::To {
                        .offset = absoluteOffsetPoint(offsetPoint),
                        .controlPoint1 = absoluteControlPoint(controlPoint),
                        .controlPoint2 = std::nullopt,
                    }
                }
            );
            break;
        case RelativeCoordinates:
            m_commands.append(
                CurveCommand {
                    .toBy = CurveCommand::By {
                        .offset = relativeOffsetPoint(offsetPoint),
                        .controlPoint1 = relativeControlPoint(controlPoint),
                        .controlPoint2 = std::nullopt,
                    }
                }
            );
            break;
        }
    }

    void curveToCubicSmooth(const FloatPoint& controlPoint, const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        switch (mode) {
        case AbsoluteCoordinates:
            m_commands.append(
                SmoothCommand {
                    .toBy = SmoothCommand::To {
                        .offset = absoluteOffsetPoint(offsetPoint),
                        .controlPoint = absoluteControlPoint(controlPoint),
                    }
                }
            );
            break;
        case RelativeCoordinates:
            m_commands.append(
                SmoothCommand {
                    .toBy = SmoothCommand::By {
                        .offset = relativeOffsetPoint(offsetPoint),
                        .controlPoint = relativeControlPoint(controlPoint),
                    }
                }
            );
            break;
        }
    }

    void curveToQuadraticSmooth(const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        switch (mode) {
        case AbsoluteCoordinates:
            m_commands.append(
                SmoothCommand {
                    .toBy = SmoothCommand::To {
                        .offset = absoluteOffsetPoint(offsetPoint),
                        .controlPoint = std::nullopt,
                    }
                }
            );
            break;
        case RelativeCoordinates:
            m_commands.append(
                SmoothCommand {
                    .toBy = SmoothCommand::By {
                        .offset = relativeOffsetPoint(offsetPoint),
                        .controlPoint = std::nullopt,
                    }
                }
            );
            break;
        }
    }

    void arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        m_commands.append(
            ArcCommand {
                .toBy = fromOffsetPoint(offsetPoint, mode),
                .size = { Length<> { r1 }, Length<> { r2 } },
                .arcSweep = sweepFlag ? ArcSweep { Cw { } } : ArcSweep { Ccw { } },
                .arcSize = largeArcFlag ? ArcSize { Large { } } : ArcSize { Small { } },
                .rotation = { angle },
            }
        );
    }

    void closePath() override
    {
        m_commands.append(CloseCommand { });
    }

    Vector<ShapeCommand>& m_commands;
    std::optional<Position> m_initialMove;
};

template<typename T> T blendWithPreferredValue(const T& from, const T& to, const T& preferredValue, const BlendingContext& context)
{
    if (context.progress <= 0)
        return from;

    if (context.progress >= 1)
        return to;

    if (from == to)
        return from;

    return preferredValue;
}

// MARK: - RelativeControlPoint (blending)

auto Blending<RelativeControlPoint>::canBlend(const RelativeControlPoint& a, const RelativeControlPoint& b) -> bool
{
    return WebCore::Style::canBlend(a.offset, b.offset)
        && a.anchor.value_or(RelativeControlPoint::defaultAnchor) == b.anchor.value_or(RelativeControlPoint::defaultAnchor);
}

auto Blending<RelativeControlPoint>::blend(const RelativeControlPoint& a, const RelativeControlPoint& b, const BlendingContext& context) -> RelativeControlPoint
{
    return {
        .offset = WebCore::Style::blend(a.offset, b.offset, context),
        .anchor = a.anchor.has_value() && b.anchor.has_value() ? a.anchor : std::nullopt
    };
}

// MARK: - AbsoluteControlPoint (blending)

auto Blending<AbsoluteControlPoint>::canBlend(const AbsoluteControlPoint& a, const AbsoluteControlPoint& b) -> bool
{
    return WebCore::Style::canBlend(a.offset, b.offset)
        && a.anchor.value_or(AbsoluteControlPoint::defaultAnchor) == b.anchor.value_or(AbsoluteControlPoint::defaultAnchor);
}

auto Blending<AbsoluteControlPoint>::blend(const AbsoluteControlPoint& a, const AbsoluteControlPoint& b, const BlendingContext& context) -> AbsoluteControlPoint
{
    return {
        .offset = WebCore::Style::blend(a.offset, b.offset, context),
        .anchor = a.anchor.has_value() && b.anchor.has_value() ? a.anchor : std::nullopt
    };
}

// MARK: - ArcCommand (blending)

auto Blending<ArcCommand>::canBlend(const ArcCommand& a, const ArcCommand& b) -> bool
{
    return WebCore::Style::canBlend(a.toBy, b.toBy);
}

auto Blending<ArcCommand>::blend(const ArcCommand& a, const ArcCommand& b, const BlendingContext& context) -> ArcCommand
{
    return {
        .toBy = WebCore::Style::blend(a.toBy, b.toBy, context),
        .size = WebCore::Style::blend(a.size, b.size, context),
        .arcSweep = blendWithPreferredValue(a.arcSweep, b.arcSweep, ArcSweep { Cw { } }, context),
        .arcSize = blendWithPreferredValue(a.arcSize, b.arcSize, ArcSize { Large { } }, context),
        .rotation = WebCore::Style::blend(a.rotation, b.rotation, context),
    };
}

// MARK: - Shape (path conversion)

WebCore::Path PathComputation<Shape>::operator()(const Shape& value, const FloatRect& boundingBox)
{
    // FIXME: We should do some caching here.
    auto pathSource = ShapeSVGPathSource(value.startingPoint, value, boundingBox.size());

    WebCore::Path path;
    SVGPathBuilder builder(path);
    SVGPathParser::parse(pathSource, builder);

    path.translate(toFloatSize(boundingBox.location()));

    return path;
}

// MARK: - Wind Rule

WebCore::WindRule WindRuleComputation<Shape>::operator()(const Shape& value)
{
    return (!value.fillRule || std::holds_alternative<Nonzero>(*value.fillRule)) ? WindRule::NonZero : WindRule::EvenOdd;
}

// MARK: - Shape (blending)

auto Blending<Shape>::canBlend(const Shape& a, const Shape& b) -> bool
{
    return windRule(a) == windRule(b)
        && WebCore::Style::canBlend(a.commands, b.commands);
}

auto Blending<Shape>::blend(const Shape& a, const Shape& b, const BlendingContext& context) -> Shape
{
    return {
        .fillRule = a.fillRule,
        .startingPoint = WebCore::Style::blend(a.startingPoint, b.startingPoint, context),
        .commands = WebCore::Style::blend(a.commands, b.commands, context)
    };
}

bool canBlendShapeWithPath(const Shape& shape, const Path& path)
{
    if (windRule(shape) != windRule(path))
        return false;

    // FIXME: This can be made less expensive by specializing a path
    // consumer to check validity, rather than fully constructing the
    // shape just for the canBlend check.
    //
    // Alternatively, the canBlend() and blend() functions could be
    // merged, allowing for only a single traversal.

    auto shapeFromPath = makeShapeFromPath(path);
    return shapeFromPath && WebCore::Style::canBlend(shape, *shapeFromPath);
}

std::optional<Shape> makeShapeFromPath(const Path& path)
{
    // FIXME: Not clear how to convert a initial Move command to the Shape's "from" parameter.
    // https://github.com/w3c/csswg-drafts/issues/10740

    CommaSeparatedVector<ShapeCommand>::Vector shapeCommands;
    ShapeConversionPathConsumer converter(shapeCommands);
    SVGPathByteStreamSource source(path.data.byteStream);

    if (!SVGPathParser::parse(source, converter, UnalteredParsing))
        return { };

    return Shape {
        .fillRule = path.fillRule,
        .startingPoint = converter.initialMove().value_or(Position { LengthPercentage<> { Length<> { 0 } }, LengthPercentage<> { Length<> { 0 } } }),
        .commands = { WTFMove(shapeCommands) }
    };
}

} // namespace Style
} // namespace WebCore
