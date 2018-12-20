/*
 * Copyright (C) 2004, 2005, 2006, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#include "SVGLengthValue.h"
#include <wtf/Vector.h>

namespace WebCore {

class SVGLength;
class SVGLengthList;

class SVGLengthListValues final : public Vector<SVGLengthValue> {
public:
    void parse(const String& value, SVGLengthMode);
    String valueAsString() const;
};

template<>
struct SVGPropertyTraits<SVGLengthListValues> {
    static SVGLengthListValues initialValue() { return { }; }
    static SVGLengthListValues fromString(const String& string, SVGLengthMode lengthMode)
    {
        SVGLengthListValues list;
        list.parse(string, lengthMode);
        return list;
    }
    static Optional<SVGLengthListValues> parse(const QualifiedName& attrName, const String& string)
    {
        SVGLengthListValues list;
        list.parse(string, SVGLengthValue::lengthModeForAnimatedLengthAttribute(attrName));
        return list;
    }
    static String toString(const SVGLengthListValues& type) { return type.valueAsString(); }

    using ListItemType = SVGLengthValue;
    using ListItemTearOff = SVGLength;
    using ListPropertyTearOff = SVGLengthList;
};

} // namespace WebCore
