/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
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

#if ENABLE(SVG)
#include "SVGPathBlender.h"

#include "SVGPathSeg.h"

namespace WebCore {

SVGPathBlender::SVGPathBlender()
    : m_fromSource(0)
    , m_toSource(0)
    , m_consumer(0)
    , m_progress(0)
{
}

float SVGPathBlender::blendAnimatedFloat(float from, float to)
{
    return (to - from) * m_progress + from;
}

FloatPoint SVGPathBlender::blendAnimatedFloatPoint(FloatPoint& from, FloatPoint& to)
{
    return FloatPoint((to.x() - from.x()) * m_progress + from.x(), (to.y() - from.y()) * m_progress + from.y());
}

bool SVGPathBlender::blendMoveToSegment()
{
    FloatPoint fromTargetPoint;
    FloatPoint toTargetPoint;
    if (!m_fromSource->parseMoveToSegment(fromTargetPoint)
        || !m_toSource->parseMoveToSegment(toTargetPoint))
        return false;

    m_consumer->moveTo(blendAnimatedFloatPoint(fromTargetPoint, toTargetPoint), false, m_mode);
    return true;
}

bool SVGPathBlender::blendLineToSegment()
{
    FloatPoint fromTargetPoint;
    FloatPoint toTargetPoint;
    if (!m_fromSource->parseLineToSegment(fromTargetPoint)
        || !m_toSource->parseLineToSegment(toTargetPoint))
        return false;

    m_consumer->lineTo(blendAnimatedFloatPoint(fromTargetPoint, toTargetPoint), m_mode);
    return true;
}

bool SVGPathBlender::blendLineToHorizontalSegment()
{
    float fromX;
    float toX;
    if (!m_fromSource->parseLineToHorizontalSegment(fromX)
        || !m_toSource->parseLineToHorizontalSegment(toX))
        return false;

    m_consumer->lineToHorizontal(blendAnimatedFloat(fromX, toX), m_mode);
    return true;
}

bool SVGPathBlender::blendLineToVerticalSegment()
{
    float fromY;
    float toY;
    if (!m_fromSource->parseLineToVerticalSegment(fromY)
        || !m_toSource->parseLineToVerticalSegment(toY))
        return false;

    m_consumer->lineToVertical(blendAnimatedFloat(fromY, toY), m_mode);
    return true;
}

bool SVGPathBlender::blendCurveToCubicSegment()
{
    FloatPoint fromTargetPoint;
    FloatPoint fromPoint1;
    FloatPoint fromPoint2;
    FloatPoint toTargetPoint;
    FloatPoint toPoint1;
    FloatPoint toPoint2;
    if (!m_fromSource->parseCurveToCubicSegment(fromPoint1, fromPoint2, fromTargetPoint)
        || !m_toSource->parseCurveToCubicSegment(toPoint1, toPoint2, toTargetPoint))
        return false;

    m_consumer->curveToCubic(blendAnimatedFloatPoint(fromPoint1, toPoint1),
                             blendAnimatedFloatPoint(fromPoint2, toPoint2),
                             blendAnimatedFloatPoint(fromTargetPoint, toTargetPoint),
                             m_mode);
    return true;
}

bool SVGPathBlender::blendCurveToCubicSmoothSegment()
{
    FloatPoint fromTargetPoint;
    FloatPoint fromPoint2;
    FloatPoint toTargetPoint;
    FloatPoint toPoint2;
    if (!m_fromSource->parseCurveToCubicSmoothSegment(fromPoint2, fromTargetPoint)
        || !m_toSource->parseCurveToCubicSmoothSegment(toPoint2, toTargetPoint))
        return false;

    m_consumer->curveToCubicSmooth(blendAnimatedFloatPoint(fromPoint2, toPoint2),
                                   blendAnimatedFloatPoint(fromTargetPoint, toTargetPoint),
                                   m_mode);
    return true;
}

bool SVGPathBlender::blendCurveToQuadraticSegment()
{
    FloatPoint fromTargetPoint;
    FloatPoint fromPoint1;
    FloatPoint toTargetPoint;
    FloatPoint toPoint1;
    if (!m_fromSource->parseCurveToQuadraticSegment(fromPoint1, fromTargetPoint)
        || !m_toSource->parseCurveToQuadraticSegment(toPoint1, toTargetPoint))
        return false;

    m_consumer->curveToQuadratic(blendAnimatedFloatPoint(fromPoint1, toPoint1),
                                 blendAnimatedFloatPoint(fromTargetPoint, toTargetPoint),
                                 m_mode);
    return true;
}

bool SVGPathBlender::blendCurveToQuadraticSmoothSegment()
{
    FloatPoint fromTargetPoint;
    FloatPoint toTargetPoint;
    if (!m_fromSource->parseCurveToQuadraticSmoothSegment(fromTargetPoint)
        || !m_toSource->parseCurveToQuadraticSmoothSegment(toTargetPoint))
        return false;

    m_consumer->curveToQuadraticSmooth(blendAnimatedFloatPoint(fromTargetPoint, toTargetPoint), m_mode);
    return true;
}

bool SVGPathBlender::blendArcToSegment()
{
    float fromRx;
    float fromRy;
    float fromAngle;
    bool fromLargeArc;
    bool fromSweep;
    FloatPoint fromTargetPoint;
    float toRx;
    float toRy;
    float toAngle;
    bool toLargeArc;
    bool toSweep;
    FloatPoint toTargetPoint;
    if (!m_fromSource->parseArcToSegment(fromRx, fromRy, fromAngle, fromLargeArc, fromSweep, fromTargetPoint)
        || !m_toSource->parseArcToSegment(toRx, toRy, toAngle, toLargeArc, toSweep, toTargetPoint))
        return false;

    m_consumer->arcTo(blendAnimatedFloat(fromRx, toRx),
                      blendAnimatedFloat(fromRy, toRy),
                      blendAnimatedFloat(fromAngle, toAngle),
                      m_progress < 0.5 ? fromLargeArc : toLargeArc,
                      m_progress < 0.5 ? fromSweep : toSweep,
                      blendAnimatedFloatPoint(fromTargetPoint, toTargetPoint),
                      m_mode);
    return true;
}

bool SVGPathBlender::blendAnimatedPath(float progress, SVGPathSource* fromSource, SVGPathSource* toSource, SVGPathConsumer* consumer)
{
    ASSERT(fromSource);
    ASSERT(toSource);
    ASSERT(consumer);
    m_fromSource = fromSource;
    m_toSource = toSource;
    m_consumer = consumer;

    m_progress = progress;
    while (true) {
        SVGPathSegType fromCommand;
        SVGPathSegType toCommand;
        if (!m_fromSource->parseSVGSegmentType(fromCommand) || !m_toSource->parseSVGSegmentType(toCommand))
            return false;
        if (fromCommand != toCommand)
            return false;

        m_mode = AbsoluteCoordinates;
        switch (fromCommand) {
        case PathSegMoveToRel:
            m_mode = RelativeCoordinates;
        case PathSegMoveToAbs:
            if (!blendMoveToSegment())
                return false;
            break;
        case PathSegLineToRel:
            m_mode = RelativeCoordinates;
        case PathSegLineToAbs:
            if (!blendLineToSegment())
                return false;
            break;
        case PathSegLineToHorizontalRel:
            m_mode = RelativeCoordinates;
        case PathSegLineToHorizontalAbs:
            if (!blendLineToHorizontalSegment())
                return false;
            break;
        case PathSegLineToVerticalRel:
            m_mode = RelativeCoordinates;
        case PathSegLineToVerticalAbs:
            if (!blendLineToVerticalSegment())
                return false;
            break;
        case PathSegClosePath:
            m_consumer->closePath();
            break;
        case PathSegCurveToCubicRel:
            m_mode = RelativeCoordinates;
        case PathSegCurveToCubicAbs:
            if (!blendCurveToCubicSegment())
                return false;
            break;
        case PathSegCurveToCubicSmoothRel:
            m_mode = RelativeCoordinates;
        case PathSegCurveToCubicSmoothAbs:
            if (!blendCurveToCubicSmoothSegment())
                return false;
            break;
        case PathSegCurveToQuadraticRel:
            m_mode = RelativeCoordinates;
        case PathSegCurveToQuadraticAbs:
            if (!blendCurveToQuadraticSegment())
                return false;
            break;
        case PathSegCurveToQuadraticSmoothRel:
            m_mode = RelativeCoordinates;
        case PathSegCurveToQuadraticSmoothAbs:
            if (!blendCurveToQuadraticSmoothSegment())
                return false;
            break;
        case PathSegArcRel:
            m_mode = RelativeCoordinates;
        case PathSegArcAbs:
            if (!blendArcToSegment())
                return false;
            break;
        default:
            return false;
        }
        if (m_fromSource->hasMoreData() != m_toSource->hasMoreData())
            return false;
        if (!m_fromSource->hasMoreData() || !m_toSource->hasMoreData())
            break;
    }
    return true;
}

void SVGPathBlender::cleanup()
{
    ASSERT(m_toSource);
    ASSERT(m_fromSource);
    ASSERT(m_consumer);

    m_consumer->cleanup();
    m_toSource = 0;
    m_fromSource = 0;
    m_consumer = 0;
}

}

#endif // ENABLE(SVG)
