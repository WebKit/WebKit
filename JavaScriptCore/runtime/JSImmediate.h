/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>
#include <wtf/AlwaysInline.h>
#include <wtf/MathExtras.h>
#include <limits>
#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>

namespace JSC {

    class ExecState;
    class JSCell;
    class JSObject;
    class JSValue;
    class UString;

    inline JSValue* noValue() { return 0; }
    inline void* asPointer(JSValue* value) { return value; }

    /*
     * A JSValue* is either a pointer to a cell (a heap-allocated object) or an immediate (a type-tagged 
     * value masquerading as a pointer). The low two bits in a JSValue* are available for type tagging
     * because allocator alignment guarantees they will be 00 in cell pointers.
     *
     * For example, on a 32 bit system:
     *
     * JSCell*:             XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX                     00
     *                      [ high 30 bits: pointer address ]  [ low 2 bits -- always 0 ]
     * JSImmediate:         XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX                     TT
     *                      [ high 30 bits: 'payload' ]             [ low 2 bits -- tag ]
     *
     * Where the bottom two bits are non-zero they either indicate that the immediate is a 31 bit signed
     * integer, or they mark the value as being an immediate of a type other than integer, with a secondary
     * tag used to indicate the exact type.
     *
     * Where the lowest bit is set (TT is equal to 01 or 11) the high 31 bits form a 31 bit signed int value.
     * Where TT is equal to 10 this indicates this is a type of immediate other than an integer, and the next
     * two bits will form an extended tag.
     *
     * 31 bit signed int:   XXXXXXXXXXXXXXXXXXXXXXXXXXXXXX                     X1
     *                      [ high 30 bits of the value ]      [ high bit part of value ]
     * Other:               YYYYYYYYYYYYYYYYYYYYYYYYYYYY      ZZ               10
     *                      [ extended 'payload' ]  [  extended tag  ]  [  tag 'other'  ]
     *
     * Where the first bit of the extended tag is set this flags the value as being a boolean, and the following
     * bit would flag the value as undefined.  If neither bits are set, the value is null.
     *
     * Other:               YYYYYYYYYYYYYYYYYYYYYYYYYYYY      UB               10
     *                      [ extended 'payload' ]  [ undefined | bool ]  [ tag 'other' ]
     *
     * For boolean value the lowest bit in the payload holds the value of the bool, all remaining bits are zero.
     * For undefined or null immediates the payload is zero.
     *
     * Boolean:             000000000000000000000000000V      01               10
     *                      [ boolean value ]              [ bool ]       [ tag 'other' ]
     * Undefined:           0000000000000000000000000000      10               10
     *                      [ zero ]                    [ undefined ]     [ tag 'other' ]
     * Null:                0000000000000000000000000000      00               10
     *                      [ zero ]                       [ zero ]       [ tag 'other' ]
     */

    class JSImmediate {
    private:
        friend class JIT;
    
        static const int32_t TagMask           = 0x3; // primary tag is 2 bits long
        static const int32_t TagBitTypeInteger = 0x1; // bottom bit set indicates integer, this dominates the following bit
        static const int32_t TagBitTypeOther   = 0x2; // second bit set indicates immediate other than an integer

        static const int32_t ExtendedTagMask         = 0xC; // extended tag holds a further two bits
        static const int32_t ExtendedTagBitBool      = 0x4;
        static const int32_t ExtendedTagBitUndefined = 0x8;

        static const int32_t FullTagTypeMask      = TagMask | ExtendedTagMask;
        static const int32_t FullTagTypeBool      = TagBitTypeOther | ExtendedTagBitBool;
        static const int32_t FullTagTypeUndefined = TagBitTypeOther | ExtendedTagBitUndefined;
        static const int32_t FullTagTypeNull      = TagBitTypeOther;

        static const int32_t IntegerPayloadShift  = 1;
        static const int32_t ExtendedPayloadShift = 4;

