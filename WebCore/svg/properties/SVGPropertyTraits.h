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
#include "FloatRect.h"
#include "SVGAngle.h"
#include "SVGLength.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGPointList.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGStringList.h"
#include "SVGTransformList.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

template<typename PropertyType>
struct SVGPropertyTraits { };

template<>
struct SVGPropertyTraits<SVGAngle> {
    static SVGAngle initialValue() { return SVGAngle(); }
    static String toString(const SVGAngle& type) { return type.valueAsString(); }
};

template<>
struct SVGPropertyTraits<bool> {
    static bool initialValue() { return false; }
    static String toString(bool type) { return type ? "true" : "false"; }
};

template<>
struct SVGPropertyTraits<int> {
    static int initialValue() { return 0; }
    static String toString(int type) { return String::number(type); }
};

template<>
struct SVGPropertyTraits<long> {
    static long initialValue() { return 0; }
    static String toString(long type) { return String::number(type); }
};

template<>
struct SVGPropertyTraits<SVGLength> {
    static SVGLength initialValue() { return SVGLength(); }
    static String toString(const SVGLength& type) { return type.valueAsString(); }
};

template<>
struct SVGPropertyTraits<SVGLengthList> {
    typedef SVGLength ListItemType;

    static SVGLengthList initialValue() { return SVGLengthList(); }
    static String toString(const SVGLengthList& type) { return type.valueAsString(); }
};

template<>
struct SVGPropertyTraits<float> {
    static float initialValue() { return 0; }
    static String toString(float type) { return String::number(type); }
};

template<>
struct SVGPropertyTraits<SVGNumberList> {
    typedef float ListItemType;

    static SVGNumberList initialValue() { return SVGNumberList(); }
    static String toString(const SVGNumberList& type) { return type.valueAsString(); }
};

template<>
struct SVGPropertyTraits<SVGPreserveAspectRatio> {
    static SVGPreserveAspectRatio initialValue() { return SVGPreserveAspectRatio(); }
    static String toString(const SVGPreserveAspectRatio& type) { return type.valueAsString(); }
};

template<>
struct SVGPropertyTraits<FloatRect> {
    static FloatRect initialValue() { return FloatRect(); }
    static String toString(const FloatRect& type)
    {
        StringBuilder builder;
        builder.append(String::number(type.x()));
        builder.append(' ');
        builder.append(String::number(type.y()));
        builder.append(' ');
        builder.append(String::number(type.width()));
        builder.append(' ');
        builder.append(String::number(type.height()));
        builder.append(' ');
        return builder.toString();
    }
};

template<>
struct SVGPropertyTraits<String> {
    static String initialValue() { return String(); }
    static String toString(const String& type) { return type; }
};

template<>
struct SVGPropertyTraits<SVGStringList> {
    typedef String ListItemType;
};

template<>
struct SVGPropertyTraits<SVGPointList> {
    static SVGPointList initialValue() { return SVGPointList(); }
    typedef FloatPoint ListItemType;
};

template<>
struct SVGPropertyTraits<SVGTransformList> {
    static SVGTransformList initialValue() { return SVGTransformList(); }
    static String toString(const SVGTransformList& type) { return type.valueAsString(); }
    typedef SVGTransform ListItemType;
};

}

#endif
#endif
