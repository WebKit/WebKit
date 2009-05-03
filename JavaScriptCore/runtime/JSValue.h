/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2007, 2008 Apple Inc. All rights reserved.
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

#include <stddef.h> // for size_t
#include <stdint.h>

#ifndef JSValue_h
#define JSValue_h

#include "CallData.h"
#include "ConstructData.h"
#include <wtf/HashTraits.h>
#include <wtf/AlwaysInline.h>

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

    typedef void* EncodedJSValue;

    class JSValue {
        friend class JSImmediate;
        friend struct JSValueHashTraits;

        static JSValue makeImmediate(intptr_t value)
        {
            return JSValue(reinterpret_cast<JSCell*>(value));
        }

        intptr_t immediateValue()
        {
            return reinterpret_cast<intptr_t>(m_ptr);
        }
        
    public:
        enum JSNullTag { JSNull };
        enum JSUndefinedTag { JSUndefined };
        enum JSTrueTag { JSTrue };
        enum JSFalseTag { JSFalse };

        static EncodedJSValue encode(JSValue value);
        static JSValue decode(EncodedJSValue ptr);

        JSValue();
        JSValue(JSNullTag);
        JSValue(JSUndefinedTag);
        JSValue(JSTrueTag);
        JSValue(JSFalseTag);
        JSValue(JSCell* ptr);
        JSValue(const JSCell* ptr);

        // Numbers
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
        JSValue(JSGlobalData*, char);
        JSValue(JSGlobalData*, unsigned char);
        JSValue(JSGlobalData*, short);
        JSValue(JSGlobalData*, unsigned short);
        JSValue(JSGlobalData*, int);
        JSValue(JSGlobalData*, unsigned);
        JSValue(JSGlobalData*, long);
        JSValue(JSGlobalData*, unsigned long);
        JSValue(JSGlobalData*, long long);
        JSValue(JSGlobalData*, unsigned long long);

        operator bool() const;
        bool operator==(const JSValue other) const;
        bool operator!=(const JSValue other) const;

        // Querying the type.
        bool isUndefined() const;
        bool isNull() const;
        bool isUndefinedOrNull() const;
        bool isBoolean() const;
        bool isNumber() const;
        bool isString() const;
        bool isGetterSetter() const;
        bool isObject() const;
        bool isObject(const ClassInfo*) const;
        
        // Extracting the value.
        bool getBoolean(bool&) const;
        bool getBoolean() const; // false if not a boolean
        bool getNumber(double&) const;
        double uncheckedGetNumber() const;
        bool getString(UString&) const;
        UString getString() const; // null string if not a string
        JSObject* getObject() const; // 0 if not an object

        CallType getCallData(CallData&);
        ConstructType getConstructData(ConstructData&);

        // Extracting integer values.
        bool getUInt32(uint32_t&) const;
        bool getTruncatedInt32(int32_t&) const;
        bool getTruncatedUInt32(uint32_t&) const;
        
        // Basic conversions.
        JSValue toPrimitive(ExecState*, PreferredPrimitiveType = NoPreference) const;
        bool getPrimitiveNumber(ExecState*, double& number, JSValue&);

        bool toBoolean(ExecState*) const;

        // toNumber conversion is expected to be side effect free if an exception has
        // been set in the ExecState already.
        double toNumber(ExecState*) const;
        JSValue toJSNumber(ExecState*) const; // Fast path for when you expect that the value is an immediate number.
        UString toString(ExecState*) const;
        JSObject* toObject(ExecState*) const;

        // Integer conversions.
        // 'x.numberToInt32(output)' is equivalent to 'x.isNumber() && x.toInt32(output)'
        double toInteger(ExecState*) const;
        double toIntegerPreserveNaN(ExecState*) const;
        int32_t toInt32(ExecState*) const;
        int32_t toInt32(ExecState*, bool& ok) const;
        bool numberToInt32(int32_t& arg);
        uint32_t toUInt32(ExecState*) const;
        uint32_t toUInt32(ExecState*, bool& ok) const;
        bool numberToUInt32(uint32_t& arg);

        // Fast integer operations; these values return results where the value is trivially available
        // in a convenient form, for use in optimizations.  No assumptions should be made based on the
        // results of these operations, for example !isInt32Fast() does not necessarily indicate the
        // result of getNumber will not be 0.
        bool isInt32Fast() const;
        int32_t getInt32Fast() const;
        bool isUInt32Fast() const;
        uint32_t getUInt32Fast() const;
        static JSValue makeInt32Fast(int32_t);
        static bool areBothInt32Fast(JSValue, JSValue);

        // Floating point conversions (this is a convenience method for webcore;
        // signle precision float is not a representation used in JS or JSC).
        float toFloat(ExecState* exec) const { return static_cast<float>(toNumber(exec)); }

        // API Mangled Numbers
        bool isAPIMangledNumber();

        // Garbage collection.
        void mark();
        bool marked() const;

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
        JSString* toThisJSString(ExecState*);

        static bool equal(ExecState* exec, JSValue v1, JSValue v2);
        static bool equalSlowCase(ExecState* exec, JSValue v1, JSValue v2);
        static bool equalSlowCaseInline(ExecState* exec, JSValue v1, JSValue v2);
        static bool strictEqual(JSValue v1, JSValue v2);
        static bool strictEqualSlowCase(JSValue v1, JSValue v2);
        static bool strictEqualSlowCaseInline(JSValue v1, JSValue v2);

        JSValue getJSNumber(); // JSValue() if this is not a JSNumber or number object

        bool isCell() const;
        JSCell* asCell() const;

    private:
        enum HashTableDeletedValueTag { HashTableDeletedValue };
        JSValue(HashTableDeletedValueTag);

        inline const JSValue asValue() const { return *this; }

        bool isDoubleNumber() const;
        double getDoubleNumber() const;

        JSCell* m_ptr;
    };

    struct JSValueHashTraits : HashTraits<EncodedJSValue> {
        static void constructDeletedValue(EncodedJSValue& slot) { slot = JSValue::encode(JSValue(JSValue::HashTableDeletedValue)); }
        static bool isDeletedValue(EncodedJSValue value) { return value == JSValue::encode(JSValue(JSValue::HashTableDeletedValue)); }
    };

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

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, char i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, unsigned char i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, short i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, unsigned short i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, int i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, unsigned i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, long i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, unsigned long i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, long long i)
    {
        return JSValue(globalData, i);
    }

    ALWAYS_INLINE JSValue jsNumber(JSGlobalData* globalData, unsigned long long i)
    {
        return JSValue(globalData, i);
    }

    inline bool operator==(const JSValue a, const JSCell* b) { return a == JSValue(b); }
    inline bool operator==(const JSCell* a, const JSValue b) { return JSValue(a) == b; }

    inline bool operator!=(const JSValue a, const JSCell* b) { return a != JSValue(b); }
    inline bool operator!=(const JSCell* a, const JSValue b) { return JSValue(a) != b; }

    // JSValue member functions.
    inline EncodedJSValue JSValue::encode(JSValue value)
    {
        return reinterpret_cast<EncodedJSValue>(value.m_ptr);
    }

    inline JSValue JSValue::decode(EncodedJSValue ptr)
    {
        return JSValue(reinterpret_cast<JSCell*>(ptr));
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
    }

    inline JSValue::JSValue(const JSCell* ptr)
        : m_ptr(const_cast<JSCell*>(ptr))
    {
    }

    inline JSValue::operator bool() const
    {
        return m_ptr;
    }

    inline bool JSValue::operator==(const JSValue other) const
    {
        return m_ptr == other.m_ptr;
    }

    inline bool JSValue::operator!=(const JSValue other) const
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

} // namespace JSC

#endif // JSValue_h