        static const int32_t ExtendedPayloadBitBoolValue = 1 << ExtendedPayloadShift;

#if USE(ALTERNATE_JSIMMEDIATE)
        static const intptr_t signBit = 0x100000000ll;
#else
        static const int32_t signBit = 0x80000000;
#endif
 
    public:
        static ALWAYS_INLINE bool isImmediate(JSValue* v)
        {
            return rawValue(v) & TagMask;
        }
        
        static ALWAYS_INLINE bool isNumber(JSValue* v)
        {
            return rawValue(v) & TagBitTypeInteger;
        }

        static ALWAYS_INLINE bool isPositiveNumber(JSValue* v)
        {
            // A single mask to check for the sign bit and the number tag all at once.
            return (rawValue(v) & (signBit | TagBitTypeInteger)) == TagBitTypeInteger;
        }
        
        static ALWAYS_INLINE bool isBoolean(JSValue* v)
        {
            return (rawValue(v) & FullTagTypeMask) == FullTagTypeBool;
        }
        
        static ALWAYS_INLINE bool isUndefinedOrNull(JSValue* v)
        {
            // Undefined and null share the same value, bar the 'undefined' bit in the extended tag.
            return (rawValue(v) & ~ExtendedTagBitUndefined) == FullTagTypeNull;
        }

