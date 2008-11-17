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

#ifndef JSValue_h
#define JSValue_h

#include "CallData.h"
#include "ConstructData.h"
#include "JSImmediate.h"
#include "UString.h"
#include <stddef.h> // for size_t

namespace JSC {

    class Identifier;
    class JSString;
    class PropertySlot;
    class PutPropertySlot;

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
        JSValue* toPrimitive(ExecState*, PreferredPrimitiveType = NoPreference) const;
        bool getPrimitiveNumber(ExecState*, double& number, JSValue*&);

        bool toBoolean(ExecState*) const;

        // toNumber conversion is expected to be side effect free if an exception has
        // been set in the ExecState already.
        double toNumber(ExecState*) const;
        JSValue* toJSNumber(ExecState*) const; // Fast path for when you expect that the value is an immediate number.

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
        JSValue* get(ExecState*, const Identifier& propertyName) const;
        JSValue* get(ExecState*, const Identifier& propertyName, PropertySlot&) const;
        JSValue* get(ExecState*, unsigned propertyName) const;
        JSValue* get(ExecState*, unsigned propertyName, PropertySlot&) const;
        void put(ExecState*, const Identifier& propertyName, JSValue*, PutPropertySlot&);
        void put(ExecState*, unsigned propertyName, JSValue*);
        bool deleteProperty(ExecState*, const Identifier& propertyName);
        bool deleteProperty(ExecState*, unsigned propertyName);

        bool needsThisConversion() const;
        JSObject* toThisObject(ExecState*) const;
        UString toThisString(ExecState*) const;
        JSString* toThisJSString(ExecState*);

        JSValue* getJSNumber(); // 0 if this is not a JSNumber or number object

        JSValue* asValue() const;

        JSCell* asCell() const;

    private:
        bool getPropertySlot(ExecState*, const Identifier& propertyName, PropertySlot&);
        bool getPropertySlot(ExecState*, unsigned propertyName, PropertySlot&);
        int32_t toInt32SlowCase(ExecState*, bool& ok) const;
        uint32_t toUInt32SlowCase(ExecState*, bool& ok) const;
    };

    // These are identical logic to the JSValue functions above, and faster than jsNumber(number)->toInt32().
    int32_t toInt32(double);
    uint32_t toUInt32(double);
    int32_t toInt32SlowCase(double, bool& ok);
    uint32_t toUInt32SlowCase(double, bool& ok);

    inline JSValue* JSValue::asValue() const
    {
        return const_cast<JSValue*>(this);
    }

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

#endif // JSValue_h
