/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "BasicShapesShape.h"

#include "AnimationUtilities.h"
#include "BasicShapeConversion.h"
#include "CalculationValue.h"
#include "FloatRect.h"
#include "FloatRoundedRect.h"
#include "LengthFunctions.h"
#include "Path.h"
#include "RenderBox.h"
#include "SVGPathBuilder.h"
#include "SVGPathByteStreamSource.h"
#include "SVGPathParser.h"
#include "SVGPathSegList.h"
#include "SVGPathSegValue.h"
#include "SVGPathSource.h"
#include <wtf/MathExtras.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

class ShapeSVGPathSource final : public SVGPathSource {
public:
    explicit ShapeSVGPathSource(const LengthPoint& startPoint, const BasicShapeShape& shape, const FloatSize& boxSize)
        : m_start(startPoint)
        , m_shape(shape)
        , m_boxSize(boxSize)
        , m_endIndex(shape.segments().size())
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

    std::optional<MoveToSegment> parseMoveToSegment() override
    {
        if (!m_nextIndex)
            return MoveToSegment { floatPointForLengthPoint(m_start, m_boxSize) };

        auto& moveSegment = currentValue<ShapeMoveSegment>();
        return MoveToSegment { floatPointForLengthPoint(moveSegment.offset(), m_boxSize) };
    }

    std::optional<LineToSegment> parseLineToSegment() override
    {
        auto& lineSegment = currentValue<ShapeLineSegment>();
        return LineToSegment { floatPointForLengthPoint(lineSegment.offset(), m_boxSize) };
    }

    std::optional<LineToHorizontalSegment> parseLineToHorizontalSegment() override
    {
        auto& lineSegment = currentValue<ShapeHorizontalLineSegment>();
        return LineToHorizontalSegment { floatValueForLength(lineSegment.length(), m_boxSize.width()) };
    }

    std::optional<LineToVerticalSegment> parseLineToVerticalSegment() override
    {
        auto& lineSegment = currentValue<ShapeVerticalLineSegment>();
        return LineToVerticalSegment { floatValueForLength(lineSegment.length(), m_boxSize.height()) };
    }

    std::optional<CurveToCubicSegment> parseCurveToCubicSegment() override
    {
        auto& curveSegment = currentValue<ShapeCurveSegment>();
        ASSERT(curveSegment.controlPoint2());
        return CurveToCubicSegment {
            floatPointForLengthPoint(curveSegment.controlPoint1(), m_boxSize),
            floatPointForLengthPoint(curveSegment.controlPoint2().value(), m_boxSize),
            floatPointForLengthPoint(curveSegment.offset(), m_boxSize)
        };
    }

    std::optional<CurveToQuadraticSegment> parseCurveToQuadraticSegment() override
    {
        auto& curveSegment = currentValue<ShapeCurveSegment>();
        ASSERT(!curveSegment.controlPoint2());
        return CurveToQuadraticSegment {
            floatPointForLengthPoint(curveSegment.controlPoint1(), m_boxSize),
            floatPointForLengthPoint(curveSegment.offset(), m_boxSize)
        };
    }

    std::optional<CurveToCubicSmoothSegment> parseCurveToCubicSmoothSegment() override
    {
        auto& smoothSegment = currentValue<ShapeSmoothSegment>();
        ASSERT(smoothSegment.intermediatePoint());
        return CurveToCubicSmoothSegment {
            floatPointForLengthPoint(smoothSegment.intermediatePoint().value(), m_boxSize),
            floatPointForLengthPoint(smoothSegment.offset(), m_boxSize)
        };
    }

    std::optional<CurveToQuadraticSmoothSegment> parseCurveToQuadraticSmoothSegment() override
    {
        auto& smoothSegment = currentValue<ShapeSmoothSegment>();
        return CurveToQuadraticSmoothSegment { floatPointForLengthPoint(smoothSegment.offset(), m_boxSize) };
    }

