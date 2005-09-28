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

#define KJS_MIN_MACRO(a, b) ((a) < (b) ? (a) : (b))

namespace KJS {

    class ValueImp;

    static inline bool isNegativeZero(double num)
    {
#if WIN32
        return _fpclass(num) == _FPCLASS_NZ;
#else
        return num == -0.0 && signbit(num);
#endif
    }
    
    class SimpleNumber {
    public:
        static const int           tagBits   = 2;   // Pointer alignment guarantees that we have the low two bits to play with for type tagging
        static const unsigned long tag       = 1UL; // 01 is the full tag
        static const unsigned long tagMask   = (1UL << tagBits) - 1;
        static const int           valueBits = KJS_MIN_MACRO(sizeof(ValueImp *) * 8 - tagBits, sizeof(long) * 8);
        static const unsigned long signMask  = 1UL << sizeof(long) * 8 - 1;
        static const long          maxValue  = (signMask >> tagBits) - 1;
        static const unsigned long umaxValue = maxValue;
        static const long          minValue  = -(maxValue + 1);

        static unsigned long rightShiftSignExtended(uintptr_t l, int n)
        {
            return (l >> n) | ((l & signMask) ? (~0UL << valueBits) : 0);
        }
        
        static  ValueImp *make(long i)          { return reinterpret_cast<ValueImp *>((i << tagBits) | tag); }
        static  bool is(const ValueImp *imp)    { return (reinterpret_cast<uintptr_t>(imp) & tagMask) == tag; }
        static  long value(const ValueImp *imp) { return rightShiftSignExtended(reinterpret_cast<uintptr_t>(imp), tagBits); }

        static  bool fits(int i)                { return !(i > maxValue || i < minValue); }
        static  bool fits(long i)               { return !(i > maxValue || i < minValue); }
        static  bool fits(long long i)          { return !(i > maxValue || i < minValue); }
        static  bool integerFits(double i)      { return !(i > maxValue || i < minValue); }

        static  bool fits(double d)             { return !(d > maxValue || d < minValue) && d == (double)(long)d && !isNegativeZero(d); }

        static  bool fits(unsigned i)           { return !(i > umaxValue); }
        static  bool fits(unsigned long i)      { return !(i > umaxValue); }
        static  bool fits(unsigned long long i) { return !(i > umaxValue); }
    };

}

#endif