        static bool isNegative(JSValue* v)
        {
            ASSERT(isNumber(v));
            return rawValue(v) & signBit;
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

        static ALWAYS_INLINE bool isEitherImmediate(JSValue* v1, JSValue* v2)
        {
            return (rawValue(v1) | rawValue(v2)) & TagMask;
        }

        static ALWAYS_INLINE bool isAnyImmediate(JSValue* v1, JSValue* v2, JSValue* v3)
        {
            return (rawValue(v1) | rawValue(v2) | rawValue(v3)) & TagMask;
        }

        static ALWAYS_INLINE bool areBothImmediate(JSValue* v1, JSValue* v2)
        {
            return isImmediate(v1) & isImmediate(v2);
        }

        static ALWAYS_INLINE bool areBothImmediateNumbers(JSValue* v1, JSValue* v2)
        {
            return rawValue(v1) & rawValue(v2) & TagBitTypeInteger;
        }

        static ALWAYS_INLINE JSValue* andImmediateNumbers(JSValue* v1, JSValue* v2)
        {
            ASSERT(areBothImmediateNumbers(v1, v2));
            return makeValue(rawValue(v1) & rawValue(v2));
        }

        static ALWAYS_INLINE JSValue* xorImmediateNumbers(JSValue* v1, JSValue* v2)
        {
            ASSERT(areBothImmediateNumbers(v1, v2));
            return makeValue((rawValue(v1) ^ rawValue(v2)) | TagBitTypeInteger);
        }

        static ALWAYS_INLINE JSValue* orImmediateNumbers(JSValue* v1, JSValue* v2)
        {
            ASSERT(areBothImmediateNumbers(v1, v2));
            return makeValue(rawValue(v1) | rawValue(v2));
        }

        static ALWAYS_INLINE JSValue* rightShiftImmediateNumbers(JSValue* val, JSValue* shift)
        {
            ASSERT(areBothImmediateNumbers(val, shift));
            return makeValue((rawValue(val) >> ((rawValue(shift) >> IntegerPayloadShift) & 0x1f)) | TagBitTypeInteger);
        }

        static ALWAYS_INLINE bool canDoFastAdditiveOperations(JSValue* v)
        {
            // Number is non-negative and an operation involving two of these can't overflow.
            // Checking for allowed negative numbers takes more time than it's worth on SunSpider.
            return (rawValue(v) & (TagBitTypeInteger + (signBit | (signBit >> 1)))) == TagBitTypeInteger;
        }

        static ALWAYS_INLINE JSValue* addImmediateNumbers(JSValue* v1, JSValue* v2)
        {
            ASSERT(canDoFastAdditiveOperations(v1));
            ASSERT(canDoFastAdditiveOperations(v2));
            return makeValue(rawValue(v1) + rawValue(v2) - TagBitTypeInteger);
        }

        static ALWAYS_INLINE JSValue* subImmediateNumbers(JSValue* v1, JSValue* v2)
        {
            ASSERT(canDoFastAdditiveOperations(v1));
            ASSERT(canDoFastAdditiveOperations(v2));
            return makeValue(rawValue(v1) - rawValue(v2) + TagBitTypeInteger);
        }

        static ALWAYS_INLINE JSValue* incImmediateNumber(JSValue* v)
        {
            ASSERT(canDoFastAdditiveOperations(v));
            return makeValue(rawValue(v) + (1 << IntegerPayloadShift));
        }

        static ALWAYS_INLINE JSValue* decImmediateNumber(JSValue* v)
        {
            ASSERT(canDoFastAdditiveOperations(v));
            return makeValue(rawValue(v) - (1 << IntegerPayloadShift));
        }

        static double toDouble(JSValue*);
        static bool toBoolean(JSValue*);
        static JSObject* toObject(JSValue*, ExecState*);
        static JSObject* toThisObject(JSValue*, ExecState*);
        static UString toString(JSValue*);

        static bool getUInt32(JSValue*, uint32_t&);
        static bool getTruncatedInt32(JSValue*, int32_t&);
        static bool getTruncatedUInt32(JSValue*, uint32_t&);

        static int32_t getTruncatedInt32(JSValue*);
        static uint32_t getTruncatedUInt32(JSValue*);

        static JSValue* trueImmediate();
        static JSValue* falseImmediate();
        static JSValue* undefinedImmediate();
        static JSValue* nullImmediate();
        static JSValue* zeroImmediate();
        static JSValue* oneImmediate();

        static JSValue* impossibleValue();
        
        static JSObject* prototype(JSValue*, ExecState*);

    private:
#if USE(ALTERNATE_JSIMMEDIATE)
        static const int minImmediateInt = ((-INT_MAX) - 1);
        static const int maxImmediateInt = INT_MAX;
#else
        static const int minImmediateInt = ((-INT_MAX) - 1) >> IntegerPayloadShift;
        static const int maxImmediateInt = INT_MAX >> IntegerPayloadShift;
#endif
        static const unsigned maxImmediateUInt = maxImmediateInt;

        static ALWAYS_INLINE JSValue* makeValue(intptr_t integer)
        {
            return reinterpret_cast<JSValue*>(integer);
        }

        static ALWAYS_INLINE JSValue* makeInt(int32_t value)
        {
            return makeValue((static_cast<intptr_t>(value) << IntegerPayloadShift) | TagBitTypeInteger);
        }
        
        static ALWAYS_INLINE JSValue* makeBool(bool b)
        {
            return makeValue((static_cast<intptr_t>(b) << ExtendedPayloadShift) | FullTagTypeBool);
        }
        
        static ALWAYS_INLINE JSValue* makeUndefined()
        {
            return makeValue(FullTagTypeUndefined);
        }
        
        static ALWAYS_INLINE JSValue* makeNull()
        {
            return makeValue(FullTagTypeNull);
        }
        
        static ALWAYS_INLINE int32_t intValue(JSValue* v)
        {
            return static_cast<int32_t>(rawValue(v) >> IntegerPayloadShift);
        }
        
        static ALWAYS_INLINE uint32_t uintValue(JSValue* v)
        {
            return static_cast<uint32_t>(rawValue(v) >> IntegerPayloadShift);
        }
        
        static ALWAYS_INLINE bool boolValue(JSValue* v)
        {
            return rawValue(v) & ExtendedPayloadBitBoolValue;
        }
        
        static ALWAYS_INLINE intptr_t rawValue(JSValue* v)
        {
            return reinterpret_cast<intptr_t>(v);
        }

        static double nonInlineNaN();
    };

