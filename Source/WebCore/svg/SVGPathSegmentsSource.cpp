/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "SVGPathSegmentsSource.h"

#include "FloatPoint.h"
#include <cmath>

namespace WebCore {

// Values each command carries, per the path grammar.
// https://w3c.github.io/svgwg/specs/paths/#PathDataBNF
static size_t valueCountForPathSegType(SVGPathSegType type)
{
    switch (type) {
    case SVGPathSegType::ClosePath:
        return 0;
    case SVGPathSegType::LineToHorizontalAbs:
    case SVGPathSegType::LineToHorizontalRel:
    case SVGPathSegType::LineToVerticalAbs:
    case SVGPathSegType::LineToVerticalRel:
        return 1;
    case SVGPathSegType::MoveToAbs:
    case SVGPathSegType::MoveToRel:
    case SVGPathSegType::LineToAbs:
    case SVGPathSegType::LineToRel:
    case SVGPathSegType::CurveToQuadraticSmoothAbs:
    case SVGPathSegType::CurveToQuadraticSmoothRel:
        return 2;
    case SVGPathSegType::CurveToQuadraticAbs:
    case SVGPathSegType::CurveToQuadraticRel:
    case SVGPathSegType::CurveToCubicSmoothAbs:
    case SVGPathSegType::CurveToCubicSmoothRel:
        return 4;
    case SVGPathSegType::CurveToCubicAbs:
    case SVGPathSegType::CurveToCubicRel:
        return 6;
    case SVGPathSegType::ArcAbs:
    case SVGPathSegType::ArcRel:
        return 7;
    case SVGPathSegType::Unknown:
        break;
    }
    return 0;
}

SVGPathSegmentsSource::SVGPathSegmentsSource(const Vector<SVGPathSegment>& segments)
    : m_segments(segments)
{
}

SVGPathSegType SVGPathSegmentsSource::validatedTypeAtCurrentIndex() const
{
    if (m_index >= m_segments.size())
        return SVGPathSegType::Unknown;

    auto& segment = m_segments[m_index];

    // "MM" is not an alias for "M". Only a single command letter is a type.
    if (segment.type.length() != 1)
        return SVGPathSegType::Unknown;

    auto type = pathSegTypeForLetter(segment.type[0]);
    if (type == SVGPathSegType::Unknown)
        return SVGPathSegType::Unknown;

    // Unlike the "d" attribute, extra values are not an implicit repeat here:
    // the count has to be exactly right.
    if (segment.values.size() != valueCountForPathSegType(type))
        return SVGPathSegType::Unknown;

    for (auto value : segment.values) {
        if (!std::isfinite(value))
            return SVGPathSegType::Unknown;
    }

    return type;
}

bool SVGPathSegmentsSource::hasMoreData() const
{
    return m_index < m_segments.size();
}

bool SVGPathSegmentsSource::moveToNextToken()
{
    // Nothing to skip: there is no whitespace between structured segments.
    return hasMoreData();
}

std::optional<SVGPathSegType> SVGPathSegmentsSource::parseSVGSegmentType()
{
    return consumeTypeAtCurrentIndex();
}

SVGPathSegType SVGPathSegmentsSource::nextCommand(SVGPathSegType)
{
    // Every entry states its own command, so there is no implicit repeat to
    // work out from the previous one.
    return consumeTypeAtCurrentIndex();
}

SVGPathSegType SVGPathSegmentsSource::consumeTypeAtCurrentIndex()
{
    auto type = validatedTypeAtCurrentIndex();

    // Closepath is the one command SVGPathParser handles without calling back
    // into the source: parseClosePathSegment() takes no values, so takeValues()
    // never runs for it. Step past the entry here, or the walk never advances
    // and the parser emits closepaths until it runs out of memory.
    if (type == SVGPathSegType::ClosePath)
        ++m_index;

    return type;
}

const Vector<float>& SVGPathSegmentsSource::takeValues()
{
    ASSERT(m_index < m_segments.size());
    return m_segments[m_index++].values;
}

std::optional<SVGPathSource::MoveToSegment> SVGPathSegmentsSource::parseMoveToSegment(FloatPoint)
{
    auto& values = takeValues();
    return MoveToSegment { FloatPoint { values[0], values[1] } };
}

std::optional<SVGPathSource::LineToSegment> SVGPathSegmentsSource::parseLineToSegment(FloatPoint)
{
    auto& values = takeValues();
    return LineToSegment { FloatPoint { values[0], values[1] } };
}

std::optional<SVGPathSource::LineToHorizontalSegment> SVGPathSegmentsSource::parseLineToHorizontalSegment(FloatPoint)
{
    return LineToHorizontalSegment { takeValues()[0] };
}

std::optional<SVGPathSource::LineToVerticalSegment> SVGPathSegmentsSource::parseLineToVerticalSegment(FloatPoint)
{
    return LineToVerticalSegment { takeValues()[0] };
}

std::optional<SVGPathSource::CurveToCubicSegment> SVGPathSegmentsSource::parseCurveToCubicSegment(FloatPoint)
{
    auto& values = takeValues();
    return CurveToCubicSegment {
        FloatPoint { values[0], values[1] },
        FloatPoint { values[2], values[3] },
        FloatPoint { values[4], values[5] }
    };
}

std::optional<SVGPathSource::CurveToCubicSmoothSegment> SVGPathSegmentsSource::parseCurveToCubicSmoothSegment(FloatPoint)
{
    auto& values = takeValues();
    return CurveToCubicSmoothSegment {
        FloatPoint { values[0], values[1] },
        FloatPoint { values[2], values[3] }
    };
}

std::optional<SVGPathSource::CurveToQuadraticSegment> SVGPathSegmentsSource::parseCurveToQuadraticSegment(FloatPoint)
{
    auto& values = takeValues();
    return CurveToQuadraticSegment {
        FloatPoint { values[0], values[1] },
        FloatPoint { values[2], values[3] }
    };
}

std::optional<SVGPathSource::CurveToQuadraticSmoothSegment> SVGPathSegmentsSource::parseCurveToQuadraticSmoothSegment(FloatPoint)
{
    auto& values = takeValues();
    return CurveToQuadraticSmoothSegment { FloatPoint { values[0], values[1] } };
}

std::optional<SVGPathSource::ArcToSegment> SVGPathSegmentsSource::parseArcToSegment(FloatPoint)
{
    auto& values = takeValues();
    ArcToSegment segment;
    segment.rx = values[0];
    segment.ry = values[1];
    segment.angle = values[2];
    // SVG 1.1 F.6.2: "Any nonzero value for either of the flags is taken to mean
    // the value 1." SVG 2 deleted that sentence and the Paths module never had
    // it, which is why Gecko rejects a flag that is not exactly 0 or 1 while
    // Blink coerces. setPathData-segment-types.html sides with Blink.
    segment.largeArc = !!values[3];
    segment.sweep = !!values[4];
    segment.targetPoint = FloatPoint { values[5], values[6] };
    return segment;
}

} // namespace WebCore
