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
class ObjectImp;

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
    ObjectType        = 6
};

/**
 * ValueImp is the base type for all primitives (Undefined, Null, Boolean,
 * String, Number) and objects in ECMAScript.
 *
 * Note: you should never inherit from ValueImp as it is for primitive types
 * only (all of which are provided internally by KJS). Instead, inherit from
 * ObjectImp.
 */
class ValueImp {
    friend class AllocatedValueImp; // so it can derive from this class
    friend class ProtectedValues; // so it can call downcast()

private:
    ValueImp();
    virtual ~ValueImp();

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
    ObjectImp *getObject(); // NULL if not an object
    const ObjectImp *getObject() const; // NULL if not an object

    // Extracting integer values.
    bool getUInt32(uint32_t&) const;

    // Basic conversions.
    ValueImp *toPrimitive(ExecState *exec, Type preferredType = UnspecifiedType) const;
    bool toBoolean(ExecState *exec) const;
    double toNumber(ExecState *exec) const;
    double toNumber(ExecState *exec, bool& knownToBeInteger) const;
    UString toString(ExecState *exec) const;
    ObjectImp *toObject(ExecState *exec) const;

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
    AllocatedValueImp *downcast();
    const AllocatedValueImp *downcast() const;

    // Give a compile time error if we try to copy one of these.
    ValueImp(const ValueImp&);
    ValueImp& operator=(const ValueImp&);
};

class AllocatedValueImp : public ValueImp {
    friend class Collector;
    friend class UndefinedImp;
    friend class NullImp;
    friend class BooleanImp;
    friend class NumberImp;
    friend class StringImp;
    friend class ObjectImp;
private:
    AllocatedValueImp();
    virtual ~AllocatedValueImp();
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
    ObjectImp *getObject(); // NULL if not an object
    const ObjectImp *getObject() const; // NULL if not an object

    // Extracting integer values.
    virtual bool getUInt32(uint32_t&) const;

    // Basic conversions.
    virtual ValueImp *toPrimitive(ExecState *exec, Type preferredType = UnspecifiedType) const = 0;
    virtual bool toBoolean(ExecState *exec) const = 0;
    virtual double toNumber(ExecState *exec) const = 0;
    double toNumber(ExecState *exec, bool& knownToBeInteger) const;
    virtual UString toString(ExecState *exec) const = 0;
    virtual ObjectImp *toObject(ExecState *exec) const = 0;

    // Garbage collection.
    void *operator new(size_t);
    virtual void mark();
    bool marked() const;

private:
    bool m_marked;
};

AllocatedValueImp *jsUndefined();
AllocatedValueImp *jsNull();

AllocatedValueImp *jsBoolean(bool = false);

ValueImp *jsNumber(double);
ValueImp *jsNumber(double, bool knownToBeInteger);
AllocatedValueImp *jsNaN();
ValueImp *jsZero();
ValueImp *jsOne();
ValueImp *jsTwo();
ValueImp *jsNumber(int);
ValueImp *jsNumber(unsigned);
ValueImp *jsNumber(long);
ValueImp *jsNumber(unsigned long);
ValueImp *jsNumber(long long);
ValueImp *jsNumber(unsigned long long);

AllocatedValueImp *jsString(const UString &); // returns empty string if passed null string
AllocatedValueImp *jsString(const char * = ""); // returns empty string if passed 0

extern const double NaN;
extern const double Inf;

class ConstantValues {
public:
    static AllocatedValueImp *undefined;
    static AllocatedValueImp *null;
    static AllocatedValueImp *jsFalse;
    static AllocatedValueImp *jsTrue;
    static AllocatedValueImp *NaN;

    static void init();
    static void clear();
    static void mark();
};

inline AllocatedValueImp *jsUndefined()
{
    return ConstantValues::undefined;
}

inline AllocatedValueImp *jsNull()
{
    return ConstantValues::null;
}

inline AllocatedValueImp *jsBoolean(bool b)
{
    return b ? ConstantValues::jsTrue : ConstantValues::jsFalse;
}

inline AllocatedValueImp *jsNaN()
{
    return ConstantValues::NaN;
}

inline ValueImp::ValueImp()
{
}

inline ValueImp::~ValueImp()
{
}

inline AllocatedValueImp::AllocatedValueImp()
    : m_marked(false)
{
}

inline AllocatedValueImp::~AllocatedValueImp()
{
}

inline bool AllocatedValueImp::isBoolean() const
{
    return type() == BooleanType;
}

inline bool AllocatedValueImp::isNumber() const
{
    return type() == NumberType;
}

inline bool AllocatedValueImp::isString() const
{
    return type() == StringType;
}

inline bool AllocatedValueImp::isObject() const
{
    return type() == ObjectType;
}

inline bool AllocatedValueImp::marked() const
{
    return m_marked;
}

inline void AllocatedValueImp::mark()
{
    m_marked = true;
}

