/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SVGPathSegListSource.h"

#include "SVGPathSeg.h"
#include "SVGPathSegList.h"
#include "SVGPathSegValue.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

SVGPathSegListSource::SVGPathSegListSource(const SVGPathSegList& pathSegList)
    : m_pathSegList(pathSegList)
{
    m_itemCurrent = 0;
    m_itemEnd = m_pathSegList.size();
}

bool SVGPathSegListSource::hasMoreData() const
{
    return m_itemCurrent < m_itemEnd;
}

SVGPathSegType SVGPathSegListSource::nextCommand(SVGPathSegType)
{
    m_segment = m_pathSegList.at(m_itemCurrent);
    SVGPathSegType pathSegType = static_cast<SVGPathSegType>(m_segment->pathSegType());
    ++m_itemCurrent;
    return pathSegType;
}

Optional<SVGPathSegType> SVGPathSegListSource::parseSVGSegmentType()
{
    m_segment = m_pathSegList.at(m_itemCurrent);
    SVGPathSegType pathSegType = static_cast<SVGPathSegType>(m_segment->pathSegType());
    ++m_itemCurrent;
    return pathSegType;
}

Optional<SVGPathSource::MoveToSegment> SVGPathSegListSource::parseMoveToSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == PathSegMoveToAbs || m_segment->pathSegType() == PathSegMoveToRel);
    SVGPathSegSingleCoordinate* moveTo = static_cast<SVGPathSegSingleCoordinate*>(m_segment.get());
    
    MoveToSegment segment;
    segment.targetPoint = FloatPoint(moveTo->x(), moveTo->y());
    return segment;
}

Optional<SVGPathSource::LineToSegment> SVGPathSegListSource::parseLineToSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == PathSegLineToAbs || m_segment->pathSegType() == PathSegLineToRel);
    SVGPathSegSingleCoordinate* lineTo = static_cast<SVGPathSegSingleCoordinate*>(m_segment.get());
    
    LineToSegment segment;
    segment.targetPoint = FloatPoint(lineTo->x(), lineTo->y());
    return segment;
}

Optional<SVGPathSource::LineToHorizontalSegment> SVGPathSegListSource::parseLineToHorizontalSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == PathSegLineToHorizontalAbs || m_segment->pathSegType() == PathSegLineToHorizontalRel);
    SVGPathSegLinetoHorizontal* horizontal = static_cast<SVGPathSegLinetoHorizontal*>(m_segment.get());
    
    LineToHorizontalSegment segment;
    segment.x = horizontal->x();
    return segment;
}

Optional<SVGPathSource::LineToVerticalSegment> SVGPathSegListSource::parseLineToVerticalSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == PathSegLineToVerticalAbs || m_segment->pathSegType() == PathSegLineToVerticalRel);
    SVGPathSegLinetoVertical* vertical = static_cast<SVGPathSegLinetoVertical*>(m_segment.get());
    
    LineToVerticalSegment segment;
    segment.y = vertical->y();
    return segment;
}

Optional<SVGPathSource::CurveToCubicSegment> SVGPathSegListSource::parseCurveToCubicSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == PathSegCurveToCubicAbs || m_segment->pathSegType() == PathSegCurveToCubicRel);
    SVGPathSegCurvetoCubic* cubic = static_cast<SVGPathSegCurvetoCubic*>(m_segment.get());
    
    CurveToCubicSegment segment;
    segment.point1 = FloatPoint(cubic->x1(), cubic->y1());
    segment.point2 = FloatPoint(cubic->x2(), cubic->y2());
    segment.targetPoint = FloatPoint(cubic->x(), cubic->y());
    return segment;
}

Optional<SVGPathSource::CurveToCubicSmoothSegment> SVGPathSegListSource::parseCurveToCubicSmoothSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == PathSegCurveToCubicSmoothAbs || m_segment->pathSegType() == PathSegCurveToCubicSmoothRel);
    SVGPathSegCurvetoCubicSmooth* cubicSmooth = static_cast<SVGPathSegCurvetoCubicSmooth*>(m_segment.get());
    
    CurveToCubicSmoothSegment segment;
    segment.point2 = FloatPoint(cubicSmooth->x2(), cubicSmooth->y2());
    segment.targetPoint = FloatPoint(cubicSmooth->x(), cubicSmooth->y());
    return segment;
}

Optional<SVGPathSource::CurveToQuadraticSegment> SVGPathSegListSource::parseCurveToQuadraticSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == PathSegCurveToQuadraticAbs || m_segment->pathSegType() == PathSegCurveToQuadraticRel);
    SVGPathSegCurvetoQuadratic* quadratic = static_cast<SVGPathSegCurvetoQuadratic*>(m_segment.get());
    
    CurveToQuadraticSegment segment;
    segment.point1 = FloatPoint(quadratic->x1(), quadratic->y1());
    segment.targetPoint = FloatPoint(quadratic->x(), quadratic->y());
    return segment;
}

Optional<SVGPathSource::CurveToQuadraticSmoothSegment> SVGPathSegListSource::parseCurveToQuadraticSmoothSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == PathSegCurveToQuadraticSmoothAbs || m_segment->pathSegType() == PathSegCurveToQuadraticSmoothRel);
    SVGPathSegSingleCoordinate* quadraticSmooth = static_cast<SVGPathSegSingleCoordinate*>(m_segment.get());
    
    CurveToQuadraticSmoothSegment segment;
    segment.targetPoint = FloatPoint(quadraticSmooth->x(), quadraticSmooth->y());
    return segment;
}

Optional<SVGPathSource::ArcToSegment> SVGPathSegListSource::parseArcToSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == PathSegArcAbs || m_segment->pathSegType() == PathSegArcRel);
    SVGPathSegArc* arcTo = static_cast<SVGPathSegArc*>(m_segment.get());
    
    ArcToSegment segment;
    segment.rx = arcTo->r1();
    segment.ry = arcTo->r2();
    segment.angle = arcTo->angle();
    segment.largeArc = arcTo->largeArcFlag();
    segment.sweep = arcTo->sweepFlag();
    segment.targetPoint = FloatPoint(arcTo->x(), arcTo->y());
    return segment;
}

}
