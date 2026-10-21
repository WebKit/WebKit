/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006, 2008 Rob Buis <buis@kde.org>
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#pragma once

#include <cstdint>

namespace WebCore {

enum class SVGPathSegType : uint8_t {
    Unknown = 0,
    ClosePath = 1,
    MoveToAbs = 2,
    MoveToRel = 3,
    LineToAbs = 4,
    LineToRel = 5,
    CurveToCubicAbs = 6,
    CurveToCubicRel = 7,
    CurveToQuadraticAbs = 8,
    CurveToQuadraticRel = 9,
    ArcAbs = 10,
    ArcRel = 11,
    LineToHorizontalAbs = 12,
    LineToHorizontalRel = 13,
    LineToVerticalAbs = 14,
    LineToVerticalRel = 15,
    CurveToCubicSmoothAbs = 16,
    CurveToCubicSmoothRel = 17,
    CurveToQuadraticSmoothAbs = 18,
    CurveToQuadraticSmoothRel = 19
};

// Maps a path data command letter to its segment type, per the grammar in
// https://w3c.github.io/svgwg/specs/paths/#PathDataBNF. Returns Unknown for
// anything that is not one of the twenty command letters. Both 'Z' and 'z'
// map to ClosePath; the grammar gives closepath no relative form.
constexpr SVGPathSegType pathSegTypeForLetter(char16_t letter)
{
    switch (letter) {
    case 'Z':
    case 'z':
        return SVGPathSegType::ClosePath;
    case 'M':
        return SVGPathSegType::MoveToAbs;
    case 'm':
        return SVGPathSegType::MoveToRel;
    case 'L':
        return SVGPathSegType::LineToAbs;
    case 'l':
        return SVGPathSegType::LineToRel;
    case 'C':
        return SVGPathSegType::CurveToCubicAbs;
    case 'c':
        return SVGPathSegType::CurveToCubicRel;
    case 'Q':
        return SVGPathSegType::CurveToQuadraticAbs;
    case 'q':
        return SVGPathSegType::CurveToQuadraticRel;
    case 'A':
        return SVGPathSegType::ArcAbs;
    case 'a':
        return SVGPathSegType::ArcRel;
    case 'H':
        return SVGPathSegType::LineToHorizontalAbs;
    case 'h':
        return SVGPathSegType::LineToHorizontalRel;
    case 'V':
        return SVGPathSegType::LineToVerticalAbs;
    case 'v':
        return SVGPathSegType::LineToVerticalRel;
    case 'S':
        return SVGPathSegType::CurveToCubicSmoothAbs;
    case 's':
        return SVGPathSegType::CurveToCubicSmoothRel;
    case 'T':
        return SVGPathSegType::CurveToQuadraticSmoothAbs;
    case 't':
        return SVGPathSegType::CurveToQuadraticSmoothRel;
    default:
        return SVGPathSegType::Unknown;
    }
}

// The inverse of pathSegTypeForLetter(). ClosePath gives 'Z': the grammar has no
// relative closepath, so 'z' on the way in comes back out canonicalised.
// Unknown gives '\0', which no caller should ever serialize.
constexpr char letterForPathSegType(SVGPathSegType type)
{
    switch (type) {
    case SVGPathSegType::ClosePath:
        return 'Z';
    case SVGPathSegType::MoveToAbs:
        return 'M';
    case SVGPathSegType::MoveToRel:
        return 'm';
    case SVGPathSegType::LineToAbs:
        return 'L';
    case SVGPathSegType::LineToRel:
        return 'l';
    case SVGPathSegType::CurveToCubicAbs:
        return 'C';
    case SVGPathSegType::CurveToCubicRel:
        return 'c';
    case SVGPathSegType::CurveToQuadraticAbs:
        return 'Q';
    case SVGPathSegType::CurveToQuadraticRel:
        return 'q';
    case SVGPathSegType::ArcAbs:
        return 'A';
    case SVGPathSegType::ArcRel:
        return 'a';
    case SVGPathSegType::LineToHorizontalAbs:
        return 'H';
    case SVGPathSegType::LineToHorizontalRel:
        return 'h';
    case SVGPathSegType::LineToVerticalAbs:
        return 'V';
    case SVGPathSegType::LineToVerticalRel:
        return 'v';
    case SVGPathSegType::CurveToCubicSmoothAbs:
        return 'S';
    case SVGPathSegType::CurveToCubicSmoothRel:
        return 's';
    case SVGPathSegType::CurveToQuadraticSmoothAbs:
        return 'T';
    case SVGPathSegType::CurveToQuadraticSmoothRel:
        return 't';
    case SVGPathSegType::Unknown:
        return '\0';
    }
    return '\0';
}

} // namespace WebCore
