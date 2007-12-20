/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *  Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
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

#ifndef KJS_JS_IMMEDIATE_H
#define KJS_JS_IMMEDIATE_H

#include "JSType.h"
#include <wtf/Assertions.h>
#include <wtf/AlwaysInline.h>
#include <wtf/MathExtras.h>
#include <limits>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

namespace KJS {

class ExecState;
class JSObject;
class JSValue;
class UString;

/*
 * A JSValue*  is either a pointer to a cell (a heap-allocated object) or an immediate (a type-tagged 
 * signed int masquerading as a pointer). The low two bits in a JSValue* are available 
 * for type tagging because allocator alignment guarantees they will be 00 in cell pointers.
 *
 * For example, on a 32 bit system:
 *
 * JSCell*:       XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX                 00
 *               [ high 30 bits: pointer address ]  [ low 2 bits -- always 0 ]
 *
 * JSImmediate:   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX                 TT
 *               [ high 30 bits: signed int ]       [ low 2 bits -- type tag ]
 *
 * The bit "payload" (the high 30 bits) is a 30 bit signed int for immediate numbers, a flag to distinguish true/false
 * and undefined/null.
 *
 * Notice that the JSType value of NullType is 4, which requires 3 bits to encode. Since we only have 2 bits 
 * available for type tagging, we tag the null immediate with UndefinedType, and JSImmediate::type() has 
 * to sort them out.
 */

class JSImmediate {
public:
    static ALWAYS_INLINE bool isImmediate(const JSValue* v)
    {
        return getTag(v) != 0;
    }
    
    static ALWAYS_INLINE bool isNumber(const JSValue* v)
    {
        return (getTag(v) == NumberType);
    }
    
    static ALWAYS_INLINE bool isBoolean(const JSValue* v)
    {
        return (getTag(v) == BooleanType);
    }
    
    // Since we have room for only 3 unique tags, null and undefined have to share.
    static ALWAYS_INLINE bool isUndefinedOrNull(const JSValue* v)
    {
        return (getTag(v) == UndefinedType);
    }

    static JSValue* from(char);
    static JSValue* from(signed char);
    static JSValue* from(unsigned char);
    static JSValue* from(short);
    static JSValue* from(unsigned short);
    static JSValue* from(int);
    static JSValue* from(unsigned);
    static JSValue* from(long);
    static JSValue* from(unsigned long);
    static JSValue* from(long long);
    static JSValue* from(unsigned long long);
    static JSValue* from(double);

    static ALWAYS_INLINE bool areBothImmediateNumbers(const JSValue* v1, const JSValue* v2)
    {
        return (reinterpret_cast<uintptr_t>(v1) & reinterpret_cast<uintptr_t>(v2) & TagMask) == NumberType;
    }

    static ALWAYS_INLINE JSValue* andImmediateNumbers(const JSValue* v1, const JSValue* v2)
    {
        ASSERT(areBothImmediateNumbers(v1, v2));
        return reinterpret_cast<JSValue*>(reinterpret_cast<uintptr_t>(v1) & reinterpret_cast<uintptr_t>(v2));
    }

    static double toDouble(const JSValue*);
    static bool toBoolean(const JSValue*);
    static JSObject* toObject(const JSValue*, ExecState*);
    static UString toString(const JSValue*);
    static JSType type(const JSValue*);

    static bool getUInt32(const JSValue*, uint32_t&);
    static bool getTruncatedInt32(const JSValue*, int32_t&);
    static bool getTruncatedUInt32(const JSValue*, uint32_t&);

    static int32_t getTruncatedInt32(const JSValue*);

    static JSValue* trueImmediate();
    static JSValue* falseImmediate();
    static JSValue* undefinedImmediate();
    static JSValue* nullImmediate();
    
private:
    static const uintptr_t TagMask = 3; // type tags are 2 bits long

    // Immediate values are restricted to a 30 bit signed value.
    static const int minImmediateInt = -(1 << 29);
    static const int maxImmediateInt = (1 << 29) - 1;
    static const unsigned maxImmediateUInt = maxImmediateInt;
    
