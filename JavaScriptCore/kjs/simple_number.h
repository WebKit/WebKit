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

#define KJS_MIN_MACRO(a, b) ((a) < (b) ? (a) : (b))

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
	enum {
            tag = 1,
            shift = 2,
            mask = (1 << shift) - 1,
            bits = KJS_MIN_MACRO(sizeof(ValueImp *) * 8 - shift, sizeof(long) * 8),
            sign = 1UL << (bits + shift - 1),
            umax = sign >> 1,
            smax = static_cast<long>(umax),
            smin = static_cast<long>(-umax - 1)
        };
        
	static inline bool is(const ValueImp *imp) { return ((long)imp & mask) == tag; }
        static inline long value(const ValueImp *imp) { return ((long)imp >> shift) | (((long)imp & sign) ? ~umax : 0); }

        static inline bool fits(int i) { return i <= smax && i >= smin; }
	static inline bool fits(unsigned i) { return i <= umax; }
	static inline bool fits(long i) { return i <= smax && i >= smin; }
	static inline bool fits(unsigned long i) { return i <= umax; }
	static inline bool fits(long long i) { return i <= smax && i >= smin; }
	static inline bool fits(unsigned long long i) { return i <= umax; }
	static inline bool integerFits(double d) { return !(d < smin || d > smax); }
	static inline bool fits(double d) { return d >= smin && d <= smax && d == (double)(long)d && !isNegativeZero(d); }
	static inline ValueImp *make(long i) { return reinterpret_cast<ValueImp *>(static_cast<uintptr_t>((i << shift) | tag)); }
    };

}

#endif
