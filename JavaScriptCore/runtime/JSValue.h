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
#include <wtf/Noncopyable.h>

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

    class JSValue : Noncopyable {
    protected:
        JSValue() { }
        virtual ~JSValue() { }

    public:
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
        double getNumber() const; // NaN if not a number
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
        double toInteger(ExecState*) const;
        double toIntegerPreserveNaN(ExecState*) const;
        int32_t toInt32(ExecState*) const;
        int32_t toInt32(ExecState*, bool& ok) const;
        uint32_t toUInt32(ExecState*) const;
        uint32_t toUInt32(ExecState*, bool& ok) const;

        // Floating point conversions.
        float toFloat(ExecState*) const;

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
        bool deleteProperty(ExecState*, const Identifier& propertyName);
        bool deleteProperty(ExecState*, unsigned propertyName);

        bool needsThisConversion() const;
        JSObject* toThisObject(ExecState*) const;
        UString toThisString(ExecState*) const;
        JSString* toThisJSString(ExecState*);

        JSValuePtr getJSNumber(); // 0 if this is not a JSNumber or number object

        JSValuePtr asValue() const;

        JSCell* asCell() const;

    private:
        bool getPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        int32_t toInt32SlowCase(ExecState*, bool& ok) const;
        uint32_t toUInt32SlowCase(ExecState*, bool& ok) const;
    };

    class JSImmediate;
    class JSValueEncodedAsPointer;

    class JSValuePtr {
        friend class JSImmediate;

        static JSValuePtr makeImmediate(intptr_t value)
        {
            return JSValuePtr(reinterpret_cast<JSValue*>(value));
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

        JSValuePtr(JSValue* ptr)
            : m_ptr(ptr)
        {
        }

        JSValuePtr(const JSValue* ptr)
            : m_ptr(const_cast<JSValue*>(ptr))
        {
        }

        JSValue* operator->() const
        {
            return m_ptr;
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
            return JSValuePtr(reinterpret_cast<JSValue*>(ptr));
        }

    private:
        JSValue* m_ptr;
    };

    inline JSValuePtr JSValue::asValue() const
    {
        return JSValuePtr(this);
    }

    inline JSValuePtr noValue()
    {
        return JSValuePtr();
    }

    inline bool operator==(const JSValuePtr a, const JSValue* b) { return a == JSValuePtr(b); }
    inline bool operator==(const JSValue* a, const JSValuePtr b) { return JSValuePtr(a) == b; }

    inline bool operator!=(const JSValuePtr a, const JSValue* b) { return a != JSValuePtr(b); }
    inline bool operator!=(const JSValue* a, const JSValuePtr b) { return JSValuePtr(a) != b; }

} // namespace JSC

#endif // JSValue_h
