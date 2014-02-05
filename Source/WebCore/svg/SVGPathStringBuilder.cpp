/*
 * Copyright (C) Research In Motion Limited 2010-2011. All rights reserved.
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

SVGPathStringBuilder::SVGPathStringBuilder()
{
}

SVGPathStringBuilder::~SVGPathStringBuilder()
{
}

String SVGPathStringBuilder::result()
{
    unsigned size = m_stringBuilder.length();
    if (!size)
        return String();

    // Remove trailing space.
    m_stringBuilder.resize(size - 1);
    return m_stringBuilder.toString();
}

void SVGPathStringBuilder::cleanup()
{
    m_stringBuilder.clear();
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
    stringBuilder.append(flag ? '1' : '0');
    stringBuilder.append(' ');
}

static void appendNumber(StringBuilder& stringBuilder, float number)
{
    stringBuilder.appendNumber(number);
    stringBuilder.append(' ');
}

static void appendPoint(StringBuilder& stringBuilder, const FloatPoint& point)
{
    stringBuilder.appendNumber(point.x());
    stringBuilder.append(' ');
    stringBuilder.appendNumber(point.y());
    stringBuilder.append(' ');
}

void SVGPathStringBuilder::moveTo(const FloatPoint& targetPoint, bool, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.appendLiteral("M ");
    else
        m_stringBuilder.appendLiteral("m ");

    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::lineTo(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.appendLiteral("L ");
    else
        m_stringBuilder.appendLiteral("l ");

    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::lineToHorizontal(float x, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.appendLiteral("H ");
    else
        m_stringBuilder.appendLiteral("h ");

    appendNumber(m_stringBuilder, x);
}

void SVGPathStringBuilder::lineToVertical(float y, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.appendLiteral("V ");
    else
        m_stringBuilder.appendLiteral("v ");

    appendNumber(m_stringBuilder, y);
}

void SVGPathStringBuilder::curveToCubic(const FloatPoint& point1, const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.appendLiteral("C ");
    else
        m_stringBuilder.appendLiteral("c ");

    appendPoint(m_stringBuilder, point1);
    appendPoint(m_stringBuilder, point2);
    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::curveToCubicSmooth(const FloatPoint& point2, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.appendLiteral("S ");
    else
        m_stringBuilder.appendLiteral("s ");

    appendPoint(m_stringBuilder, point2);
    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::curveToQuadratic(const FloatPoint& point1, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.appendLiteral("Q ");
    else
        m_stringBuilder.appendLiteral("q ");

    appendPoint(m_stringBuilder, point1);
    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::curveToQuadraticSmooth(const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.appendLiteral("T ");
    else
        m_stringBuilder.appendLiteral("t ");

    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::arcTo(float r1, float r2, float angle, bool largeArcFlag, bool sweepFlag, const FloatPoint& targetPoint, PathCoordinateMode mode)
{
    if (mode == AbsoluteCoordinates)
        m_stringBuilder.appendLiteral("A ");
    else
        m_stringBuilder.appendLiteral("a ");

    appendNumber(m_stringBuilder, r1);
    appendNumber(m_stringBuilder, r2);
    appendNumber(m_stringBuilder, angle);
    appendFlag(m_stringBuilder, largeArcFlag);
    appendFlag(m_stringBuilder, sweepFlag);
    appendPoint(m_stringBuilder, targetPoint);
}

void SVGPathStringBuilder::closePath()
{
    m_stringBuilder.appendLiteral("Z ");
}

} // namespace WebCore