    std::optional<ArcToSegment> parseArcToSegment() override
    {
        auto& arcSegment = currentValue<ShapeArcSegment>();
        auto radius = floatSizeForLengthSize(arcSegment.ellipseSize(), m_boxSize);
        return ArcToSegment {
            .rx = radius.width(),
            .ry = radius.height(),
            .angle = narrowPrecisionToFloat(arcSegment.angle()),
            .largeArc = arcSegment.arcSize() == ShapeArcSegment::ArcSize::Large,
            .sweep = arcSegment.sweep() == RotationDirection::Clockwise,
            .targetPoint = floatPointForLengthPoint(arcSegment.offset(), m_boxSize)
        };
    }

    SVGPathSegType segmentTypeAtIndex(size_t index) const
    {
        if (index >= m_shape.segments().size())
            return SVGPathSegType::Unknown;

        auto& segment = m_shape.segments()[index];
        return WTF::switchOn(segment,
            [&](const ShapeMoveSegment& segment) {
                return segment.affinity() == CoordinateAffinity::Absolute ? SVGPathSegType::MoveToAbs : SVGPathSegType::MoveToRel;
            },
            [&](const ShapeLineSegment& segment) {
                return segment.affinity() == CoordinateAffinity::Absolute ? SVGPathSegType::LineToAbs : SVGPathSegType::LineToRel;
            },
            [&](const ShapeHorizontalLineSegment& segment) {
                return segment.affinity() == CoordinateAffinity::Absolute ? SVGPathSegType::LineToHorizontalAbs : SVGPathSegType::LineToHorizontalRel;
            },
            [&](const ShapeVerticalLineSegment& segment) {
                return segment.affinity() == CoordinateAffinity::Absolute ? SVGPathSegType::LineToVerticalAbs : SVGPathSegType::LineToVerticalRel;
            },
            [&](const ShapeCurveSegment& segment) {
                if (segment.controlPoint2())
                    return segment.affinity() == CoordinateAffinity::Absolute ? SVGPathSegType::CurveToCubicAbs : SVGPathSegType::CurveToCubicRel;

                return segment.affinity() == CoordinateAffinity::Absolute ? SVGPathSegType::CurveToQuadraticAbs : SVGPathSegType::CurveToQuadraticRel;
            },
            [&](const ShapeSmoothSegment& segment) {
                if (segment.intermediatePoint())
                    return segment.affinity() == CoordinateAffinity::Absolute ? SVGPathSegType::CurveToCubicSmoothAbs : SVGPathSegType::CurveToCubicSmoothRel;

                return segment.affinity() == CoordinateAffinity::Absolute ? SVGPathSegType::CurveToQuadraticSmoothAbs : SVGPathSegType::CurveToQuadraticSmoothRel;
            },
            [&](const ShapeArcSegment& segment) {
                return segment.affinity() == CoordinateAffinity::Absolute ? SVGPathSegType::ArcAbs : SVGPathSegType::ArcRel;
            },
            [&](const ShapeCloseSegment&) {
                return SVGPathSegType::ClosePath;
            }
        );
    }

    template<typename T>
    const T& currentValue() const
    {
        ASSERT(m_nextIndex);
        ASSERT(m_nextIndex <= m_shape.segments().size());
        return std::get<T>(m_shape.segments()[m_nextIndex - 1]);
    }

    const LengthPoint& m_start;
    const BasicShapeShape& m_shape;
    FloatSize m_boxSize;
    size_t m_endIndex { 0 };
    size_t m_nextIndex { 0 };
};

// MARK: -

class ShapeConversionPathConsumer final : public SVGPathConsumer {
public:
    ShapeConversionPathConsumer(Vector<BasicShapeShape::ShapeSegment>& segmentList)
        : m_segmentList(segmentList)
    { }

    const std::optional<CoordinatePair>& initialMove() const { return m_initialMove; }

private:
    static CoordinateAffinity fromCoordinateMode(PathCoordinateMode mode)
    {
        switch (mode) {
        case AbsoluteCoordinates:
            return CoordinateAffinity::Absolute;
        case RelativeCoordinates:
            return CoordinateAffinity::Relative;
        }
        return CoordinateAffinity::Absolute;
    }

    static CoordinatePair fromPoint(FloatPoint p)
    {
        return { Length(p.x(), LengthType::Fixed), Length(p.y(), LengthType::Fixed) };
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
        if (m_segmentList.isEmpty() && mode == PathCoordinateMode::AbsoluteCoordinates && !m_initialMove) {
            m_initialMove = fromPoint(offsetPoint);
            return;
        }

        auto offset = fromPoint(offsetPoint);
        m_segmentList.append(ShapeMoveSegment(fromCoordinateMode(mode), WTFMove(offset)));
    }

