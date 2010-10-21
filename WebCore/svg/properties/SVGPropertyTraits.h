/*
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef SVGPropertyTraits_h
#define SVGPropertyTraits_h

#if ENABLE(SVG)
#include "SVGLength.h"
#include "SVGLengthList.h"

namespace WebCore {

template<typename PropertyType>
struct SVGPropertyTraits { };

// SVGAnimatedLength
template<>
struct SVGPropertyTraits<SVGLength> {
    static SVGLength& initialValue()
    {
        DEFINE_STATIC_LOCAL(SVGLength, s_initialValue, ());
        s_initialValue = SVGLength();
        return s_initialValue;
    }

    static String toString(const SVGLength& type) { return type.valueAsString(); }
};

// SVGAnimatedLengthList
template<>
struct SVGPropertyTraits<SVGLengthList> {
    typedef SVGLength ListItemType;

    static SVGLengthList& initialValue()
    {
        DEFINE_STATIC_LOCAL(SVGLengthList, s_initialValue, ());
        s_initialValue = SVGLengthList();
        return s_initialValue;
    }

    static String toString(const SVGLengthList& type) { return type.valueAsString(); }
};

}

#endif
#endif