inline AllocatedValueImp *ValueImp::downcast()
{
    assert(!SimpleNumber::is(this));
    return static_cast<AllocatedValueImp *>(this);
}

inline const AllocatedValueImp *ValueImp::downcast() const
{
    assert(!SimpleNumber::is(this));
    return static_cast<const AllocatedValueImp *>(this);
}

inline bool ValueImp::isUndefined() const
{
    return this == jsUndefined();
}

inline bool ValueImp::isNull() const
{
    return this == jsNull();
}

inline bool ValueImp::isUndefinedOrNull() const
{
    return this == jsUndefined() || this == jsNull();
}

inline bool ValueImp::isBoolean() const
{
    return !SimpleNumber::is(this) && downcast()->isBoolean();
}

inline bool ValueImp::isNumber() const
{
    return SimpleNumber::is(this) || downcast()->isNumber();
}

inline bool ValueImp::isString() const
{
    return !SimpleNumber::is(this) && downcast()->isString();
}

inline bool ValueImp::isObject() const
{
    return !SimpleNumber::is(this) && downcast()->isObject();
}

inline bool ValueImp::isObject(const ClassInfo *c) const
{
    return !SimpleNumber::is(this) && downcast()->isObject(c);
}

inline bool ValueImp::getBoolean(bool& v) const
{
    return !SimpleNumber::is(this) && downcast()->getBoolean(v);
}

inline bool ValueImp::getNumber(double& v) const
{
    if (SimpleNumber::is(this)) {
        v = SimpleNumber::value(this);
        return true;
    }
    return downcast()->getNumber(v);
}

inline double ValueImp::getNumber() const
{
    return SimpleNumber::is(this) ? SimpleNumber::value(this) : downcast()->getNumber();
}

inline bool ValueImp::getString(UString& s) const
{
    return !SimpleNumber::is(this) && downcast()->getString(s);
}

inline UString ValueImp::getString() const
{
    return SimpleNumber::is(this) ? UString() : downcast()->getString();
}

inline ObjectImp *ValueImp::getObject()
{
    return SimpleNumber::is(this) ? 0 : downcast()->getObject();
}

inline const ObjectImp *ValueImp::getObject() const
{
    return SimpleNumber::is(this) ? 0 : downcast()->getObject();
}

inline bool ValueImp::getUInt32(uint32_t& v) const
{
    if (SimpleNumber::is(this)) {
        long i = SimpleNumber::value(this);
        if (i < 0)
            return false;
        v = i;
        return true;
    }
    return downcast()->getUInt32(v);
}

inline void ValueImp::mark()
{
    if (!SimpleNumber::is(this))
        downcast()->mark();
}

inline bool ValueImp::marked() const
{
    return SimpleNumber::is(this) || downcast()->marked();
}

inline Type ValueImp::type() const
{
    return SimpleNumber::is(this) ? NumberType : downcast()->type();
}

inline ValueImp *ValueImp::toPrimitive(ExecState *exec, Type preferredType) const
{
    return SimpleNumber::is(this) ? const_cast<ValueImp *>(this) : downcast()->toPrimitive(exec, preferredType);
}

inline bool ValueImp::toBoolean(ExecState *exec) const
{
    return SimpleNumber::is(this) ? SimpleNumber::value(this) : downcast()->toBoolean(exec);
}

inline double ValueImp::toNumber(ExecState *exec) const
{
    return SimpleNumber::is(this) ? SimpleNumber::value(this) : downcast()->toNumber(exec);
}

inline double ValueImp::toNumber(ExecState *exec, bool& knownToBeInteger) const
{
    if (SimpleNumber::is(this)) {
        knownToBeInteger = true;
        return SimpleNumber::value(this);
    }
    knownToBeInteger = false;
    return downcast()->toNumber(exec);
}

inline UString ValueImp::toString(ExecState *exec) const
{
   return SimpleNumber::is(this) ? UString::from(SimpleNumber::value(this)) : downcast()->toString(exec);
}

inline ValueImp *jsZero()
{
    return SimpleNumber::make(0);
}

inline ValueImp *jsOne()
{
    return SimpleNumber::make(1);
}

inline ValueImp *jsTwo()
{
    return SimpleNumber::make(2);
}

// compatibility names so we don't have to change so much code

inline AllocatedValueImp *Undefined() { return jsUndefined(); }
inline AllocatedValueImp *Null() { return jsNull(); }
inline AllocatedValueImp *Boolean(bool b) { return jsBoolean(b); }
inline ValueImp *Number(double n) { return jsNumber(n); }
inline ValueImp *Number(int n) { return jsNumber(n); }
inline ValueImp *Number(unsigned n) { return jsNumber(n); }
inline ValueImp *Number(long n) { return jsNumber(n); }
inline ValueImp *Number(unsigned long n) { return jsNumber(n); }
inline AllocatedValueImp *String(const UString& s) { return jsString(s); }
inline AllocatedValueImp *String(const char *s) { return jsString(s); }

} // namespace

#endif // KJS_VALUE_H
