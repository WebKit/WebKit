/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2007 Apple Inc. All rights reserved.
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

#ifndef KJS_VALUE_H
#define KJS_VALUE_H

#include "JSImmediate.h"
#include "collector.h"
#include "ustring.h"
#include <stddef.h> // for size_t

namespace KJS {

class ExecState;
class JSObject;
class JSCell;

struct ClassInfo;

/**
 * JSValue is the base type for all primitives (Undefined, Null, Boolean,
 * String, Number) and objects in ECMAScript.
 *
 * Note: you should never inherit from JSValue as it is for primitive types
 * only (all of which are provided internally by KJS). Instead, inherit from
 * JSObject.
 */
class JSValue : Noncopyable {
    friend class JSCell; // so it can derive from this class
    friend class Collector; // so it can call asCell()

private:
    JSValue();
    virtual ~JSValue();

public:
    // Querying the type.
    JSType type() const;
    bool isUndefined() const;
    bool isNull() const;
    bool isUndefinedOrNull() const;
    bool isBoolean() const;
    bool isNumber() const;
    bool isString() const;
    bool isObject() const;
    bool isObject(const ClassInfo *) const;

    // Extracting the value.
    bool getBoolean(bool&) const;
    bool getBoolean() const; // false if not a boolean
    bool getNumber(double&) const;
    double getNumber() const; // NaN if not a number
    bool getString(UString&) const;
    UString getString() const; // null string if not a string
    JSObject *getObject(); // NULL if not an object
    const JSObject *getObject() const; // NULL if not an object

    // Extracting integer values.
    bool getUInt32(uint32_t&) const;
    bool getTruncatedInt32(int32_t&) const;
    bool getTruncatedUInt32(uint32_t&) const;

    // Basic conversions.
    JSValue* toPrimitive(ExecState* exec, JSType preferredType = UnspecifiedType) const;
    bool getPrimitiveNumber(ExecState* exec, double& number, JSValue*& value);

    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    JSValue* toJSNumber(ExecState*) const; // Fast path for when you expect that the value is an immediate number.
    UString toString(ExecState *exec) const;
    JSObject *toObject(ExecState *exec) const;

    // Integer conversions.
    double toInteger(ExecState*) const;
    double toIntegerPreserveNaN(ExecState*) const;
    int32_t toInt32(ExecState*) const;
    int32_t toInt32(ExecState*, bool& ok) const;
    uint32_t toUInt32(ExecState*) const;
    uint32_t toUInt32(ExecState*, bool& ok) const;

    // These are identical logic to above, and faster than jsNumber(number)->toInt32(exec)
    static int32_t toInt32(double);
    static int32_t toUInt32(double);

    // Floating point conversions.
    float toFloat(ExecState*) const;

    // Garbage collection.
    void mark();
    bool marked() const;

    static int32_t toInt32SlowCase(double, bool& ok);
    static uint32_t toUInt32SlowCase(double, bool& ok);

private:
    int32_t toInt32SlowCase(ExecState*, bool& ok) const;
    uint32_t toUInt32SlowCase(ExecState*, bool& ok) const;

    // Implementation details.
    JSCell *asCell();
    const JSCell *asCell() const;

    // Give a compile time error if we try to copy one of these.
    JSValue(const JSValue&);
    JSValue& operator=(const JSValue&);
};

class JSCell : public JSValue {
    friend class Collector;
    friend class NumberImp;
    friend class StringImp;
    friend class JSObject;
    friend class GetterSetterImp;
private:
    JSCell();
    virtual ~JSCell();
public:
    // Querying the type.
    virtual JSType type() const = 0;
    bool isNumber() const;
    bool isString() const;
    bool isObject() const;
    bool isObject(const ClassInfo *) const;

    // Extracting the value.
    bool getNumber(double&) const;
    double getNumber() const; // NaN if not a number
    bool getString(UString&) const;
    UString getString() const; // null string if not a string
    JSObject *getObject(); // NULL if not an object
    const JSObject *getObject() const; // NULL if not an object

    // Extracting integer values.
    virtual bool getUInt32(uint32_t&) const;
    virtual bool getTruncatedInt32(int32_t&) const;
    virtual bool getTruncatedUInt32(uint32_t&) const;

    // Basic conversions.
    virtual JSValue *toPrimitive(ExecState *exec, JSType preferredType = UnspecifiedType) const = 0;
    virtual bool getPrimitiveNumber(ExecState* exec, double& number, JSValue*& value) = 0;
    virtual bool toBoolean(ExecState *exec) const = 0;
    virtual double toNumber(ExecState *exec) const = 0;
    virtual UString toString(ExecState *exec) const = 0;
    virtual JSObject *toObject(ExecState *exec) const = 0;

