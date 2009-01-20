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

namespace JSC {

    class Identifier;
    class JSCell;
    class JSObject;
    class JSString;
    class PropertySlot;
    class PutPropertySlot;
    class UString;

    struct ClassInfo;
    struct Instruction;

    enum PreferredPrimitiveType { NoPreference, PreferNumber, PreferString };

    class JSImmediate;
    class JSValueEncodedAsPointer;

    class JSValuePtr {
        friend class JSImmediate;

        static JSValuePtr makeImmediate(intptr_t value)
        {
            return JSValuePtr(reinterpret_cast<JSCell*>(value));
        }

        intptr_t immediateValue()
        {
            return reinterpret_cast<intptr_t>(m_ptr);
        }
        
    public:
        JSValuePtr()
            : m_ptr(0)
        {
        }

        JSValuePtr(JSCell* ptr)
            : m_ptr(ptr)
        {
        }

        JSValuePtr(const JSCell* ptr)
            : m_ptr(const_cast<JSCell*>(ptr))
        {
        }

        operator bool() const
        {
            return m_ptr;
        }

        bool operator==(const JSValuePtr other) const
        {
            return m_ptr == other.m_ptr;
        }

        bool operator!=(const JSValuePtr other) const
        {
            return m_ptr != other.m_ptr;
        }

        static JSValueEncodedAsPointer* encode(JSValuePtr value)
        {
            return reinterpret_cast<JSValueEncodedAsPointer*>(value.m_ptr);
        }

        static JSValuePtr decode(JSValueEncodedAsPointer* ptr)
        {
            return JSValuePtr(reinterpret_cast<JSCell*>(ptr));
        }

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
        JSValuePtr toPrimitive(ExecState*, PreferredPrimitiveType = NoPreference) const;
        bool getPrimitiveNumber(ExecState*, double& number, JSValuePtr&);

        bool toBoolean(ExecState*) const;

        // toNumber conversion is expected to be side effect free if an exception has
        // been set in the ExecState already.
        double toNumber(ExecState*) const;
        JSValuePtr toJSNumber(ExecState*) const; // Fast path for when you expect that the value is an immediate number.
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
        static JSValuePtr makeInt32Fast(int32_t);
        static bool areBothInt32Fast(JSValuePtr, JSValuePtr);

        // Floating point conversions (this is a convenience method for webcore;
        // signle precision float is not a representation used in JS or JSC).
        float toFloat(ExecState* exec) const { return static_cast<float>(toNumber(exec)); }

        // Garbage collection.
        void mark();
        bool marked() const;

        // Object operations, with the toObject operation included.
        JSValuePtr get(ExecState*, const Identifier& propertyName) const;
        JSValuePtr get(ExecState*, const Identifier& propertyName, PropertySlot&) const;
        JSValuePtr get(ExecState*, unsigned propertyName) const;
        JSValuePtr get(ExecState*, unsigned propertyName, PropertySlot&) const;
        void put(ExecState*, const Identifier& propertyName, JSValuePtr, PutPropertySlot&);
        void put(ExecState*, unsigned propertyName, JSValuePtr);

        bool needsThisConversion() const;
        JSObject* toThisObject(ExecState*) const;
        UString toThisString(ExecState*) const;
        JSString* toThisJSString(ExecState*);

        static bool equal(ExecState* exec, JSValuePtr v1, JSValuePtr v2);
        static bool equalSlowCase(ExecState* exec, JSValuePtr v1, JSValuePtr v2);
        static bool equalSlowCaseInline(ExecState* exec, JSValuePtr v1, JSValuePtr v2);
        static bool strictEqual(JSValuePtr v1, JSValuePtr v2);
        static bool strictEqualSlowCase(JSValuePtr v1, JSValuePtr v2);
        static bool strictEqualSlowCaseInline(JSValuePtr v1, JSValuePtr v2);

        JSValuePtr getJSNumber(); // noValue() if this is not a JSNumber or number object

        bool isCell() const;
        JSCell* asCell() const;

    private:
        inline const JSValuePtr asValue() const { return *this; }

        bool isDoubleNumber() const;
        double getDoubleNumber() const;

        JSCell* m_ptr;
    };

    inline JSValuePtr noValue()
    {
        return JSValuePtr();
    }

    inline bool operator==(const JSValuePtr a, const JSCell* b) { return a == JSValuePtr(b); }
    inline bool operator==(const JSCell* a, const JSValuePtr b) { return JSValuePtr(a) == b; }

    inline bool operator!=(const JSValuePtr a, const JSCell* b) { return a != JSValuePtr(b); }
    inline bool operator!=(const JSCell* a, const JSValuePtr b) { return JSValuePtr(a) != b; }

} // namespace JSC

#endif // JSValue_h
