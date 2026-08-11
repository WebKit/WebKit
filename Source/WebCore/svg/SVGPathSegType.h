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

} // namespace WebCore
