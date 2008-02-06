/*
 *  Copyright (C) 2003,2007 Apple Computer, Inc
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_COMMON_IDENTIFIERS_H
#define KJS_COMMON_IDENTIFIERS_H

#include "identifier.h"
#include <wtf/Noncopyable.h>

// List of property names, passed to a macro so we can do set them up various
// ways without repeating the list.
#define KJS_COMMON_IDENTIFIERS_EACH_PROPERTY_NAME(macro) \
    macro(arguments) \
    macro(callee) \
    macro(caller) \
    macro(constructor) \
    macro(fromCharCode) \
    macro(global) \
    macro(ignoreCase) \
    macro(index) \
    macro(input) \
    macro(length) \
    macro(message) \
    macro(multiline) \
    macro(name) \
    macro(prototype) \
    macro(source) \
    macro(toExponential) \
    macro(toFixed) \
    macro(toLocaleString) \
    macro(toPrecision) \
    macro(toString) \
    macro(valueOf)

namespace KJS {

    class CommonIdentifiers : Noncopyable {

    private:
        CommonIdentifiers();

    public:
        static CommonIdentifiers* shared();

        const Identifier nullIdentifier;
        const Identifier underscoreProto;

#define KJS_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL(name) const Identifier name;
        KJS_COMMON_IDENTIFIERS_EACH_PROPERTY_NAME(KJS_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL)
#undef KJS_IDENTIFIER_DECLARE_PROPERTY_NAME_GLOBAL
    };
} // namespace KJS

#endif // KJS_COMMON_IDENTIFIERS_H

