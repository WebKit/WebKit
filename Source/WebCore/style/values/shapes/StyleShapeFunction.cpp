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

#include "AcceleratedEffectShapeFunction.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "SVGPathBuilder.h"
#include "SVGPathByteStreamSource.h"
#include "SVGPathParser.h"
#include "SVGPathSource.h"
#include "StyleLengthWrapper+Blending.h"
#include "StylePathFunction.h"
#include "StylePrimitiveNumericTypes+Blending.h"
#include "StylePrimitiveNumericTypes+Evaluation.h"
#include "TransformOperationData.h"

namespace WebCore {
namespace Style {

// MARK: - Control Point Evaluation

template<typename ControlPoint> static ControlPointAnchor NODELETE evaluateControlPointAnchoring(const ControlPoint& value, ControlPointAnchor defaultValue)
{
    if (value.anchor)
        return *value.anchor;
    return defaultValue;
}

template<typename ControlPoint> static FloatPoint evaluateControlPointOffset(const ControlPoint& value, const FloatSize& boxSize, ZoomFactor zoom)
{
    return evaluate<FloatPoint>(value.offset, boxSize, zoom);
}

template<typename ControlPoint> static FloatPoint resolveControlPoint(CommandAffinity affinity, FloatPoint currentPosition, FloatPoint segmentOffset, const ControlPoint& controlPoint, const FloatSize& boxSize, ZoomFactor zoom)
{
    auto controlPointOffset = evaluateControlPointOffset(controlPoint, boxSize, zoom);

    auto defaultAnchor = (std::holds_alternative<CSS::Keyword::By>(affinity)) ? RelativeControlPoint::defaultAnchor : AbsoluteControlPoint::defaultAnchor;
    auto controlPointAnchoring = evaluateControlPointAnchoring(controlPoint, defaultAnchor);

    auto absoluteControlPoint = WTF::switchOn(controlPointAnchoring,
        [&](CSS::Keyword::Start) {
            auto absoluteStartPoint = currentPosition;
            return absoluteStartPoint + controlPointOffset;
        },
        [&](CSS::Keyword::End) {
            auto absoluteEndPoint = (std::holds_alternative<CSS::Keyword::By>(affinity)) ? currentPosition + toFloatSize(segmentOffset) : segmentOffset;
            return absoluteEndPoint + controlPointOffset;
        },
        [&](CSS::Keyword::Origin) {
            return controlPointOffset;
        }
    );

    if (std::holds_alternative<CSS::Keyword::By>(affinity))
        return absoluteControlPoint - toFloatSize(currentPosition);
    return absoluteControlPoint;
}

// MARK: - ShapeSVGPathSource

class ShapeSVGPathSource final : public SVGPathSource {
public:
    explicit ShapeSVGPathSource(const Position& startPoint, const Shape& shape, const FloatSize& boxSize, ZoomFactor zoom)
        : m_start(startPoint)
        , m_shape(shape)
        , m_boxSize(boxSize)
        , m_endIndex(shape.commands.size())
        , m_zoom(zoom)
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
            return MoveToSegment { evaluate<FloatPoint>(m_start, m_boxSize, m_zoom) };

        auto& moveCommand = currentValue<MoveCommand>();

        return MoveToSegment { evaluate<FloatPoint>(moveCommand.toBy, m_boxSize, m_zoom) };
    }

    std::optional<LineToSegment> parseLineToSegment(FloatPoint) override
    {
        auto& lineCommand = currentValue<LineCommand>();

        return LineToSegment { evaluate<FloatPoint>(lineCommand.toBy, m_boxSize, m_zoom) };
    }

    std::optional<LineToHorizontalSegment> parseLineToHorizontalSegment(FloatPoint) override
    {
        auto& lineCommand = currentValue<HLineCommand>();

        return LineToHorizontalSegment { evaluate<float>(lineCommand.toBy, m_boxSize.width(), m_zoom) };
    }

    std::optional<LineToVerticalSegment> parseLineToVerticalSegment(FloatPoint) override
    {
        auto& lineCommand = currentValue<VLineCommand>();

        return LineToVerticalSegment { evaluate<float>(lineCommand.toBy, m_boxSize.height(), m_zoom) };
    }

