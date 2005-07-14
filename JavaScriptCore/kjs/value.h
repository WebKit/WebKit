// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003 Apple Computer, Inc.
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

#ifndef _KJS_VALUE_H_
#define _KJS_VALUE_H_

#ifndef NDEBUG // protection against problems if committing with KJS_VERBOSE on

// Uncomment this to enable very verbose output from KJS
//#define KJS_VERBOSE
// Uncomment this to debug memory allocation and garbage collection
//#define KJS_DEBUG_MEM

#endif

#include <stdlib.h> // Needed for size_t

#include "ustring.h"

#include "simple_number.h"

// Primitive data types

namespace KJS {

  class Value;
  class ValueImp;
  class ValueImpPrivate;
  class Undefined;
  class UndefinedImp;
  class Null;
  class NullImp;
  class Boolean;
  class BooleanImp;
  class String;
  class StringImp;
  class Number;
  class NumberImp;
  class Object;
  class ObjectImp;
  class Reference;
  class ReferenceImp;
  class List;
  class ListImp;
  class Completion;
  class ExecState;
  class ClassInfo;

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
    friend class Collector;
    friend class Value;
    friend class ContextImp;
    friend class FunctionCallNode;
  public:
    ValueImp() : _marked(false) { }
    virtual ~ValueImp() { }

    virtual void mark();
    bool marked() const;
    void* operator new(size_t);
    void operator delete(void*);

    double toInteger(ExecState *exec) const;
    int32_t toInt32(ExecState *exec) const;
    uint32_t toUInt32(ExecState *exec) const;
    uint16_t toUInt16(ExecState *exec) const;

    // Dispatch wrappers that handle the special small number case

    Type dispatchType() const;
    Value dispatchToPrimitive(ExecState *exec, Type preferredType = UnspecifiedType) const;
    bool dispatchToBoolean(ExecState *exec) const;
    double dispatchToNumber(ExecState *exec) const;
    double dispatchToNumber(ExecState *exec, bool &knownToBeInteger) const;
    UString dispatchToString(ExecState *exec) const;
    bool dispatchToUInt32(uint32_t&) const;
    Object dispatchToObject(ExecState *exec) const;

    bool isUndefined() const { return dispatchType() == UndefinedType; }
    bool isNull() const { return dispatchType() == NullType; }
    bool isUndefinedOrNull() const;

    bool isBoolean() const { return dispatchType() == BooleanType; }
    bool isBoolean(bool &booleanValue) const;

    bool isNumber() const { return dispatchType() == NumberType; }
    bool isNumber(double &numericValue) const;

    bool isString() const { return dispatchType() == StringType; }
    bool isString(UString &stringValue) const;
    UString asString() const; // null string if not a string

    bool isObject() const { return dispatchType() == ObjectType; }
    bool isObject(ObjectImp *&object);
    ObjectImp *asObject(); // 0 if not an object
    bool isObject(const ClassInfo *) const; // combine an isObject check with an inherits check

  private:
    virtual Type type() const = 0;

    // The conversion operations

    virtual Value toPrimitive(ExecState *exec, Type preferredType = UnspecifiedType) const = 0;
    virtual bool toBoolean(ExecState *exec) const = 0;
    virtual double toNumber(ExecState *exec) const = 0;
    virtual UString toString(ExecState *exec) const = 0;
    virtual Object toObject(ExecState *exec) const = 0;
    virtual bool toUInt32(unsigned&) const;

    bool _marked;

