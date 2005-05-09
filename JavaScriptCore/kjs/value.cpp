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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "value.h"
#include "object.h"
#include "types.h"
#include "interpreter.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "internal.h"
#include "collector.h"
#include "operations.h"
#include "error_object.h"
#include "nodes.h"
#include "simple_number.h"

namespace KJS {

// ----------------------------- ValueImp -------------------------------------

void ValueImp::mark()
{
  _marked = true;
}

void* ValueImp::operator new(size_t s)
{
  return Collector::allocate(s);
}

void ValueImp::operator delete(void*)
{
  // Do nothing. So far.
}

bool ValueImp::toUInt32(unsigned&) const
{
  return false;
}

// ECMA 9.4
double ValueImp::toInteger(ExecState *exec) const
{
  uint32_t i;
  if (dispatchToUInt32(i))
    return i;
  return roundValue(exec, Value(const_cast<ValueImp*>(this)));
}

int32_t ValueImp::toInt32(ExecState *exec) const
{
  uint32_t i;
  if (dispatchToUInt32(i))
    return i;

  double d = roundValue(exec, Value(const_cast<ValueImp*>(this)));
  if (isNaN(d) || isInf(d))
    return 0;
  double d32 = fmod(d, D32);

  if (d32 >= D32 / 2.0)
    d32 -= D32;
  else if (d32 < -D32 / 2.0)
    d32 += D32;

  return static_cast<int32_t>(d32);
}

uint32_t ValueImp::toUInt32(ExecState *exec) const
{
  uint32_t i;
  if (dispatchToUInt32(i))
    return i;

  double d = roundValue(exec, Value(const_cast<ValueImp*>(this)));
  if (isNaN(d) || isInf(d))
    return 0;
  double d32 = fmod(d, D32);

  if (d32 < 0)
    d32 += D32;

  return static_cast<uint32_t>(d32);
}

uint16_t ValueImp::toUInt16(ExecState *exec) const
{
  uint32_t i;
  if (dispatchToUInt32(i))
    return i;

  double d = roundValue(exec, Value(const_cast<ValueImp*>(this)));
  if (isNaN(d) || isInf(d))
    return 0;
  double d16 = fmod(d, D16);

  if (d16 < 0)
    d16 += D16;

  return static_cast<uint16_t>(d16);
}

Object ValueImp::dispatchToObject(ExecState *exec) const
{
  if (SimpleNumber::is(this))
    return static_cast<const NumberImp *>(this)->NumberImp::toObject(exec);
  return toObject(exec);
}

bool ValueImp::isUndefinedOrNull() const
{
    switch (dispatchType()) {
        case BooleanType:
        case NumberType:
        case ObjectType:
        case StringType:
            break;
        case NullType:
        case UndefinedType:
            return true;
        case UnspecifiedType:
            assert(false);
            break;
    }
    return false;
}

bool ValueImp::isBoolean(bool &booleanValue) const
{
    if (!isBoolean())
        return false;
    booleanValue = static_cast<const BooleanImp *>(this)->value();
    return true;
}

bool ValueImp::isNumber(double &numericValue) const
{
    if (!isNumber())
        return false;
    numericValue = static_cast<const NumberImp *>(this)->value();
    return true;
}

bool ValueImp::isString(UString &stringValue) const
{
    if (!isString())
        return false;
    stringValue = static_cast<const StringImp *>(this)->value();
    return true;
}

UString ValueImp::asString() const
{
    return isString() ? static_cast<const StringImp *>(this)->value() : UString();
}

bool ValueImp::isObject(ObjectImp *&object)
{
    if (!isObject())
        return false;
    object = static_cast<ObjectImp *>(this);
    return true;
}

// ------------------------------ Value ----------------------------------------

Value::Value(bool b) : rep(b ? BooleanImp::staticTrue : BooleanImp::staticFalse) { }

Value::Value(int i)
    : rep(SimpleNumber::fits(i) ? SimpleNumber::make(i) : new NumberImp(static_cast<double>(i))) { }

Value::Value(unsigned u)
    : rep(SimpleNumber::fits(u) ? SimpleNumber::make(u) : new NumberImp(static_cast<double>(u))) { }

Value::Value(double d)
    : rep(SimpleNumber::fits(d)
        ? SimpleNumber::make(static_cast<long>(d))
        : (KJS::isNaN(d) ? NumberImp::staticNaN : new NumberImp(d)))
{ }

Value::Value(double d, bool knownToBeInteger)
    : rep((knownToBeInteger ? SimpleNumber::integerFits(d) : SimpleNumber::fits(d))
        ? SimpleNumber::make(static_cast<long>(d))
        : ((!knownToBeInteger && KJS::isNaN(d)) ? NumberImp::staticNaN : new NumberImp(d)))
{ }

Value::Value(long l)
    : rep(SimpleNumber::fits(l) ? SimpleNumber::make(l) : new NumberImp(static_cast<double>(l))) { }

Value::Value(unsigned long l)
    : rep(SimpleNumber::fits(l) ? SimpleNumber::make(l) : new NumberImp(static_cast<double>(l))) { }

Value::Value(const char *s) : rep(new StringImp(s)) { }

Value::Value(const UString &s) : rep(new StringImp(s)) { }

// ------------------------------ Undefined ------------------------------------

Undefined::Undefined() : Value(UndefinedImp::staticUndefined)
{
}

Undefined Undefined::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != UndefinedType)
    return Undefined(0);

  return Undefined();
}

