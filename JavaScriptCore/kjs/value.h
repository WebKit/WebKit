// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
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
  class CompletionImp;
  class ExecState;

  /**
   * Primitive types
   */
  enum Type {
    UnspecifiedType = 0,
    UndefinedType   = 1,
    NullType        = 2,
    BooleanType     = 3,
    StringType      = 4,
    NumberType      = 5,
    ObjectType      = 6,
    ReferenceType   = 7,
    ListType        = 8,
    CompletionType  = 9
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
  public:
    ValueImp();
    virtual ~ValueImp();

    inline ValueImp* ref() { refcount++; return this; }
    inline bool deref() { return (!--refcount); }
    unsigned int refcount;

    virtual void mark();
    bool marked() const;
    void* operator new(size_t);
    void operator delete(void*);

    /**
     * @internal
     *
     * set by Object() so that the collector is allowed to delete us
     */
    void setGcAllowed();

    virtual Type type() const = 0;

    // The conversion operations

    virtual Value toPrimitive(ExecState *exec,
                              Type preferredType = UnspecifiedType) const = 0;
    virtual bool toBoolean(ExecState *exec) const = 0;
    virtual double toNumber(ExecState *exec) const = 0;
    virtual int toInteger(ExecState *exec) const;
    virtual int toInt32(ExecState *exec) const;
    virtual unsigned int toUInt32(ExecState *exec) const;
    virtual unsigned short toUInt16(ExecState *exec) const;
    virtual UString toString(ExecState *exec) const = 0;
    virtual Object toObject(ExecState *exec) const = 0;

    // Reference operations

    virtual Value getBase(ExecState *exec) const;
    virtual UString getPropertyName(ExecState *exec) const;
    virtual Value getValue(ExecState *exec) const;
    virtual void putValue(ExecState *exec, const Value w);

  private:
    enum {
      VI_MARKED = 1,
      VI_GCALLOWED = 2,
      VI_CREATED = 4,
      VI_DESTRUCTED = 8
    }; // VI means VALUEIMPL

    ValueImpPrivate *_vd;
    unsigned int _flags;
  };

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
    Value();
    explicit Value(ValueImp *v);
    Value(const Value &v);
    virtual ~Value();

    Value& operator=(const Value &v);
    bool isNull() const;
    ValueImp *imp() const;

    /**
     * Returns the type of value. This is one of UndefinedType, NullType,
     * BooleanType, StringType NumberType, ObjectType, ReferenceType,
     * ListType or CompletionType.
     *
     * @return The type of value
     */
    Type type() const;

    /**
     * Checks whether or not the value is of a particular tpye
     *
     * @param The type to compare with
     * @return true if the value is of the specified type, otherwise false
     */
    bool isA(Type t) const;

    /**
     * Performs the ToPrimitive type conversion operation on this value
     * (ECMA 9.1)
     */
    Value toPrimitive(ExecState *exec,
                      Type preferredType = UnspecifiedType) const;

    /**
     * Performs the ToBoolean type conversion operation on this value (ECMA 9.2)
     */
    bool toBoolean(ExecState *exec) const;

    /**
     * Performs the ToNumber type conversion operation on this value (ECMA 9.3)
     */
    double toNumber(ExecState *exec) const;

    /**
     * Performs the ToInteger type conversion operation on this value (ECMA 9.4)
     */
    int toInteger(ExecState *exec) const;

    /**
     * Performs the ToInt32 type conversion operation on this value (ECMA 9.5)
     */
    int toInt32(ExecState *exec) const;

    /**
     * Performs the ToUint32 type conversion operation on this value (ECMA 9.6)
     */
    unsigned int toUInt32(ExecState *exec) const;

    /**
     * Performs the ToUint16 type conversion operation on this value (ECMA 9.7)
     */
    unsigned short toUInt16(ExecState *exec) const;

    /**
     * Performs the ToString type conversion operation on this value (ECMA 9.8)
     */
    UString toString(ExecState *exec) const;

    /**
     * Performs the ToObject type conversion operation on this value (ECMA 9.9)
     */
    Object toObject(ExecState *exec) const;

    /**
     * Performs the GetBase type conversion operation on this value (ECMA 8.7)
     *
     * Since references are supposed to have an Object or null as their base,
     * this method is guaranteed to return either Null() or an Object value.
     */
    Value getBase(ExecState *exec) const;

    /**
     * Performs the GetPropertyName type conversion operation on this value
     * (ECMA 8.7)
     */
    UString getPropertyName(ExecState *exec) const;

    /**
     * Performs the GetValue type conversion operation on this value
     * (ECMA 8.7.1)
     */
    Value getValue(ExecState *exec) const;

    /**
     * Performs the PutValue type conversion operation on this value
     * (ECMA 8.7.1)
     */
    void putValue(ExecState *exec, const Value w);

  protected:
    ValueImp *rep;
  };

  bool operator==(const Value &v1, const Value &v2);
  bool operator!=(const Value &v1, const Value &v2);

  // Primitive types

  /**
   * Represents an primitive Undefined value. All instances of this class
   * share the same implementation object, so == will always return true
   * for any comparison between two Undefined objects.
   */
  class Undefined : public Value {
  public:
    Undefined();
    Undefined(const Undefined &v);
    virtual ~Undefined();

    Undefined& operator=(const Undefined &v);

    /**
     * Converts a Value into an Undefined. If the value's type is not
     * UndefinedType, a null object will be returned (i.e. one with it's
     * internal pointer set to 0). If you do not know for sure whether the
     * value is of type UndefinedType, you should check the @ref isNull()
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
    Null(const Null &v);
    virtual ~Null();

    Null& operator=(const Null &v);

    /**
     * Converts a Value into an Null. If the value's type is not NullType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * NullType, you should check the @ref isNull() methods afterwards before
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
   * Represents an primitive Null value
   */
  class Boolean : public Value {
  public:
    Boolean(bool b = false);
    Boolean(const Boolean &v);
    virtual ~Boolean();

    Boolean& operator=(const Boolean &v);

    /**
     * Converts a Value into an Boolean. If the value's type is not BooleanType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * BooleanType, you should check the @ref isNull() methods afterwards before
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
   * Represents an primitive Null value
   */
  class String : public Value {
  public:
    String(const UString &s = "");
    String(const String &v);
    virtual ~String();

    String& operator=(const String &v);

    /**
     * Converts a Value into an String. If the value's type is not StringType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * StringType, you should check the @ref isNull() methods afterwards before
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
  public:
    Number(int i);
    Number(unsigned int u);
    Number(double d = 0.0);
    Number(long int l);
    Number(long unsigned int l);
    Number(const Number &v);
    virtual ~Number();

    Number& operator=(const Number &v);

    double value() const;
    int intValue() const;

    bool isNaN() const;
    bool isInf() const;

    /**
     * Converts a Value into an Number. If the value's type is not NumberType,
     * a null object will be returned (i.e. one with it's internal pointer set
     * to 0). If you do not know for sure whether the value is of type
     * NumberType, you should check the @ref isNull() methods afterwards before
     * calling any methods on the returned value.
     *
     * @return The value converted to a Number
     */
    static Number dynamicCast(const Value &v);
  private:
    friend class NumberImp;
    explicit Number(NumberImp *v);
  };

}; // namespace

#endif // _KJS_VALUE_H_
