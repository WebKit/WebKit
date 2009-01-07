/*
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef JSImmediate_h
#define JSImmediate_h

#include <wtf/Assertions.h>
#include <wtf/AlwaysInline.h>
#include <wtf/MathExtras.h>
#include "JSValue.h"
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
        static ALWAYS_INLINE bool isImmediate(JSValuePtr v)
        {
            return rawValue(v) & TagMask;
        }
        
        static ALWAYS_INLINE bool isNumber(JSValuePtr v)
        {
            return rawValue(v) & TagBitTypeInteger;
        }

        static ALWAYS_INLINE bool isPositiveNumber(JSValuePtr v)
        {
            // A single mask to check for the sign bit and the number tag all at once.
            return (rawValue(v) & (signBit | TagBitTypeInteger)) == TagBitTypeInteger;
        }
        
        static ALWAYS_INLINE bool isBoolean(JSValuePtr v)
        {
            return (rawValue(v) & FullTagTypeMask) == FullTagTypeBool;
        }
        
        static ALWAYS_INLINE bool isUndefinedOrNull(JSValuePtr v)
        {
            // Undefined and null share the same value, bar the 'undefined' bit in the extended tag.
            return (rawValue(v) & ~ExtendedTagBitUndefined) == FullTagTypeNull;
        }

        static bool isNegative(JSValuePtr v)
        {
            ASSERT(isNumber(v));
            return rawValue(v) & signBit;
        }

        static JSValuePtr from(char);
        static JSValuePtr from(signed char);
        static JSValuePtr from(unsigned char);
        static JSValuePtr from(short);
        static JSValuePtr from(unsigned short);
        static JSValuePtr from(int);
        static JSValuePtr from(unsigned);
        static JSValuePtr from(long);
        static JSValuePtr from(unsigned long);
        static JSValuePtr from(long long);
        static JSValuePtr from(unsigned long long);
        static JSValuePtr from(double);

        static ALWAYS_INLINE bool isEitherImmediate(JSValuePtr v1, JSValuePtr v2)
        {
            return (rawValue(v1) | rawValue(v2)) & TagMask;
        }

        static ALWAYS_INLINE bool isAnyImmediate(JSValuePtr v1, JSValuePtr v2, JSValuePtr v3)
        {
            return (rawValue(v1) | rawValue(v2) | rawValue(v3)) & TagMask;
        }

        static ALWAYS_INLINE bool areBothImmediate(JSValuePtr v1, JSValuePtr v2)
        {
            return isImmediate(v1) & isImmediate(v2);
        }

        static ALWAYS_INLINE bool areBothImmediateNumbers(JSValuePtr v1, JSValuePtr v2)
        {
            return rawValue(v1) & rawValue(v2) & TagBitTypeInteger;
        }

        static ALWAYS_INLINE JSValuePtr andImmediateNumbers(JSValuePtr v1, JSValuePtr v2)
        {
            ASSERT(areBothImmediateNumbers(v1, v2));
            return makeValue(rawValue(v1) & rawValue(v2));
        }

        static ALWAYS_INLINE JSValuePtr xorImmediateNumbers(JSValuePtr v1, JSValuePtr v2)
        {
            ASSERT(areBothImmediateNumbers(v1, v2));
            return makeValue((rawValue(v1) ^ rawValue(v2)) | TagBitTypeInteger);
        }

        static ALWAYS_INLINE JSValuePtr orImmediateNumbers(JSValuePtr v1, JSValuePtr v2)
        {
            ASSERT(areBothImmediateNumbers(v1, v2));
            return makeValue(rawValue(v1) | rawValue(v2));
        }

        static ALWAYS_INLINE JSValuePtr rightShiftImmediateNumbers(JSValuePtr val, JSValuePtr shift)
        {
            ASSERT(areBothImmediateNumbers(val, shift));
            return makeValue((rawValue(val) >> ((rawValue(shift) >> IntegerPayloadShift) & 0x1f)) | TagBitTypeInteger);
        }

        static ALWAYS_INLINE bool canDoFastAdditiveOperations(JSValuePtr v)
        {
            // Number is non-negative and an operation involving two of these can't overflow.
            // Checking for allowed negative numbers takes more time than it's worth on SunSpider.
            return (rawValue(v) & (TagBitTypeInteger + (signBit | (signBit >> 1)))) == TagBitTypeInteger;
        }

        static ALWAYS_INLINE JSValuePtr addImmediateNumbers(JSValuePtr v1, JSValuePtr v2)
        {
            ASSERT(canDoFastAdditiveOperations(v1));
            ASSERT(canDoFastAdditiveOperations(v2));
            return makeValue(rawValue(v1) + rawValue(v2) - TagBitTypeInteger);
        }

        static ALWAYS_INLINE JSValuePtr subImmediateNumbers(JSValuePtr v1, JSValuePtr v2)
        {
            ASSERT(canDoFastAdditiveOperations(v1));
            ASSERT(canDoFastAdditiveOperations(v2));
            return makeValue(rawValue(v1) - rawValue(v2) + TagBitTypeInteger);
        }

        static ALWAYS_INLINE JSValuePtr incImmediateNumber(JSValuePtr v)
        {
            ASSERT(canDoFastAdditiveOperations(v));
            return makeValue(rawValue(v) + (1 << IntegerPayloadShift));
        }

        static ALWAYS_INLINE JSValuePtr decImmediateNumber(JSValuePtr v)
        {
            ASSERT(canDoFastAdditiveOperations(v));
            return makeValue(rawValue(v) - (1 << IntegerPayloadShift));
        }

        static double toDouble(JSValuePtr);
        static bool toBoolean(JSValuePtr);
        static JSObject* toObject(JSValuePtr, ExecState*);
        static JSObject* toThisObject(JSValuePtr, ExecState*);
        static UString toString(JSValuePtr);

        static bool getUInt32(JSValuePtr, uint32_t&);
        static bool getTruncatedInt32(JSValuePtr, int32_t&);
        static bool getTruncatedUInt32(JSValuePtr, uint32_t&);

        static int32_t getTruncatedInt32(JSValuePtr);
        static uint32_t getTruncatedUInt32(JSValuePtr);

        static JSValuePtr trueImmediate();
        static JSValuePtr falseImmediate();
        static JSValuePtr undefinedImmediate();
        static JSValuePtr nullImmediate();
        static JSValuePtr zeroImmediate();
        static JSValuePtr oneImmediate();

        static JSValuePtr impossibleValue();
        
        static JSObject* prototype(JSValuePtr, ExecState*);

    private:
#if USE(ALTERNATE_JSIMMEDIATE)
        static const int minImmediateInt = ((-INT_MAX) - 1);
        static const int maxImmediateInt = INT_MAX;
#else
        static const int minImmediateInt = ((-INT_MAX) - 1) >> IntegerPayloadShift;
        static const int maxImmediateInt = INT_MAX >> IntegerPayloadShift;
#endif
        static const unsigned maxImmediateUInt = maxImmediateInt;

        static ALWAYS_INLINE JSValuePtr makeValue(intptr_t integer)
        {
            return JSValuePtr::makeImmediate(integer);
        }

        static ALWAYS_INLINE JSValuePtr makeInt(int32_t value)
        {
            return makeValue((static_cast<intptr_t>(value) << IntegerPayloadShift) | TagBitTypeInteger);
        }
        
        static ALWAYS_INLINE JSValuePtr makeBool(bool b)
        {
            return makeValue((static_cast<intptr_t>(b) << ExtendedPayloadShift) | FullTagTypeBool);
        }
        
        static ALWAYS_INLINE JSValuePtr makeUndefined()
        {
            return makeValue(FullTagTypeUndefined);
        }
        
        static ALWAYS_INLINE JSValuePtr makeNull()
        {
            return makeValue(FullTagTypeNull);
        }
        
        static ALWAYS_INLINE int32_t intValue(JSValuePtr v)
        {
            return static_cast<int32_t>(rawValue(v) >> IntegerPayloadShift);
        }
        
        static ALWAYS_INLINE uint32_t uintValue(JSValuePtr v)
        {
            return static_cast<uint32_t>(rawValue(v) >> IntegerPayloadShift);
        }
        
        static ALWAYS_INLINE bool boolValue(JSValuePtr v)
        {
            return rawValue(v) & ExtendedPayloadBitBoolValue;
        }
        
        static ALWAYS_INLINE intptr_t rawValue(JSValuePtr v)
        {
            return v.immediateValue();
        }

        static double nonInlineNaN();
    };

    ALWAYS_INLINE JSValuePtr JSImmediate::trueImmediate() { return makeBool(true); }
    ALWAYS_INLINE JSValuePtr JSImmediate::falseImmediate() { return makeBool(false); }
    ALWAYS_INLINE JSValuePtr JSImmediate::undefinedImmediate() { return makeUndefined(); }
    ALWAYS_INLINE JSValuePtr JSImmediate::nullImmediate() { return makeNull(); }
    ALWAYS_INLINE JSValuePtr JSImmediate::zeroImmediate() { return makeInt(0); }
    ALWAYS_INLINE JSValuePtr JSImmediate::oneImmediate() { return makeInt(1); }

    // This value is impossible because 0x4 is not a valid pointer but a tag of 0 would indicate non-immediate
    ALWAYS_INLINE JSValuePtr JSImmediate::impossibleValue() { return makeValue(0x4); }

    ALWAYS_INLINE bool JSImmediate::toBoolean(JSValuePtr v)
    {
        ASSERT(isImmediate(v));
        intptr_t bits = rawValue(v);
        return (bits & TagBitTypeInteger)
            ? bits != TagBitTypeInteger // !0 ints
            : bits == (FullTagTypeBool | ExtendedPayloadBitBoolValue); // bool true
    }

    ALWAYS_INLINE uint32_t JSImmediate::getTruncatedUInt32(JSValuePtr v)
    {
        ASSERT(isNumber(v));
        return intValue(v);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(char i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(signed char i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(unsigned char i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(short i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(unsigned short i)
    {
        return makeInt(i);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(int i)
    {
        if ((i < minImmediateInt) | (i > maxImmediateInt))
            return noValue();
        return makeInt(i);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(unsigned i)
    {
        if (i > maxImmediateUInt)
            return noValue();
        return makeInt(i);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(long i)
    {
        if ((i < minImmediateInt) | (i > maxImmediateInt))
            return noValue();
        return makeInt(i);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(unsigned long i)
    {
        if (i > maxImmediateUInt)
            return noValue();
        return makeInt(i);
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(long long i)
    {
        if ((i < minImmediateInt) | (i > maxImmediateInt))
            return noValue();
        return makeInt(static_cast<intptr_t>(i));
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(unsigned long long i)
    {
        if (i > maxImmediateUInt)
            return noValue();
        return makeInt(static_cast<intptr_t>(i));
    }

    ALWAYS_INLINE JSValuePtr JSImmediate::from(double d)
    {
        const int intVal = static_cast<int>(d);

        if ((intVal < minImmediateInt) | (intVal > maxImmediateInt))
            return noValue();

        // Check for data loss from conversion to int.
        if (intVal != d || (!intVal && signbit(d)))
            return noValue();

        return makeInt(intVal);
    }

    ALWAYS_INLINE int32_t JSImmediate::getTruncatedInt32(JSValuePtr v)
    {
        ASSERT(isNumber(v));
        return intValue(v);
    }

    ALWAYS_INLINE double JSImmediate::toDouble(JSValuePtr v)
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

    ALWAYS_INLINE bool JSImmediate::getUInt32(JSValuePtr v, uint32_t& i)
    {
        i = uintValue(v);
        return isPositiveNumber(v);
    }

    ALWAYS_INLINE bool JSImmediate::getTruncatedInt32(JSValuePtr v, int32_t& i)
    {
        i = intValue(v);
        return isNumber(v);
    }

    ALWAYS_INLINE bool JSImmediate::getTruncatedUInt32(JSValuePtr v, uint32_t& i)
    {
        return getUInt32(v, i);
    }

    inline JSValuePtr jsNull()
    {
        return JSImmediate::nullImmediate();
    }

    inline JSValuePtr jsBoolean(bool b)
    {
        return b ? JSImmediate::trueImmediate() : JSImmediate::falseImmediate();
    }

    inline JSValuePtr jsUndefined()
    {
        return JSImmediate::undefinedImmediate();
    }

    // These are identical logic to the JSValue functions above, and faster than jsNumber(number)->toInt32().
    int32_t toInt32(double);
    uint32_t toUInt32(double);
    int32_t toInt32SlowCase(double, bool& ok);
    uint32_t toUInt32SlowCase(double, bool& ok);

    inline bool JSValue::isUndefined() const
    {
        return asValue() == jsUndefined();
    }

    inline bool JSValue::isNull() const
    {
        return asValue() == jsNull();
    }

    inline bool JSValue::isUndefinedOrNull() const
    {
        return JSImmediate::isUndefinedOrNull(asValue());
    }

    inline bool JSValue::isBoolean() const
    {
        return JSImmediate::isBoolean(asValue());
    }

    inline bool JSValue::getBoolean(bool& v) const
    {
        if (JSImmediate::isBoolean(asValue())) {
            v = JSImmediate::toBoolean(asValue());
            return true;
        }
        
        return false;
    }

    inline bool JSValue::getBoolean() const
    {
        return asValue() == jsBoolean(true);
    }

    ALWAYS_INLINE int32_t JSValue::toInt32(ExecState* exec) const
    {
        int32_t i;
        if (getTruncatedInt32(i))
            return i;
        bool ok;
        return toInt32SlowCase(exec, ok);
    }

    inline uint32_t JSValue::toUInt32(ExecState* exec) const
    {
        uint32_t i;
        if (getTruncatedUInt32(i))
            return i;
        bool ok;
        return toUInt32SlowCase(exec, ok);
    }

    inline int32_t toInt32(double val)
    {
        if (!(val >= -2147483648.0 && val < 2147483648.0)) {
            bool ignored;
            return toInt32SlowCase(val, ignored);
        }
        return static_cast<int32_t>(val);
    }

    inline uint32_t toUInt32(double val)
    {
        if (!(val >= 0.0 && val < 4294967296.0)) {
            bool ignored;
            return toUInt32SlowCase(val, ignored);
        }
        return static_cast<uint32_t>(val);
    }

    inline int32_t JSValue::toInt32(ExecState* exec, bool& ok) const
    {
        int32_t i;
        if (getTruncatedInt32(i)) {
            ok = true;
            return i;
        }
        return toInt32SlowCase(exec, ok);
    }

    inline uint32_t JSValue::toUInt32(ExecState* exec, bool& ok) const
    {
        uint32_t i;
        if (getTruncatedUInt32(i)) {
            ok = true;
            return i;
        }
        return toUInt32SlowCase(exec, ok);
    }

} // namespace JSC

#endif // JSImmediate_h
