/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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
#include <wtf/RefCounted.h>

namespace WebCore {

class SVGUnitTypes final : public RefCounted<SVGUnitTypes> {
public:
    enum SVGUnitType {
        SVG_UNIT_TYPE_UNKNOWN               = 0,
        SVG_UNIT_TYPE_USERSPACEONUSE        = 1,
        SVG_UNIT_TYPE_OBJECTBOUNDINGBOX     = 2
    };

private:
    SVGUnitTypes() { }
};

template<>
struct SVGPropertyTraits<SVGUnitTypes::SVGUnitType> {
    static unsigned highestEnumValue() { return SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX; }

    static String toString(SVGUnitTypes::SVGUnitType type)
    {
        switch (type) {
        case SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN:
            return emptyString();
        case SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE:
            return "userSpaceOnUse"_s;
        case SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX:
            return "objectBoundingBox"_s;
        }

        ASSERT_NOT_REACHED();
        return emptyString();
    }

    static SVGUnitTypes::SVGUnitType fromString(const String& value)
    {
        if (value == "userSpaceOnUse"_s)
            return SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE;
        if (value == "objectBoundingBox"_s)
            return SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX;
        return SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN;
    }
};

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::SVGUnitTypes::SVGUnitType> {
    using values = EnumValues<
        WebCore::SVGUnitTypes::SVGUnitType,

        WebCore::SVGUnitTypes::SVG_UNIT_TYPE_UNKNOWN,
        WebCore::SVGUnitTypes::SVG_UNIT_TYPE_USERSPACEONUSE,
        WebCore::SVGUnitTypes::SVG_UNIT_TYPE_OBJECTBOUNDINGBOX
    >;
};

} // namespace WTF
