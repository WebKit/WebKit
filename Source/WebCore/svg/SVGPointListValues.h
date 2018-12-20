/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "FloatPoint.h"
#include "SVGPropertyTraits.h"
#include <wtf/Vector.h>

namespace WebCore {

class SVGPoint;
class SVGPointList;

class SVGPointListValues final : public Vector<FloatPoint> {
public:
    String valueAsString() const;
};

template<>
struct SVGPropertyTraits<SVGPointListValues> {
    static SVGPointListValues initialValue() { return { }; }
    static SVGPointListValues fromString(const String& string)
    {
        SVGPointListValues list;
        pointsListFromSVGData(list, string);
        return list;
    }
    static Optional<SVGPointListValues> parse(const QualifiedName&, const String&) { ASSERT_NOT_REACHED(); return { }; }
    static String toString(const SVGPointListValues& list) { return list.valueAsString(); }

    using ListItemType = FloatPoint;
    using ListItemTearOff = SVGPoint;
    using ListPropertyTearOff = SVGPointList;
};

} // namespace WebCore