    void lineTo(const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        auto offset = fromPoint(offsetPoint);
        m_segmentList.append(ShapeLineSegment(fromCoordinateMode(mode), WTFMove(offset)));
    }

    void lineToHorizontal(float len, PathCoordinateMode mode) override
    {
        auto length = Length(len, LengthType::Fixed);
        m_segmentList.append(ShapeHorizontalLineSegment(fromCoordinateMode(mode), WTFMove(length)));
    }

    void lineToVertical(float len, PathCoordinateMode mode) override
    {
        auto length = Length(len, LengthType::Fixed);
        m_segmentList.append(ShapeVerticalLineSegment(fromCoordinateMode(mode), WTFMove(length)));
    }

    void curveToCubic(const FloatPoint& controlPoint1, const FloatPoint& controlPoint2, const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        auto offset = fromPoint(offsetPoint);
        auto c1 = fromPoint(controlPoint1);
        auto c2 = fromPoint(controlPoint2);
        m_segmentList.append(ShapeCurveSegment(fromCoordinateMode(mode), WTFMove(offset), WTFMove(c1), WTFMove(c2)));
    }

    void curveToQuadratic(const FloatPoint& controlPoint, const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        auto offset = fromPoint(offsetPoint);
        auto cp = fromPoint(controlPoint);
        m_segmentList.append(ShapeCurveSegment(fromCoordinateMode(mode), WTFMove(offset), WTFMove(cp)));
    }

    void curveToCubicSmooth(const FloatPoint& controlPoint2, const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        auto offset = fromPoint(offsetPoint);
        auto c2 = fromPoint(controlPoint2);
        m_segmentList.append(ShapeSmoothSegment(fromCoordinateMode(mode), WTFMove(offset), WTFMove(c2)));
    }

    void curveToQuadraticSmooth(const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        auto offset = fromPoint(offsetPoint);
        m_segmentList.append(ShapeSmoothSegment(fromCoordinateMode(mode), WTFMove(offset)));
    }

    void arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& offsetPoint, PathCoordinateMode mode) override
    {
        auto offset = fromPoint(offsetPoint);
        auto ellipseSize = LengthSize { Length(r1, LengthType::Fixed), Length(r2, LengthType::Fixed) };
        auto sweep = sweepFlag ? RotationDirection::Clockwise : RotationDirection::Counterclockwise;
        auto arcSize = largeArcFlag ? ShapeArcSegment::ArcSize::Large : ShapeArcSegment::ArcSize::Small;
        m_segmentList.append(ShapeArcSegment(fromCoordinateMode(mode), WTFMove(offset), WTFMove(ellipseSize), sweep, arcSize, angle));
    }

    void closePath() override
    {
        m_segmentList.append(ShapeCloseSegment());
    }

    Vector<BasicShapeShape::ShapeSegment>& m_segmentList;
    std::optional<CoordinatePair> m_initialMove;
};

// MARK: -

Ref<BasicShapeShape> BasicShapeShape::create(WindRule windRule, const CoordinatePair& startPoint, Vector<ShapeSegment>&& commands)
{
    return adoptRef(* new BasicShapeShape(windRule, startPoint, WTFMove(commands)));
}

RefPtr<BasicShapeShape> BasicShapeShape::createFromPath(const BasicShapePath& path)
{
    auto* pathData = path.pathData();
    if (!pathData)
        return nullptr;

    // FIXME: Isn't not totally clear how to convert a initial Move command to the Shape's "from" parameter.
    // https://github.com/w3c/csswg-drafts/issues/10740
    Vector<ShapeSegment> shapeCommands;

    ShapeConversionPathConsumer converter(shapeCommands);
    SVGPathByteStreamSource source(*pathData);

    if (!SVGPathParser::parse(source, converter, UnalteredParsing))
        return nullptr;

    return adoptRef(* new BasicShapeShape(path.windRule(), converter.initialMove().value_or(CoordinatePair { }), WTFMove(shapeCommands)));
}

