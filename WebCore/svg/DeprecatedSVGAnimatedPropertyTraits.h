/*
 * Copyright (C) Research In Motion Limited 2010. All rights reserved.
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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

#ifndef DeprecatedSVGAnimatedPropertyTraits_h
#define DeprecatedSVGAnimatedPropertyTraits_h

#if ENABLE(SVG)
#include "PlatformString.h"
#include "SVGTransformList.h"

namespace WebCore {

template<typename Type>
struct DeprecatedSVGAnimatedPropertyTraits : public Noncopyable { };

// SVGAnimatedString
template<>
struct DeprecatedSVGAnimatedPropertyTraits<String> : public Noncopyable {
    typedef const String& PassType;
    typedef String ReturnType;
    typedef String StoredType;

    static ReturnType null() { return String(); }
    static ReturnType toReturnType(const StoredType& type) { return type; }
    static String toString(PassType type) { return type; }
};

// SVGAnimatedTransformList
template<>
struct DeprecatedSVGAnimatedPropertyTraits<SVGTransformList*> : public Noncopyable {
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