    // Garbage collection.
    void *operator new(size_t);
    virtual void mark();
    bool marked() const;
};

JSValue *jsNumberCell(double);

JSCell *jsString(const UString&); // returns empty string if passed null string
JSCell *jsString(const char* = ""); // returns empty string if passed 0

// should be used for strings that are owned by an object that will
// likely outlive the JSValue this makes, such as the parse tree or a
// DOM object that contains a UString
JSCell *jsOwnedString(const UString&); 

extern const double NaN;
extern const double Inf;

inline JSValue *jsUndefined()
{
    return JSImmediate::undefinedImmediate();
}

inline JSValue *jsNull()
{
    return JSImmediate::nullImmediate();
}

inline JSValue *jsNaN()
{
    static const union {
        uint64_t bits;
        double d;
    } nan = { 0x7ff80000ULL << 32 };
    return jsNumberCell(nan.d);
}

inline JSValue *jsBoolean(bool b)
{
    return b ? JSImmediate::trueImmediate() : JSImmediate::falseImmediate();
}

ALWAYS_INLINE JSValue* jsNumber(double d)
{
    JSValue* v = JSImmediate::from(d);
    return v ? v : jsNumberCell(d);
}

ALWAYS_INLINE JSValue* jsNumber(int i)
{
    JSValue* v = JSImmediate::from(i);
    return v ? v : jsNumberCell(i);
}

ALWAYS_INLINE JSValue* jsNumber(unsigned i)
{
    JSValue* v = JSImmediate::from(i);
    return v ? v : jsNumberCell(i);
}

ALWAYS_INLINE JSValue* jsNumber(long i)
{
    JSValue* v = JSImmediate::from(i);
    return v ? v : jsNumberCell(i);
}

ALWAYS_INLINE JSValue* jsNumber(unsigned long i)
{
    JSValue* v = JSImmediate::from(i);
    return v ? v : jsNumberCell(i);
}

ALWAYS_INLINE JSValue* jsNumber(long long i)
{
    JSValue* v = JSImmediate::from(i);
    return v ? v : jsNumberCell(static_cast<double>(i));
}

ALWAYS_INLINE JSValue* jsNumber(unsigned long long i)
{
    JSValue* v = JSImmediate::from(i);
    return v ? v : jsNumberCell(static_cast<double>(i));
}

ALWAYS_INLINE JSValue* jsNumberFromAnd(ExecState *exec, JSValue* v1, JSValue* v2)
{
    if (JSImmediate::areBothImmediateNumbers(v1, v2))
        return JSImmediate::andImmediateNumbers(v1, v2);
    return jsNumber(v1->toInt32(exec) & v2->toInt32(exec));
}

inline JSValue::JSValue()
{
}

inline JSValue::~JSValue()
{
}

inline JSCell::JSCell()
{
}

inline JSCell::~JSCell()
{
}

inline bool JSCell::isNumber() const
{
    return type() == NumberType;
}

inline bool JSCell::isString() const
{
    return type() == StringType;
}

inline bool JSCell::isObject() const
{
    return type() == ObjectType;
}

inline bool JSCell::marked() const
{
    return Collector::isCellMarked(this);
}

inline void JSCell::mark()
{
    return Collector::markCell(this);
}

ALWAYS_INLINE JSCell* JSValue::asCell()
{
    ASSERT(!JSImmediate::isImmediate(this));
    return static_cast<JSCell*>(this);
}

ALWAYS_INLINE const JSCell* JSValue::asCell() const
{
    ASSERT(!JSImmediate::isImmediate(this));
    return static_cast<const JSCell*>(this);
}

inline bool JSValue::isUndefined() const
{
    return this == jsUndefined();
}

inline bool JSValue::isNull() const
{
    return this == jsNull();
}

inline bool JSValue::isUndefinedOrNull() const
{
    return JSImmediate::isUndefinedOrNull(this);
}

inline bool JSValue::isBoolean() const
{
    return JSImmediate::isBoolean(this);
}

inline bool JSValue::isNumber() const
{
    return JSImmediate::isNumber(this) || (!JSImmediate::isImmediate(this) && asCell()->isNumber());
}

inline bool JSValue::isString() const
{
    return !JSImmediate::isImmediate(this) && asCell()->isString();
}

inline bool JSValue::isObject() const
{
    return !JSImmediate::isImmediate(this) && asCell()->isObject();
}

inline bool JSValue::getBoolean(bool& v) const
{
    if (JSImmediate::isBoolean(this)) {
        v = JSImmediate::toBoolean(this);
        return true;
    }
    
    return false;
}

inline bool JSValue::getBoolean() const
{
    return JSImmediate::isBoolean(this) ? JSImmediate::toBoolean(this) : false;
}

inline bool JSValue::getNumber(double& v) const
{
    if (JSImmediate::isImmediate(this)) {
        v = JSImmediate::toDouble(this);
        return true;
    }
    return asCell()->getNumber(v);
}

inline double JSValue::getNumber() const
{
    return JSImmediate::isImmediate(this) ? JSImmediate::toDouble(this) : asCell()->getNumber();
}

inline bool JSValue::getString(UString& s) const
{
    return !JSImmediate::isImmediate(this) && asCell()->getString(s);
}

inline UString JSValue::getString() const
{
    return JSImmediate::isImmediate(this) ? UString() : asCell()->getString();
}

inline JSObject *JSValue::getObject()
{
    return JSImmediate::isImmediate(this) ? 0 : asCell()->getObject();
}

inline const JSObject *JSValue::getObject() const
{
    return JSImmediate::isImmediate(this) ? 0 : asCell()->getObject();
}

ALWAYS_INLINE bool JSValue::getUInt32(uint32_t& v) const
{
    return JSImmediate::isImmediate(this) ? JSImmediate::getUInt32(this, v) : asCell()->getUInt32(v);
}

ALWAYS_INLINE bool JSValue::getTruncatedInt32(int32_t& v) const
{
    return JSImmediate::isImmediate(this) ? JSImmediate::getTruncatedInt32(this, v) : asCell()->getTruncatedInt32(v);
}

inline bool JSValue::getTruncatedUInt32(uint32_t& v) const
{
    return JSImmediate::isImmediate(this) ? JSImmediate::getTruncatedUInt32(this, v) : asCell()->getTruncatedUInt32(v);
}

inline void JSValue::mark()
{
    ASSERT(!JSImmediate::isImmediate(this)); // callers should check !marked() before calling mark()
    asCell()->mark();
}

inline bool JSValue::marked() const
{
    return JSImmediate::isImmediate(this) || asCell()->marked();
}

inline JSType JSValue::type() const
{
    return JSImmediate::isImmediate(this) ? JSImmediate::type(this) : asCell()->type();
}

inline JSValue* JSValue::toPrimitive(ExecState* exec, JSType preferredType) const
{
    return JSImmediate::isImmediate(this) ? const_cast<JSValue*>(this) : asCell()->toPrimitive(exec, preferredType);
}

inline bool JSValue::getPrimitiveNumber(ExecState* exec, double& number, JSValue*& value)
{
    if (JSImmediate::isImmediate(this)) {
        number = JSImmediate::toDouble(this);
        value = this;
        return true;
    }
    return asCell()->getPrimitiveNumber(exec, number, value);
}

inline bool JSValue::toBoolean(ExecState *exec) const
{
    return JSImmediate::isImmediate(this) ? JSImmediate::toBoolean(this) : asCell()->toBoolean(exec);
}

ALWAYS_INLINE double JSValue::toNumber(ExecState *exec) const
{
    return JSImmediate::isImmediate(this) ? JSImmediate::toDouble(this) : asCell()->toNumber(exec);
}

ALWAYS_INLINE JSValue* JSValue::toJSNumber(ExecState* exec) const
{
    return JSImmediate::isNumber(this) ? const_cast<JSValue*>(this) : jsNumber(this->toNumber(exec));
}

inline UString JSValue::toString(ExecState *exec) const
{
    return JSImmediate::isImmediate(this) ? JSImmediate::toString(this) : asCell()->toString(exec);
}

inline JSObject* JSValue::toObject(ExecState* exec) const
{
    return JSImmediate::isImmediate(this) ? JSImmediate::toObject(this, exec) : asCell()->toObject(exec);
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

inline int32_t JSValue::toInt32(double val)
{
    if (!(val >= -2147483648.0 && val < 2147483648.0)) {
        bool ignored;
        return toInt32SlowCase(val, ignored);
    }
    return static_cast<int32_t>(val);
}

inline int32_t JSValue::toUInt32(double val)
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

} // namespace

#endif // KJS_VALUE_H
