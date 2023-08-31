/*
 * Copyright (C) 2002, 2003 The Karbon Developers
 * Copyright (C) 2006 Alexander Kellett <lypanov@kde.org>
 * Copyright (C) 2006, 2007 Rob Buis <buis@kde.org>
 * Copyright (C) 2007, 2009, 2015 Apple Inc. All rights reserved.
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
#include "SVGPathParser.h"

#include "AffineTransform.h"
#include "SVGPathByteStreamBuilder.h"
#include "SVGPathSource.h"
#include "SVGPathStringBuilder.h"
#include <wtf/MathExtras.h>

static const float gOneOverThree = 1 / 3.f;

namespace WebCore {

bool SVGPathParser::parse(SVGPathSource& source, SVGPathConsumer& consumer, PathParsingMode mode, bool checkForInitialMoveTo)
{
    SVGPathParser parser(consumer, source, mode);
    return parser.parsePathData(checkForInitialMoveTo);
}

bool SVGPathParser::parseToByteStream(SVGPathSource& source, SVGPathByteStream& byteStream, PathParsingMode mode, bool checkForInitialMoveTo)
{
    SVGPathByteStreamBuilder builder(byteStream);
    auto result = parse(source, builder, mode, checkForInitialMoveTo);
    byteStream.shrinkToFit();
    return result;
}

bool SVGPathParser::parseToString(SVGPathSource& source, String& result, PathParsingMode mode, bool checkForInitialMoveTo)
{
    SVGPathStringBuilder builder;
    SVGPathParser parser(builder, source, mode);
    bool ok = parser.parsePathData(checkForInitialMoveTo);
    result = builder.result();
    return ok;
}

SVGPathParser::SVGPathParser(SVGPathConsumer& consumer, SVGPathSource& source, PathParsingMode parsingMode)
    : m_source(source)
    , m_consumer(consumer)
    , m_pathParsingMode(parsingMode)
{
}

void SVGPathParser::parseClosePathSegment()
{
    // Reset m_currentPoint for the next path.
    if (m_pathParsingMode == NormalizedParsing)
        m_currentPoint = m_subPathPoint;
    m_closePath = true;
    m_consumer.closePath();
}

bool SVGPathParser::parseMoveToSegment()
{
    auto result = m_source.parseMoveToSegment();
    if (!result)
        return false;

    if (m_pathParsingMode == NormalizedParsing) {
        if (m_mode == RelativeCoordinates)
            m_currentPoint += result->targetPoint;
        else
            m_currentPoint = result->targetPoint;
        m_subPathPoint = m_currentPoint;
        m_consumer.moveTo(m_currentPoint, m_closePath, AbsoluteCoordinates);
    } else
        m_consumer.moveTo(result->targetPoint, m_closePath, m_mode);
    m_closePath = false;
    return true;
}

bool SVGPathParser::parseLineToSegment()
{
    auto result = m_source.parseLineToSegment();
    if (!result)
        return false;

    if (m_pathParsingMode == NormalizedParsing) {
        if (m_mode == RelativeCoordinates)
            m_currentPoint += result->targetPoint;
        else
            m_currentPoint = result->targetPoint;
        m_consumer.lineTo(m_currentPoint, AbsoluteCoordinates);
    } else
        m_consumer.lineTo(result->targetPoint, m_mode);
    return true;
}

bool SVGPathParser::parseLineToHorizontalSegment()
{
    auto result = m_source.parseLineToHorizontalSegment();
    if (!result)
        return false;

    if (m_pathParsingMode == NormalizedParsing) {
        if (m_mode == RelativeCoordinates)
            m_currentPoint.move(result->x, 0);
        else
            m_currentPoint.setX(result->x);
        m_consumer.lineTo(m_currentPoint, AbsoluteCoordinates);
    } else
        m_consumer.lineToHorizontal(result->x, m_mode);
    return true;
}

bool SVGPathParser::parseLineToVerticalSegment()
{
    auto result = m_source.parseLineToVerticalSegment();
    if (!result)
        return false;

    if (m_pathParsingMode == NormalizedParsing) {
        if (m_mode == RelativeCoordinates)
            m_currentPoint.move(0, result->y);
        else
            m_currentPoint.setY(result->y);
        m_consumer.lineTo(m_currentPoint, AbsoluteCoordinates);
    } else
        m_consumer.lineToVertical(result->y, m_mode);
    return true;
}

bool SVGPathParser::parseCurveToCubicSegment()
{
    auto result = m_source.parseCurveToCubicSegment();
    if (!result)
        return false;

    if (m_pathParsingMode == NormalizedParsing) {
        if (m_mode == RelativeCoordinates) {
            result->point1 += m_currentPoint;
            result->point2 += m_currentPoint;
            result->targetPoint += m_currentPoint;
        }
        m_consumer.curveToCubic(result->point1, result->point2, result->targetPoint, AbsoluteCoordinates);

        m_controlPoint = result->point2;
        m_currentPoint = result->targetPoint;
    } else
        m_consumer.curveToCubic(result->point1, result->point2, result->targetPoint, m_mode);
    return true;
}

bool SVGPathParser::parseCurveToCubicSmoothSegment()
{
    auto result = m_source.parseCurveToCubicSmoothSegment();
    if (!result)
        return false;

    if (m_lastCommand != SVGPathSegType::CurveToCubicAbs
        && m_lastCommand != SVGPathSegType::CurveToCubicRel
        && m_lastCommand != SVGPathSegType::CurveToCubicSmoothAbs
        && m_lastCommand != SVGPathSegType::CurveToCubicSmoothRel)
        m_controlPoint = m_currentPoint;

    if (m_pathParsingMode == NormalizedParsing) {
        FloatPoint point1 = m_currentPoint;
        point1.scale(2);
        point1.move(-m_controlPoint.x(), -m_controlPoint.y());
        if (m_mode == RelativeCoordinates) {
            result->point2 += m_currentPoint;
            result->targetPoint += m_currentPoint;
        }

        m_consumer.curveToCubic(point1, result->point2, result->targetPoint, AbsoluteCoordinates);

        m_controlPoint = result->point2;
        m_currentPoint = result->targetPoint;
    } else
        m_consumer.curveToCubicSmooth(result->point2, result->targetPoint, m_mode);
    return true;
}

bool SVGPathParser::parseCurveToQuadraticSegment()
{
    auto result = m_source.parseCurveToQuadraticSegment();
    if (!result)
        return false;

    if (m_pathParsingMode == NormalizedParsing) {
        m_controlPoint = result->point1;

        FloatPoint point1 = m_currentPoint;
        point1.move(2 * m_controlPoint.x(), 2 * m_controlPoint.y());
        FloatPoint point2(result->targetPoint.x() + 2 * m_controlPoint.x(), result->targetPoint.y() + 2 * m_controlPoint.y());
        if (m_mode == RelativeCoordinates) {
            point1.move(2 * m_currentPoint.x(), 2 * m_currentPoint.y());
            point2.move(3 * m_currentPoint.x(), 3 * m_currentPoint.y());
            result->targetPoint += m_currentPoint;
        }
        point1.scale(gOneOverThree);
        point2.scale(gOneOverThree);

        m_consumer.curveToCubic(point1, point2, result->targetPoint, AbsoluteCoordinates);

        if (m_mode == RelativeCoordinates)
            m_controlPoint += m_currentPoint;
        m_currentPoint = result->targetPoint;
    } else
        m_consumer.curveToQuadratic(result->point1, result->targetPoint, m_mode);
    return true;
}

bool SVGPathParser::parseCurveToQuadraticSmoothSegment()
{
    auto result = m_source.parseCurveToQuadraticSmoothSegment();
    if (!result)
        return false;

    if (m_lastCommand != SVGPathSegType::CurveToQuadraticAbs
        && m_lastCommand != SVGPathSegType::CurveToQuadraticRel
        && m_lastCommand != SVGPathSegType::CurveToQuadraticSmoothAbs
        && m_lastCommand != SVGPathSegType::CurveToQuadraticSmoothRel)
        m_controlPoint = m_currentPoint;

    if (m_pathParsingMode == NormalizedParsing) {
        FloatPoint cubicPoint = m_currentPoint;
        cubicPoint.scale(2);
        cubicPoint.move(-m_controlPoint.x(), -m_controlPoint.y());
        FloatPoint point1(m_currentPoint.x() + 2 * cubicPoint.x(), m_currentPoint.y() + 2 * cubicPoint.y());
        FloatPoint point2(result->targetPoint.x() + 2 * cubicPoint.x(), result->targetPoint.y() + 2 * cubicPoint.y());
        if (m_mode == RelativeCoordinates) {
            point2 += m_currentPoint;
            result->targetPoint += m_currentPoint;
        }
        point1.scale(gOneOverThree);
        point2.scale(gOneOverThree);

        m_consumer.curveToCubic(point1, point2, result->targetPoint, AbsoluteCoordinates);

        m_controlPoint = cubicPoint;
        m_currentPoint = result->targetPoint;
    } else
        m_consumer.curveToQuadraticSmooth(result->targetPoint, m_mode);
    return true;
}

bool SVGPathParser::parseArcToSegment()
{
    auto result = m_source.parseArcToSegment();
    if (!result)
        return false;

    // If rx = 0 or ry = 0 then this arc is treated as a straight line segment (a "lineto") joining the endpoints.
    // http://www.w3.org/TR/SVG/implnote.html#ArcOutOfRangeParameters
    // If the current point and target point for the arc are identical, it should be treated as a zero length
    // path. This ensures continuity in animations.
    result->rx = std::abs(result->rx);
    result->ry = std::abs(result->ry);
    bool arcIsZeroLength = false;
    if (m_pathParsingMode == NormalizedParsing) {
        if (m_mode == RelativeCoordinates)
            arcIsZeroLength = result->targetPoint == FloatPoint::zero();
        else
            arcIsZeroLength = result->targetPoint == m_currentPoint;
    }
    if (!result->rx || !result->ry || arcIsZeroLength) {
        if (m_pathParsingMode == NormalizedParsing) {
            if (m_mode == RelativeCoordinates)
                m_currentPoint += result->targetPoint;
            else
                m_currentPoint = result->targetPoint;
            m_consumer.lineTo(m_currentPoint, AbsoluteCoordinates);
        } else
            m_consumer.lineTo(result->targetPoint, m_mode);
        return true;
    }

    if (m_pathParsingMode == NormalizedParsing) {
        FloatPoint point1 = m_currentPoint;
        if (m_mode == RelativeCoordinates)
            result->targetPoint += m_currentPoint;
        m_currentPoint = result->targetPoint;
        return decomposeArcToCubic(result->angle, result->rx, result->ry, point1, result->targetPoint, result->largeArc, result->sweep);
    }
    m_consumer.arcTo(result->rx, result->ry, result->angle, result->largeArc, result->sweep, result->targetPoint, m_mode);
    return true;
}

bool SVGPathParser::parsePathData(bool checkForInitialMoveTo)
{
    // Skip any leading spaces.
    if (!m_source.moveToNextToken())
        return true;

    auto parsedCommand = m_source.parseSVGSegmentType();
    if (!parsedCommand)
        return false;

    auto command = *parsedCommand;

    // Path must start with moveto.
    if (checkForInitialMoveTo && command != SVGPathSegType::MoveToAbs && command != SVGPathSegType::MoveToRel)
        return false;

    while (true) {
        // Skip spaces between command and first coordinate.
        m_source.moveToNextToken();
        m_mode = AbsoluteCoordinates;
        switch (command) {
        case SVGPathSegType::MoveToRel:
            m_mode = RelativeCoordinates;
            FALLTHROUGH;
        case SVGPathSegType::MoveToAbs:
            if (!parseMoveToSegment())
                return false;
            break;
        case SVGPathSegType::LineToRel:
            m_mode = RelativeCoordinates;
            FALLTHROUGH;
        case SVGPathSegType::LineToAbs:
            if (!parseLineToSegment())
                return false;
            break;
        case SVGPathSegType::LineToHorizontalRel:
            m_mode = RelativeCoordinates;
            FALLTHROUGH;
        case SVGPathSegType::LineToHorizontalAbs:
            if (!parseLineToHorizontalSegment())
                return false;
            break;
        case SVGPathSegType::LineToVerticalRel:
            m_mode = RelativeCoordinates;
            FALLTHROUGH;
        case SVGPathSegType::LineToVerticalAbs:
            if (!parseLineToVerticalSegment())
                return false;
            break;
        case SVGPathSegType::ClosePath:
            parseClosePathSegment();
            break;
        case SVGPathSegType::CurveToCubicRel:
            m_mode = RelativeCoordinates;
            FALLTHROUGH;
        case SVGPathSegType::CurveToCubicAbs:
            if (!parseCurveToCubicSegment())
                return false;
            break;
        case SVGPathSegType::CurveToCubicSmoothRel:
            m_mode = RelativeCoordinates;
            FALLTHROUGH;
        case SVGPathSegType::CurveToCubicSmoothAbs:
            if (!parseCurveToCubicSmoothSegment())
                return false;
            break;
        case SVGPathSegType::CurveToQuadraticRel:
            m_mode = RelativeCoordinates;
            FALLTHROUGH;
        case SVGPathSegType::CurveToQuadraticAbs:
            if (!parseCurveToQuadraticSegment())
                return false;
            break;
        case SVGPathSegType::CurveToQuadraticSmoothRel:
            m_mode = RelativeCoordinates;
            FALLTHROUGH;
        case SVGPathSegType::CurveToQuadraticSmoothAbs:
            if (!parseCurveToQuadraticSmoothSegment())
                return false;
            break;
        case SVGPathSegType::ArcRel:
            m_mode = RelativeCoordinates;
            FALLTHROUGH;
        case SVGPathSegType::ArcAbs:
            if (!parseArcToSegment())
                return false;
            break;
        default:
            return false;
        }
        if (!m_consumer.continueConsuming())
            return true;

        m_lastCommand = command;

        if (!m_source.hasMoreData())
            return true;

        command = m_source.nextCommand(command);

        if (m_lastCommand != SVGPathSegType::CurveToCubicAbs
            && m_lastCommand != SVGPathSegType::CurveToCubicRel
            && m_lastCommand != SVGPathSegType::CurveToCubicSmoothAbs
            && m_lastCommand != SVGPathSegType::CurveToCubicSmoothRel
            && m_lastCommand != SVGPathSegType::CurveToQuadraticAbs
            && m_lastCommand != SVGPathSegType::CurveToQuadraticRel
            && m_lastCommand != SVGPathSegType::CurveToQuadraticSmoothAbs
            && m_lastCommand != SVGPathSegType::CurveToQuadraticSmoothRel)
            m_controlPoint = m_currentPoint;

        m_consumer.incrementPathSegmentCount();
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
    FloatPoint centerPoint = point1 + point2;
    centerPoint.scale(0.5f);
    centerPoint.move(-delta.height(), delta.width());

    float theta1 = FloatPoint(point1 - centerPoint).slopeAngleRadians();
    float theta2 = FloatPoint(point2 - centerPoint).slopeAngleRadians();

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
    int segments = ceilf(std::abs(thetaArc / (piOverTwoFloat + 0.001f)));
    for (int i = 0; i < segments; ++i) {
        float startTheta = theta1 + i * thetaArc / segments;
        float endTheta = theta1 + (i + 1) * thetaArc / segments;

        float t = (8 / 6.f) * tanf(0.25f * (endTheta - startTheta));
        if (!std::isfinite(t))
            return false;
        float sinStartTheta = sinf(startTheta);
        float cosStartTheta = cosf(startTheta);
        float sinEndTheta = sinf(endTheta);
        float cosEndTheta = cosf(endTheta);

        point1 = FloatPoint(cosStartTheta - t * sinStartTheta, sinStartTheta + t * cosStartTheta);
        point1.move(centerPoint.x(), centerPoint.y());
        FloatPoint targetPoint = FloatPoint(cosEndTheta, sinEndTheta);
        targetPoint.move(centerPoint.x(), centerPoint.y());
        point2 = targetPoint;
        point2.move(t * sinEndTheta, -t * cosEndTheta);

        m_consumer.curveToCubic(pointTransform.mapPoint(point1), pointTransform.mapPoint(point2),
                                 pointTransform.mapPoint(targetPoint), AbsoluteCoordinates);
    }
    return true;
}

}
