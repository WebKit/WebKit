/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "SVGPathStringBuilder.h"

#include <wtf/text/WTFString.h>

namespace WebCore {

SVGPathStringBuilder::SVGPathStringBuilder() = default;

SVGPathStringBuilder::~SVGPathStringBuilder() = default;

String SVGPathStringBuilder::result()
{
    unsigned size = m_stringBuilder.length();
    if (!size)
        return String();

    // Remove trailing space.
    m_stringBuilder.shrink(size - 1);
    return m_stringBuilder.toString();
}

void SVGPathStringBuilder::incrementPathSegmentCount()
{
}

bool SVGPathStringBuilder::continueConsuming()
{
    return true;
}

static void appendFlag(StringBuilder& stringBuilder, bool flag)
{
    stringBuilder.append(flag ? '1' : '0', ' ');
}

static void appendNumber(StringBuilder& stringBuilder, float number)
{
    // FIXME: Shortest form would be better, but fixed precision is required for now to smooth over precision errors caused by converting float to double and back in CGPath on Cocoa platforms.
    stringBuilder.append(FormattedNumber::fixedPrecision(number), ' ');
}

static void appendPoint(StringBuilder& stringBuilder, const FloatPoint& point)
{
    appendNumber(stringBuilder, point.x());
    appendNumber(stringBuilder, point.y());
}

void SVGPathStringBuilder::moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.append("M ");
    else
        m_stringBuilder.append("m ");

    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.append("L ");
    else
        m_stringBuilder.append("l ");

    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::lineToHorizontal(float x, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.append("H ");
    else
        m_stringBuilder.append("h ");

    appendNumber(m_stringBuilder, x);
}

void SVGPathStringBuilder::lineToVertical(float y, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.append("V ");
    else
        m_stringBuilder.append("v ");

    appendNumber(m_stringBuilder, y);
}

void SVGPathStringBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.append("C ");
    else
        m_stringBuilder.append("c ");

    appendPoint(m_stringBuilder, point1);
    appendPoint(m_stringBuilder, point2);
    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.append("S ");
    else
        m_stringBuilder.append("s ");

    appendPoint(m_stringBuilder, point2);
    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.append("Q ");
    else
        m_stringBuilder.append("q ");

    appendPoint(m_stringBuilder, point1);
    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.append("T ");
    else
        m_stringBuilder.append("t ");

    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.append("A ");
    else
        m_stringBuilder.append("a ");

    appendNumber(m_stringBuilder, r1);
    appendNumber(m_stringBuilder, r2);
    appendNumber(m_stringBuilder, angle);
    appendFlag(m_stringBuilder, largeArcFlag);
    appendFlag(m_stringBuilder, sweepFlag);
    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::closePath()
{
    m_stringBuilder.append("Z ");
}

} // namespace WebCore