    std::optional<CurveToCubicSegment> parseCurveToCubicSegment(FloatPoint currentPosition) override
    {
        auto& curveCommand = currentValue<CurveCommand>();

        return WTF::switchOn(curveCommand.toBy,
            [&](const auto& value) {
                auto offset = evaluate<FloatPoint>(value.offset, m_boxSize, m_zoom);
                return CurveToCubicSegment {
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint1, m_boxSize, m_zoom),
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint2.value(), m_boxSize, m_zoom),
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
                auto offset = evaluate<FloatPoint>(value.offset, m_boxSize, m_zoom);
                return CurveToQuadraticSegment {
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint1, m_boxSize, m_zoom),
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
                auto offset = evaluate<FloatPoint>(value.offset, m_boxSize, m_zoom);
                return CurveToCubicSmoothSegment {
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint.value(), m_boxSize, m_zoom),
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
                    evaluate<FloatPoint>(value.offset, m_boxSize, m_zoom)
                };
            }
        );
    }

    std::optional<ArcToSegment> parseArcToSegment(FloatPoint) override
    {
        auto& arcCommand = currentValue<ArcCommand>();

        auto radius = evaluate<FloatSize>(arcCommand.size, m_boxSize, m_zoom);
        return ArcToSegment {
            .rx = radius.width(),
            .ry = radius.height(),
            .angle = narrowPrecisionToFloat(arcCommand.rotation.value),
            .largeArc = std::holds_alternative<CSS::Keyword::Large>(arcCommand.arcSize),
            .sweep = std::holds_alternative<CSS::Keyword::Cw>(arcCommand.arcSweep),
            .targetPoint = evaluate<FloatPoint>(arcCommand.toBy, m_boxSize, m_zoom)
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
                            return std::holds_alternative<CSS::Keyword::To>(value.affinity) ? SVGPathSegType::CurveToCubicAbs : SVGPathSegType::CurveToCubicRel;
                        return std::holds_alternative<CSS::Keyword::To>(value.affinity) ? SVGPathSegType::CurveToQuadraticAbs : SVGPathSegType::CurveToQuadraticRel;
                    }
                );
            },
            [&](const SmoothCommand& command) {
                return WTF::switchOn(command.toBy,
                    [](const auto& value) {
                        if (value.controlPoint)
                            return std::holds_alternative<CSS::Keyword::To>(value.affinity) ? SVGPathSegType::CurveToCubicSmoothAbs : SVGPathSegType::CurveToCubicSmoothRel;
                        return std::holds_alternative<CSS::Keyword::To>(value.affinity) ? SVGPathSegType::CurveToQuadraticSmoothAbs : SVGPathSegType::CurveToQuadraticSmoothRel;
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
    ZoomFactor m_zoom;
};

// MARK: - ShapeConversionPathConsumer

class ShapeConversionPathConsumer final : public SVGPathConsumer {
public:
    ShapeConversionPathConsumer(Vector<ShapeCommand>& commands)
        : m_commands(commands)
    {
    }

    const std::optional<Position>& NODELETE initialMove() const { return m_initialMove; }

private:
    static Position NODELETE toPosition(FloatPoint p)
    {
        return { p };
    }

    static CoordinatePair toCoordinatePair(FloatPoint p)
    {
        return { CoordinatePair::value_type::Dimension { p.x() }, CoordinatePair::value_type::Dimension { p.y() } };
    }

    static Position NODELETE absoluteOffsetPoint(FloatPoint p)
    {
        return toPosition(p);
    }

    static CoordinatePair relativeOffsetPoint(FloatPoint p)
    {
        return toCoordinatePair(p);
    }

    static Variant<ToPosition, ByCoordinatePair> fromOffsetPoint(const FloatPoint& offsetPoint, PathCoordinateMode mode)
    {
        switch (mode) {
        case AbsoluteCoordinates:
            return ToPosition { absoluteOffsetPoint(offsetPoint) };
        case RelativeCoordinates:
            return ByCoordinatePair { relativeOffsetPoint(offsetPoint) };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template<typename Command> static Variant<typename Command::To, typename Command::By> fromOffsetLength(float offset, PathCoordinateMode mode)
    {
        switch (mode) {
        case AbsoluteCoordinates:
            return typename Command::To { .offset = { LengthPercentage<CSS::AllUnzoomed>::Dimension { offset } } };
        case RelativeCoordinates:
            return typename Command::By { .offset = LengthPercentage<CSS::AllUnzoomed>::Dimension { offset } };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    static AbsoluteControlPoint NODELETE absoluteControlPoint(const FloatPoint& controlPoint)
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
                .size = {
                    LengthPercentage<CSS::AllUnzoomed>::Dimension { r1 },
                    LengthPercentage<CSS::AllUnzoomed>::Dimension { r2 }
                },
                .arcSweep = sweepFlag ? ArcSweep { CSS::Keyword::Cw { } } : ArcSweep { CSS::Keyword::Ccw { } },
                .arcSize = largeArcFlag ? ArcSize { CSS::Keyword::Large { } } : ArcSize { CSS::Keyword::Small { } },
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
        .arcSweep = blendWithPreferredValue(a.arcSweep, b.arcSweep, ArcSweep { CSS::Keyword::Cw { } }, context),
        .arcSize = blendWithPreferredValue(a.arcSize, b.arcSize, ArcSize { CSS::Keyword::Large { } }, context),
        .rotation = WebCore::Style::blend(a.rotation, b.rotation, context),
    };
}

// MARK: - Shape (path conversion)

WebCore::Path PathComputation<Shape>::operator()(const Shape& value, const FloatRect& boundingBox, ZoomFactor zoom)
{
    // FIXME: We should do some caching here.
    auto pathSource = ShapeSVGPathSource(value.startingPoint, value, boundingBox.size(), zoom);

    WebCore::Path path;
    SVGPathBuilder builder(path);
    SVGPathParser::parse(pathSource, builder);

    path.translate(toFloatSize(boundingBox.location()));

    return path;
}

// MARK: - Wind Rule

WebCore::WindRule WindRuleComputation<Shape>::operator()(const Shape& value)
{
    return (!value.fillRule || std::holds_alternative<CSS::Keyword::Nonzero>(*value.fillRule)) ? WindRule::NonZero : WindRule::EvenOdd;
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
    using namespace CSS::Literals;

    // FIXME: Not clear how to convert a initial Move command to the Shape's "from" parameter.
    // https://github.com/w3c/csswg-drafts/issues/10740

    CommaSeparatedVector<ShapeCommand>::Container shapeCommands;
    ShapeConversionPathConsumer converter(shapeCommands);
    SVGPathByteStreamSource source(path.data.byteStream);

    if (!SVGPathParser::parse(source, converter, UnalteredParsing))
        return { };

    return Shape {
        .fillRule = path.fillRule,
        .startingPoint = converter.initialMove().value_or(Position { 0_css_px, 0_css_px }),
        .commands = { WTF::move(shapeCommands) }
    };
}

// MARK: - Evaluation

#if ENABLE(THREADED_ANIMATIONS)

template<> struct Evaluation<ControlPointAnchor, AcceleratedEffectShapeFunction::ControlPointAnchor> { AcceleratedEffectShapeFunction::ControlPointAnchor operator()(const ControlPointAnchor&); };

template<> struct Evaluation<ToPosition, AcceleratedEffectShapeFunction::ToPosition> { AcceleratedEffectShapeFunction::ToPosition operator()(const ToPosition&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<ByCoordinatePair, AcceleratedEffectShapeFunction::ByCoordinatePair> { AcceleratedEffectShapeFunction::ByCoordinatePair operator()(const ByCoordinatePair&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<RelativeControlPoint, AcceleratedEffectShapeFunction::RelativeControlPoint> { AcceleratedEffectShapeFunction::RelativeControlPoint operator()(const RelativeControlPoint&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<AbsoluteControlPoint, AcceleratedEffectShapeFunction::AbsoluteControlPoint> { AcceleratedEffectShapeFunction::AbsoluteControlPoint operator()(const AbsoluteControlPoint&, const FloatRect&, ZoomFactor); };

template<> struct Evaluation<MoveCommand, AcceleratedEffectShapeFunction::MoveCommand> { AcceleratedEffectShapeFunction::MoveCommand operator()(const MoveCommand&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<LineCommand, AcceleratedEffectShapeFunction::LineCommand> { AcceleratedEffectShapeFunction::LineCommand operator()(const LineCommand&, const FloatRect&, ZoomFactor); };

template<> struct Evaluation<HLineCommand, AcceleratedEffectShapeFunction::HLineCommand> { AcceleratedEffectShapeFunction::HLineCommand operator()(const HLineCommand&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<HLineCommand::To, AcceleratedEffectShapeFunction::HLineCommand::To> { AcceleratedEffectShapeFunction::HLineCommand::To operator()(const HLineCommand::To&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<HLineCommand::By, AcceleratedEffectShapeFunction::HLineCommand::By> { AcceleratedEffectShapeFunction::HLineCommand::By operator()(const HLineCommand::By&, const FloatRect&, ZoomFactor); };

template<> struct Evaluation<VLineCommand, AcceleratedEffectShapeFunction::VLineCommand> { AcceleratedEffectShapeFunction::VLineCommand operator()(const VLineCommand&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<VLineCommand::To, AcceleratedEffectShapeFunction::VLineCommand::To> { AcceleratedEffectShapeFunction::VLineCommand::To operator()(const VLineCommand::To&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<VLineCommand::By, AcceleratedEffectShapeFunction::VLineCommand::By> { AcceleratedEffectShapeFunction::VLineCommand::By operator()(const VLineCommand::By&, const FloatRect&, ZoomFactor); };

template<> struct Evaluation<CurveCommand, AcceleratedEffectShapeFunction::CurveCommand> { AcceleratedEffectShapeFunction::CurveCommand operator()(const CurveCommand&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<CurveCommand::To, AcceleratedEffectShapeFunction::CurveCommand::To> { AcceleratedEffectShapeFunction::CurveCommand::To operator()(const CurveCommand::To&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<CurveCommand::By, AcceleratedEffectShapeFunction::CurveCommand::By> { AcceleratedEffectShapeFunction::CurveCommand::By operator()(const CurveCommand::By&, const FloatRect&, ZoomFactor); };

template<> struct Evaluation<SmoothCommand, AcceleratedEffectShapeFunction::SmoothCommand> { AcceleratedEffectShapeFunction::SmoothCommand operator()(const SmoothCommand&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<SmoothCommand::To, AcceleratedEffectShapeFunction::SmoothCommand::To> { AcceleratedEffectShapeFunction::SmoothCommand::To operator()(const SmoothCommand::To&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<SmoothCommand::By, AcceleratedEffectShapeFunction::SmoothCommand::By> { AcceleratedEffectShapeFunction::SmoothCommand::By operator()(const SmoothCommand::By&, const FloatRect&, ZoomFactor); };

template<> struct Evaluation<ArcCommand, AcceleratedEffectShapeFunction::ArcCommand> { AcceleratedEffectShapeFunction::ArcCommand operator()(const ArcCommand&, const FloatRect&, ZoomFactor); };
template<> struct Evaluation<CloseCommand, AcceleratedEffectShapeFunction::CloseCommand> { AcceleratedEffectShapeFunction::CloseCommand operator()(const CloseCommand&, const FloatRect&, ZoomFactor); };

AcceleratedEffectShapeFunction::ControlPointAnchor Evaluation<ControlPointAnchor, AcceleratedEffectShapeFunction::ControlPointAnchor>::operator()(const ControlPointAnchor& value)
{
    return WTF::switchOn(value,
        [&](CSS::Keyword::Start) { return AcceleratedEffectShapeFunction::ControlPointAnchor::Start; },
        [&](CSS::Keyword::End) { return AcceleratedEffectShapeFunction::ControlPointAnchor::End; },
        [&](CSS::Keyword::Origin) { return AcceleratedEffectShapeFunction::ControlPointAnchor::Origin; }
    );
}

AcceleratedEffectShapeFunction::ToPosition Evaluation<ToPosition, AcceleratedEffectShapeFunction::ToPosition>::operator()(const ToPosition& value, const FloatRect& rect, ZoomFactor zoom)
{
    return { .offset = evaluate<FloatPoint>(value.offset, rect.size(), zoom) };
}

AcceleratedEffectShapeFunction::ByCoordinatePair Evaluation<ByCoordinatePair, AcceleratedEffectShapeFunction::ByCoordinatePair>::operator()(const ByCoordinatePair& value, const FloatRect& rect, ZoomFactor zoom)
{
    return { .offset = evaluate<FloatPoint>(value.offset, rect.size(), zoom) };
}

AcceleratedEffectShapeFunction::RelativeControlPoint Evaluation<RelativeControlPoint, AcceleratedEffectShapeFunction::RelativeControlPoint>::operator()(const RelativeControlPoint& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .offset = evaluate<FloatPoint>(value.offset, rect.size(), zoom),
        .anchor = value.anchor ? std::optional { evaluate<AcceleratedEffectShapeFunction::ControlPointAnchor>(*value.anchor) } : std::nullopt,
    };
}

AcceleratedEffectShapeFunction::AbsoluteControlPoint Evaluation<AbsoluteControlPoint, AcceleratedEffectShapeFunction::AbsoluteControlPoint>::operator()(const AbsoluteControlPoint& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .offset = evaluate<FloatPoint>(value.offset, rect.size(), zoom),
        .anchor = value.anchor ? std::optional { evaluate<AcceleratedEffectShapeFunction::ControlPointAnchor>(*value.anchor) } : std::nullopt,
    };
}

AcceleratedEffectShapeFunction::MoveCommand Evaluation<MoveCommand, AcceleratedEffectShapeFunction::MoveCommand>::operator()(const MoveCommand& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .toBy = WTF::switchOn(value.toBy,
            [&](const MoveCommand::To& to) -> AcceleratedEffectShapeFunction::MoveCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::MoveCommand::To>(to, rect, zoom);
            },
            [&](const MoveCommand::By& by) -> AcceleratedEffectShapeFunction::MoveCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::MoveCommand::By>(by, rect, zoom);
            }
        )
    };
}

AcceleratedEffectShapeFunction::LineCommand Evaluation<LineCommand, AcceleratedEffectShapeFunction::LineCommand>::operator()(const LineCommand& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .toBy = WTF::switchOn(value.toBy,
            [&](const LineCommand::To& to) -> AcceleratedEffectShapeFunction::LineCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::LineCommand::To>(to, rect, zoom);
            },
            [&](const LineCommand::By& by) -> AcceleratedEffectShapeFunction::LineCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::LineCommand::By>(by, rect, zoom);
            }
        )
    };
}

AcceleratedEffectShapeFunction::HLineCommand Evaluation<HLineCommand, AcceleratedEffectShapeFunction::HLineCommand>::operator()(const HLineCommand& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .toBy = WTF::switchOn(value.toBy,
            [&](const HLineCommand::To& to) -> AcceleratedEffectShapeFunction::HLineCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::HLineCommand::To>(to, rect, zoom);
            },
            [&](const HLineCommand::By& by) -> AcceleratedEffectShapeFunction::HLineCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::HLineCommand::By>(by, rect, zoom);
            }
        )
    };
}

AcceleratedEffectShapeFunction::HLineCommand::To Evaluation<HLineCommand::To, AcceleratedEffectShapeFunction::HLineCommand::To>::operator()(const HLineCommand::To& value, const FloatRect& rect, ZoomFactor zoom)
{
    return { .offset = evaluate<float>(value.offset, rect.width(), zoom) };
}

AcceleratedEffectShapeFunction::HLineCommand::By Evaluation<HLineCommand::By, AcceleratedEffectShapeFunction::HLineCommand::By>::operator()(const HLineCommand::By& value, const FloatRect& rect, ZoomFactor zoom)
{
    return { .offset = evaluate<float>(value.offset, rect.width(), zoom) };
}

AcceleratedEffectShapeFunction::VLineCommand Evaluation<VLineCommand, AcceleratedEffectShapeFunction::VLineCommand>::operator()(const VLineCommand& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .toBy = WTF::switchOn(value.toBy,
            [&](const VLineCommand::To& to) -> AcceleratedEffectShapeFunction::VLineCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::VLineCommand::To>(to, rect, zoom);
            },
            [&](const VLineCommand::By& by) -> AcceleratedEffectShapeFunction::VLineCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::VLineCommand::By>(by, rect, zoom);
            }
        )
    };
}

AcceleratedEffectShapeFunction::VLineCommand::To Evaluation<VLineCommand::To, AcceleratedEffectShapeFunction::VLineCommand::To>::operator()(const VLineCommand::To& value, const FloatRect& rect, ZoomFactor zoom)
{
    return { .offset = evaluate<float>(value.offset, rect.height(), zoom) };
}

AcceleratedEffectShapeFunction::VLineCommand::By Evaluation<VLineCommand::By, AcceleratedEffectShapeFunction::VLineCommand::By>::operator()(const VLineCommand::By& value, const FloatRect& rect, ZoomFactor zoom)
{
    return { .offset = evaluate<float>(value.offset, rect.height(), zoom) };
}

AcceleratedEffectShapeFunction::CurveCommand Evaluation<CurveCommand, AcceleratedEffectShapeFunction::CurveCommand>::operator()(const CurveCommand& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .toBy = WTF::switchOn(value.toBy,
            [&](const CurveCommand::To& to) -> AcceleratedEffectShapeFunction::CurveCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::CurveCommand::To>(to, rect, zoom);
            },
            [&](const CurveCommand::By& by) -> AcceleratedEffectShapeFunction::CurveCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::CurveCommand::By>(by, rect, zoom);
            }
        )
    };
}

AcceleratedEffectShapeFunction::CurveCommand::To Evaluation<CurveCommand::To, AcceleratedEffectShapeFunction::CurveCommand::To>::operator()(const CurveCommand::To& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .offset = evaluate<FloatPoint>(value.offset, rect.size(), zoom),
        .controlPoint1 = evaluate<AcceleratedEffectShapeFunction::AbsoluteControlPoint>(value.controlPoint1, rect, zoom),
        .controlPoint2 = value.controlPoint2 ? std::optional  { evaluate<AcceleratedEffectShapeFunction::AbsoluteControlPoint>(*value.controlPoint2, rect, zoom) } : std::nullopt,
    };
}

AcceleratedEffectShapeFunction::CurveCommand::By Evaluation<CurveCommand::By, AcceleratedEffectShapeFunction::CurveCommand::By>::operator()(const CurveCommand::By& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .offset = evaluate<FloatPoint>(value.offset, rect.size(), zoom),
        .controlPoint1 = evaluate<AcceleratedEffectShapeFunction::RelativeControlPoint>(value.controlPoint1, rect, zoom),
        .controlPoint2 = value.controlPoint2 ? std::optional  { evaluate<AcceleratedEffectShapeFunction::RelativeControlPoint>(*value.controlPoint2, rect, zoom) } : std::nullopt,
    };
}

AcceleratedEffectShapeFunction::SmoothCommand Evaluation<SmoothCommand, AcceleratedEffectShapeFunction::SmoothCommand>::operator()(const SmoothCommand& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .toBy = WTF::switchOn(value.toBy,
            [&](const SmoothCommand::To& to) -> AcceleratedEffectShapeFunction::SmoothCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::SmoothCommand::To>(to, rect, zoom);
            },
            [&](const SmoothCommand::By& by) -> AcceleratedEffectShapeFunction::SmoothCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::SmoothCommand::By>(by, rect, zoom);
            }
        )
    };
}

AcceleratedEffectShapeFunction::SmoothCommand::To Evaluation<SmoothCommand::To, AcceleratedEffectShapeFunction::SmoothCommand::To>::operator()(const SmoothCommand::To& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .offset = evaluate<FloatPoint>(value.offset, rect.size(), zoom),
        .controlPoint = value.controlPoint ? std::optional  { evaluate<AcceleratedEffectShapeFunction::AbsoluteControlPoint>(*value.controlPoint, rect, zoom) } : std::nullopt,
    };
}

AcceleratedEffectShapeFunction::SmoothCommand::By Evaluation<SmoothCommand::By, AcceleratedEffectShapeFunction::SmoothCommand::By>::operator()(const SmoothCommand::By& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .offset = evaluate<FloatPoint>(value.offset, rect.size(), zoom),
        .controlPoint = value.controlPoint ? std::optional  { evaluate<AcceleratedEffectShapeFunction::RelativeControlPoint>(*value.controlPoint, rect, zoom) } : std::nullopt,
    };
}

AcceleratedEffectShapeFunction::ArcCommand Evaluation<ArcCommand, AcceleratedEffectShapeFunction::ArcCommand>::operator()(const ArcCommand& value, const FloatRect& rect, ZoomFactor zoom)
{
    return {
        .toBy = WTF::switchOn(value.toBy,
            [&](const ArcCommand::To& to) -> AcceleratedEffectShapeFunction::ArcCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::ArcCommand::To>(to, rect, zoom);
            },
            [&](const ArcCommand::By& by) -> AcceleratedEffectShapeFunction::ArcCommand::ToBy {
                return evaluate<AcceleratedEffectShapeFunction::ArcCommand::By>(by, rect, zoom);
            }
        ),
        .size = evaluate<FloatSize>(value.size, rect.size(), zoom),
        .arcSweep = std::holds_alternative<CSS::Keyword::Cw>(value.arcSweep) ? AcceleratedEffectShapeFunction::ArcSweep::Cw : AcceleratedEffectShapeFunction::ArcSweep::Ccw,
        .arcSize = std::holds_alternative<CSS::Keyword::Large>(value.arcSize) ? AcceleratedEffectShapeFunction::ArcSize::Large : AcceleratedEffectShapeFunction::ArcSize::Small,
        .rotation = value.rotation.value,
    };
}

AcceleratedEffectShapeFunction::CloseCommand Evaluation<CloseCommand, AcceleratedEffectShapeFunction::CloseCommand>::operator()(const CloseCommand&, const FloatRect&, ZoomFactor)
{
    return { };
}

AcceleratedEffectShapeFunction Evaluation<ShapeFunction, AcceleratedEffectShapeFunction>::operator()(const ShapeFunction& value, const TransformOperationData& data, ZoomFactor zoom)
{
    auto rect = data.motionPathData->offsetRect().rect();

    auto evaluatedCommands = [&] {
        return WTF::map(value->commands, [&](auto& command) -> AcceleratedEffectShapeFunction::Command {
            return WTF::switchOn(command,
                [&](const MoveCommand& command) -> AcceleratedEffectShapeFunction::Command {
                    return evaluate<AcceleratedEffectShapeFunction::MoveCommand>(command, rect, zoom);
                },
                [&](const LineCommand& command) -> AcceleratedEffectShapeFunction::Command {
                    return evaluate<AcceleratedEffectShapeFunction::LineCommand>(command, rect, zoom);
                },
                [&](const HLineCommand& command) -> AcceleratedEffectShapeFunction::Command {
                    return evaluate<AcceleratedEffectShapeFunction::HLineCommand>(command, rect, zoom);
                },
                [&](const VLineCommand& command) -> AcceleratedEffectShapeFunction::Command {
                    return evaluate<AcceleratedEffectShapeFunction::VLineCommand>(command, rect, zoom);
                },
                [&](const CurveCommand& command) -> AcceleratedEffectShapeFunction::Command {
                    return evaluate<AcceleratedEffectShapeFunction::CurveCommand>(command, rect, zoom);
                },
                [&](const SmoothCommand& command) -> AcceleratedEffectShapeFunction::Command {
                    return evaluate<AcceleratedEffectShapeFunction::SmoothCommand>(command, rect, zoom);
                },
                [&](const ArcCommand& command) -> AcceleratedEffectShapeFunction::Command {
                    return evaluate<AcceleratedEffectShapeFunction::ArcCommand>(command, rect, zoom);
                },
                [&](const CloseCommand& command) -> AcceleratedEffectShapeFunction::Command {
                    return evaluate<AcceleratedEffectShapeFunction::CloseCommand>(command, rect, zoom);
                }
            );
        });
    };

    return {
        .fillRule = windRule(*value),
        .startingPoint = evaluate<FloatPoint>(value->startingPoint, rect.size(), zoom),
        .commands = evaluatedCommands(),
    };
}

#endif

} // namespace Style
} // namespace WebCore
