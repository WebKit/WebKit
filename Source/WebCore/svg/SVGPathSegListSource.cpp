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
    ++m_itemCurrent;
    return m_segment->pathSegType();
}

std::optional<SVGPathSegType> SVGPathSegListSource::parseSVGSegmentType()
{
    m_segment = m_pathSegList->at(m_itemCurrent);
    ++m_itemCurrent;
    return m_segment->pathSegType();
}

std::optional<SVGPathSource::MoveToSegment> SVGPathSegListSource::parseMoveToSegment(FloatPoint)
{
    ASSERT(m_segment);
    RefPtr moveTo = downcast<SVGPathSegMoveto>(m_segment.get());

    MoveToSegment segment;
    segment.targetPoint = FloatPoint(moveTo->x(), moveTo->y());
    return segment;
}

std::optional<SVGPathSource::LineToSegment> SVGPathSegListSource::parseLineToSegment(FloatPoint)
{
    ASSERT(m_segment);
    RefPtr lineTo = downcast<SVGPathSegLineto>(m_segment.get());

    LineToSegment segment;
    segment.targetPoint = FloatPoint(lineTo->x(), lineTo->y());
    return segment;
}

std::optional<SVGPathSource::LineToHorizontalSegment> SVGPathSegListSource::parseLineToHorizontalSegment(FloatPoint)
{
    ASSERT(m_segment);
    RefPtr horizontal = downcast<SVGPathSegLinetoHorizontal>(m_segment.get());
    
    LineToHorizontalSegment segment;
    segment.x = horizontal->x();
    return segment;
}

std::optional<SVGPathSource::LineToVerticalSegment> SVGPathSegListSource::parseLineToVerticalSegment(FloatPoint)
{
    ASSERT(m_segment);
    RefPtr vertical = downcast<SVGPathSegLinetoVertical>(m_segment.get());
    
    LineToVerticalSegment segment;
    segment.y = vertical->y();
    return segment;
}

std::optional<SVGPathSource::CurveToCubicSegment> SVGPathSegListSource::parseCurveToCubicSegment(FloatPoint)
{
    ASSERT(m_segment);
    RefPtr cubic = downcast<SVGPathSegCurvetoCubic>(m_segment.get());

    CurveToCubicSegment segment;
    segment.point1 = FloatPoint(cubic->x1(), cubic->y1());
    segment.point2 = FloatPoint(cubic->x2(), cubic->y2());
    segment.targetPoint = FloatPoint(cubic->x(), cubic->y());
    return segment;
}

std::optional<SVGPathSource::CurveToCubicSmoothSegment> SVGPathSegListSource::parseCurveToCubicSmoothSegment(FloatPoint)
{
    ASSERT(m_segment);
    RefPtr cubicSmooth = downcast<SVGPathSegCurvetoCubicSmooth>(m_segment.get());

    CurveToCubicSmoothSegment segment;
    segment.point2 = FloatPoint(cubicSmooth->x2(), cubicSmooth->y2());
    segment.targetPoint = FloatPoint(cubicSmooth->x(), cubicSmooth->y());
    return segment;
}

std::optional<SVGPathSource::CurveToQuadraticSegment> SVGPathSegListSource::parseCurveToQuadraticSegment(FloatPoint)
{
    ASSERT(m_segment);
    RefPtr quadratic = downcast<SVGPathSegCurvetoQuadratic>(m_segment.get());

    CurveToQuadraticSegment segment;
    segment.point1 = FloatPoint(quadratic->x1(), quadratic->y1());
    segment.targetPoint = FloatPoint(quadratic->x(), quadratic->y());
    return segment;
}

std::optional<SVGPathSource::CurveToQuadraticSmoothSegment> SVGPathSegListSource::parseCurveToQuadraticSmoothSegment(FloatPoint)
{
    ASSERT(m_segment);
    RefPtr quadraticSmooth = downcast<SVGPathSegCurvetoQuadraticSmooth>(m_segment.get());

    CurveToQuadraticSmoothSegment segment;
    segment.targetPoint = FloatPoint(quadraticSmooth->x(), quadraticSmooth->y());
    return segment;
}

std::optional<SVGPathSource::ArcToSegment> SVGPathSegListSource::parseArcToSegment(FloatPoint)
{
    ASSERT(m_segment);
    RefPtr arcTo = downcast<SVGPathSegArc>(m_segment.get());

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
