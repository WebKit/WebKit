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
#include <wtf/MathExtras.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

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

Path BasicShapeShape::path(const FloatRect&) const
{
    // Not yet implemented.
    return Path();
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