    // Give a compile time error if we try to copy one of these.
    ValueImp(const ValueImp&);
    ValueImp& operator=(const ValueImp&);
  };
  
  ValueImp *undefined();
  ValueImp *null();

  ValueImp *boolean(bool = false);

  ValueImp *string(const char * = ""); // returns empty string if passed 0
  ValueImp *string(const UString &); // returns empty string if passed null string

  ValueImp *zero();
  ValueImp *one();
  ValueImp *two();
  ValueImp *number(int);
  ValueImp *number(unsigned);
  ValueImp *number(long);
  ValueImp *number(unsigned long);
  ValueImp *number(double);
  ValueImp *number(double, bool knownToBeInteger);

  /**
   * FIXME: Now that we have conservative GC, we will be deprecating the
   * Value wrappers and programming in terms of the ValueImp objects.
   * Eventually we will remove Value and rename ValueImp to Value.
   * We'll need to move the comments from Value to ValueImp too.
   */
  /**
   * Value objects are act as wrappers ("smart pointers") around ValueImp
   * objects and their descendents. Instead of using ValueImps
   * (and derivatives) during normal program execution, you should use a
   * Value-derived class.
   *
   * Value maintains a pointer to a ValueImp object and uses a reference
   * counting scheme to ensure that the ValueImp object is not deleted or
   * garbage collected.
   *
   * Note: The conversion operations all return values of various types -
   * if an error occurs during conversion, an error object will instead
   * be returned (where possible), and the execution state's exception
   * will be set appropriately.
   */
  class Value {
  public:
    Value() : rep(0) { }
    Value(ValueImp *v) : rep(v) { }
    operator ValueImp *() const { return rep; }

    explicit Value(bool);

    explicit Value(int);
    explicit Value(unsigned);
    explicit Value(double);
    explicit Value(long);
    explicit Value(unsigned long);
    Value(double, bool knownToBeInteger);

    explicit Value(const char *);
    Value(const UString &);
    
    /**
     * Returns whether or not this is a valid value. An invalid value
     * has a 0 implementation pointer and should not be used for
     * any other operation than this check. Current use: as a
     * distinct return value signalling failing dynamicCast() calls.
     */
    bool isValid() const { return rep != 0; }
    /**
     * @deprecated
     * Use !isValid() instead.
     */
    bool isNull() const { return rep == 0; }
    ValueImp *imp() const { return rep; }

    /**
     * Returns the type of value. This is one of UndefinedType, NullType,
     * BooleanType, StringType, NumberType, or ObjectType.
     *
     * @return The type of value
     */
    Type type() const { return rep->dispatchType(); }

    /**
     * Checks whether or not the value is of a particular tpye
     *
     * @param t The type to compare with
     * @return true if the value is of the specified type, otherwise false
     */
    bool isA(Type t) const { return rep->dispatchType() == t; }

    /**
     * Performs the ToPrimitive type conversion operation on this value
     * (ECMA 9.1)
     */
    Value toPrimitive(ExecState *exec,
                      Type preferredType = UnspecifiedType) const
      { return rep->dispatchToPrimitive(exec, preferredType); }

    /**
     * Performs the ToBoolean type conversion operation on this value (ECMA 9.2)
     */
    bool toBoolean(ExecState *exec) const { return rep->dispatchToBoolean(exec); }

    /**
     * Performs the ToNumber type conversion operation on this value (ECMA 9.3)
     */
    double toNumber(ExecState *exec) const { return rep->dispatchToNumber(exec); }
    double toNumber(ExecState *exec, bool &knownToBeInteger) const { return rep->dispatchToNumber(exec, knownToBeInteger); }

    /**
     * Performs the ToInteger type conversion operation on this value (ECMA 9.4)
     */
    double toInteger(ExecState *exec) const { return rep->toInteger(exec); }

    /**
     * Performs the ToInt32 type conversion operation on this value (ECMA 9.5)
     */
    int32_t toInt32(ExecState *exec) const { return rep->toInt32(exec); }

    /**
     * Performs the ToUint32 type conversion operation on this value (ECMA 9.6)
     */
    uint32_t toUInt32(ExecState *exec) const { return rep->toUInt32(exec); }

    /**
     * Performs the ToUint16 type conversion operation on this value (ECMA 9.7)
     */
    uint16_t toUInt16(ExecState *exec) const { return rep->toUInt16(exec); }

    /**
     * Performs the ToString type conversion operation on this value (ECMA 9.8)
     */
    UString toString(ExecState *exec) const { return rep->dispatchToString(exec); }

    /**
     * Performs the ToObject type conversion operation on this value (ECMA 9.9)
     */
    Object toObject(ExecState *exec) const;

    /**
     * Checks if we can do a lossless conversion to UInt32.
     */
    bool toUInt32(uint32_t& i) const { return rep->dispatchToUInt32(i); }

  protected:
    ValueImp *rep;
  };

  // Primitive types

  /**
   * Represents an primitive Undefined value. All instances of this class
   * share the same implementation object, so == will always return true
   * for any comparison between two Undefined objects.
   */
  class Undefined : public Value {
  public:
    Undefined();

    /**
     * Converts a Value into an Undefined. If the value's type is not
     * UndefinedType, a null object will be returned (i.e. one with it's
     * internal pointer set to 0). If you do not know for sure whether the
     * value is of type UndefinedType, you should check the isValid()
     * methods afterwards before calling any methods on the returned value.
     *
     * @return The value converted to an Undefined
     */
    static Undefined dynamicCast(const Value &v);
  private:
    friend class UndefinedImp;
    explicit Undefined(UndefinedImp *v);

  };

  /**
   * Represents an primitive Null value. All instances of this class
   * share the same implementation object, so == will always return true
   * for any comparison between two Null objects.
   */
  class Null : public Value {
  public:
    Null();

    /**
     * Converts a Value into an Null. If the value's type is not NullType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * NullType, you should check the isValid() methods afterwards before
     * calling any methods on the returned value.
     *
     * @return The value converted to a Null
     */
    static Null dynamicCast(const Value &v);
  private:
    friend class NullImp;
    explicit Null(NullImp *v);
  };

  /**
   * Represents an primitive Boolean value
   */
  class Boolean : public Value {
  public:
    Boolean(bool b = false);

    /**
     * Converts a Value into an Boolean. If the value's type is not BooleanType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * BooleanType, you should check the isValid() methods afterwards before
     * calling any methods on the returned value.
     *
     * @return The value converted to a Boolean
     */
    static Boolean dynamicCast(const Value &v);

    bool value() const;
  private:
    friend class BooleanImp;
    explicit Boolean(BooleanImp *v);
  };

  /**
   * Represents an primitive String value
   */
  class String : public Value {
  public:
    String(const UString &s = "");

    /**
     * Converts a Value into an String. If the value's type is not StringType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * StringType, you should check the isValid() methods afterwards before
     * calling any methods on the returned value.
     *
     * @return The value converted to a String
     */
    static String dynamicCast(const Value &v);

    UString value() const;
  private:
    friend class StringImp;
    explicit String(StringImp *v);
  };

  extern const double NaN;
  extern const double Inf;

  /**
   * Represents an primitive Number value
   */
  class Number : public Value {
    friend class ValueImp;
  public:
    Number(int i);
    Number(unsigned int u);
    Number(double d = 0.0);
    Number(long int l);
    Number(long unsigned int l);
    Number(double d, bool knownToBeInteger);

    double value() const;
    int intValue() const;

    bool isNaN() const;
    bool isInf() const;

    /**
     * Converts a Value into an Number. If the value's type is not NumberType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * NumberType, you should check the isNull() methods afterwards before
     * calling any methods on the returned value.
     *
     * @return The value converted to a Number
     */
    static Number dynamicCast(const Value &v);
  private:
    friend class NumberImp;
    explicit Number(NumberImp *v);
  };