BasicShapeShape::BasicShapeShape(WindRule windRule, const CoordinatePair& startPoint, Vector<ShapeSegment>&& commands)
    : m_startPoint(startPoint)
    , m_windRule(windRule)
    , m_segments(WTFMove(commands))
{
}

Ref<BasicShape> BasicShapeShape::clone() const
{
    auto segmentsCopy = m_segments;
    return BasicShapeShape::create(windRule(), startPoint(), WTFMove(segmentsCopy));
}

Path BasicShapeShape::path(const FloatRect& referenceRect) const
{
    // FIXME: We should do some caching here.
    auto pathSource = ShapeSVGPathSource(m_startPoint, *this, referenceRect.size());
    Path path;
    SVGPathBuilder builder(path);
    SVGPathParser::parse(pathSource, builder);
    path.translate(toFloatSize(referenceRect.location()));
    return path;
}

// https://drafts.csswg.org/css-shapes-2/#interpolating-shape
bool BasicShapeShape::canBlend(const ShapeSegment& segment1, const ShapeSegment& segment2)
{
    // For now, just check the types. There's some discussion about allowing blending between different segment types.
    if (segment1.index() != segment2.index())
        return false;

    return WTF::switchOn(segment1,
        [&](const ShapeMoveSegment& segment) {
            const auto& otherSegment = get<ShapeMoveSegment>(segment2);
            return segment.affinity() == otherSegment.affinity();
        },
        [&](const ShapeLineSegment& segment) {
            const auto& otherSegment = get<ShapeLineSegment>(segment2);
            return segment.affinity() == otherSegment.affinity();
        },
        [&](const ShapeHorizontalLineSegment& segment) {
            const auto& otherSegment = get<ShapeHorizontalLineSegment>(segment2);
            return segment.affinity() == otherSegment.affinity();
        },
        [&](const ShapeVerticalLineSegment& segment) {
            const auto& otherSegment = get<ShapeVerticalLineSegment>(segment2);
            return segment.affinity() == otherSegment.affinity();
        },
        [&](const ShapeCurveSegment& segment) {
            const auto& otherSegment = get<ShapeCurveSegment>(segment2);
            if (segment.affinity() != otherSegment.affinity())
                return false;
            return segment.controlPoint2().has_value() == otherSegment.controlPoint2().has_value();
        },
        [&](const ShapeSmoothSegment& segment) {
            const auto& otherSegment = get<ShapeSmoothSegment>(segment2);
            return segment.intermediatePoint().has_value() == otherSegment.intermediatePoint().has_value();
        },
        [&](const ShapeArcSegment& segment) {
            const auto& otherSegment = get<ShapeArcSegment>(segment2);
            return segment.affinity() == otherSegment.affinity();
        },
        [&](const ShapeCloseSegment&) {
            return true;
        }
    );

    ASSERT_NOT_REACHED();
    return false;
}

template <typename T>
T blendWithPreferredValue(const T& from, const T& to, const T& preferredValue, const BlendingContext& context)
{
    if (context.progress <= 0)
        return from;

    if (context.progress >= 1)
        return to;

    if (from == to)
        return from;

    return preferredValue;
}

