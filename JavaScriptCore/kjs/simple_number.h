// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002 Apple Computer, Inc
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#ifndef _KJS_SIMPLE_NUMBER_H_
#define _KJS_SIMPLE_NUMBER_H_

#include <limits.h>
#include <math.h>

namespace KJS {
    class ValueImp;

    class SimpleNumber {
    public:
	enum { TAG = 1, MASK = (1 + 2) };

	static inline bool isSimpleNumber(const ValueImp *imp) { return ((long)imp & MASK) == TAG; }
	static inline long longValue(const ValueImp *imp) { return ((long)imp & ~MASK) >> 2; } 
	static inline bool fitsInSimpleNumber(int i) { return i < (LONG_MAX >> 2) && i > (LONG_MIN >> 2); }
	static inline bool fitsInSimpleNumber(unsigned i) { return i < (unsigned)(LONG_MAX >> 2); }
	static inline bool fitsInSimpleNumber(double d) { return d < (LONG_MAX >> 2) && d > (LONG_MIN >> 2) && remainder(d, 1) == 0; }

	static inline bool fitsInSimpleNumber(long i) { return i < (LONG_MAX >> 2) && i > (LONG_MIN >> 2); }
	static inline bool fitsInSimpleNumber(unsigned long i) { return i < (unsigned)(LONG_MAX >> 2); }

	static inline ValueImp *makeSimpleNumber(long i) { return (ValueImp *)((i << 2) | TAG); }
    };
}

#endif