inline bool ValueImp::marked() const
{
  // Simple numbers are always considered marked.
  return SimpleNumber::is(this) || _marked;
}

// Dispatchers for virtual functions, to special-case simple numbers which
// won't be real pointers.

inline Type ValueImp::dispatchType() const
{
  if (SimpleNumber::is(this))
    return NumberType;
  return type();
}

inline Value ValueImp::dispatchToPrimitive(ExecState *exec, Type preferredType) const
{
    if (SimpleNumber::is(this))
        return Value(const_cast<ValueImp *>(this));
    return toPrimitive(exec, preferredType);
}

inline bool ValueImp::dispatchToBoolean(ExecState *exec) const
{
    if (SimpleNumber::is(this))
        return SimpleNumber::value(this);
    return toBoolean(exec);
}

inline double ValueImp::dispatchToNumber(ExecState *exec) const
{
    if (SimpleNumber::is(this))
        return SimpleNumber::value(this);
    return toNumber(exec);
}

inline double ValueImp::dispatchToNumber(ExecState *exec, bool &knownToBeInteger) const
{
    if (SimpleNumber::is(this)) {
        knownToBeInteger = true;
        return SimpleNumber::value(this);
    }
    knownToBeInteger = false;
    return toNumber(exec);
}

inline UString ValueImp::dispatchToString(ExecState *exec) const
{
    if (SimpleNumber::is(this))
        return UString::from(SimpleNumber::value(this));
    return toString(exec);
}

inline bool ValueImp::dispatchToUInt32(uint32_t& result) const
{
    if (SimpleNumber::is(this)) {
        long i = SimpleNumber::value(this);
        if (i < 0)
            return false;
        result = i;
        return true;
    }
    return toUInt32(result);
}

} // namespace

#endif // _KJS_VALUE_H_