auto BasicShapeShape::blend(const ShapeSegment& fromSegment, const ShapeSegment& toSegment, const BlendingContext& context) -> ShapeSegment
{
    ASSERT(fromSegment.index() == toSegment.index());
    ASSERT(canBlend(fromSegment, toSegment));

    return WTF::switchOn(fromSegment,
        [&](const ShapeMoveSegment& fromSegment) {
            const auto& toMoveSegment = get<ShapeMoveSegment>(toSegment);
            auto result = fromSegment;
            result.setOffset(WebCore::blend(fromSegment.offset(), toMoveSegment.offset(), context));
            return ShapeSegment(result);
        },
        [&](const ShapeLineSegment& fromSegment) {
            const auto& toLineSegment = get<ShapeLineSegment>(toSegment);
            auto result = fromSegment;
            result.setOffset(WebCore::blend(fromSegment.offset(), toLineSegment.offset(), context));
            return ShapeSegment(result);
        },
        [&](const ShapeHorizontalLineSegment& fromSegment) {
            const auto& toHLineSegment = get<ShapeHorizontalLineSegment>(toSegment);
            auto result = fromSegment;
            result.setLength(WebCore::blend(fromSegment.length(), toHLineSegment.length(), context));
            return ShapeSegment(result);
        },
        [&](const ShapeVerticalLineSegment& fromSegment) {
            const auto& toVLineSegment = get<ShapeVerticalLineSegment>(toSegment);
            auto result = fromSegment;
            result.setLength(WebCore::blend(fromSegment.length(), toVLineSegment.length(), context));
            return ShapeSegment(result);
        },
        [&](const ShapeCurveSegment& fromSegment) {
            const auto& toCurveSegment = get<ShapeCurveSegment>(toSegment);
            auto result = fromSegment;

            result.setOffset(WebCore::blend(fromSegment.offset(), toCurveSegment.offset(), context));
            result.setControlPoint1(WebCore::blend(fromSegment.controlPoint1(), toCurveSegment.controlPoint1(), context));
            if (fromSegment.controlPoint2()) {
                ASSERT(toCurveSegment.controlPoint2().has_value());
                result.setControlPoint2(WebCore::blend(fromSegment.controlPoint2().value(), toCurveSegment.controlPoint2().value(), context));
            }

            return ShapeSegment(result);
        },
        [&](const ShapeSmoothSegment& fromSegment) {
            const auto& toSmoothSegment = get<ShapeSmoothSegment>(toSegment);
            auto result = fromSegment;

            result.setOffset(WebCore::blend(fromSegment.offset(), toSmoothSegment.offset(), context));
            if (fromSegment.intermediatePoint()) {
                ASSERT(toSmoothSegment.intermediatePoint().has_value());
                result.setIntermediatePoint(WebCore::blend(fromSegment.intermediatePoint().value(), toSmoothSegment.intermediatePoint().value(), context));
            }

            return ShapeSegment(result);
        },
        [&](const ShapeArcSegment& fromSegment) {
            const auto& toArcSegment = get<ShapeArcSegment>(toSegment);
            auto result = fromSegment;

            result.setOffset(WebCore::blend(fromSegment.offset(), toArcSegment.offset(), context));
            result.setEllipseSize(WebCore::blend(fromSegment.ellipseSize(), toArcSegment.ellipseSize(), context));
            result.setAngle(WebCore::blend(fromSegment.angle(), toArcSegment.angle(), context));

            // /If an arc command has different <arc-sweep> between its starting and ending list,
            // then the interpolated result uses cw for any progress value between 0 and 1.
            // If it has different <arc-size> keywords, then the interpolated result uses large
            // for any progress value between 0 and 1.
            result.setSweep(blendWithPreferredValue(fromSegment.sweep(), toArcSegment.sweep(), RotationDirection::Clockwise, context));
            result.setArcSize(blendWithPreferredValue(fromSegment.arcSize(), toArcSegment.arcSize(), ShapeArcSegment::ArcSize::Large, context));

            return ShapeSegment(result);

        },
        [&](const ShapeCloseSegment& fromSegment) {
            return ShapeSegment(fromSegment);
        }
    );

    ASSERT_NOT_REACHED();
    return ShapeCloseSegment();
}

bool BasicShapeShape::canBlend(const BasicShape& other) const
{
    if (other.type() == Type::Path)
        return canBlendWithPath(downcast<BasicShapePath>(other));

    if (other.type() != type())
        return false;

    const auto& otherShape = downcast<BasicShapeShape>(other);
    if (otherShape.windRule() != windRule())
        return false;

    if (otherShape.segments().size() != segments().size())
        return false;

    for (size_t i = 0; i < segments().size(); ++i) {
        const auto& thisSegment = segments()[i];
        const auto& otherSegment = otherShape.segments()[i];

        if (!canBlend(thisSegment, otherSegment))
            return false;
    }

    return true;
}

// https://drafts.csswg.org/css-shapes-2/#interpolating-shape
bool BasicShapeShape::canBlendWithPath(const BasicShapePath& path) const
{
    if (path.windRule() != windRule())
        return false;

    RefPtr pathAsShape = BasicShapeShape::createFromPath(path);
    return pathAsShape && canBlend(*pathAsShape);
}

