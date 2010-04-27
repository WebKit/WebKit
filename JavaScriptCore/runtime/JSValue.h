/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

#ifndef JSValue_h
#define JSValue_h

#include "CallData.h"
#include "ConstructData.h"
#include <math.h>
#include <stddef.h> // for size_t
#include <stdint.h>
#include <wtf/AlwaysInline.h>
#include <wtf/Assertions.h>
#include <wtf/HashTraits.h>
#include <wtf/MathExtras.h>

namespace JSC {

    class Identifier;
    class JSCell;
    class JSGlobalData;
    class JSImmediate;
    class JSObject;
    class JSString;
    class PropertySlot;
    class PutPropertySlot;
    class UString;

    struct ClassInfo;
    struct Instruction;

    enum PreferredPrimitiveType { NoPreference, PreferNumber, PreferString };

#if USE(JSVALUE32_64)
    typedef int64_t EncodedJSValue;
#else
    typedef void* EncodedJSValue;
#endif

    double nonInlineNaN();
    int32_t toInt32SlowCase(double, bool& ok);
    uint32_t toUInt32SlowCase(double, bool& ok);

    class JSValue {
        friend class JSImmediate;
        friend struct EncodedJSValueHashTraits;
        friend class JIT;
        friend class JITStubs;
        friend class JITStubCall;
        friend class JSInterfaceJIT;
        friend class SpecializedThunkJIT;

    public:
        static EncodedJSValue encode(JSValue value);
        static JSValue decode(EncodedJSValue ptr);
#if !USE(JSVALUE32_64)
    private:
        static JSValue makeImmediate(intptr_t value);
        intptr_t immediateValue();
    public:
#endif
        enum JSNullTag { JSNull };
        enum JSUndefinedTag { JSUndefined };
        enum JSTrueTag { JSTrue };
        enum JSFalseTag { JSFalse };
        enum EncodeAsDoubleTag { EncodeAsDouble };

        JSValue();
        JSValue(JSNullTag);
        JSValue(JSUndefinedTag);
        JSValue(JSTrueTag);
        JSValue(JSFalseTag);
        JSValue(JSCell* ptr);
        JSValue(const JSCell* ptr);

        // Numbers
        JSValue(EncodeAsDoubleTag, ExecState*, double);
        JSValue(ExecState*, double);
        JSValue(ExecState*, char);
        JSValue(ExecState*, unsigned char);
        JSValue(ExecState*, short);
        JSValue(ExecState*, unsigned short);
        JSValue(ExecState*, int);
        JSValue(ExecState*, unsigned);
        JSValue(ExecState*, long);
        JSValue(ExecState*, unsigned long);
        JSValue(ExecState*, long long);
        JSValue(ExecState*, unsigned long long);
        JSValue(JSGlobalData*, double);
        JSValue(JSGlobalData*, int);
        JSValue(JSGlobalData*, unsigned);

        operator bool() const;
        bool operator==(const JSValue& other) const;
        bool operator!=(const JSValue& other) const;

        bool isInt32() const;
        bool isUInt32() const;
        bool isDouble() const;
        bool isTrue() const;
        bool isFalse() const;

        int32_t asInt32() const;
        uint32_t asUInt32() const;
        double asDouble() const;

        // Querying the type.
        bool isUndefined() const;
        bool isNull() const;
        bool isUndefinedOrNull() const;
        bool isBoolean() const;
        bool isNumber() const;
        bool isString() const;
        bool isGetterSetter() const;
        bool isObject() const;
        bool inherits(const ClassInfo*) const;
        
        // Extracting the value.
        bool getBoolean(bool&) const;
        bool getBoolean() const; // false if not a boolean
        bool getNumber(double&) const;
        double uncheckedGetNumber() const;
        bool getString(ExecState* exec, UString&) const;
        UString getString(ExecState* exec) const; // null string if not a string
        JSObject* getObject() const; // 0 if not an object

        CallType getCallData(CallData&);
        ConstructType getConstructData(ConstructData&);

        // Extracting integer values.
        bool getUInt32(uint32_t&) const;
        
        // Basic conversions.
        JSValue toPrimitive(ExecState*, PreferredPrimitiveType = NoPreference) const;
        bool getPrimitiveNumber(ExecState*, double& number, JSValue&);

        bool toBoolean(ExecState*) const;

        // toNumber conversion is expected to be side effect free if an exception has
        // been set in the ExecState already.
        double toNumber(ExecState*) const;
        JSValue toJSNumber(ExecState*) const; // Fast path for when you expect that the value is an immediate number.
        UString toString(ExecState*) const;
        UString toPrimitiveString(ExecState*) const;
        JSObject* toObject(ExecState*) const;

