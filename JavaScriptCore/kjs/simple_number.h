/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003 Apple Computer, Inc
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_SIMPLE_NUMBER_H
#define KJS_SIMPLE_NUMBER_H

#include <stdlib.h>
#include "kxmlcore/AlwaysInline.h"

namespace KJS {

    class JSValue;

    class SimpleNumber {
    public:
        static const unsigned long tag     = 1; // 01 is the full tag, since it's 2 bits long.
        static const unsigned long tagMask = 3; // 11 is the tag mask, since it's 2 bits long.
        
        ALWAYS_INLINE
        static JSValue *make(double d)
        {
            if (sizeof(float) == sizeof(unsigned long) &&
                sizeof(double) == sizeof(unsigned long long) &&
                sizeof(JSValue *) >= sizeof(unsigned long)) {
                // 32-bit
                union {
                    unsigned long asBits;
                    float         asFloat;
                } floatUnion;
                floatUnion.asFloat = d;
                
                if ((floatUnion.asBits & tagMask) != 0)
                  return 0;
                
                // Check for loss in conversion to float
                union {
                    unsigned long long asBits;
                    double             asDouble;
                } doubleUnion1, doubleUnion2;
                doubleUnion1.asDouble = floatUnion.asFloat;
                doubleUnion2.asDouble = d;
                if (doubleUnion1.asBits != doubleUnion2.asBits)
                    return 0;
                
                return reinterpret_cast<JSValue *>(floatUnion.asBits | tag);
            } else if (sizeof(double) == sizeof(unsigned long) &&
                       sizeof(JSValue*) >= sizeof(unsigned long)) {
                // 64-bit
                union {
                    unsigned long asBits;
                    double        asDouble;
                } doubleUnion;
                doubleUnion.asDouble = d;
                
                if ((doubleUnion.asBits & tagMask) != 0)
                    return 0;

                return reinterpret_cast<JSValue *>(doubleUnion.asBits | tag);
            } else {
                // could just return 0 here, but nicer to be explicit about not supporting the platform well
                abort();
            }
        }

        static bool is(const JSValue *imp)
        {
            return (reinterpret_cast<unsigned long>(imp) & tagMask) == tag;
        }
        
        ALWAYS_INLINE
        static double value(const JSValue *imp)
        {
            assert(is(imp));
            
            if (sizeof(float) == sizeof(unsigned long)) {
                // 32-bit
                union {
                    unsigned long asBits;
                    float         asFloat;
                } floatUnion;
                floatUnion.asBits = reinterpret_cast<unsigned long>(imp) & ~tagMask;
                return floatUnion.asFloat;
            } else if (sizeof(double) == sizeof(unsigned long)) {
                // 64-bit
                union {
                    unsigned long asBits;
                    double        asDouble;
                } doubleUnion;
                doubleUnion.asBits = reinterpret_cast<unsigned long>(imp) & ~tagMask;
                return doubleUnion.asDouble;
            } else {
                // could just return 0 here, but nicer to be explicit about not supporting the platform well
                abort();
            }
        }
        
    };

}

#endif
