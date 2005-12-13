/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003-2005 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 51 Franklin Steet, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef KJS_VALUE_H
#define KJS_VALUE_H

#ifndef NDEBUG // protection against problems if committing with KJS_VERBOSE on

// Uncomment this to enable very verbose output from KJS
//#define KJS_VERBOSE
// Uncomment this to debug memory allocation and garbage collection
//#define KJS_DEBUG_MEM

#endif

#include <assert.h>
#include <stdlib.h> // for size_t
#include "simple_number.h"
#include "ustring.h"

namespace KJS {

class ClassInfo;
class ExecState;
class JSObject;

/**
 * Primitive types
 */
enum Type {
    UnspecifiedType   = 0,
    UndefinedType     = 1,
    NullType          = 2,
    BooleanType       = 3,
    StringType        = 4,
    NumberType        = 5,
    ObjectType        = 6,
    GetterSetterType  = 7
};

/**
 * JSValue is the base type for all primitives (Undefined, Null, Boolean,
 * String, Number) and objects in ECMAScript.
 *
 * Note: you should never inherit from JSValue as it is for primitive types
 * only (all of which are provided internally by KJS). Instead, inherit from
 * JSObject.
 */
class JSValue {
    friend class JSCell; // so it can derive from this class
    friend class Collector; // so it can call downcast()

private:
    JSValue();
    virtual ~JSValue();

public:
    // Querying the type.
    Type type() const;
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
    bool getNumber(double&) const;
    double getNumber() const; // NaN if not a number
    bool getString(UString&) const;
    UString getString() const; // null string if not a string
    JSObject *getObject(); // NULL if not an object
    const JSObject *getObject() const; // NULL if not an object

    // Extracting integer values.
    bool getUInt32(uint32_t&) const;

    // Basic conversions.
    JSValue *toPrimitive(ExecState *exec, Type preferredType = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    UString toString(ExecState *exec) const;
    JSObject *toObject(ExecState *exec) const;

    // Integer conversions.
    double toInteger(ExecState *exec) const;
    int32_t toInt32(ExecState *exec) const;
    uint32_t toUInt32(ExecState *exec) const;
    uint16_t toUInt16(ExecState *exec) const;

    // Garbage collection.
    void mark();
    bool marked() const;

private:
    // Implementation details.
    JSCell *downcast();
    const JSCell *downcast() const;

    // Give a compile time error if we try to copy one of these.
    JSValue(const JSValue&);
    JSValue& operator=(const JSValue&);
};

class JSCell : public JSValue {
    friend class Collector;
    friend class UndefinedImp;
    friend class NullImp;
    friend class BooleanImp;
    friend class NumberImp;
    friend class StringImp;
    friend class JSObject;
    friend class GetterSetterImp;
private:
    JSCell();
    virtual ~JSCell();
public:
    // Querying the type.
    virtual Type type() const = 0;
    bool isBoolean() const;
    bool isNumber() const;
    bool isString() const;
    bool isObject() const;
    bool isObject(const ClassInfo *) const;

    // Extracting the value.
    bool getBoolean(bool&) const;
    bool getNumber(double&) const;
    double getNumber() const; // NaN if not a number
    bool getString(UString&) const;
    UString getString() const; // null string if not a string
    JSObject *getObject(); // NULL if not an object
    const JSObject *getObject() const; // NULL if not an object

    // Extracting integer values.
    virtual bool getUInt32(uint32_t&) const;

    // Basic conversions.
    virtual JSValue *toPrimitive(ExecState *exec, Type preferredType = UnspecifiedType) const = 0;
    virtual bool toBoolean(ExecState *exec) const = 0;
    virtual double toNumber(ExecState *exec) const = 0;
    virtual UString toString(ExecState *exec) const = 0;
    virtual JSObject *toObject(ExecState *exec) const = 0;

    // Garbage collection.
    void *operator new(size_t);
    virtual void mark();
    bool marked() const;

private:
    bool m_marked;
};

JSCell *jsUndefined();
JSCell *jsNull();

JSCell *jsBoolean(bool);

JSValue *jsNumber(double);
JSValue *jsNaN();

JSCell *jsString(const UString &); // returns empty string if passed null string
JSCell *jsString(const char * = ""); // returns empty string if passed 0

extern const double NaN;
extern const double Inf;

class ConstantValues {
public:
    static JSCell *undefined;
    static JSCell *null;
    static JSCell *jsFalse;
    static JSCell *jsTrue;

