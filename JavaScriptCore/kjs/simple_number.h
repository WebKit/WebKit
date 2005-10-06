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

// We include these headers here because simple_number.h is the most low-level header we have.
#include <float.h>
#include <math.h>
#include <stdint.h>
#include <string.h>

// Workaround for a bug in GCC library headers.
// We'd prefer to just use math.h.
#if !WIN32
#include <cmath>
using std::isfinite;
using std::isinf;
using std::isnan;
using std::signbit;
#endif

#if defined(__GNUC__) && (__GNUC__ > 3)
#define ALWAYS_INLINE __attribute__ ((always_inline))
#else
#define ALWAYS_INLINE inline
#endif

namespace KJS {

    class ValueImp;

    class SimpleNumber {
    public:
        static const unsigned long tag     = 1; // 01 is the full tag, since it's 2 bits long.
        static const unsigned long tagMask = 3; // 11 is the tag mask, since it's 2 bits long.
        
        ALWAYS_INLINE
        static ValueImp *make(double d)
        {
            if (sizeof(float) == sizeof(unsigned long) &&
                sizeof(double) == sizeof(unsigned long long) &&
                sizeof(ValueImp*) >= sizeof(unsigned long)) {
                // 32-bit
                union {
                    unsigned long asBits;
                    float         asFloat;
                } floatunion;
                floatunion.asFloat = d;
                
                if ((floatunion.asBits & tagMask) != 0)
                  return 0;
                
                // Check for loss in conversion to float
                union {
                    unsigned long long asBits;
                    double             asDouble;
                } doubleunion1, doubleunion2;
                doubleunion1.asDouble = floatunion.asFloat;
                doubleunion2.asDouble = d;
                if (doubleunion1.asBits != doubleunion2.asBits)
                    return 0;
                
                return reinterpret_cast<ValueImp *>(floatunion.asBits | tag);
            } else if (sizeof(double) == sizeof(unsigned long) &&
                       sizeof(ValueImp*) >= sizeof(unsigned long)) {
                // 64-bit
                union {
                    unsigned long asBits;
                    double        asDouble;
                } doubleunion;
                doubleunion.asDouble = d;
                
                if ((doubleunion.asBits & tagMask) != 0)
                    return 0;

                return reinterpret_cast<ValueImp *>(doubleunion.asBits | tag);
            } else {
                // could just return 0 here, but nicer to be explicit about not supporting the platform well
                abort();
            }
        }

        static bool is(const ValueImp *imp)
        {
            return (reinterpret_cast<unsigned long>(imp) & tagMask) == tag;
        }
        
        ALWAYS_INLINE
        static double value(const ValueImp *imp)
        {
            assert(is(imp));
            
            if (sizeof(float) == sizeof(unsigned long)) {
                // 32-bit
                union {
                    unsigned long asBits;
                    float         asFloat;
                } floatunion;
                floatunion.asBits = reinterpret_cast<unsigned long>(imp) & ~tagMask;
                return floatunion.asFloat;
            } else if (sizeof(double) == sizeof(unsigned long)) {
                // 64-bit
                union {
                    unsigned long asBits;
                    double        asDouble;
                } doubleunion;
                doubleunion.asBits = reinterpret_cast<unsigned long>(imp) & ~tagMask;
                return doubleunion.asDouble;
            } else {
                // could just return 0 here, but nicer to be explicit about not supporting the platform well
                abort();
            }
        }
        
    };

}

#endif
