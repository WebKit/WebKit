/*
 * Copyright (C) 2004, 2005, 2008 Nikolas Zimmermann <zimmermann@kde.org>
 * Copyright (C) 2004, 2005 Rob Buis <buis@kde.org>
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
#include "SVGTransform.h"
#include <wtf/Vector.h>

namespace WebCore {

template<typename T>
class SVGListPropertyTearOff;

class SVGTransformList;

class SVGTransformListValues final : public Vector<SVGTransformValue, 0, CrashOnOverflow, 2> {
public:
    Ref<SVGTransform> createSVGTransformFromMatrix(SVGMatrix&) const;
    Ref<SVGTransform> consolidate();

    bool concatenate(AffineTransform& result) const;

    String valueAsString() const;
    void parse(const String&);
};

template<> struct SVGPropertyTraits<SVGTransformListValues> {
    static SVGTransformListValues initialValue() { return { }; }
    static SVGTransformListValues fromString(const String& string)
    {
        SVGTransformListValues values;
        values.parse(string);
        return values;
    }
    static Optional<SVGTransformListValues> parse(const QualifiedName&, const String&) { ASSERT_NOT_REACHED(); return initialValue(); }
    static String toString(const SVGTransformListValues& list) { return list.valueAsString(); }

    using ListItemType = SVGTransformValue;
    using ListItemTearOff = SVGTransform;
    using ListPropertyTearOff = SVGTransformList;
};

} // namespace WebCore