        // Integer conversions.
        double toInteger(ExecState*) const;
        double toIntegerPreserveNaN(ExecState*) const;
        int32_t toInt32(ExecState*) const;
        int32_t toInt32(ExecState*, bool& ok) const;
        uint32_t toUInt32(ExecState*) const;
        uint32_t toUInt32(ExecState*, bool& ok) const;

#if ENABLE(JSC_ZOMBIES)
        bool isZombie() const;
#endif

        // Floating point conversions (this is a convenience method for webcore;
        // signle precision float is not a representation used in JS or JSC).
        float toFloat(ExecState* exec) const { return static_cast<float>(toNumber(exec)); }

        // Object operations, with the toObject operation included.
        JSValue get(ExecState*, const Identifier& propertyName) const;
        JSValue get(ExecState*, const Identifier& propertyName, PropertySlot&) const;
        JSValue get(ExecState*, unsigned propertyName) const;
        JSValue get(ExecState*, unsigned propertyName, PropertySlot&) const;
        void put(ExecState*, const Identifier& propertyName, JSValue, PutPropertySlot&);
        void put(ExecState*, unsigned propertyName, JSValue);

        bool needsThisConversion() const;
        JSObject* toThisObject(ExecState*) const;
        UString toThisString(ExecState*) const;
        JSString* toThisJSString(ExecState*) const;

        static bool equal(ExecState* exec, JSValue v1, JSValue v2);
        static bool equalSlowCase(ExecState* exec, JSValue v1, JSValue v2);
        static bool equalSlowCaseInline(ExecState* exec, JSValue v1, JSValue v2);
        static bool strictEqual(ExecState* exec, JSValue v1, JSValue v2);
        static bool strictEqualSlowCase(ExecState* exec, JSValue v1, JSValue v2);
        static bool strictEqualSlowCaseInline(ExecState* exec, JSValue v1, JSValue v2);

        JSValue getJSNumber(); // JSValue() if this is not a JSNumber or number object

        bool isCell() const;
        JSCell* asCell() const;

#ifndef NDEBUG
        char* description();
#endif

    private:
        enum HashTableDeletedValueTag { HashTableDeletedValue };
        JSValue(HashTableDeletedValueTag);

        inline const JSValue asValue() const { return *this; }
        JSObject* toObjectSlowCase(ExecState*) const;
        JSObject* toThisObjectSlowCase(ExecState*) const;

        JSObject* synthesizePrototype(ExecState*) const;
        JSObject* synthesizeObject(ExecState*) const;

#if USE(JSVALUE32_64)
        enum { Int32Tag =        0xffffffff };
        enum { CellTag =         0xfffffffe };
        enum { TrueTag =         0xfffffffd };
        enum { FalseTag =        0xfffffffc };
        enum { NullTag =         0xfffffffb };
        enum { UndefinedTag =    0xfffffffa };
        enum { EmptyValueTag =   0xfffffff9 };
        enum { DeletedValueTag = 0xfffffff8 };
        
        enum { LowestTag =  DeletedValueTag };
        
        uint32_t tag() const;
        int32_t payload() const;

        union {
            EncodedJSValue asEncodedJSValue;
            double asDouble;
#if CPU(BIG_ENDIAN)
            struct {
                int32_t tag;
                int32_t payload;
            } asBits;
#else
            struct {
                int32_t payload;
                int32_t tag;
            } asBits;
#endif
        } u;
#else // USE(JSVALUE32_64)
        JSCell* m_ptr;
#endif // USE(JSVALUE32_64)
    };

#if USE(JSVALUE32_64)
    typedef IntHash<EncodedJSValue> EncodedJSValueHash;

    struct EncodedJSValueHashTraits : HashTraits<EncodedJSValue> {
        static const bool emptyValueIsZero = false;
        static EncodedJSValue emptyValue() { return JSValue::encode(JSValue()); }
        static void constructDeletedValue(EncodedJSValue& slot) { slot = JSValue::encode(JSValue(JSValue::HashTableDeletedValue)); }
        static bool isDeletedValue(EncodedJSValue value) { return value == JSValue::encode(JSValue(JSValue::HashTableDeletedValue)); }
    };
#else
    typedef PtrHash<EncodedJSValue> EncodedJSValueHash;

    struct EncodedJSValueHashTraits : HashTraits<EncodedJSValue> {
        static void constructDeletedValue(EncodedJSValue& slot) { slot = JSValue::encode(JSValue(JSValue::HashTableDeletedValue)); }
        static bool isDeletedValue(EncodedJSValue value) { return value == JSValue::encode(JSValue(JSValue::HashTableDeletedValue)); }
    };
#endif

