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
#include "SVGPathStringSource.h"

#include "SVGParserUtilities.h"

namespace WebCore {

SVGPathStringSource::SVGPathStringSource(const String& string)
    : m_string(string)
    , m_current(string.characters())
    , m_end(m_current + string.length())
{
    ASSERT(!string.isEmpty());
}

SVGPathStringSource::~SVGPathStringSource()
{
}

bool SVGPathStringSource::hasMoreData() const
{
    return m_current < m_end;
}

bool SVGPathStringSource::parseFloat(float& result)
{
    return parseNumber(m_current, m_end, result);
}

bool SVGPathStringSource::parseFlag(bool& result)
{
    return parseArcFlag(m_current, m_end, result);
}

bool SVGPathStringSource::moveToNextToken()
{
    return skipOptionalSpaces(m_current, m_end);
}

bool SVGPathStringSource::parseSVGSegmentType(SVGPathSegType& pathSegType)
{
    switch (*(m_current++)) {
    case 'Z':
    case 'z':
        pathSegType = PathSegClosePath;
        break;
    case 'M':
        pathSegType = PathSegMoveToAbs;
        break;
    case 'm':
        pathSegType = PathSegMoveToRel;
        break;
    case 'L':
        pathSegType = PathSegLineToAbs;
        break;
    case 'l':
        pathSegType = PathSegLineToRel;
        break;
    case 'C':
        pathSegType = PathSegCurveToCubicAbs;
        break;
    case 'c':
        pathSegType = PathSegCurveToCubicRel;
        break;
    case 'Q':
        pathSegType = PathSegCurveToQuadraticAbs;
        break;
    case 'q':
        pathSegType = PathSegCurveToQuadraticRel;
        break;
    case 'A':
        pathSegType = PathSegArcAbs;
        break;
    case 'a':
        pathSegType = PathSegArcRel;
        break;
    case 'H':
        pathSegType = PathSegLineToHorizontalAbs;
        break;
    case 'h':
        pathSegType = PathSegLineToHorizontalRel;
        break;
    case 'V':
        pathSegType = PathSegLineToVerticalAbs;
        break;
    case 'v':
        pathSegType = PathSegLineToVerticalRel;
        break;
    case 'S':
        pathSegType = PathSegCurveToCubicSmoothAbs;
        break;
    case 's':
        pathSegType = PathSegCurveToCubicSmoothRel;
        break;
    case 'T':
        pathSegType = PathSegCurveToQuadraticSmoothAbs;
        break;
    case 't':
        pathSegType = PathSegCurveToQuadraticSmoothRel;
        break;
    default:
        pathSegType = PathSegUnknown;
    }
    return true;
}

SVGPathSegType SVGPathStringSource::nextCommand(SVGPathSegType previousCommand)
{
    // Check for remaining coordinates in the current command.
    if ((*m_current == '+' || *m_current == '-' || *m_current == '.' || (*m_current >= '0' && *m_current <= '9'))
        && previousCommand != PathSegClosePath) {
        if (previousCommand == PathSegMoveToAbs)
            return PathSegLineToAbs;
        if (previousCommand == PathSegMoveToRel)
            return PathSegLineToRel;
        return previousCommand;
    }
    SVGPathSegType nextCommand;
    parseSVGSegmentType(nextCommand);
    return nextCommand;
}

}

#endif // ENABLE(SVG)
