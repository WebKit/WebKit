/*
    Copyright (C) Research In Motion Limited 2010. All rights reserved.
    Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#ifndef SVGAnimatedPropertyTraits_h
#define SVGAnimatedPropertyTraits_h

#if ENABLE(SVG)
#include "FloatRect.h"
#include "PlatformString.h"
#include "SVGAngle.h"
#include "SVGLength.h"
#include "SVGLengthList.h"
#include "SVGNumberList.h"
#include "SVGPreserveAspectRatio.h"
#include "SVGTransformList.h"

namespace WebCore {

template<typename Type>
struct SVGAnimatedPropertyTraits : public Noncopyable { };

// SVGAnimatedAngle
template<>
struct SVGAnimatedPropertyTraits<SVGAngle> : public Noncopyable {
    typedef const SVGAngle& PassType;
    typedef SVGAngle ReturnType;
    typedef SVGAngle StoredType;

    static ReturnType null() { return SVGAngle(); }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return type.valueAsString(); }
};

// SVGAnimatedBoolean
template<>
struct SVGAnimatedPropertyTraits<bool> : public Noncopyable {
    typedef const bool& PassType;
    typedef bool ReturnType;
    typedef bool StoredType;

    static ReturnType null() { return false; }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return type ? "true" : "false"; }
};

// SVGAnimatedEnumeration
template<>
struct SVGAnimatedPropertyTraits<int> : public Noncopyable {
    typedef const int& PassType;
    typedef int ReturnType;
    typedef int StoredType;

    static ReturnType null() { return 0; }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return String::number(type); }
};

// SVGAnimatedInteger
template<>
struct SVGAnimatedPropertyTraits<long> : public Noncopyable {
    typedef const long& PassType;
    typedef long ReturnType;
    typedef long StoredType;

    static ReturnType null() { return 0l; }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return String::number(type); }
};

// SVGAnimatedLength
template<>
struct SVGAnimatedPropertyTraits<SVGLength> : public Noncopyable {
    typedef const SVGLength& PassType;
    typedef SVGLength ReturnType;
    typedef SVGLength StoredType;

    static ReturnType null() { return SVGLength(); }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return type.valueAsString(); }
};

// SVGAnimatedLengthList
template<>
struct SVGAnimatedPropertyTraits<SVGLengthList*> : public Noncopyable {
    typedef SVGLengthList* PassType;
    typedef SVGLengthList* ReturnType;
    typedef RefPtr<SVGLengthList> StoredType;

    static ReturnType null() { return 0; }
    static ReturnType toReturnType(const StoredType& type) { return type.get(); }
    static String toString(PassType type) { return type ? type->valueAsString() : String(); }
};

// SVGAnimatedNumber
template<>
struct SVGAnimatedPropertyTraits<float> : public Noncopyable {
    typedef const float& PassType;
    typedef float ReturnType;
    typedef float StoredType;

    static ReturnType null() { return 0.0f; }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return String::number(type); }
};

// SVGAnimatedNumberList
template<>
struct SVGAnimatedPropertyTraits<SVGNumberList*> : public Noncopyable {
    typedef SVGNumberList* PassType;
    typedef SVGNumberList* ReturnType;
    typedef RefPtr<SVGNumberList> StoredType;

    static ReturnType null() { return 0; }
    static ReturnType toReturnType(const StoredType& type) { return type.get(); }
    static String toString(PassType type) { return type ? type->valueAsString() : String(); }
};

// SVGAnimatedPreserveAspectRatio
template<>
struct SVGAnimatedPropertyTraits<SVGPreserveAspectRatio> : public Noncopyable {
    typedef const SVGPreserveAspectRatio& PassType;
    typedef SVGPreserveAspectRatio ReturnType;
    typedef SVGPreserveAspectRatio StoredType;

    static ReturnType null() { return SVGPreserveAspectRatio(); }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return type.valueAsString(); }
};

// SVGAnimatedRect
template<>
struct SVGAnimatedPropertyTraits<FloatRect> : public Noncopyable {
    typedef const FloatRect& PassType;
    typedef FloatRect ReturnType;
    typedef FloatRect StoredType;

    static ReturnType null() { return FloatRect(); }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return String::format("%f %f %f %f", type.x(), type.y(), type.width(), type.height()); }
};

// SVGAnimatedString
template<>
struct SVGAnimatedPropertyTraits<String> : public Noncopyable {
    typedef const String& PassType;
    typedef String ReturnType;
    typedef String StoredType;

    static ReturnType null() { return String(); }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return type; }
};

// SVGAnimatedTransformList
template<>
struct SVGAnimatedPropertyTraits<SVGTransformList*> : public Noncopyable {
    typedef SVGTransformList* PassType;
    typedef SVGTransformList* ReturnType;
    typedef RefPtr<SVGTransformList> StoredType;

    static ReturnType null() { return 0; }
    static ReturnType toReturnType(const StoredType& type) { return type.get(); }
    static String toString(PassType type) { return type ? type->valueAsString() : String(); }
};

}

#endif
#endif