// ------------------------------ Null -----------------------------------------

Null::Null() : Value(NullImp::staticNull)
{
}

Null Null::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != NullType)
    return Null(0);

  return Null();
}

// ------------------------------ Boolean --------------------------------------

Boolean::Boolean(bool b)
  : Value(b ? BooleanImp::staticTrue : BooleanImp::staticFalse)
{
}

bool Boolean::value() const
{
  assert(rep);
  return ((BooleanImp*)rep)->value();
}

Boolean Boolean::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != BooleanType)
    return static_cast<BooleanImp*>(0);

  return static_cast<BooleanImp*>(v.imp());
}

// ------------------------------ String ---------------------------------------

String::String(const UString &s) : Value(new StringImp(s))
{
}

UString String::value() const
{
  assert(rep);
  return ((StringImp*)rep)->value();
}

String String::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != StringType)
    return String(0);

  return String(static_cast<StringImp*>(v.imp()));
}

// ------------------------------ Number ---------------------------------------

Number::Number(int i)
  : Value(SimpleNumber::fits(i) ? SimpleNumber::make(i) : new NumberImp(static_cast<double>(i))) { }

Number::Number(unsigned int u)
  : Value(SimpleNumber::fits(u) ? SimpleNumber::make(u) : new NumberImp(static_cast<double>(u))) { }

Number::Number(double d)
  : Value(SimpleNumber::fits(d)
        ? SimpleNumber::make(static_cast<long>(d))
        : (KJS::isNaN(d) ? NumberImp::staticNaN : new NumberImp(d)))
{ }

Number::Number(double d, bool knownToBeInteger)
  : Value((knownToBeInteger ? SimpleNumber::integerFits(d) : SimpleNumber::fits(d))
        ? SimpleNumber::make(static_cast<long>(d))
        : ((!knownToBeInteger && KJS::isNaN(d)) ? NumberImp::staticNaN : new NumberImp(d)))
{ }

Number::Number(long int l)
  : Value(SimpleNumber::fits(l) ? SimpleNumber::make(l) : new NumberImp(static_cast<double>(l))) { }

Number::Number(long unsigned int l)
  : Value(SimpleNumber::fits(l) ? SimpleNumber::make(l) : new NumberImp(static_cast<double>(l))) { }

Number Number::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != NumberType)
    return Number((NumberImp*)0);

  return Number(static_cast<NumberImp*>(v.imp()));
}

double Number::value() const
{
  if (SimpleNumber::is(rep))
    return (double)SimpleNumber::value(rep);
  assert(rep);
  return ((NumberImp*)rep)->value();
}

int Number::intValue() const
{
  if (SimpleNumber::is(rep))
    return SimpleNumber::value(rep);
  return (int)((NumberImp*)rep)->value();
}

bool Number::isNaN() const
{
  return rep == NumberImp::staticNaN;
}

bool Number::isInf() const
{
  if (SimpleNumber::is(rep))
    return false;
  return KJS::isInf(((NumberImp*)rep)->value());
}

ValueImp *undefined()
{
    return UndefinedImp::staticUndefined;
}

ValueImp *null()
{
    return NullImp::staticNull;
}

ValueImp *boolean(bool b)
{
    return b ? BooleanImp::staticTrue : BooleanImp::staticFalse;
}

ValueImp *string(const char *s)
{
    return new StringImp(s ? s : "");
}

ValueImp *string(const UString &s)
{
    return s.isNull() ? new StringImp("") : new StringImp(s);
}

ValueImp *zero()
{
    return SimpleNumber::make(0);
}

ValueImp *one()
{
    return SimpleNumber::make(1);
}

ValueImp *two()
{
    return SimpleNumber::make(2);
}

ValueImp *number(int i)
{
    return SimpleNumber::fits(i) ? SimpleNumber::make(i) : new NumberImp(static_cast<double>(i));
}

ValueImp *number(unsigned i)
{
    return SimpleNumber::fits(i) ? SimpleNumber::make(i) : new NumberImp(static_cast<double>(i));
}

ValueImp *number(long i)
{
    return SimpleNumber::fits(i) ? SimpleNumber::make(i) : new NumberImp(static_cast<double>(i));
}

ValueImp *number(unsigned long i)
{
    return SimpleNumber::fits(i) ? SimpleNumber::make(i) : new NumberImp(static_cast<double>(i));
}

ValueImp *number(double d)
{
    return SimpleNumber::fits(d)
        ? SimpleNumber::make(static_cast<long>(d))
        : (isNaN(d) ? NumberImp::staticNaN : new NumberImp(d));
}

ValueImp *number(double d, bool knownToBeInteger)
{
    return (knownToBeInteger ? SimpleNumber::integerFits(d) : SimpleNumber::fits(d))
        ? SimpleNumber::make(static_cast<long>(d))
        : ((!knownToBeInteger && isNaN(d)) ? NumberImp::staticNaN : new NumberImp(d));
}

}
