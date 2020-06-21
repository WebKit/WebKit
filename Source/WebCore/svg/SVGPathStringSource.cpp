/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include "SVGPathStringSource.h"

#include "FloatPoint.h"
#include "SVGParserUtilities.h"

namespace WebCore {

SVGPathStringSource::SVGPathStringSource(const String& string)
    : m_string(string)
    , m_is8BitSource(string.is8Bit())
{
    ASSERT(!string.isEmpty());

    if (m_is8BitSource) {
        m_current.m_character8 = string.characters8();
        m_end.m_character8 = m_current.m_character8 + string.length();
    } else {
        m_current.m_character16 = string.characters16();
        m_end.m_character16 = m_current.m_character16 + string.length();
    }
}

bool SVGPathStringSource::hasMoreData() const
{
    if (m_is8BitSource)
        return m_current.m_character8 < m_end.m_character8;
    return m_current.m_character16 < m_end.m_character16;
}

bool SVGPathStringSource::moveToNextToken()
{
    if (m_is8BitSource)
        return skipOptionalSVGSpaces(m_current.m_character8, m_end.m_character8);
    return skipOptionalSVGSpaces(m_current.m_character16, m_end.m_character16);
}

template <typename CharacterType> static Optional<SVGPathSegType> nextCommandHelper(const CharacterType*& current, SVGPathSegType previousCommand)
{
    // Check for remaining coordinates in the current command.
    if ((*current == '+' || *current == '-' || *current == '.' || isASCIIDigit(*current))
        && previousCommand != PathSegClosePath) {
        if (previousCommand == PathSegMoveToAbs)
            return PathSegLineToAbs;
        if (previousCommand == PathSegMoveToRel)
            return PathSegLineToRel;
        return previousCommand;
    }

    return WTF::nullopt;
}

SVGPathSegType SVGPathStringSource::nextCommand(SVGPathSegType previousCommand)
{
    if (m_is8BitSource) {
        if (auto nextCommand = nextCommandHelper(m_current.m_character8, previousCommand))
            return *nextCommand;
    } else {
        if (auto nextCommand = nextCommandHelper(m_current.m_character16, previousCommand))
            return *nextCommand;
    }

    return *parseSVGSegmentType();
}

template <typename CharacterType> static SVGPathSegType parseSVGSegmentTypeHelper(const CharacterType*& current)
{
    switch (*(current++)) {
    case 'Z':
    case 'z':
        return PathSegClosePath;
    case 'M':
        return PathSegMoveToAbs;
    case 'm':
        return PathSegMoveToRel;
    case 'L':
        return PathSegLineToAbs;
    case 'l':
        return PathSegLineToRel;
    case 'C':
        return PathSegCurveToCubicAbs;
    case 'c':
        return PathSegCurveToCubicRel;
    case 'Q':
        return PathSegCurveToQuadraticAbs;
    case 'q':
        return PathSegCurveToQuadraticRel;
    case 'A':
        return PathSegArcAbs;
    case 'a':
        return PathSegArcRel;
    case 'H':
        return PathSegLineToHorizontalAbs;
    case 'h':
        return PathSegLineToHorizontalRel;
    case 'V':
        return PathSegLineToVerticalAbs;
    case 'v':
        return PathSegLineToVerticalRel;
    case 'S':
        return PathSegCurveToCubicSmoothAbs;
    case 's':
        return PathSegCurveToCubicSmoothRel;
    case 'T':
        return PathSegCurveToQuadraticSmoothAbs;
    case 't':
        return PathSegCurveToQuadraticSmoothRel;
    default:
        return PathSegUnknown;
    }
}

Optional<SVGPathSegType> SVGPathStringSource::parseSVGSegmentType()
{
    if (m_is8BitSource)
        return parseSVGSegmentTypeHelper(m_current.m_character8);
    return parseSVGSegmentTypeHelper(m_current.m_character16);
}

Optional<SVGPathSource::MoveToSegment> SVGPathStringSource::parseMoveToSegment()
{
    auto helper = [](const auto*& current, const auto* end) -> Optional<MoveToSegment> {
        auto targetPoint = parseFloatPoint(current, end);
        if (!targetPoint)
            return WTF::nullopt;
        
        MoveToSegment segment;
        segment.targetPoint = WTFMove(*targetPoint);
        return segment;
    };

    if (m_is8BitSource)
        return helper(m_current.m_character8, m_end.m_character8);
    return helper(m_current.m_character16, m_end.m_character16);
}

Optional<SVGPathSource::LineToSegment> SVGPathStringSource::parseLineToSegment()
{
    auto helper = [](const auto*& current, const auto* end) -> Optional<LineToSegment> {
        auto targetPoint = parseFloatPoint(current, end);
        if (!targetPoint)
            return WTF::nullopt;
        
        LineToSegment segment;
        segment.targetPoint = WTFMove(*targetPoint);
        return segment;
    };

    if (m_is8BitSource)
        return helper(m_current.m_character8, m_end.m_character8);
    return helper(m_current.m_character16, m_end.m_character16);
}

Optional<SVGPathSource::LineToHorizontalSegment> SVGPathStringSource::parseLineToHorizontalSegment()
{
    auto helper = [](const auto*& current, const auto* end) -> Optional<LineToHorizontalSegment> {
        auto x = parseNumber(current, end);
        if (!x)
            return WTF::nullopt;
        
        LineToHorizontalSegment segment;
        segment.x = *x;
        return segment;
    };

    if (m_is8BitSource)
        return helper(m_current.m_character8, m_end.m_character8);
    return helper(m_current.m_character16, m_end.m_character16);
}

Optional<SVGPathSource::LineToVerticalSegment> SVGPathStringSource::parseLineToVerticalSegment()
{
    auto helper = [](const auto*& current, const auto* end) -> Optional<LineToVerticalSegment> {
        auto y = parseNumber(current, end);
        if (!y)
            return WTF::nullopt;
        
        LineToVerticalSegment segment;
        segment.y = *y;
        return segment;
    };

    if (m_is8BitSource)
        return helper(m_current.m_character8, m_end.m_character8);
    return helper(m_current.m_character16, m_end.m_character16);
}

Optional<SVGPathSource::CurveToCubicSegment> SVGPathStringSource::parseCurveToCubicSegment()
{
    auto helper = [](const auto*& current, const auto* end) -> Optional<CurveToCubicSegment> {
        auto point1 = parseFloatPoint(current, end);
        if (!point1)
            return WTF::nullopt;

        auto point2 = parseFloatPoint(current, end);
        if (!point2)
            return WTF::nullopt;

        auto targetPoint = parseFloatPoint(current, end);
        if (!targetPoint)
            return WTF::nullopt;

        CurveToCubicSegment segment;
        segment.point1 = *point1;
        segment.point2 = *point2;
        segment.targetPoint = *targetPoint;
        return segment;
    };

    if (m_is8BitSource)
        return helper(m_current.m_character8, m_end.m_character8);
    return helper(m_current.m_character16, m_end.m_character16);
}

Optional<SVGPathSource::CurveToCubicSmoothSegment> SVGPathStringSource::parseCurveToCubicSmoothSegment()
{
    auto helper = [](const auto*& current, const auto* end) -> Optional<CurveToCubicSmoothSegment> {
        auto point2 = parseFloatPoint(current, end);
        if (!point2)
            return WTF::nullopt;

        auto targetPoint = parseFloatPoint(current, end);
        if (!targetPoint)
            return WTF::nullopt;

        CurveToCubicSmoothSegment segment;
        segment.point2 = *point2;
        segment.targetPoint = *targetPoint;
        return segment;
    };

    if (m_is8BitSource)
        return helper(m_current.m_character8, m_end.m_character8);
    return helper(m_current.m_character16, m_end.m_character16);
}

Optional<SVGPathSource::CurveToQuadraticSegment> SVGPathStringSource::parseCurveToQuadraticSegment()
{
    auto helper = [](const auto*& current, const auto* end) -> Optional<CurveToQuadraticSegment> {
        auto point1 = parseFloatPoint(current, end);
        if (!point1)
            return WTF::nullopt;

        auto targetPoint = parseFloatPoint(current, end);
        if (!targetPoint)
            return WTF::nullopt;

        CurveToQuadraticSegment segment;
        segment.point1 = *point1;
        segment.targetPoint = *targetPoint;
        return segment;
    };

    if (m_is8BitSource)
        return helper(m_current.m_character8, m_end.m_character8);
    return helper(m_current.m_character16, m_end.m_character16);
}

Optional<SVGPathSource::CurveToQuadraticSmoothSegment> SVGPathStringSource::parseCurveToQuadraticSmoothSegment()
{
    auto helper = [](const auto*& current, const auto* end) -> Optional<CurveToQuadraticSmoothSegment> {
        auto targetPoint = parseFloatPoint(current, end);
        if (!targetPoint)
            return WTF::nullopt;

        CurveToQuadraticSmoothSegment segment;
        segment.targetPoint = *targetPoint;
        return segment;
    };

    if (m_is8BitSource)
        return helper(m_current.m_character8, m_end.m_character8);
    return helper(m_current.m_character16, m_end.m_character16);
}

Optional<SVGPathSource::ArcToSegment> SVGPathStringSource::parseArcToSegment()
{
    auto helper = [](const auto*& current, const auto* end) -> Optional<ArcToSegment> {
        auto rx = parseNumber(current, end);
        if (!rx)
            return WTF::nullopt;
        auto ry = parseNumber(current, end);
        if (!ry)
            return WTF::nullopt;
        auto angle = parseNumber(current, end);
        if (!angle)
            return WTF::nullopt;
        auto largeArc = parseArcFlag(current, end);
        if (!largeArc)
            return WTF::nullopt;
        auto sweep = parseArcFlag(current, end);
        if (!sweep)
            return WTF::nullopt;
        auto targetPoint = parseFloatPoint(current, end);
        if (!targetPoint)
            return WTF::nullopt;

        ArcToSegment segment;
        segment.rx = *rx;
        segment.ry = *ry;
        segment.angle = *angle;
        segment.largeArc = *largeArc;
        segment.sweep = *sweep;
        segment.targetPoint = *targetPoint;
        return segment;
    };

    if (m_is8BitSource)
        return helper(m_current.m_character8, m_end.m_character8);
    return helper(m_current.m_character16, m_end.m_character16);
}

} // namespace WebKit
