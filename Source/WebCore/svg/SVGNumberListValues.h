/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005, 2006 Rob Buis <buis@kde.org>
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

#include "SVGPropertyTraits.h"
#include <wtf/Vector.h>

namespace WebCore {

class SVGNumber;
class SVGNumberList;

class SVGNumberListValues final : public Vector<float> {
public:
    void parse(const String&);
    String valueAsString() const;
};

template<> struct SVGPropertyTraits<SVGNumberListValues> {
    static SVGNumberListValues initialValue() { return { }; }
    static SVGNumberListValues fromString(const String& string)
    {
        SVGNumberListValues list;
        list.parse(string);
        return list;
    }
    static Optional<SVGNumberListValues> parse(const QualifiedName&, const String&) { ASSERT_NOT_REACHED(); return initialValue(); }
    static String toString(const SVGNumberListValues& list) { return list.valueAsString(); }

    using ListItemType = float;
    using ListItemTearOff = SVGNumber;
    using ListPropertyTearOff = SVGNumberList;
};

} // namespace WebCore