    // Stand-alone helper functions.
    inline JSValue jsNull()
    {
        return JSValue(JSValue::JSNull);
    }

    inline JSValue jsUndefined()
    {
        return JSValue(JSValue::JSUndefined);
    }

    inline JSValue jsBoolean(bool b)
    {
        return b ? JSValue(JSValue::JSTrue) : JSValue(JSValue::JSFalse);
    }

    ALWAYS_INLINE JSValue jsDoubleNumber(ExecState* exec, double d)
    {
        return JSValue(JSValue::EncodeAsDouble, exec, d);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, double d)
    {
        return JSValue(exec, d);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, char i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, unsigned char i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, short i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, unsigned short i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, int i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, unsigned i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, long i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, unsigned long i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, long long i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(ExecState* exec, unsigned long long i)
    {
        return JSValue(exec, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, double d)
    {
        return JSValue(globalData, d);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, int i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, unsigned i)
    {
        return JSValue(globalData, i);
    }

    inline bool operator==(const JSValue a, const JSCell* b) { return a == JSValue(b); }
    inline bool operator==(const JSCell* a, const JSValue b) { return JSValue(a) == b; }

    inline bool operator!=(const JSValue a, const JSCell* b) { return a != JSValue(b); }
    inline bool operator!=(const JSCell* a, const JSValue b) { return JSValue(a) != b; }

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

    // FIXME: We should deprecate this and just use JSValue::asCell() instead.
    JSCell* asCell(JSValue);

    inline JSCell* asCell(JSValue value)
    {
        return value.asCell();
    }

    ALWAYS_INLINE int32_t JSValue::toInt32(ExecState* exec) const
    {
        if (isInt32())
            return asInt32();
        bool ignored;
        return toInt32SlowCase(toNumber(exec), ignored);
    }

    inline uint32_t JSValue::toUInt32(ExecState* exec) const
    {
        if (isUInt32())
            return asInt32();
        bool ignored;
        return toUInt32SlowCase(toNumber(exec), ignored);
    }

    inline int32_t JSValue::toInt32(ExecState* exec, bool& ok) const
    {
        if (isInt32()) {
            ok = true;
            return asInt32();
        }
        return toInt32SlowCase(toNumber(exec), ok);
    }

    inline uint32_t JSValue::toUInt32(ExecState* exec, bool& ok) const
    {
        if (isUInt32()) {
            ok = true;
            return asInt32();
        }
        return toUInt32SlowCase(toNumber(exec), ok);
    }

#if USE(JSVALUE32_64)
    inline JSValue jsNaN(ExecState* exec)
    {
        return JSValue(exec, nonInlineNaN());
    }

    // JSValue member functions.
    inline EncodedJSValue JSValue::encode(JSValue value)
    {
        return value.u.asEncodedJSValue;
    }

    inline JSValue JSValue::decode(EncodedJSValue encodedJSValue)
    {
        JSValue v;
        v.u.asEncodedJSValue = encodedJSValue;
#if ENABLE(JSC_ZOMBIES)
        ASSERT(!v.isZombie());
#endif
        return v;
    }

    inline JSValue::JSValue()
    {
        u.asBits.tag = EmptyValueTag;
        u.asBits.payload = 0;
    }

    inline JSValue::JSValue(JSNullTag)
    {
        u.asBits.tag = NullTag;
        u.asBits.payload = 0;
    }
    
    inline JSValue::JSValue(JSUndefinedTag)
    {
        u.asBits.tag = UndefinedTag;
        u.asBits.payload = 0;
    }
    
    inline JSValue::JSValue(JSTrueTag)
    {
        u.asBits.tag = TrueTag;
        u.asBits.payload = 0;
    }
    
    inline JSValue::JSValue(JSFalseTag)
    {
        u.asBits.tag = FalseTag;
        u.asBits.payload = 0;
    }

    inline JSValue::JSValue(HashTableDeletedValueTag)
    {
        u.asBits.tag = DeletedValueTag;
        u.asBits.payload = 0;
    }

    inline JSValue::JSValue(JSCell* ptr)
    {
        if (ptr)
            u.asBits.tag = CellTag;
        else
            u.asBits.tag = EmptyValueTag;
        u.asBits.payload = reinterpret_cast<int32_t>(ptr);
#if ENABLE(JSC_ZOMBIES)
        ASSERT(!isZombie());
#endif
    }

    inline JSValue::JSValue(const JSCell* ptr)
    {
        if (ptr)
            u.asBits.tag = CellTag;
        else
            u.asBits.tag = EmptyValueTag;
        u.asBits.payload = reinterpret_cast<int32_t>(const_cast<JSCell*>(ptr));
#if ENABLE(JSC_ZOMBIES)
        ASSERT(!isZombie());
#endif
    }

    inline JSValue::operator bool() const
    {
        ASSERT(tag() != DeletedValueTag);
        return tag() != EmptyValueTag;
    }

    inline bool JSValue::operator==(const JSValue& other) const
    {
        return u.asEncodedJSValue == other.u.asEncodedJSValue;
    }

    inline bool JSValue::operator!=(const JSValue& other) const
    {
        return u.asEncodedJSValue != other.u.asEncodedJSValue;
    }

    inline bool JSValue::isUndefined() const
    {
        return tag() == UndefinedTag;
    }

    inline bool JSValue::isNull() const
    {
        return tag() == NullTag;
    }

    inline bool JSValue::isUndefinedOrNull() const
    {
        return isUndefined() || isNull();
    }

    inline bool JSValue::isCell() const
    {
        return tag() == CellTag;
    }

    inline bool JSValue::isInt32() const
    {
        return tag() == Int32Tag;
    }

    inline bool JSValue::isUInt32() const
    {
        return tag() == Int32Tag && asInt32() > -1;
    }

    inline bool JSValue::isDouble() const
    {
        return tag() < LowestTag;
    }

    inline bool JSValue::isTrue() const
    {
        return tag() == TrueTag;
    }

    inline bool JSValue::isFalse() const
    {
        return tag() == FalseTag;
    }

    inline uint32_t JSValue::tag() const
    {
        return u.asBits.tag;
    }
    
    inline int32_t JSValue::payload() const
    {
        return u.asBits.payload;
    }
    
    inline int32_t JSValue::asInt32() const
    {
        ASSERT(isInt32());
        return u.asBits.payload;
    }
    
    inline uint32_t JSValue::asUInt32() const
    {
        ASSERT(isUInt32());
        return u.asBits.payload;
    }
    
    inline double JSValue::asDouble() const
    {
        ASSERT(isDouble());
        return u.asDouble;
    }
    
    ALWAYS_INLINE JSCell* JSValue::asCell() const
    {
        ASSERT(isCell());
        return reinterpret_cast<JSCell*>(u.asBits.payload);
    }

    ALWAYS_INLINE JSValue::JSValue(EncodeAsDoubleTag, ExecState*, double d)
    {
        u.asDouble = d;
    }

    inline JSValue::JSValue(ExecState* exec, double d)
    {
        const int32_t asInt32 = static_cast<int32_t>(d);
        if (asInt32 != d || (!asInt32 && signbit(d))) { // true for -0.0
            u.asDouble = d;
            return;
        }
        *this = JSValue(exec, static_cast<int32_t>(d));
    }

    inline JSValue::JSValue(ExecState* exec, char i)
    {
        *this = JSValue(exec, static_cast<int32_t>(i));
    }

    inline JSValue::JSValue(ExecState* exec, unsigned char i)
    {
        *this = JSValue(exec, static_cast<int32_t>(i));
    }

    inline JSValue::JSValue(ExecState* exec, short i)
    {
        *this = JSValue(exec, static_cast<int32_t>(i));
    }

    inline JSValue::JSValue(ExecState* exec, unsigned short i)
    {
        *this = JSValue(exec, static_cast<int32_t>(i));
    }

    inline JSValue::JSValue(ExecState*, int i)
    {
        u.asBits.tag = Int32Tag;
        u.asBits.payload = i;
    }

    inline JSValue::JSValue(ExecState* exec, unsigned i)
    {
        if (static_cast<int32_t>(i) < 0) {
            *this = JSValue(exec, static_cast<double>(i));
            return;
        }
        *this = JSValue(exec, static_cast<int32_t>(i));
    }

    inline JSValue::JSValue(ExecState* exec, long i)
    {
        if (static_cast<int32_t>(i) != i) {
            *this = JSValue(exec, static_cast<double>(i));
            return;
        }
        *this = JSValue(exec, static_cast<int32_t>(i));
    }

    inline JSValue::JSValue(ExecState* exec, unsigned long i)
    {
        if (static_cast<uint32_t>(i) != i) {
            *this = JSValue(exec, static_cast<double>(i));
            return;
        }
        *this = JSValue(exec, static_cast<uint32_t>(i));
    }

    inline JSValue::JSValue(ExecState* exec, long long i)
    {
        if (static_cast<int32_t>(i) != i) {
            *this = JSValue(exec, static_cast<double>(i));
            return;
        }
        *this = JSValue(exec, static_cast<int32_t>(i));
    }

    inline JSValue::JSValue(ExecState* exec, unsigned long long i)
    {
        if (static_cast<uint32_t>(i) != i) {
            *this = JSValue(exec, static_cast<double>(i));
            return;
        }
        *this = JSValue(exec, static_cast<uint32_t>(i));
    }

    inline JSValue::JSValue(JSGlobalData* globalData, double d)
    {
        const int32_t asInt32 = static_cast<int32_t>(d);
        if (asInt32 != d || (!asInt32 && signbit(d))) { // true for -0.0
            u.asDouble = d;
            return;
        }
        *this = JSValue(globalData, static_cast<int32_t>(d));
    }
    
    inline JSValue::JSValue(JSGlobalData*, int i)
    {
        u.asBits.tag = Int32Tag;
        u.asBits.payload = i;
    }
    
    inline JSValue::JSValue(JSGlobalData* globalData, unsigned i)
    {
        if (static_cast<int32_t>(i) < 0) {
            *this = JSValue(globalData, static_cast<double>(i));
            return;
        }
        *this = JSValue(globalData, static_cast<int32_t>(i));
    }

    inline bool JSValue::isNumber() const
    {
        return isInt32() || isDouble();
    }

    inline bool JSValue::isBoolean() const
    {
        return isTrue() || isFalse();
    }

    inline bool JSValue::getBoolean(bool& v) const
    {
        if (isTrue()) {
            v = true;
            return true;
        }
        if (isFalse()) {
            v = false;
            return true;
        }
        
        return false;
    }

    inline bool JSValue::getBoolean() const
    {
        ASSERT(isBoolean());
        return tag() == TrueTag;
    }

    inline double JSValue::uncheckedGetNumber() const
    {
        ASSERT(isNumber());
        return isInt32() ? asInt32() : asDouble();
    }

    ALWAYS_INLINE JSValue JSValue::toJSNumber(ExecState* exec) const
    {
        return isNumber() ? asValue() : jsNumber(exec, this->toNumber(exec));
    }

    inline bool JSValue::getNumber(double& result) const
    {
        if (isInt32()) {
            result = asInt32();
            return true;
        }
        if (isDouble()) {
            result = asDouble();
            return true;
        }
        return false;
    }

#else // USE(JSVALUE32_64)

    // JSValue member functions.
    inline EncodedJSValue JSValue::encode(JSValue value)
    {
        return reinterpret_cast<EncodedJSValue>(value.m_ptr);
    }

    inline JSValue JSValue::decode(EncodedJSValue ptr)
    {
        return JSValue(reinterpret_cast<JSCell*>(ptr));
    }

    inline JSValue JSValue::makeImmediate(intptr_t value)
    {
        return JSValue(reinterpret_cast<JSCell*>(value));
    }

    inline intptr_t JSValue::immediateValue()
    {
        return reinterpret_cast<intptr_t>(m_ptr);
    }
    
    // 0x0 can never occur naturally because it has a tag of 00, indicating a pointer value, but a payload of 0x0, which is in the (invalid) zero page.
    inline JSValue::JSValue()
        : m_ptr(0)
    {
    }

    // 0x4 can never occur naturally because it has a tag of 00, indicating a pointer value, but a payload of 0x4, which is in the (invalid) zero page.
    inline JSValue::JSValue(HashTableDeletedValueTag)
        : m_ptr(reinterpret_cast<JSCell*>(0x4))
    {
    }

    inline JSValue::JSValue(JSCell* ptr)
        : m_ptr(ptr)
    {
#if ENABLE(JSC_ZOMBIES)
        ASSERT(!isZombie());
#endif
    }

    inline JSValue::JSValue(const JSCell* ptr)
        : m_ptr(const_cast<JSCell*>(ptr))
    {
#if ENABLE(JSC_ZOMBIES)
        ASSERT(!isZombie());
#endif
    }

    inline JSValue::operator bool() const
    {
        return m_ptr;
    }

    inline bool JSValue::operator==(const JSValue& other) const
    {
        return m_ptr == other.m_ptr;
    }

    inline bool JSValue::operator!=(const JSValue& other) const
    {
        return m_ptr != other.m_ptr;
    }

    inline bool JSValue::isUndefined() const
    {
        return asValue() == jsUndefined();
    }

    inline bool JSValue::isNull() const
    {
        return asValue() == jsNull();
    }
#endif // USE(JSVALUE32_64)

} // namespace JSC

#endif // JSValue_h
