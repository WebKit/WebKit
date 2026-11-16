/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "SVGPathAbsoluteConverter.h"

namespace WebCore {

SVGPathAbsoluteConverter::SVGPathAbsoluteConverter(SVGPathConsumer& consumer)
    : SVGPathConsumer()
    , m_consumer(consumer)
{
}

void SVGPathAbsoluteConverter::incrementPathSegmentCount()
{
    m_consumer->incrementPathSegmentCount();
}

bool SVGPathAbsoluteConverter::continueConsuming()
{
    return m_consumer->continueConsuming();
}

void SVGPathAbsoluteConverter::moveTo(const FloatPoint& targetPoint, bool closed, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates) {
        m_consumer->moveTo(targetPoint, closed, AbsoluteCoordinates);
        m_currentPoint = targetPoint;
    } else {
        m_consumer->moveTo(m_currentPoint + targetPoint, closed, AbsoluteCoordinates);
        m_currentPoint += targetPoint;
    }

    m_subpathPoint = m_currentPoint;
}

void SVGPathAbsoluteConverter::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates) {
        m_consumer->lineTo(targetPoint, AbsoluteCoordinates);
        m_currentPoint = targetPoint;
    } else {
        m_consumer->lineTo(m_currentPoint + targetPoint, AbsoluteCoordinates);
        m_currentPoint += targetPoint;
    }
}

void SVGPathAbsoluteConverter::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates) {
        m_consumer->curveToCubic(point1, point2, targetPoint, AbsoluteCoordinates);
        m_currentPoint = targetPoint;
    } else {
        m_consumer->curveToCubic(m_currentPoint + point1, m_currentPoint + point2, m_currentPoint + targetPoint, AbsoluteCoordinates);
        m_currentPoint += targetPoint;
    }
}

void SVGPathAbsoluteConverter::closePath()
{
    m_consumer->closePath();
    m_currentPoint = m_subpathPoint;
}

void SVGPathAbsoluteConverter::lineToHorizontal(float targetX, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates) {
        m_consumer->lineToHorizontal(targetX, AbsoluteCoordinates);
        m_currentPoint.setX(targetX);
    } else {
        auto absoluteTargetX = m_currentPoint.x() + targetX;

        m_consumer->lineToHorizontal(absoluteTargetX, AbsoluteCoordinates);
        m_currentPoint.setX(absoluteTargetX);
    }
}

void SVGPathAbsoluteConverter::lineToVertical(float targetY, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates) {
        m_consumer->lineToVertical(targetY, AbsoluteCoordinates);
        m_currentPoint.setY(targetY);
    } else {
        auto absoluteTargetY = m_currentPoint.y() + targetY;

        m_consumer->lineToVertical(absoluteTargetY, AbsoluteCoordinates);
        m_currentPoint.setY(absoluteTargetY);
    }
}

void SVGPathAbsoluteConverter::curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates) {
        m_consumer->curveToCubicSmooth(point2, targetPoint, AbsoluteCoordinates);
        m_currentPoint = targetPoint;
    } else {
        m_consumer->curveToCubicSmooth(m_currentPoint + point2, m_currentPoint + targetPoint, AbsoluteCoordinates);
        m_currentPoint += targetPoint;
    }
}

void SVGPathAbsoluteConverter::curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates) {
        m_consumer->curveToQuadratic(point1, targetPoint, AbsoluteCoordinates);
        m_currentPoint = targetPoint;
    } else {
        m_consumer->curveToQuadratic(m_currentPoint + point1, m_currentPoint + targetPoint, AbsoluteCoordinates);
        m_currentPoint += targetPoint;
    }
}

void SVGPathAbsoluteConverter::curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates) {
        m_consumer->curveToQuadraticSmooth(targetPoint, AbsoluteCoordinates);
        m_currentPoint = targetPoint;
    } else {
        m_consumer->curveToQuadraticSmooth(m_currentPoint + targetPoint, AbsoluteCoordinates);
        m_currentPoint += targetPoint;
    }
}

void SVGPathAbsoluteConverter::arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates) {
        m_consumer->arcTo(r1, r2, angle, largeArcFlag, sweepFlag, targetPoint, AbsoluteCoordinates);
        m_currentPoint = targetPoint;
    } else {
        m_consumer->arcTo(r1, r2, angle, largeArcFlag, sweepFlag, m_currentPoint + targetPoint, AbsoluteCoordinates);
        m_currentPoint += targetPoint;
    }
}

} // namespace WebCore
