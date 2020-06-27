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

#include "SVGParserUtilities.h"

namespace WebCore {

SVGPathStringSource::SVGPathStringSource(const String& string)
    : m_string(string)
    , m_is8BitSource(string.is8Bit())
{
    ASSERT(!string.isEmpty());

    if (m_is8BitSource)
        m_buffer8 = { string.characters8(), string.length() };
    else
        m_buffer16 = { string.characters16(), string.length() };
}

bool SVGPathStringSource::hasMoreData() const
{
    if (m_is8BitSource)
        return m_buffer8.hasCharactersRemaining();
    return m_buffer16.hasCharactersRemaining();
}

bool SVGPathStringSource::moveToNextToken()
{
    if (m_is8BitSource)
        return skipOptionalSVGSpaces(m_buffer8);
    return skipOptionalSVGSpaces(m_buffer16);
}

template <typename CharacterType> static Optional<SVGPathSegType> nextCommandHelper(StringParsingBuffer<CharacterType>& buffer, SVGPathSegType previousCommand)
{
    // Check for remaining coordinates in the current command.
    if ((*buffer == '+' || *buffer == '-' || *buffer == '.' || isASCIIDigit(*buffer))
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
        if (auto nextCommand = nextCommandHelper(m_buffer8, previousCommand))
            return *nextCommand;
    } else {
        if (auto nextCommand = nextCommandHelper(m_buffer16, previousCommand))
            return *nextCommand;
    }

    return *parseSVGSegmentType();
}

template<typename F> decltype(auto) SVGPathStringSource::parse(F&& functor)
{
    if (m_is8BitSource)
        return functor(m_buffer8);
    return functor(m_buffer16);
}

Optional<SVGPathSegType> SVGPathStringSource::parseSVGSegmentType()
{
    return parse([](auto& buffer) -> SVGPathSegType {
        auto character = *buffer;
        buffer++;
        switch (character) {
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
    });
}

Optional<SVGPathSource::MoveToSegment> SVGPathStringSource::parseMoveToSegment()
{
    return parse([](auto& buffer) -> Optional<MoveToSegment> {
        auto targetPoint = parseFloatPoint(buffer);
        if (!targetPoint)
            return WTF::nullopt;
        
        MoveToSegment segment;
        segment.targetPoint = WTFMove(*targetPoint);
        return segment;
    });
}

Optional<SVGPathSource::LineToSegment> SVGPathStringSource::parseLineToSegment()
{
    return parse([](auto& buffer) -> Optional<LineToSegment> {
        auto targetPoint = parseFloatPoint(buffer);
        if (!targetPoint)
            return WTF::nullopt;
        
        LineToSegment segment;
        segment.targetPoint = WTFMove(*targetPoint);
        return segment;
    });
}

Optional<SVGPathSource::LineToHorizontalSegment> SVGPathStringSource::parseLineToHorizontalSegment()
{
    return parse([](auto& buffer) -> Optional<LineToHorizontalSegment> {
        auto x = parseNumber(buffer);
        if (!x)
            return WTF::nullopt;
        
        LineToHorizontalSegment segment;
        segment.x = *x;
        return segment;
    });
}

Optional<SVGPathSource::LineToVerticalSegment> SVGPathStringSource::parseLineToVerticalSegment()
{
    return parse([](auto& buffer) -> Optional<LineToVerticalSegment> {
        auto y = parseNumber(buffer);
        if (!y)
            return WTF::nullopt;
        
        LineToVerticalSegment segment;
        segment.y = *y;
        return segment;
    });
}

Optional<SVGPathSource::CurveToCubicSegment> SVGPathStringSource::parseCurveToCubicSegment()
{
    return parse([](auto& buffer) -> Optional<CurveToCubicSegment> {
        auto point1 = parseFloatPoint(buffer);
        if (!point1)
            return WTF::nullopt;

        auto point2 = parseFloatPoint(buffer);
        if (!point2)
            return WTF::nullopt;

        auto targetPoint = parseFloatPoint(buffer);
        if (!targetPoint)
            return WTF::nullopt;

        CurveToCubicSegment segment;
        segment.point1 = *point1;
        segment.point2 = *point2;
        segment.targetPoint = *targetPoint;
        return segment;
    });
}

Optional<SVGPathSource::CurveToCubicSmoothSegment> SVGPathStringSource::parseCurveToCubicSmoothSegment()
{
    return parse([](auto& buffer) -> Optional<CurveToCubicSmoothSegment> {
        auto point2 = parseFloatPoint(buffer);
        if (!point2)
            return WTF::nullopt;

        auto targetPoint = parseFloatPoint(buffer);
        if (!targetPoint)
            return WTF::nullopt;

        CurveToCubicSmoothSegment segment;
        segment.point2 = *point2;
        segment.targetPoint = *targetPoint;
        return segment;
    });
}

Optional<SVGPathSource::CurveToQuadraticSegment> SVGPathStringSource::parseCurveToQuadraticSegment()
{
    return parse([](auto& buffer) -> Optional<CurveToQuadraticSegment> {
        auto point1 = parseFloatPoint(buffer);
        if (!point1)
            return WTF::nullopt;

        auto targetPoint = parseFloatPoint(buffer);
        if (!targetPoint)
            return WTF::nullopt;

        CurveToQuadraticSegment segment;
        segment.point1 = *point1;
        segment.targetPoint = *targetPoint;
        return segment;
    });
}

Optional<SVGPathSource::CurveToQuadraticSmoothSegment> SVGPathStringSource::parseCurveToQuadraticSmoothSegment()
{
    return parse([](auto& buffer) -> Optional<CurveToQuadraticSmoothSegment> {
        auto targetPoint = parseFloatPoint(buffer);
        if (!targetPoint)
            return WTF::nullopt;

        CurveToQuadraticSmoothSegment segment;
        segment.targetPoint = *targetPoint;
        return segment;
    });
}

Optional<SVGPathSource::ArcToSegment> SVGPathStringSource::parseArcToSegment()
{
    return parse([](auto& buffer) -> Optional<ArcToSegment> {
        auto rx = parseNumber(buffer);
        if (!rx)
            return WTF::nullopt;
        auto ry = parseNumber(buffer);
        if (!ry)
            return WTF::nullopt;
        auto angle = parseNumber(buffer);
        if (!angle)
            return WTF::nullopt;
        auto largeArc = parseArcFlag(buffer);
        if (!largeArc)
            return WTF::nullopt;
        auto sweep = parseArcFlag(buffer);
        if (!sweep)
            return WTF::nullopt;
        auto targetPoint = parseFloatPoint(buffer);
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
    });
}

} // namespace WebKit