    ALWAYS_INLINE JSValue* JSImmediate::trueImmediate() { return makeBool(true); }
    ALWAYS_INLINE JSValue* JSImmediate::falseImmediate() { return makeBool(false); }
    ALWAYS_INLINE JSValue* JSImmediate::undefinedImmediate() { return makeUndefined(); }
    ALWAYS_INLINE JSValue* JSImmediate::nullImmediate() { return makeNull(); }
    ALWAYS_INLINE JSValue* JSImmediate::zeroImmediate() { return makeInt(0); }
    ALWAYS_INLINE JSValue* JSImmediate::oneImmediate() { return makeInt(1); }

    // This value is impossible because 0x4 is not a valid pointer but a tag of 0 would indicate non-immediate
    ALWAYS_INLINE JSValue* JSImmediate::impossibleValue() { return makeValue(0x4); }

    ALWAYS_INLINE bool JSImmediate::toBoolean(JSValue* v)
    {
        ASSERT(isImmediate(v));
        intptr_t bits = rawValue(v);
        return (bits & TagBitTypeInteger)
            ? bits != TagBitTypeInteger // !0 ints
            : bits == (FullTagTypeBool | ExtendedPayloadBitBoolValue); // bool true
    }

    ALWAYS_INLINE uint32_t JSImmediate::getTruncatedUInt32(JSValue* v)
    {
        ASSERT(isNumber(v));
        return intValue(v);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(char i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(signed char i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(unsigned char i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(short i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(unsigned short i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(int i)
    {
        if ((i < minImmediateInt) | (i > maxImmediateInt))
            return noValue();
        return makeInt(i);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(unsigned i)
    {
        if (i > maxImmediateUInt)
            return noValue();
        return makeInt(i);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(long i)
    {
        if ((i < minImmediateInt) | (i > maxImmediateInt))
            return noValue();
        return makeInt(i);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(unsigned long i)
    {
        if (i > maxImmediateUInt)
            return noValue();
        return makeInt(i);
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(long long i)
    {
        if ((i < minImmediateInt) | (i > maxImmediateInt))
            return noValue();
        return makeInt(static_cast<intptr_t>(i));
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(unsigned long long i)
    {
        if (i > maxImmediateUInt)
            return noValue();
        return makeInt(static_cast<intptr_t>(i));
    }

    ALWAYS_INLINE JSValue* JSImmediate::from(double d)
    {
        const int intVal = static_cast<int>(d);

        if ((intVal < minImmediateInt) | (intVal > maxImmediateInt))
            return noValue();

        // Check for data loss from conversion to int.
        if (intVal != d || (!intVal && signbit(d)))
            return noValue();

        return makeInt(intVal);
    }

    ALWAYS_INLINE int32_t JSImmediate::getTruncatedInt32(JSValue* v)
    {
        ASSERT(isNumber(v));
        return intValue(v);
    }

    ALWAYS_INLINE double JSImmediate::toDouble(JSValue* v)
    {
        ASSERT(isImmediate(v));
        int i;
        if (isNumber(v))
            i = intValue(v);
        else if (rawValue(v) == FullTagTypeUndefined)
            return nonInlineNaN();
        else
            i = rawValue(v) >> ExtendedPayloadShift;
        return i;
    }

    ALWAYS_INLINE bool JSImmediate::getUInt32(JSValue* v, uint32_t& i)
    {
        i = uintValue(v);
        return isPositiveNumber(v);
    }

    ALWAYS_INLINE bool JSImmediate::getTruncatedInt32(JSValue* v, int32_t& i)
    {
        i = intValue(v);
        return isNumber(v);
    }

    ALWAYS_INLINE bool JSImmediate::getTruncatedUInt32(JSValue* v, uint32_t& i)
    {
        return getUInt32(v, i);
    }

    ALWAYS_INLINE JSValue* jsUndefined()
    {
        return JSImmediate::undefinedImmediate();
    }

    inline JSValue* jsNull()
    {
        return JSImmediate::nullImmediate();
    }

    inline JSValue* jsBoolean(bool b)
    {
        return b ? JSImmediate::trueImmediate() : JSImmediate::falseImmediate();
    }

} // namespace JSC

#endif
