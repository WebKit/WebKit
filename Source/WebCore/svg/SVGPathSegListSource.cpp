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

namespace WebCore {

SVGPathSegListSource::SVGPathSegListSource(const SVGPathSegList& pathSegList)
    : m_pathSegList(pathSegList)
{
    m_itemCurrent = 0;
    m_itemEnd = m_pathSegList->size();
}

bool SVGPathSegListSource::hasMoreData() const
{
    return m_itemCurrent < m_itemEnd;
}

SVGPathSegType SVGPathSegListSource::nextCommand(SVGPathSegType)
{
    m_segment = m_pathSegList->at(m_itemCurrent);
    SVGPathSegType pathSegType = static_cast<SVGPathSegType>(m_segment->pathSegType());
    ++m_itemCurrent;
    return pathSegType;
}

std::optional<SVGPathSegType> SVGPathSegListSource::parseSVGSegmentType()
{
    m_segment = m_pathSegList->at(m_itemCurrent);
    SVGPathSegType pathSegType = static_cast<SVGPathSegType>(m_segment->pathSegType());
    ++m_itemCurrent;
    return pathSegType;
}

std::optional<SVGPathSource::MoveToSegment> SVGPathSegListSource::parseMoveToSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == SVGPathSegType::MoveToAbs || m_segment->pathSegType() == SVGPathSegType::MoveToRel);
    RefPtr moveTo = static_cast<SVGPathSegSingleCoordinate*>(m_segment.get());

    MoveToSegment segment;
    segment.targetPoint = FloatPoint(moveTo->x(), moveTo->y());
    return segment;
}

std::optional<SVGPathSource::LineToSegment> SVGPathSegListSource::parseLineToSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == SVGPathSegType::LineToAbs || m_segment->pathSegType() == SVGPathSegType::LineToRel);
    RefPtr lineTo = static_cast<SVGPathSegSingleCoordinate*>(m_segment.get());
    
    LineToSegment segment;
    segment.targetPoint = FloatPoint(lineTo->x(), lineTo->y());
    return segment;
}

std::optional<SVGPathSource::LineToHorizontalSegment> SVGPathSegListSource::parseLineToHorizontalSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == SVGPathSegType::LineToHorizontalAbs || m_segment->pathSegType() == SVGPathSegType::LineToHorizontalRel);
    RefPtr horizontal = static_cast<SVGPathSegLinetoHorizontal*>(m_segment.get());
    
    LineToHorizontalSegment segment;
    segment.x = horizontal->x();
    return segment;
}

std::optional<SVGPathSource::LineToVerticalSegment> SVGPathSegListSource::parseLineToVerticalSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == SVGPathSegType::LineToVerticalAbs || m_segment->pathSegType() == SVGPathSegType::LineToVerticalRel);
    RefPtr vertical = static_cast<SVGPathSegLinetoVertical*>(m_segment.get());
    
    LineToVerticalSegment segment;
    segment.y = vertical->y();
    return segment;
}

std::optional<SVGPathSource::CurveToCubicSegment> SVGPathSegListSource::parseCurveToCubicSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == SVGPathSegType::CurveToCubicAbs || m_segment->pathSegType() == SVGPathSegType::CurveToCubicRel);
    RefPtr cubic = static_cast<SVGPathSegCurvetoCubic*>(m_segment.get());
    
    CurveToCubicSegment segment;
    segment.point1 = FloatPoint(cubic->x1(), cubic->y1());
    segment.point2 = FloatPoint(cubic->x2(), cubic->y2());
    segment.targetPoint = FloatPoint(cubic->x(), cubic->y());
    return segment;
}

std::optional<SVGPathSource::CurveToCubicSmoothSegment> SVGPathSegListSource::parseCurveToCubicSmoothSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == SVGPathSegType::CurveToCubicSmoothAbs || m_segment->pathSegType() == SVGPathSegType::CurveToCubicSmoothRel);
    RefPtr cubicSmooth = static_cast<SVGPathSegCurvetoCubicSmooth*>(m_segment.get());
    
    CurveToCubicSmoothSegment segment;
    segment.point2 = FloatPoint(cubicSmooth->x2(), cubicSmooth->y2());
    segment.targetPoint = FloatPoint(cubicSmooth->x(), cubicSmooth->y());
    return segment;
}

std::optional<SVGPathSource::CurveToQuadraticSegment> SVGPathSegListSource::parseCurveToQuadraticSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == SVGPathSegType::CurveToQuadraticAbs || m_segment->pathSegType() == SVGPathSegType::CurveToQuadraticRel);
    RefPtr quadratic = static_cast<SVGPathSegCurvetoQuadratic*>(m_segment.get());
    
    CurveToQuadraticSegment segment;
    segment.point1 = FloatPoint(quadratic->x1(), quadratic->y1());
    segment.targetPoint = FloatPoint(quadratic->x(), quadratic->y());
    return segment;
}

std::optional<SVGPathSource::CurveToQuadraticSmoothSegment> SVGPathSegListSource::parseCurveToQuadraticSmoothSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == SVGPathSegType::CurveToQuadraticSmoothAbs || m_segment->pathSegType() == SVGPathSegType::CurveToQuadraticSmoothRel);
    RefPtr quadraticSmooth = static_cast<SVGPathSegSingleCoordinate*>(m_segment.get());
    
    CurveToQuadraticSmoothSegment segment;
    segment.targetPoint = FloatPoint(quadraticSmooth->x(), quadraticSmooth->y());
    return segment;
}

std::optional<SVGPathSource::ArcToSegment> SVGPathSegListSource::parseArcToSegment()
{
    ASSERT(m_segment);
    ASSERT(m_segment->pathSegType() == SVGPathSegType::ArcAbs || m_segment->pathSegType() == SVGPathSegType::ArcRel);
    RefPtr arcTo = static_cast<SVGPathSegArc*>(m_segment.get());
    
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
