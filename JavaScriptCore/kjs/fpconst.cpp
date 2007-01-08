/*
 *  Copyright (C) 2003, 2006 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301,
 *  USA.
 *
 */

#include "config.h"

namespace KJS {

// This file exists because JavaScriptCore needs to define the NaN and Inf globals in a way
// that does not use a static initializer so we don't have a framework initialization routine.

// The trick is to define the NaN and Inf globals with a different type than the declaration.
// This trick works because the mangled name of the globals does not include the type, although
// I'm not sure that's guaranteed. There could be alignment issues with this, since arrays of
// characters don't necessarily need the same alignment doubles do, but for now it seems to work.
// It would be good to figure out a 100% clean way that still avoids code that runs at init time.

#if PLATFORM(DARWIN)

#if PLATFORM(BIG_ENDIAN)
    extern const unsigned char NaN[sizeof(double)] = { 0x7f, 0xf8, 0, 0, 0, 0, 0, 0 };
    extern const unsigned char Inf[sizeof(double)] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };
#elif PLATFORM(MIDDLE_ENDIAN)
    extern const unsigned char NaN[] = { 0, 0, 0xf8, 0x7f, 0, 0, 0, 0 };
    extern const unsigned char Inf[] = { 0, 0, 0xf0, 0x7f, 0, 0, 0, 0 };
#else
    extern const unsigned char NaN[sizeof(double)] = { 0, 0, 0, 0, 0, 0, 0xf8, 0x7f };
    extern const unsigned char Inf[sizeof(double)] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
#endif // PLATFORM(MIDDLE_ENDIAN)

#else // !PLATFORM(DARWIN)

// Note, we have to use union to ensure alignment. Otherwise, NaN_Bytes can start anywhere,
// while NaN_double has to be 4-byte aligned for 32-bits.
// With -fstrict-aliasing enabled, unions are the only safe way to do type masquerading.

static const union {
    struct {
        unsigned char NaN_Bytes[8];
        unsigned char Inf_Bytes[8];
    } bytes;
    
    struct {
        double NaN_Double;
        double Inf_Double;
    } doubles;
    
} NaNInf = { {
#if PLATFORM(BIG_ENDIAN)
    { 0x7f, 0xf8, 0, 0, 0, 0, 0, 0 },
    { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 }
#elif PLATFORM(MIDDLE_ENDIAN)
    { 0, 0, 0xf8, 0x7f, 0, 0, 0, 0 },
    { 0, 0, 0xf0, 0x7f, 0, 0, 0, 0 }
#else
    { 0, 0, 0, 0, 0, 0, 0xf8, 0x7f },
    { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f }
#endif
} } ;

    extern const double NaN = NaNInf.doubles.NaN_Double;
    extern const double Inf = NaNInf.doubles.Inf_Double;
 
#endif // !PLATFORM(DARWIN)


} // namespace KJS