    static ALWAYS_INLINE JSValue* tag(uintptr_t bits, uintptr_t tag)
    {
        return reinterpret_cast<JSValue*>(bits | tag);
    }
    
    static ALWAYS_INLINE uintptr_t unTag(const JSValue* v)
    {
        return reinterpret_cast<uintptr_t>(v) & ~TagMask;
    }
    
    static ALWAYS_INLINE uintptr_t getTag(const JSValue* v)
    {
        return reinterpret_cast<uintptr_t>(v) & TagMask;
    }
};

ALWAYS_INLINE JSValue* JSImmediate::trueImmediate() { return tag(1 << 2, BooleanType); }
ALWAYS_INLINE JSValue* JSImmediate::falseImmediate() { return tag(0, BooleanType); }
ALWAYS_INLINE JSValue* JSImmediate::undefinedImmediate() { return tag(1 << 2, UndefinedType); }
ALWAYS_INLINE JSValue* JSImmediate::nullImmediate() { return tag(0, UndefinedType); }

ALWAYS_INLINE bool JSImmediate::toBoolean(const JSValue* v)
{
    ASSERT(isImmediate(v));
    uintptr_t bits = unTag(v);
    return (bits != 0) & (JSImmediate::getTag(v) != UndefinedType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(char i)
{
    return tag(i << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(signed char i)
{
    return tag(i << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(unsigned char i)
{
    return tag(i << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(short i)
{
    return tag(i << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(unsigned short i)
{
    return tag(i << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(int i)
{
    if ((i < minImmediateInt) | (i > maxImmediateInt))
        return 0;
    return tag(i << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(unsigned i)
{
    if (i > maxImmediateUInt)
        return 0;
    return tag(i << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(long i)
{
    if ((i < minImmediateInt) | (i > maxImmediateInt))
        return 0;
    return tag(i << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(unsigned long i)
{
    if (i > maxImmediateUInt)
        return 0;
    return tag(i << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(long long i)
{
    if ((i < minImmediateInt) | (i > maxImmediateInt))
        return 0;
    return tag(static_cast<uintptr_t>(i) << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(unsigned long long i)
{
    if (i > maxImmediateUInt)
        return 0;
    return tag(static_cast<uintptr_t>(i) << 2, NumberType);
}

ALWAYS_INLINE JSValue* JSImmediate::from(double d)
{
    const int intVal = static_cast<int>(d);

    if ((intVal < minImmediateInt) | (intVal > maxImmediateInt))
        return 0;

    // Check for data loss from conversion to int.
    if ((intVal != d) || (!intVal && signbit(d)))
        return 0;

    return tag(intVal << 2, NumberType);
}

ALWAYS_INLINE int32_t JSImmediate::getTruncatedInt32(const JSValue* v)
{
    ASSERT(isNumber(v));
    return static_cast<int32_t>(unTag(v)) >> 2;
}

ALWAYS_INLINE double JSImmediate::toDouble(const JSValue* v)
{
    ASSERT(isImmediate(v));
    const int32_t i = static_cast<int32_t>(unTag(v)) >> 2;
    if (JSImmediate::getTag(v) == UndefinedType && i)
        return std::numeric_limits<double>::quiet_NaN();
    return i;
}

ALWAYS_INLINE bool JSImmediate::getUInt32(const JSValue* v, uint32_t& i)
{
    const int32_t si = static_cast<int32_t>(unTag(v)) >> 2;
    i = si;
    return isNumber(v) & (si >= 0);
}

ALWAYS_INLINE bool JSImmediate::getTruncatedInt32(const JSValue* v, int32_t& i)
{
    i = static_cast<int32_t>(unTag(v)) >> 2;
    return isNumber(v);
}

ALWAYS_INLINE bool JSImmediate::getTruncatedUInt32(const JSValue* v, uint32_t& i)
{
    return getUInt32(v, i);
}

} // namespace KJS

#endif
