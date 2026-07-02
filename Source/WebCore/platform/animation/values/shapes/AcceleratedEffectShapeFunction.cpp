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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "AcceleratedEffectShapeFunction.h"

#if ENABLE(THREADED_ANIMATIONS)

#include "AcceleratedEffectAnimationUtilities.h"
#include "AcceleratedEffectPathFunction.h"
#include "FloatConversion.h"
#include "FloatRect.h"
#include "GeometryUtilities.h"
#include "Path.h"
#include "SVGPathBuilder.h"
#include "SVGPathByteStreamSource.h"
#include "SVGPathParser.h"
#include "SVGPathSource.h"
#include <wtf/ZippedRange.h>

namespace WebCore {

static WindRule windRule(const AcceleratedEffectShapeFunction& value)
{
    return value.fillRule.value_or(WindRule::NonZero);
}

// MARK: - Path Evaluation

template<typename ControlPoint> static AcceleratedEffectShapeFunction::ControlPointAnchor NODELETE evaluateControlPointAnchoring(const ControlPoint& value, AcceleratedEffectShapeFunction::ControlPointAnchor defaultValue)
{
    if (value.anchor)
        return *value.anchor;
    return defaultValue;
}

template<typename ControlPoint> static FloatPoint evaluateControlPointOffset(const ControlPoint& value)
{
    return value.offset;
}

template<typename ControlPoint> static FloatPoint resolveControlPoint(AcceleratedEffectShapeFunction::CommandAffinity affinity, FloatPoint currentPosition, FloatPoint segmentOffset, const ControlPoint& controlPoint, const FloatSize&)
{
    auto controlPointOffset = evaluateControlPointOffset(controlPoint);

    auto defaultAnchor = affinity == AcceleratedEffectShapeFunction::CommandAffinity::By ? AcceleratedEffectShapeFunction::RelativeControlPoint::defaultAnchor : AcceleratedEffectShapeFunction::AbsoluteControlPoint::defaultAnchor;
    auto controlPointAnchoring = evaluateControlPointAnchoring(controlPoint, defaultAnchor);

    auto absoluteControlPoint = [&] {
        switch (controlPointAnchoring) {
        case AcceleratedEffectShapeFunction::ControlPointAnchor::Start: {
            auto absoluteStartPoint = currentPosition;
            return absoluteStartPoint + controlPointOffset;
        }
        case AcceleratedEffectShapeFunction::ControlPointAnchor::End: {
            auto absoluteEndPoint = affinity == AcceleratedEffectShapeFunction::CommandAffinity::By ? currentPosition + toFloatSize(segmentOffset) : segmentOffset;
            return absoluteEndPoint + controlPointOffset;
        }
        case AcceleratedEffectShapeFunction::ControlPointAnchor::Origin:
            return controlPointOffset;
        }
        RELEASE_ASSERT_NOT_REACHED();
    }();

    if (affinity == AcceleratedEffectShapeFunction::CommandAffinity::By)
        return absoluteControlPoint - toFloatSize(currentPosition);
    return absoluteControlPoint;
}

// MARK: - AcceleratedEffectShapeSVGPathSource

class AcceleratedEffectShapeSVGPathSource final : public SVGPathSource {
public:
    explicit AcceleratedEffectShapeSVGPathSource(const FloatPoint& startPoint, const AcceleratedEffectShapeFunction& shape, const FloatSize& boxSize)
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
            return MoveToSegment { m_start };

        auto& moveCommand = currentValue<AcceleratedEffectShapeFunction::MoveCommand>();

        return MoveToSegment { WTF::switchOn(moveCommand.toBy, [&](auto& alternative) { return alternative.offset; }) };
    }

    std::optional<LineToSegment> parseLineToSegment(FloatPoint) override
    {
        auto& lineCommand = currentValue<AcceleratedEffectShapeFunction::LineCommand>();

        return LineToSegment { WTF::switchOn(lineCommand.toBy, [&](auto& alternative) { return alternative.offset; }) };
    }

    std::optional<LineToHorizontalSegment> parseLineToHorizontalSegment(FloatPoint) override
    {
        auto& lineCommand = currentValue<AcceleratedEffectShapeFunction::HLineCommand>();

        return LineToHorizontalSegment { WTF::switchOn(lineCommand.toBy, [&](auto& alternative) { return alternative.offset; }) };
    }

    std::optional<LineToVerticalSegment> parseLineToVerticalSegment(FloatPoint) override
    {
        auto& lineCommand = currentValue<AcceleratedEffectShapeFunction::VLineCommand>();

        return LineToVerticalSegment { WTF::switchOn(lineCommand.toBy, [&](auto& alternative) { return alternative.offset; }) };
    }