    static void initIfNeeded();
    static void mark();
};

inline JSCell *jsUndefined()
{
    return ConstantValues::undefined;
}

inline JSCell *jsNull()
{
    return ConstantValues::null;
}

inline JSCell *jsBoolean(bool b)
{
    return b ? ConstantValues::jsTrue : ConstantValues::jsFalse;
}

inline JSValue *jsNaN()
{
    return SimpleNumber::make(NaN);
}

inline JSValue::JSValue()
{
}

inline JSValue::~JSValue()
{
}

inline JSCell::JSCell()
    : m_marked(false)
{
}

inline JSCell::~JSCell()
{
}

inline bool JSCell::isBoolean() const
{
    return type() == BooleanType;
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
    return m_marked;
}

inline void JSCell::mark()
{
    m_marked = true;
}

inline JSCell *JSValue::downcast()
{
    assert(!SimpleNumber::is(this));
    return static_cast<JSCell *>(this);
}

inline const JSCell *JSValue::downcast() const
{
    assert(!SimpleNumber::is(this));
    return static_cast<const JSCell *>(this);
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
    return this == jsUndefined() || this == jsNull();
}

inline bool JSValue::isBoolean() const
{
    return !SimpleNumber::is(this) && downcast()->isBoolean();
}

inline bool JSValue::isNumber() const
{
    return SimpleNumber::is(this) || downcast()->isNumber();
}

inline bool JSValue::isString() const
{
    return !SimpleNumber::is(this) && downcast()->isString();
}

inline bool JSValue::isObject() const
{
    return !SimpleNumber::is(this) && downcast()->isObject();
}

inline bool JSValue::isObject(const ClassInfo *c) const
{
    return !SimpleNumber::is(this) && downcast()->isObject(c);
}

inline bool JSValue::getBoolean(bool& v) const
{
    return !SimpleNumber::is(this) && downcast()->getBoolean(v);
}

inline bool JSValue::getNumber(double& v) const
{
    if (SimpleNumber::is(this)) {
        v = SimpleNumber::value(this);
        return true;
    }
    return downcast()->getNumber(v);
}

inline double JSValue::getNumber() const
{
    return SimpleNumber::is(this) ? SimpleNumber::value(this) : downcast()->getNumber();
}

inline bool JSValue::getString(UString& s) const
{
    return !SimpleNumber::is(this) && downcast()->getString(s);
}

inline UString JSValue::getString() const
{
    return SimpleNumber::is(this) ? UString() : downcast()->getString();
}

inline JSObject *JSValue::getObject()
{
    return SimpleNumber::is(this) ? 0 : downcast()->getObject();
}

inline const JSObject *JSValue::getObject() const
{
    return SimpleNumber::is(this) ? 0 : downcast()->getObject();
}

inline bool JSValue::getUInt32(uint32_t& v) const
{
    if (SimpleNumber::is(this)) {
        double d = SimpleNumber::value(this);
        if (!(d >= 0) || d > 0xFFFFFFFFUL) // true for NaN
            return false;
        v = static_cast<uint32_t>(d);
        return true;
    }
    return downcast()->getUInt32(v);
}

inline void JSValue::mark()
{
    if (!SimpleNumber::is(this))
        downcast()->mark();
}

inline bool JSValue::marked() const
{
    return SimpleNumber::is(this) || downcast()->marked();
}

inline Type JSValue::type() const
{
    return SimpleNumber::is(this) ? NumberType : downcast()->type();
}

inline JSValue *JSValue::toPrimitive(ExecState *exec, Type preferredType) const
{
    return SimpleNumber::is(this) ? const_cast<JSValue *>(this) : downcast()->toPrimitive(exec, preferredType);
}

inline bool JSValue::toBoolean(ExecState *exec) const
{
    if (SimpleNumber::is(this)) {
        double d = SimpleNumber::value(this);
        return d < 0 || d > 0; // false for NaN
    }

    return downcast()->toBoolean(exec);
}

inline double JSValue::toNumber(ExecState *exec) const
{
    return SimpleNumber::is(this) ? SimpleNumber::value(this) : downcast()->toNumber(exec);
}

inline UString JSValue::toString(ExecState *exec) const
{
    if (SimpleNumber::is(this)) {
        double d = SimpleNumber::value(this);
        if (d == 0.0) // +0.0 or -0.0
            d = 0.0;
        return UString::from(d);
    }

    return downcast()->toString(exec);
}

} // namespace

#endif // KJS_VALUE_H