Ref<BasicShape> BasicShapeShape::blend(const BasicShape& from, const BlendingContext& context) const
{
    if (from.type() == Type::Path)
        return BasicShapeShape::blendWithPath(from, *this, context);

    ASSERT(type() == from.type());
    const auto& fromShape = downcast<BasicShapeShape>(from);

    auto startPoint = WebCore::blend(fromShape.startPoint(), this->startPoint(), context);
    auto segmentsCopy = m_segments;

    Ref result = BasicShapeShape::create(windRule(), startPoint, WTFMove(segmentsCopy));

    ASSERT(fromShape.segments().size() == segments().size());

    for (size_t i = 0; i < segments().size(); ++i) {
        const auto& toSegment = result->segments()[i];
        const auto& fromSegment = fromShape.segments()[i];
        result->segments()[i] = blend(fromSegment, toSegment, context);
    }

    return result;
}

Ref<BasicShape> BasicShapeShape::blendWithPath(const BasicShape& from, const BasicShape& to, const BlendingContext& context)
{
    if (from.type() == Type::Path) {
        RefPtr fromAsShape = BasicShapeShape::createFromPath(downcast<BasicShapePath>(from));
        if (!fromAsShape)
            return to.clone();
        return to.blend(*fromAsShape, context);
    }

    ASSERT(to.type() == Type::Path);
    RefPtr toAsShape = BasicShapeShape::createFromPath(downcast<BasicShapePath>(to));
    if (!toAsShape)
        return from.clone();
    return toAsShape->blend(from, context);
}

bool BasicShapeShape::operator==(const BasicShape& other) const
{
    if (type() != other.type())
        return false;

    const auto& otherShape = downcast<BasicShapeShape>(other);
    if (windRule() != otherShape.windRule())
        return false;

    if (startPoint() != otherShape.startPoint())
        return false;

    return segments() == otherShape.segments();
}

void BasicShapeShape::dump(TextStream& stream) const
{
    stream.dumpProperty("wind rule", windRule());
    stream.dumpProperty("start point", startPoint());
    stream << segments();
}

TextStream& operator<<(TextStream& stream, const BasicShapeShape::ShapeSegment& segment)
{
    WTF::switchOn(segment,
        [&](const ShapeMoveSegment& segment) {
            stream << "move" << (segment.affinity() == CoordinateAffinity::Relative ? " by "_s : " to "_s) << segment.offset();
        },
        [&](const ShapeLineSegment& segment) {
            stream << "line" << (segment.affinity() == CoordinateAffinity::Relative ? " by "_s : " to "_s) << segment.offset();
        },
        [&](const ShapeHorizontalLineSegment& segment) {
            stream << "hline" << (segment.affinity() == CoordinateAffinity::Relative ? " by "_s : " to "_s) << segment.length();
        },
        [&](const ShapeVerticalLineSegment& segment) {
            stream << "vline" << (segment.affinity() == CoordinateAffinity::Relative ? " by "_s : " to "_s) << segment.length();
        },
        [&](const ShapeCurveSegment& segment) {
            stream << "curve" << (segment.affinity() == CoordinateAffinity::Relative ? " by "_s : " to "_s) << segment.offset() << " using " << segment.controlPoint1();
            if (segment.controlPoint2())
                stream << " " << segment.controlPoint2().value();
        },
        [&](const ShapeSmoothSegment& segment) {
            stream << "smooth" << (segment.affinity() == CoordinateAffinity::Relative ? " by "_s : " to "_s) << segment.offset();
            if (segment.intermediatePoint())
                stream << " using " << segment.intermediatePoint().value();
        },
        [&](const ShapeArcSegment& segment) {
            stream << "arc" << (segment.affinity() == CoordinateAffinity::Relative ? " by "_s : " to "_s) << segment.offset() << " of " << segment.ellipseSize();
            stream << " " << segment.sweep() << (segment.arcSize() == ShapeArcSegment::ArcSize::Small ? " small " : " large ") << " " << segment.angle() << "deg";
        },
        [&](const ShapeCloseSegment&) {
            stream << "close";
        }
    );

    return stream;
}

} // namespace WebCore