    std::optional<CurveToCubicSegment> parseCurveToCubicSegment(FloatPoint currentPosition) override
    {
        auto& curveCommand = currentValue<AcceleratedEffectShapeFunction::CurveCommand>();

        return WTF::switchOn(curveCommand.toBy,
            [&](const auto& value) {
                auto offset = value.offset;
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
        auto& curveCommand = currentValue<AcceleratedEffectShapeFunction::CurveCommand>();

        return WTF::switchOn(curveCommand.toBy,
            [&](const auto& value) {
                auto offset = value.offset;
                return CurveToQuadraticSegment {
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint1, m_boxSize),
                    offset
                };
            }
        );
    }

    std::optional<CurveToCubicSmoothSegment> parseCurveToCubicSmoothSegment(FloatPoint currentPosition) override
    {
        auto& smoothCommand = currentValue<AcceleratedEffectShapeFunction::SmoothCommand>();

        return WTF::switchOn(smoothCommand.toBy,
            [&](const auto& value) {
                ASSERT(value.controlPoint);
                auto offset = value.offset;
                return CurveToCubicSmoothSegment {
                    resolveControlPoint(value.affinity, currentPosition, offset, value.controlPoint.value(), m_boxSize),
                    offset
                };
            }
        );
    }

    std::optional<CurveToQuadraticSmoothSegment> parseCurveToQuadraticSmoothSegment(FloatPoint) override
    {
        auto& smoothCommand = currentValue<AcceleratedEffectShapeFunction::SmoothCommand>();

        return WTF::switchOn(smoothCommand.toBy,
            [&](const auto& value) {
                return CurveToQuadraticSmoothSegment {
                    value.offset
                };
            }
        );
    }

    std::optional<ArcToSegment> parseArcToSegment(FloatPoint) override
    {
        auto& arcCommand = currentValue<AcceleratedEffectShapeFunction::ArcCommand>();

        return ArcToSegment {
            .rx = arcCommand.size.width(),
            .ry = arcCommand.size.height(),
            .angle = narrowPrecisionToFloat(arcCommand.rotation),
            .largeArc = arcCommand.arcSize == AcceleratedEffectShapeFunction::ArcSize::Large,
            .sweep = arcCommand.arcSweep == AcceleratedEffectShapeFunction::ArcSweep::Cw,
            .targetPoint = WTF::switchOn(arcCommand.toBy, [&](auto& alternative) { return alternative.offset; }),
        };
    }

    SVGPathSegType segmentTypeAtIndex(size_t index) const
    {
        if (index >= m_shape.commands.size())
            return SVGPathSegType::Unknown;

        return WTF::switchOn(m_shape.commands[index],
            [&](const AcceleratedEffectShapeFunction::MoveCommand& command) {
                return std::holds_alternative<AcceleratedEffectShapeFunction::MoveCommand::To>(command.toBy) ? SVGPathSegType::MoveToAbs : SVGPathSegType::MoveToRel;
            },
            [&](const AcceleratedEffectShapeFunction::LineCommand& command) {
                return std::holds_alternative<AcceleratedEffectShapeFunction::LineCommand::To>(command.toBy) ? SVGPathSegType::LineToAbs : SVGPathSegType::LineToRel;
            },
            [&](const AcceleratedEffectShapeFunction::HLineCommand& command) {
                return std::holds_alternative<AcceleratedEffectShapeFunction::HLineCommand::To>(command.toBy) ? SVGPathSegType::LineToHorizontalAbs : SVGPathSegType::LineToHorizontalRel;
            },
            [&](const AcceleratedEffectShapeFunction::VLineCommand& command) {
                return std::holds_alternative<AcceleratedEffectShapeFunction::VLineCommand::To>(command.toBy) ? SVGPathSegType::LineToVerticalAbs : SVGPathSegType::LineToVerticalRel;
            },
            [&](const AcceleratedEffectShapeFunction::CurveCommand& command) {
                return WTF::switchOn(command.toBy,
                    [](const auto& value) {
                        if (value.controlPoint2)
                            return value.affinity == AcceleratedEffectShapeFunction::CommandAffinity::To ? SVGPathSegType::CurveToCubicAbs : SVGPathSegType::CurveToCubicRel;
                        return value.affinity == AcceleratedEffectShapeFunction::CommandAffinity::To ? SVGPathSegType::CurveToQuadraticAbs : SVGPathSegType::CurveToQuadraticRel;
                    }
                );
            },
            [&](const AcceleratedEffectShapeFunction::SmoothCommand& command) {
                return WTF::switchOn(command.toBy,
                    [](const auto& value) {
                        if (value.controlPoint)
                            return value.affinity == AcceleratedEffectShapeFunction::CommandAffinity::To ? SVGPathSegType::CurveToCubicSmoothAbs : SVGPathSegType::CurveToCubicSmoothRel;
                        return value.affinity == AcceleratedEffectShapeFunction::CommandAffinity::To ? SVGPathSegType::CurveToQuadraticSmoothAbs : SVGPathSegType::CurveToQuadraticSmoothRel;
                    }
                );
            },
            [&](const AcceleratedEffectShapeFunction::ArcCommand& command) {
                return std::holds_alternative<AcceleratedEffectShapeFunction::ArcCommand::To>(command.toBy) ? SVGPathSegType::ArcAbs : SVGPathSegType::ArcRel;
            },
            [&](const AcceleratedEffectShapeFunction::CloseCommand&) {
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

    FloatPoint m_start;
    const AcceleratedEffectShapeFunction& m_shape;
    FloatSize m_boxSize;
    size_t m_endIndex { 0 };
    size_t m_nextIndex { 0 };
};

// MARK: - AcceleratedEffectShapeConversionPathConsumer

class AcceleratedEffectShapeConversionPathConsumer final : public SVGPathConsumer {
public:
    AcceleratedEffectShapeConversionPathConsumer(Vector<AcceleratedEffectShapeFunction::Command>& commands)
        : m_commands(commands)
    {
    }

    const std::optional<FloatPoint>& NODELETE initialMove() const { return m_initialMove; }

private:
    static FloatPoint NODELETE toPosition(FloatPoint p)
    {
        return p;
    }

    static FloatPoint NODELETE toCoordinatePair(FloatPoint p)
    {
        return p;
    }

    static FloatPoint NODELETE absoluteOffsetPoint(FloatPoint p)
    {
        return p;
    }

    static FloatPoint NODELETE relativeOffsetPoint(FloatPoint p)
    {
        return p;
    }

    static Variant<AcceleratedEffectShapeFunction::ToPosition, AcceleratedEffectShapeFunction::ByCoordinatePair> fromOffsetPoint(const FloatPoint& offsetPoint, PathCoordinateMode mode)
    {
        switch (mode) {
        case AbsoluteCoordinates:
            return AcceleratedEffectShapeFunction::ToPosition { absoluteOffsetPoint(offsetPoint) };
        case RelativeCoordinates:
            return AcceleratedEffectShapeFunction::ByCoordinatePair { relativeOffsetPoint(offsetPoint) };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    template<typename Command> static Variant<typename Command::To, typename Command::By> fromOffsetLength(float offset, PathCoordinateMode mode)
    {
        switch (mode) {
        case AbsoluteCoordinates:
            return typename Command::To { .offset = offset };
        case RelativeCoordinates:
            return typename Command::By { .offset = offset };
        }
        RELEASE_ASSERT_NOT_REACHED();
    }

    static AcceleratedEffectShapeFunction::AbsoluteControlPoint NODELETE absoluteControlPoint(const FloatPoint& controlPoint)
    {
        return { toPosition(controlPoint), std::nullopt };
    }

    static AcceleratedEffectShapeFunction::RelativeControlPoint relativeControlPoint(const FloatPoint& controlPoint)
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
            AcceleratedEffectShapeFunction::MoveCommand {
                .toBy = fromOffsetPoint(offsetPoint, mode)
            }
        );
    }

    void lineTo(const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        m_commands.append(
            AcceleratedEffectShapeFunction::LineCommand {
                .toBy = fromOffsetPoint(offsetPoint, mode)
            }
        );
    }

    void lineToHorizontal(float length, PathCoordinateMode mode) override
    {
        m_commands.append(
            AcceleratedEffectShapeFunction::HLineCommand {
                .toBy = fromOffsetLength<AcceleratedEffectShapeFunction::HLineCommand>(length, mode)
            }
        );
    }

    void lineToVertical(float length, PathCoordinateMode mode) override
    {
        m_commands.append(
            AcceleratedEffectShapeFunction::VLineCommand {
                .toBy = fromOffsetLength<AcceleratedEffectShapeFunction::VLineCommand>(length, mode)
            }
        );
    }

    void curveToCubic(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        switch (mode) {
        case AbsoluteCoordinates:
            m_commands.append(
                AcceleratedEffectShapeFunction::CurveCommand {
                    .toBy = AcceleratedEffectShapeFunction::CurveCommand::To {
                        .offset = absoluteOffsetPoint(offsetPoint),
                        .controlPoint1 = absoluteControlPoint(controlPoint1),
                        .controlPoint2 = absoluteControlPoint(controlPoint2),
                    }
                }
            );
            break;
        case RelativeCoordinates:
            m_commands.append(
                AcceleratedEffectShapeFunction::CurveCommand {
                    .toBy = AcceleratedEffectShapeFunction::CurveCommand::By {
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
                AcceleratedEffectShapeFunction::CurveCommand {
                    .toBy = AcceleratedEffectShapeFunction::CurveCommand::To {
                        .offset = absoluteOffsetPoint(offsetPoint),
                        .controlPoint1 = absoluteControlPoint(controlPoint),
                        .controlPoint2 = std::nullopt,
                    }
                }
            );
            break;
        case RelativeCoordinates:
            m_commands.append(
                AcceleratedEffectShapeFunction::CurveCommand {
                    .toBy = AcceleratedEffectShapeFunction::CurveCommand::By {
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
                AcceleratedEffectShapeFunction::SmoothCommand {
                    .toBy = AcceleratedEffectShapeFunction::SmoothCommand::To {
                        .offset = absoluteOffsetPoint(offsetPoint),
                        .controlPoint = absoluteControlPoint(controlPoint),
                    }
                }
            );
            break;
        case RelativeCoordinates:
            m_commands.append(
                AcceleratedEffectShapeFunction::SmoothCommand {
                    .toBy = AcceleratedEffectShapeFunction::SmoothCommand::By {
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
                AcceleratedEffectShapeFunction::SmoothCommand {
                    .toBy = AcceleratedEffectShapeFunction::SmoothCommand::To {
                        .offset = absoluteOffsetPoint(offsetPoint),
                        .controlPoint = std::nullopt,
                    }
                }
            );
            break;
        case RelativeCoordinates:
            m_commands.append(
                AcceleratedEffectShapeFunction::SmoothCommand {
                    .toBy = AcceleratedEffectShapeFunction::SmoothCommand::By {
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
            AcceleratedEffectShapeFunction::ArcCommand {
                .toBy = fromOffsetPoint(offsetPoint, mode),
                .size = { r1, r2 },
                .arcSweep = sweepFlag ? AcceleratedEffectShapeFunction::ArcSweep::Cw : AcceleratedEffectShapeFunction::ArcSweep::Ccw,
                .arcSize = largeArcFlag ? AcceleratedEffectShapeFunction::ArcSize::Large : AcceleratedEffectShapeFunction::ArcSize::Small,
                .rotation = angle,
            }
        );
    }

    void closePath() override
    {
        m_commands.append(AcceleratedEffectShapeFunction::CloseCommand { });
    }

    Vector<AcceleratedEffectShapeFunction::Command>& m_commands;
    std::optional<FloatPoint> m_initialMove;
};

Path path(const AcceleratedEffectShapeFunction& value, const FloatRect& boundingBox)
{
    auto pathSource = AcceleratedEffectShapeSVGPathSource(value.startingPoint, value, boundingBox.size());

    WebCore::Path path;
    SVGPathBuilder builder(path);
    SVGPathParser::parse(pathSource, builder);

    path.translate(toFloatSize(boundingBox.location()));

    return path;
}

// MARK: - Blending

template<typename T>
static T blendWithPreferredValue(const T& from, const T& to, const T& preferredValue, const BlendingContext& context)
{
    if (context.progress <= 0)
        return from;

    if (context.progress >= 1)
        return to;

    if (from == to)
        return from;

    return preferredValue;
}

template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::ToPosition> {
    auto canBlend(const AcceleratedEffectShapeFunction::ToPosition&, const AcceleratedEffectShapeFunction::ToPosition&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::ToPosition&, const AcceleratedEffectShapeFunction::ToPosition&, const BlendingContext&) -> AcceleratedEffectShapeFunction::ToPosition;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::ByCoordinatePair> {
    auto canBlend(const AcceleratedEffectShapeFunction::ByCoordinatePair&, const AcceleratedEffectShapeFunction::ByCoordinatePair&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::ByCoordinatePair&, const AcceleratedEffectShapeFunction::ByCoordinatePair&, const BlendingContext&) -> AcceleratedEffectShapeFunction::ByCoordinatePair;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::RelativeControlPoint> {
    auto canBlend(const AcceleratedEffectShapeFunction::RelativeControlPoint&, const AcceleratedEffectShapeFunction::RelativeControlPoint&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::RelativeControlPoint&, const AcceleratedEffectShapeFunction::RelativeControlPoint&, const BlendingContext&) -> AcceleratedEffectShapeFunction::RelativeControlPoint;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::AbsoluteControlPoint> {
    auto canBlend(const AcceleratedEffectShapeFunction::AbsoluteControlPoint&, const AcceleratedEffectShapeFunction::AbsoluteControlPoint&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::AbsoluteControlPoint&, const AcceleratedEffectShapeFunction::AbsoluteControlPoint&, const BlendingContext&) -> AcceleratedEffectShapeFunction::AbsoluteControlPoint;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::MoveCommand> {
    auto canBlend(const AcceleratedEffectShapeFunction::MoveCommand&, const AcceleratedEffectShapeFunction::MoveCommand&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::MoveCommand&, const AcceleratedEffectShapeFunction::MoveCommand&, const BlendingContext&) -> AcceleratedEffectShapeFunction::MoveCommand;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::LineCommand> {
    auto canBlend(const AcceleratedEffectShapeFunction::LineCommand&, const AcceleratedEffectShapeFunction::LineCommand&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::LineCommand&, const AcceleratedEffectShapeFunction::LineCommand&, const BlendingContext&) -> AcceleratedEffectShapeFunction::LineCommand;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::HLineCommand::To> {
    auto canBlend(const AcceleratedEffectShapeFunction::HLineCommand::To&, const AcceleratedEffectShapeFunction::HLineCommand::To&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::HLineCommand::To&, const AcceleratedEffectShapeFunction::HLineCommand::To&, const BlendingContext&) -> AcceleratedEffectShapeFunction::HLineCommand::To;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::HLineCommand::By> {
    auto canBlend(const AcceleratedEffectShapeFunction::HLineCommand::By&, const AcceleratedEffectShapeFunction::HLineCommand::By&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::HLineCommand::By&, const AcceleratedEffectShapeFunction::HLineCommand::By&, const BlendingContext&) -> AcceleratedEffectShapeFunction::HLineCommand::By;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::HLineCommand> {
    auto canBlend(const AcceleratedEffectShapeFunction::HLineCommand&, const AcceleratedEffectShapeFunction::HLineCommand&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::HLineCommand&, const AcceleratedEffectShapeFunction::HLineCommand&, const BlendingContext&) -> AcceleratedEffectShapeFunction::HLineCommand;
};

template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::VLineCommand::To> {
    auto canBlend(const AcceleratedEffectShapeFunction::VLineCommand::To&, const AcceleratedEffectShapeFunction::VLineCommand::To&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::VLineCommand::To&, const AcceleratedEffectShapeFunction::VLineCommand::To&, const BlendingContext&) -> AcceleratedEffectShapeFunction::VLineCommand::To;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::VLineCommand::By> {
    auto canBlend(const AcceleratedEffectShapeFunction::VLineCommand::By&, const AcceleratedEffectShapeFunction::VLineCommand::By&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::VLineCommand::By&, const AcceleratedEffectShapeFunction::VLineCommand::By&, const BlendingContext&) -> AcceleratedEffectShapeFunction::VLineCommand::By;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::VLineCommand> {
    auto canBlend(const AcceleratedEffectShapeFunction::VLineCommand&, const AcceleratedEffectShapeFunction::VLineCommand&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::VLineCommand&, const AcceleratedEffectShapeFunction::VLineCommand&, const BlendingContext&) -> AcceleratedEffectShapeFunction::VLineCommand;
};

template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CurveCommand::To> {
    auto canBlend(const AcceleratedEffectShapeFunction::CurveCommand::To&, const AcceleratedEffectShapeFunction::CurveCommand::To&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::CurveCommand::To&, const AcceleratedEffectShapeFunction::CurveCommand::To&, const BlendingContext&) -> AcceleratedEffectShapeFunction::CurveCommand::To;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CurveCommand::By> {
    auto canBlend(const AcceleratedEffectShapeFunction::CurveCommand::By&, const AcceleratedEffectShapeFunction::CurveCommand::By&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::CurveCommand::By&, const AcceleratedEffectShapeFunction::CurveCommand::By&, const BlendingContext&) -> AcceleratedEffectShapeFunction::CurveCommand::By;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CurveCommand> {
    auto canBlend(const AcceleratedEffectShapeFunction::CurveCommand&, const AcceleratedEffectShapeFunction::CurveCommand&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::CurveCommand&, const AcceleratedEffectShapeFunction::CurveCommand&, const BlendingContext&) -> AcceleratedEffectShapeFunction::CurveCommand;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::SmoothCommand::To> {
    auto canBlend(const AcceleratedEffectShapeFunction::SmoothCommand::To&, const AcceleratedEffectShapeFunction::SmoothCommand::To&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::SmoothCommand::To&, const AcceleratedEffectShapeFunction::SmoothCommand::To&, const BlendingContext&) -> AcceleratedEffectShapeFunction::SmoothCommand::To;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::SmoothCommand::By> {
    auto canBlend(const AcceleratedEffectShapeFunction::SmoothCommand::By&, const AcceleratedEffectShapeFunction::SmoothCommand::By&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::SmoothCommand::By&, const AcceleratedEffectShapeFunction::SmoothCommand::By&, const BlendingContext&) -> AcceleratedEffectShapeFunction::SmoothCommand::By;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::SmoothCommand> {
    auto canBlend(const AcceleratedEffectShapeFunction::SmoothCommand&, const AcceleratedEffectShapeFunction::SmoothCommand&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::SmoothCommand&, const AcceleratedEffectShapeFunction::SmoothCommand&, const BlendingContext&) -> AcceleratedEffectShapeFunction::SmoothCommand;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::ArcCommand> {
    auto canBlend(const AcceleratedEffectShapeFunction::ArcCommand&, const AcceleratedEffectShapeFunction::ArcCommand&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::ArcCommand&, const AcceleratedEffectShapeFunction::ArcCommand&, const BlendingContext&) -> AcceleratedEffectShapeFunction::ArcCommand;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CloseCommand> {
    auto canBlend(const AcceleratedEffectShapeFunction::CloseCommand&, const AcceleratedEffectShapeFunction::CloseCommand&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::CloseCommand&, const AcceleratedEffectShapeFunction::CloseCommand&, const BlendingContext&) -> AcceleratedEffectShapeFunction::CloseCommand;
};
template<> struct AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::Command> {
    auto canBlend(const AcceleratedEffectShapeFunction::Command&, const AcceleratedEffectShapeFunction::Command&) -> bool;
    auto blend(const AcceleratedEffectShapeFunction::Command&, const AcceleratedEffectShapeFunction::Command&, const BlendingContext&) -> AcceleratedEffectShapeFunction::Command;
};

// MARK: - AcceleratedEffectShapeFunction::ToPosition (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::ToPosition>::canBlend(const AcceleratedEffectShapeFunction::ToPosition&, const AcceleratedEffectShapeFunction::ToPosition&) -> bool
{
    return true;
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::ToPosition>::blend(const AcceleratedEffectShapeFunction::ToPosition& from, const AcceleratedEffectShapeFunction::ToPosition& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::ToPosition
{
    return { .offset = WebCore::blend(from.offset, to.offset, context) };
}

// MARK: - AcceleratedEffectShapeFunction::ByCoordinatePair (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::ByCoordinatePair>::canBlend(const AcceleratedEffectShapeFunction::ByCoordinatePair&, const AcceleratedEffectShapeFunction::ByCoordinatePair&) -> bool
{
    return true;
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::ByCoordinatePair>::blend(const AcceleratedEffectShapeFunction::ByCoordinatePair& from, const AcceleratedEffectShapeFunction::ByCoordinatePair& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::ByCoordinatePair
{
    return { .offset = WebCore::blend(from.offset, to.offset, context) };
}

// MARK: - AcceleratedEffectShapeFunction::RelativeControlPoint (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::RelativeControlPoint>::canBlend(const AcceleratedEffectShapeFunction::RelativeControlPoint& from, const AcceleratedEffectShapeFunction::RelativeControlPoint& to) -> bool
{
    return from.anchor.value_or(AcceleratedEffectShapeFunction::RelativeControlPoint::defaultAnchor) == to.anchor.value_or(AcceleratedEffectShapeFunction::RelativeControlPoint::defaultAnchor);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::RelativeControlPoint>::blend(const AcceleratedEffectShapeFunction::RelativeControlPoint& from, const AcceleratedEffectShapeFunction::RelativeControlPoint& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::RelativeControlPoint
{
    return {
        .offset = WebCore::blend(from.offset, to.offset, context),
        .anchor = from.anchor.has_value() && to.anchor.has_value() ? from.anchor : std::nullopt
    };
}

// MARK: - AcceleratedEffectShapeFunction::AbsoluteControlPoint (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::AbsoluteControlPoint>::canBlend(const AcceleratedEffectShapeFunction::AbsoluteControlPoint& from, const AcceleratedEffectShapeFunction::AbsoluteControlPoint& to) -> bool
{
    return from.anchor.value_or(AcceleratedEffectShapeFunction::AbsoluteControlPoint::defaultAnchor) == to.anchor.value_or(AcceleratedEffectShapeFunction::AbsoluteControlPoint::defaultAnchor);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::AbsoluteControlPoint>::blend(const AcceleratedEffectShapeFunction::AbsoluteControlPoint& from, const AcceleratedEffectShapeFunction::AbsoluteControlPoint& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::AbsoluteControlPoint
{
    return {
        .offset = WebCore::blend(from.offset, to.offset, context),
        .anchor = from.anchor.has_value() && to.anchor.has_value() ? from.anchor : std::nullopt
    };
}

// MARK: - AcceleratedEffectShapeFunction::MoveCommand (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::MoveCommand>::canBlend(const AcceleratedEffectShapeFunction::MoveCommand& from, const AcceleratedEffectShapeFunction::MoveCommand& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.toBy, to.toBy);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::MoveCommand>::blend(const AcceleratedEffectShapeFunction::MoveCommand& from, const AcceleratedEffectShapeFunction::MoveCommand& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::MoveCommand
{
    return { .toBy = WebCore::blendForAcceleratedEffect(from.toBy, to.toBy, context) };
}

// MARK: - AcceleratedEffectShapeFunction::LineCommand (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::LineCommand>::canBlend(const AcceleratedEffectShapeFunction::LineCommand& from, const AcceleratedEffectShapeFunction::LineCommand& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.toBy, to.toBy);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::LineCommand>::blend(const AcceleratedEffectShapeFunction::LineCommand& from, const AcceleratedEffectShapeFunction::LineCommand& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::LineCommand
{
    return { .toBy = WebCore::blendForAcceleratedEffect(from.toBy, to.toBy, context) };
}

// MARK: - AcceleratedEffectShapeFunction::HLineCommand (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::HLineCommand::To>::canBlend(const AcceleratedEffectShapeFunction::HLineCommand::To&, const AcceleratedEffectShapeFunction::HLineCommand::To&) -> bool
{
    return true;
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::HLineCommand::To>::blend(const AcceleratedEffectShapeFunction::HLineCommand::To& from, const AcceleratedEffectShapeFunction::HLineCommand::To& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::HLineCommand::To
{
    return { .offset = WebCore::blend(from.offset, to.offset, context) };
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::HLineCommand::By>::canBlend(const AcceleratedEffectShapeFunction::HLineCommand::By&, const AcceleratedEffectShapeFunction::HLineCommand::By&) -> bool
{
    return true;
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::HLineCommand::By>::blend(const AcceleratedEffectShapeFunction::HLineCommand::By& from, const AcceleratedEffectShapeFunction::HLineCommand::By& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::HLineCommand::By
{
    return { .offset = WebCore::blend(from.offset, to.offset, context) };
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::HLineCommand>::canBlend(const AcceleratedEffectShapeFunction::HLineCommand& from, const AcceleratedEffectShapeFunction::HLineCommand& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.toBy, to.toBy);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::HLineCommand>::blend(const AcceleratedEffectShapeFunction::HLineCommand& from, const AcceleratedEffectShapeFunction::HLineCommand& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::HLineCommand
{
    return { .toBy = WebCore::blendForAcceleratedEffect(from.toBy, to.toBy, context) };
}

// MARK: - AcceleratedEffectShapeFunction::VLineCommand (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::VLineCommand::To>::canBlend(const AcceleratedEffectShapeFunction::VLineCommand::To&, const AcceleratedEffectShapeFunction::VLineCommand::To&) -> bool
{
    return true;
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::VLineCommand::To>::blend(const AcceleratedEffectShapeFunction::VLineCommand::To& from, const AcceleratedEffectShapeFunction::VLineCommand::To& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::VLineCommand::To
{
    return { .offset = WebCore::blend(from.offset, to.offset, context) };
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::VLineCommand::By>::canBlend(const AcceleratedEffectShapeFunction::VLineCommand::By&, const AcceleratedEffectShapeFunction::VLineCommand::By&) -> bool
{
    return true;
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::VLineCommand::By>::blend(const AcceleratedEffectShapeFunction::VLineCommand::By& from, const AcceleratedEffectShapeFunction::VLineCommand::By& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::VLineCommand::By
{
    return { .offset = WebCore::blend(from.offset, to.offset, context) };
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::VLineCommand>::canBlend(const AcceleratedEffectShapeFunction::VLineCommand& from, const AcceleratedEffectShapeFunction::VLineCommand& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.toBy, to.toBy);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::VLineCommand>::blend(const AcceleratedEffectShapeFunction::VLineCommand& from, const AcceleratedEffectShapeFunction::VLineCommand& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::VLineCommand
{
    return { .toBy = WebCore::blendForAcceleratedEffect(from.toBy, to.toBy, context) };
}

// MARK: - AcceleratedEffectShapeFunction::CurveCommand (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CurveCommand::To>::canBlend(const AcceleratedEffectShapeFunction::CurveCommand::To& from, const AcceleratedEffectShapeFunction::CurveCommand::To& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.controlPoint1, to.controlPoint1)
        && WebCore::canBlendForAcceleratedEffect(from.controlPoint2, to.controlPoint2);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CurveCommand::To>::blend(const AcceleratedEffectShapeFunction::CurveCommand::To& from, const AcceleratedEffectShapeFunction::CurveCommand::To& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::CurveCommand::To
{
    return {
        .offset = WebCore::blend(from.offset, to.offset, context),
        .controlPoint1 = WebCore::blendForAcceleratedEffect(from.controlPoint1, to.controlPoint1, context),
        .controlPoint2 = WebCore::blendForAcceleratedEffect(from.controlPoint2, to.controlPoint2, context),
    };
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CurveCommand::By>::canBlend(const AcceleratedEffectShapeFunction::CurveCommand::By& from, const AcceleratedEffectShapeFunction::CurveCommand::By& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.controlPoint1, to.controlPoint1)
        && WebCore::canBlendForAcceleratedEffect(from.controlPoint2, to.controlPoint2);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CurveCommand::By>::blend(const AcceleratedEffectShapeFunction::CurveCommand::By& from, const AcceleratedEffectShapeFunction::CurveCommand::By& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::CurveCommand::By
{
    return {
        .offset = WebCore::blend(from.offset, to.offset, context),
        .controlPoint1 = WebCore::blendForAcceleratedEffect(from.controlPoint1, to.controlPoint1, context),
        .controlPoint2 = WebCore::blendForAcceleratedEffect(from.controlPoint2, to.controlPoint2, context),
    };
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CurveCommand>::canBlend(const AcceleratedEffectShapeFunction::CurveCommand& from, const AcceleratedEffectShapeFunction::CurveCommand& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.toBy, to.toBy);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CurveCommand>::blend(const AcceleratedEffectShapeFunction::CurveCommand& from, const AcceleratedEffectShapeFunction::CurveCommand& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::CurveCommand
{
    return { .toBy = WebCore::blendForAcceleratedEffect(from.toBy, to.toBy, context) };
}

// MARK: - AcceleratedEffectShapeFunction::SmoothCommand (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::SmoothCommand::To>::canBlend(const AcceleratedEffectShapeFunction::SmoothCommand::To& from, const AcceleratedEffectShapeFunction::SmoothCommand::To& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.controlPoint, to.controlPoint);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::SmoothCommand::To>::blend(const AcceleratedEffectShapeFunction::SmoothCommand::To& from, const AcceleratedEffectShapeFunction::SmoothCommand::To& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::SmoothCommand::To
{
    return {
        .offset = WebCore::blend(from.offset, to.offset, context),
        .controlPoint = WebCore::blendForAcceleratedEffect(from.controlPoint, to.controlPoint, context),
    };
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::SmoothCommand::By>::canBlend(const AcceleratedEffectShapeFunction::SmoothCommand::By& from, const AcceleratedEffectShapeFunction::SmoothCommand::By& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.controlPoint, to.controlPoint);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::SmoothCommand::By>::blend(const AcceleratedEffectShapeFunction::SmoothCommand::By& from, const AcceleratedEffectShapeFunction::SmoothCommand::By& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::SmoothCommand::By
{
    return {
        .offset = WebCore::blend(from.offset, to.offset, context),
        .controlPoint = WebCore::blendForAcceleratedEffect(from.controlPoint, to.controlPoint, context),
    };
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::SmoothCommand>::canBlend(const AcceleratedEffectShapeFunction::SmoothCommand& from, const AcceleratedEffectShapeFunction::SmoothCommand& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.toBy, to.toBy);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::SmoothCommand>::blend(const AcceleratedEffectShapeFunction::SmoothCommand& from, const AcceleratedEffectShapeFunction::SmoothCommand& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::SmoothCommand
{
    return { .toBy = WebCore::blendForAcceleratedEffect(from.toBy, to.toBy, context) };
}

// MARK: - AcceleratedEffectShapeFunction::ArcCommand (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::ArcCommand>::canBlend(const AcceleratedEffectShapeFunction::ArcCommand& from, const AcceleratedEffectShapeFunction::ArcCommand& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from.toBy, to.toBy);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::ArcCommand>::blend(const AcceleratedEffectShapeFunction::ArcCommand& from, const AcceleratedEffectShapeFunction::ArcCommand& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::ArcCommand
{
    return {
        .toBy = WebCore::blendForAcceleratedEffect(from.toBy, to.toBy, context),
        .size = WebCore::blendForAcceleratedEffect(from.size, to.size, context),
        .arcSweep = blendWithPreferredValue(from.arcSweep, to.arcSweep, AcceleratedEffectShapeFunction::ArcSweep::Cw, context),
        .arcSize = blendWithPreferredValue(from.arcSize, to.arcSize, AcceleratedEffectShapeFunction::ArcSize::Large, context),
        .rotation = WebCore::blend(from.rotation, to.rotation, context),
    };
}

// MARK: - AcceleratedEffectShapeFunction::CloseCommand (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CloseCommand>::canBlend(const AcceleratedEffectShapeFunction::CloseCommand&, const AcceleratedEffectShapeFunction::CloseCommand&) -> bool
{
    return true;
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::CloseCommand>::blend(const AcceleratedEffectShapeFunction::CloseCommand& from, const AcceleratedEffectShapeFunction::CloseCommand&, const BlendingContext&) -> AcceleratedEffectShapeFunction::CloseCommand
{
    return from;
}

// MARK: - AcceleratedEffectShapeFunction::Command (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::Command>::canBlend(const AcceleratedEffectShapeFunction::Command& from, const AcceleratedEffectShapeFunction::Command& to) -> bool
{
    return WebCore::canBlendForAcceleratedEffect(from, to);
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction::Command>::blend(const AcceleratedEffectShapeFunction::Command& from, const AcceleratedEffectShapeFunction::Command& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction::Command
{
    return WebCore::blendForAcceleratedEffect(from, to, context);
}

// MARK: - AcceleratedEffectShapeFunction (blending)

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction>::canBlend(const AcceleratedEffectShapeFunction& from, const AcceleratedEffectShapeFunction& to) -> bool
{
    if (windRule(from) != windRule(to))
        return false;

    if (from.commands.size() != to.commands.size())
        return false;

    for (auto&& [commandFrom, commandTo] : zippedRange(from.commands, to.commands)) {
        if (!WebCore::canBlendForAcceleratedEffect(commandFrom, commandTo))
            return false;
    }
    return true;
}

auto AcceleratedEffectInterpolation<AcceleratedEffectShapeFunction>::blend(const AcceleratedEffectShapeFunction& from, const AcceleratedEffectShapeFunction& to, const BlendingContext& context) -> AcceleratedEffectShapeFunction
{
    if (context.isDiscrete) {
        ASSERT(!context.progress || context.progress == 1.0);
        return context.progress ? to : from;
    }

    auto blendedCommands = [&] {
        Vector<AcceleratedEffectShapeFunction::Command> commands;
        commands.reserveInitialCapacity(from.commands.size());
        for (auto&& [commandFrom, commandTo] : zippedRange(from.commands, to.commands))
            commands.append(WebCore::blendForAcceleratedEffect(commandFrom, commandTo, context));
        return commands;
    };

    return {
        .fillRule = from.fillRule,
        .startingPoint = WebCore::blendForAcceleratedEffect(from.startingPoint, to.startingPoint, context),
        .commands = blendedCommands(),
    };
}

bool canBlendShapeWithPath(const AcceleratedEffectShapeFunction& shape, const AcceleratedEffectPathFunction& path)
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
    return shapeFromPath && WebCore::canBlendForAcceleratedEffect(shape, *shapeFromPath);
}

std::optional<AcceleratedEffectShapeFunction> makeShapeFromPath(const AcceleratedEffectPathFunction& path)
{
    // FIXME: Not clear how to convert a initial Move command to the Shape's "from" parameter.
    // https://github.com/w3c/csswg-drafts/issues/10740

    Vector<AcceleratedEffectShapeFunction::Command> shapeCommands;
    AcceleratedEffectShapeConversionPathConsumer converter(shapeCommands);
    SVGPathByteStreamSource source(path.data.byteStream);

    if (!SVGPathParser::parse(source, converter, UnalteredParsing))
        return { };

    return AcceleratedEffectShapeFunction {
        .fillRule = path.fillRule,
        .startingPoint = converter.initialMove().value_or(FloatPoint { 0, 0 }),
        .commands = { WTF::move(shapeCommands) },
    };
}

} // namespace WebCore

#endif // ENABLE(THREADED_ANIMATIONS)
