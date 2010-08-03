/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009 Apple Inc. All rights reserved.
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
#include "SVGPathParser.h"

#include "AffineTransform.h"
#include "SVGParserUtilities.h"
#include <wtf/MathExtras.h>

static const float gOneOverThree = 1 / 3.f;

namespace WebCore {

SVGPathParser::SVGPathParser(SVGPathConsumer* consumer)
    : m_consumer(consumer)
{
}

SVGPathParser::~SVGPathParser()
{
}

void SVGPathParser::parseClosePathSegment()
{
    // Reset m_currentPoint for the next path.
    if (m_normalized)
        m_currentPoint = m_subPathPoint;
    m_pathClosed = true;
    m_consumer->closePath();
}

bool SVGPathParser::parseMoveToSegment()
{
    float toX;
    float toY;
    if (!parseNumber(m_ptr, m_end, toX) || !parseNumber(m_ptr, m_end, toY))
        return false;

    FloatPoint toPoint(toX, toY);
    if (m_normalized) {
        if (m_mode == RelativeCoordinates)
            m_currentPoint += toPoint;
        else
            m_currentPoint = toPoint;
        m_subPathPoint = m_currentPoint;
        m_consumer->moveTo(m_currentPoint, m_pathClosed, AbsoluteCoordinates);
    } else
        m_consumer->moveTo(toPoint, m_pathClosed, m_mode);
    m_pathClosed = false;
    return true;
}

bool SVGPathParser::parseLineToSegment()
{
    float toX;
    float toY;
    if (!parseNumber(m_ptr, m_end, toX) || !parseNumber(m_ptr, m_end, toY))
        return false;

    FloatPoint toPoint(toX, toY);
    if (m_normalized) {
        if (m_mode == RelativeCoordinates)
            m_currentPoint += toPoint;
        else
            m_currentPoint = toPoint;
        m_consumer->lineTo(m_currentPoint, AbsoluteCoordinates);
    } else
        m_consumer->lineTo(toPoint, m_mode);
    return true;
}

bool SVGPathParser::parseLineToHorizontalSegment()
{
    float toX;
    if (!parseNumber(m_ptr, m_end, toX))
        return false;

    if (m_normalized) {
        if (m_mode == RelativeCoordinates)
            m_currentPoint.move(toX, 0);
        else
            m_currentPoint.setX(toX);
        m_consumer->lineTo(m_currentPoint, AbsoluteCoordinates);
    } else
        m_consumer->lineToHorizontal(toX, m_mode);
    return true;
}

bool SVGPathParser::parseLineToVerticalSegment()
{
    float toY;
    if (!parseNumber(m_ptr, m_end, toY))
        return false;

    if (m_normalized) {
        if (m_mode == RelativeCoordinates)
            m_currentPoint.move(0, toY);
        else
            m_currentPoint.setY(toY);
        m_consumer->lineTo(m_currentPoint, AbsoluteCoordinates);
    } else
        m_consumer->lineToVertical(toY, m_mode);
    return true;
}

bool SVGPathParser::parseCurveToCubicSegment()
{
    float x1;
    float y1;
    float x2;
    float y2;
    float toX;
    float toY; 
    if (!parseNumber(m_ptr, m_end, x1)
        || !parseNumber(m_ptr, m_end, y1)
        || !parseNumber(m_ptr, m_end, x2)
        || !parseNumber(m_ptr, m_end, y2)
        || !parseNumber(m_ptr, m_end, toX)
        || !parseNumber(m_ptr, m_end, toY))
        return false;

    FloatPoint point1(x1, y1);
    FloatPoint point2(x2, y2);
    FloatPoint point3(toX, toY);
    if (m_normalized) {
        if (m_mode == RelativeCoordinates) {
            point1 += m_currentPoint;
            point2 += m_currentPoint;
            point3 += m_currentPoint;
        }
        m_consumer->curveToCubic(point1, point2, point3, AbsoluteCoordinates);

        m_controlPoint = point2;
        m_currentPoint = point3;
    } else
        m_consumer->curveToCubic(point1, point2, point3, m_mode);
    return true;
}

bool SVGPathParser::parseCurveToCubicSmoothSegment()
{
    float x2;
    float y2;
    float toX;
    float toY; 
    if (!parseNumber(m_ptr, m_end, x2)
        || !parseNumber(m_ptr, m_end, y2)
        || !parseNumber(m_ptr, m_end, toX)
        || !parseNumber(m_ptr, m_end, toY))
        return false;

    if (m_lastCommand != 'c'
        && m_lastCommand != 'C'
        && m_lastCommand != 's'
        && m_lastCommand != 'S')
        m_controlPoint = m_currentPoint;

    FloatPoint point2(x2, y2);
    FloatPoint point3(toX, toY);
    if (m_normalized) {
        FloatPoint point1 = m_currentPoint;
        point1.scale(2, 2);
        point1.move(-m_controlPoint.x(), -m_controlPoint.y());
        if (m_mode == RelativeCoordinates) {
            point2 += m_currentPoint;
            point3 += m_currentPoint;
        }

        m_consumer->curveToCubic(point1, point2, point3, AbsoluteCoordinates);

        m_controlPoint = point2;
        m_currentPoint = point3;
    } else
        m_consumer->curveToCubicSmooth(point2, point3, m_mode);
    return true;
}

bool SVGPathParser::parseCurveToQuadraticSegment()
{
    float x1;
    float y1;
    float toX;
    float toY;
    if (!parseNumber(m_ptr, m_end, x1)
        || !parseNumber(m_ptr, m_end, y1)
        || !parseNumber(m_ptr, m_end, toX)
        || !parseNumber(m_ptr, m_end, toY))
        return false;

    FloatPoint point3(toX, toY);
    if (m_normalized) {
        FloatPoint point1 = m_currentPoint;
        point1.move(2 * x1, 2 * y1);
        FloatPoint point2(toX + 2 * x1, toY + 2 * y1);
        if (m_mode == RelativeCoordinates) {
            point1.move(2 * m_currentPoint.x(), 2 * m_currentPoint.y());
            point2.move(3 * m_currentPoint.x(), 3 * m_currentPoint.y());
            point3 += m_currentPoint;
        }
        point1.scale(gOneOverThree, gOneOverThree);
        point2.scale(gOneOverThree, gOneOverThree);

        m_consumer->curveToCubic(point1, point2, point3, AbsoluteCoordinates);

        m_controlPoint = FloatPoint(x1, y1);
        if (m_mode == RelativeCoordinates)
            m_controlPoint += m_currentPoint;
        m_currentPoint = point3;
    } else
        m_consumer->curveToQuadratic(FloatPoint(x1, y1), point3, m_mode);
    return true;
}

bool SVGPathParser::parseCurveToQuadraticSmoothSegment()
{
    float toX;
    float toY;
    if (!parseNumber(m_ptr, m_end, toX) || !parseNumber(m_ptr, m_end, toY))
        return false;
    if (m_lastCommand != 'q'
        && m_lastCommand != 'Q'
        && m_lastCommand != 't'
        && m_lastCommand != 'T')
        m_controlPoint = m_currentPoint;

    if (m_normalized) {
        FloatPoint cubicPoint = m_currentPoint;
        cubicPoint.scale(2, 2);
        cubicPoint.move(-m_controlPoint.x(), -m_controlPoint.y());
        FloatPoint point1(m_currentPoint.x() + 2 * cubicPoint.x(), m_currentPoint.y() + 2 * cubicPoint.y());
        FloatPoint point2(toX + 2 * cubicPoint.x(), toY + 2 * cubicPoint.y());
        FloatPoint point3(toX, toY);
        if (m_mode == RelativeCoordinates) {
            point2 += m_currentPoint;
            point3 += m_currentPoint;
        }
        point1.scale(gOneOverThree, gOneOverThree);
        point2.scale(gOneOverThree, gOneOverThree);

        m_consumer->curveToCubic(point1, point2, point3, AbsoluteCoordinates);

        m_controlPoint = cubicPoint;
        m_currentPoint = point3;
    } else
        m_consumer->curveToQuadraticSmooth(FloatPoint(toX, toY), m_mode);
    return true;
}

bool SVGPathParser::parseArcToSegment()
{
    bool largeArc;
    bool sweep;
    float angle;
    float rx;
    float ry;
    float toX;
    float toY;
    if (!parseNumber(m_ptr, m_end, rx)
        || !parseNumber(m_ptr, m_end, ry)
        || !parseNumber(m_ptr, m_end, angle)
        || !parseArcFlag(m_ptr, m_end, largeArc)
        || !parseArcFlag(m_ptr, m_end, sweep)
        || !parseNumber(m_ptr, m_end, toX)
        || !parseNumber(m_ptr, m_end, toY))
        return false;

    FloatPoint point2 = FloatPoint(toX, toY);
    // If rx = 0 or ry = 0 then this arc is treated as a straight line segment (a "lineto") joining the endpoints.
    // http://www.w3.org/TR/SVG/implnote.html#ArcOutOfRangeParameters
    rx = fabsf(rx);
    ry = fabsf(ry);
    if (!rx || !ry) {
        if (m_normalized) {
            if (m_mode == RelativeCoordinates)
                m_currentPoint += point2;
            else
                m_currentPoint = point2;
            m_consumer->lineTo(m_currentPoint, AbsoluteCoordinates);
        } else
            m_consumer->lineTo(point2, m_mode);
        return true;
    }

    if (m_normalized) {
        FloatPoint point1 = m_currentPoint;
        if (m_mode == RelativeCoordinates)
            point2 += m_currentPoint;
        m_currentPoint = point2;
        return decomposeArcToCubic(angle, rx, ry, point1, point2, largeArc, sweep);
    }
    m_consumer->arcTo(point2, rx, ry, angle, largeArc, sweep, m_mode);
    return true;
}

bool SVGPathParser::parsePathDataString(const String& s, bool normalized)
{
    m_ptr = s.characters();
    m_end = m_ptr + s.length();
    m_normalized = normalized;

    m_controlPoint = FloatPoint();
    m_currentPoint = FloatPoint();
    m_subPathPoint = FloatPoint();
    m_pathClosed = true;

    // Skip any leading spaces.
    if (!skipOptionalSpaces(m_ptr, m_end))
        return false;

    char command = *(m_ptr++);
    m_lastCommand = ' ';
    // Path must start with moveto.
    if (command != 'm' && command != 'M')
        return false;

    while (true) {
        // Skip spaces between command and first coordinate.
        skipOptionalSpaces(m_ptr, m_end);
        m_mode = command >= 'a' && command <= 'z' ? RelativeCoordinates : AbsoluteCoordinates;
        switch (command) {
        case 'm':
        case 'M':
            if (!parseMoveToSegment())
                return false;
            break;
        case 'l':
        case 'L':
            if (!parseLineToSegment())
                return false;
            break;
        case 'h':
        case 'H':
            if (!parseLineToHorizontalSegment())
                return false;
            break;
        case 'v':
        case 'V':
            if (!parseLineToVerticalSegment())
                return false;
            break;
        case 'z':
        case 'Z':
            parseClosePathSegment();
            break;
        case 'c':
        case 'C':
            if (!parseCurveToCubicSegment())
                return false;
            break;
        case 's':
        case 'S':
            if (!parseCurveToCubicSmoothSegment())
                return false;
            break;
        case 'q':
        case 'Q':
            if (!parseCurveToQuadraticSegment())
                return false;
            break;
        case 't':
        case 'T':
            if (!parseCurveToQuadraticSmoothSegment())
                return false;
            break;
        case 'a':
        case 'A':
            if (!parseArcToSegment())
                return false;
            break;
        default:
            return false;
        }
        m_lastCommand = command;

        if (m_ptr >= m_end)
            return true;

        // Check for remaining coordinates in the current command.
        if ((*m_ptr == '+' || *m_ptr == '-' || *m_ptr == '.' || (*m_ptr >= '0' && *m_ptr <= '9'))
            && command != 'z' && command != 'Z') {
            if (command == 'M')
                command = 'L';
            else if (command == 'm')
                command = 'l';
        } else
            command = *(m_ptr++);

        if (m_lastCommand != 'C' && m_lastCommand != 'c'
            && m_lastCommand != 'S' && m_lastCommand != 's'
            && m_lastCommand != 'Q' && m_lastCommand != 'q'
            && m_lastCommand != 'T' && m_lastCommand != 't')
            m_controlPoint = m_currentPoint;
    }

    return false;
}

// This works by converting the SVG arc to "simple" beziers.
// Partly adapted from Niko's code in kdelibs/kdecore/svgicons.
// See also SVG implementation notes: http://www.w3.org/TR/SVG/implnote.html#ArcConversionEndpointToCenter
bool SVGPathParser::decomposeArcToCubic(float angle, float rx, float ry, FloatPoint& point1, FloatPoint& point2, bool largeArcFlag, bool sweepFlag)
{
    FloatSize midPointDistance = point1 - point2;
    midPointDistance.scale(0.5f);

    AffineTransform pointTransform;
    pointTransform.rotate(-angle);

    FloatPoint transformedMidPoint = pointTransform.mapPoint(FloatPoint(midPointDistance.width(), midPointDistance.height()));
    float squareRx = rx * rx;
    float squareRy = ry * ry;
    float squareX = transformedMidPoint.x() * transformedMidPoint.x();
    float squareY = transformedMidPoint.y() * transformedMidPoint.y();

    // Check if the radii are big enough to draw the arc, scale radii if not.
    // http://www.w3.org/TR/SVG/implnote.html#ArcCorrectionOutOfRangeRadii
    float radiiScale = squareX / squareRx + squareY / squareRy;
    if (radiiScale > 1) {
        rx *= sqrtf(radiiScale);
        ry *= sqrtf(radiiScale);
    }

    pointTransform.makeIdentity();
    pointTransform.scale(1 / rx, 1 / ry);
    pointTransform.rotate(-angle);

    point1 = pointTransform.mapPoint(point1);
    point2 = pointTransform.mapPoint(point2);
    FloatSize delta = point2 - point1;

    float d = delta.width() * delta.width() + delta.height() * delta.height();
    float scaleFactorSquared = std::max(1 / d - 0.25f, 0.f);

    float scaleFactor = sqrtf(scaleFactorSquared);
    if (sweepFlag == largeArcFlag)
        scaleFactor = -scaleFactor;

    delta.scale(scaleFactor);
    FloatPoint centerPoint = FloatPoint(0.5f * (point1.x() + point2.x()) - delta.height(),
                                        0.5f * (point1.y() + point2.y()) + delta.width());

    float theta1 = atan2f(point1.y() - centerPoint.y(), point1.x() - centerPoint.x());
    float theta2 = atan2f(point2.y() - centerPoint.y(), point2.x() - centerPoint.x());

    float thetaArc = theta2 - theta1;
    if (thetaArc < 0 && sweepFlag)
        thetaArc += 2 * piFloat;
    else if (thetaArc > 0 && !sweepFlag)
        thetaArc -= 2 * piFloat;

    pointTransform.makeIdentity();
    pointTransform.rotate(angle);
    pointTransform.scale(rx, ry);

    // Some results of atan2 on some platform implementations are not exact enough. So that we get more
    // cubic curves than expected here. Adding 0.001f reduces the count of sgements to the correct count.
    int segments = ceilf(fabsf(thetaArc / (piOverTwoFloat + 0.001f)));
    for (int i = 0; i < segments; ++i) {
        float startTheta = theta1 + i * thetaArc / segments;
        float endTheta = theta1 + (i + 1) * thetaArc / segments;

        float t = (8 / 6.f) * tanf(0.25f * (endTheta - startTheta));
        if (!isfinite(t))
            return false;
        float sinStartTheta = sinf(startTheta);
        float cosStartTheta = cosf(startTheta);
        float sinEndTheta = sinf(endTheta);
        float cosEndTheta = cosf(endTheta);

        point1 = FloatPoint(cosStartTheta - t * sinStartTheta, sinStartTheta + t * cosStartTheta);
        point1.move(centerPoint.x(), centerPoint.y());
        FloatPoint point3 = FloatPoint(cosEndTheta, sinEndTheta);
        point3.move(centerPoint.x(), centerPoint.y());
        point2 = point3;
        point2.move(t * sinEndTheta, -t * cosEndTheta);

        m_consumer->curveToCubic(pointTransform.mapPoint(point1), pointTransform.mapPoint(point2),
                                 pointTransform.mapPoint(point3), AbsoluteCoordinates);
    }
    return true;
}

}

#endif // ENABLE(SVG)
