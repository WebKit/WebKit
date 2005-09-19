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

namespace KJS {

    class ValueImp;

    inline bool isNegativeZero(double num)
    {
#if WIN32
        return _fpclass(num) == _FPCLASS_NZ;
#else
        return num == -0.0 && signbit(num);
#endif
    }
    
    class SimpleNumber {
    public:
	enum { tag = 1, shift = 2, mask = (1 << shift) - 1, sign = 1L << (sizeof(long) * 8 - 1 ), max = (1L << ((sizeof(long) * 8 - 1) - shift)) - 1, min = -max - 1, imax = (1L << ((sizeof(int) * 8 - 1) - shift)) - 1, imin = -imax - 1 };

	static inline bool is(const ValueImp *imp) { return ((long)imp & mask) == tag; }
	static inline long value(const ValueImp *imp) { return ((long)imp >> shift) | (((long)imp & sign) ? ~max : 0); }

	static inline bool fits(int i) { return i <= imax && i >= imin; }
	static inline bool fits(unsigned i) { return i <= (unsigned)max; }
	static inline bool fits(long i) { return i <= max && i >= min; }
	static inline bool fits(unsigned long i) { return i <= (unsigned)max; }
        static inline bool fits(long long i) { return i <= max && i >= min; }
        static inline bool fits(unsigned long long i) { return i <= (unsigned)max; }
	static inline bool integerFits(double d) { return !(d < min || d > max); }
	static inline bool fits(double d) { return d >= min && d <= max && d == (double)(long)d && !isNegativeZero(d); }
	static inline ValueImp *make(long i) { return (ValueImp *)((i << shift) | tag); }
    };

}

#endif
