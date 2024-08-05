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
#include "SVGPathParser.h"
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

Ref<BasicShapeShape> BasicShapeShape::create(WindRule windRule, const CoordinatePair& startPoint, Vector<ShapeSegment>&& commands)
{
    return adoptRef(* new BasicShapeShape(windRule, startPoint, WTFMove(commands)));
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

bool BasicShapeShape::canBlend(const BasicShape&) const
{
    // Not yet implemented.
    return false;
}

Ref<BasicShape> BasicShapeShape::blend(const BasicShape&, const BlendingContext&) const
{
    // Not yet implemented.
    return BasicShapeShape::clone(); // FIXME wrong.
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
