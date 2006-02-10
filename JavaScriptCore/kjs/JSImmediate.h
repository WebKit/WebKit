/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2003-2006 Apple Computer, Inc
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
#include "kxmlcore/Assertions.h"
#include <stdlib.h> // for abort()

namespace KJS {

class JSValue;
class ExecState;
class JSObject;
class UString;

/*
 * A JSValue * is either a pointer to a cell (a heap-allocated object) or an immediate (a type-tagged 
 * IEEE floating point bit pattern masquerading as a pointer). The low two bits in a JSValue * are available 
 * for type tagging because allocator alignment guarantees they will be 00 in cell pointers.
 *
 * For example, on a 32 bit system:
 *
 * JSCell *:      XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX                 00
 *               [ high 30 bits: pointer address ]  [ low 2 bits -- always 0 ]
 *
 * JSImmediate:   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX                 TT
 *             [ high 30 bits: IEEE encoded float ] [ low 2 bits -- type tag ]
 *
 * The bit "payload" (the hight 30 bits) of a non-numeric immediate is its numeric equivalent. For example, 
 * the payload of null is 0.0. This makes JSValue::toNumber() a simple bitmask for all immediates.
 *
 * Notice that the JSType value of NullType is 4, which requires 3 bits to encode. Since we only have 2 bits 
 * available for type tagging, we tag the null immediate with UndefinedType, and JSImmediate::type() has 
 * to sort them out. Null and Undefined don't otherwise get confused because the numeric value of Undefined is 
 * NaN, not 0.0.
 */

class JSImmediate {
public:
    static bool isImmediate(const JSValue *v)
    {
        return getTag(v) != 0;
    }
    
    static bool isNumber(const JSValue *v)
    {
        return (getTag(v) == NumberType);
    }
    
    static bool isBoolean(const JSValue *v)
    {
        return (getTag(v) == BooleanType);
    }
    
    // Since we have room for only 3 unique tags, null and undefined have to share.
    static bool isUndefinedOrNull(const JSValue *v)
    {
        return (getTag(v) == UndefinedType);
    }

    static JSValue *fromDouble(double d)
    {
        if (is32bit()) {
            FloatUnion floatUnion;
            floatUnion.asFloat = d;
            
            // check for data loss from tagging
            if ((floatUnion.asBits & TagMask) != 0)
              return 0;
            
            // check for data loss from conversion to float
            DoubleUnion doubleUnion1, doubleUnion2;
            doubleUnion1.asDouble = floatUnion.asFloat;
            doubleUnion2.asDouble = d;
            if (doubleUnion1.asBits != doubleUnion2.asBits)
                return 0;
            
            return tag(floatUnion.asBits, NumberType);
        } else if (is64bit()) {
            DoubleUnion doubleUnion;
            doubleUnion.asDouble = d;
            
            // check for data loss from tagging
            if ((doubleUnion.asBits & TagMask) != 0)
                return 0;

            return tag(doubleUnion.asBits, NumberType);
        } else {
            // could just return 0 here, but nicer to be explicit about not supporting the platform well
            abort();
        }
    }
    
    static double toDouble(const JSValue *v)
    {
        ASSERT(isImmediate(v));
        
        if (is32bit()) {
            FloatUnion floatUnion;
            floatUnion.asBits = unTag(v);
            return floatUnion.asFloat;
        } else if (is64bit()) {
            DoubleUnion doubleUnion;
            doubleUnion.asBits = unTag(v);
            return doubleUnion.asDouble;
        } else
            abort();
    }

    static bool toBoolean(const JSValue *v)
    {
        ASSERT(isImmediate(v));
        
        uintptr_t bits = unTag(v);
        if ((bits << 1) == 0) // -0.0 has the sign bit set
            return false;

        return bits != NanAsBits();
    }
    
    static JSObject *toObject(const JSValue *, ExecState *);
    static UString toString(const JSValue *);
    static JSType type(const JSValue *);
    
    // It would nice just to use fromDouble() to create these values, but that would prevent them from
    // turning into compile-time constants.
    static JSValue *trueImmediate() { return tag(oneAsBits(), BooleanType); }
    static JSValue *falseImmediate() { return tag(zeroAsBits(), BooleanType); }
    static JSValue *NaNImmediate() { return tag(NanAsBits(), NumberType); }
    static JSValue *undefinedImmediate() { return tag(NanAsBits(), UndefinedType); }
    static JSValue *nullImmediate() { return tag(zeroAsBits(), UndefinedType); }
    
private:
    static const uintptr_t TagMask = 3; // type tags are 2 bits long
    
    static JSValue *tag(uintptr_t bits, uintptr_t tag)
    {
        return reinterpret_cast<JSValue *>(bits | tag);
    }
    
    static uintptr_t unTag(const JSValue *v)
    {
        return reinterpret_cast<uintptr_t>(v) & ~TagMask;
    }
    
    static uintptr_t getTag(const JSValue *v)
    {
        return reinterpret_cast<uintptr_t>(v) & TagMask;
    }
    
    // NOTE: With f-strict-aliasing enabled, unions are the only safe way to do type masquerading.

    union FloatUnion {
        u_int32_t asBits;
        float     asFloat;
    };

    union DoubleUnion {
        u_int64_t asBits;
        double    asDouble;
    };

    // IEEE compliant floating point
    static bool isIEEE()
    {
        return sizeof(float) == sizeof(u_int32_t) &&
               sizeof(double) == sizeof(u_int64_t);
    }
        
    // 32 bit systems
    static bool is32bit() 
    {
        return isIEEE() && sizeof(uintptr_t) == sizeof(u_int32_t);
    }

    // 64 bit systems
    static bool is64bit()
    {
        return isIEEE() && sizeof(uintptr_t) == sizeof(u_int64_t);
    }

    static uintptr_t NanAsBits()
    {
        const u_int32_t NaN32AsBits = 0x7fc00000;
        const u_int64_t NaN64AsBits = 0x7ff80000ULL << 32;

        if (JSImmediate::is32bit())
            return NaN32AsBits;
        else if (JSImmediate::is64bit())
            return NaN64AsBits;
        else
            abort();
    }

    static uintptr_t zeroAsBits()
    {
        return 0x0;
    }

    static uintptr_t oneAsBits()
    {
        const u_int32_t One32AsBits = 0x3f800000;
        const u_int64_t One64AsBits = 0x3ff00000ULL << 32;

        if (JSImmediate::is32bit())
            return One32AsBits;
        else if (JSImmediate::is64bit())
            return One64AsBits;
        else
            abort();
    }
};

} // namespace KJS

#endif
