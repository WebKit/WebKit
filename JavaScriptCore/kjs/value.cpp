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

using namespace KJS;

// ----------------------------- ValueImp -------------------------------------

ValueImp::ValueImp() :
  refcount(0),
  // Tell the garbage collector that this memory block corresponds to a real object now
  _flags(VI_CREATED)
{
  //fprintf(stderr,"ValueImp::ValueImp %p\n",(void*)this);
}

ValueImp::~ValueImp()
{
  //fprintf(stderr,"ValueImp::~ValueImp %p\n",(void*)this);
  _flags |= VI_DESTRUCTED;
}

void ValueImp::mark()
{
  //fprintf(stderr,"ValueImp::mark %p\n",(void*)this);
  _flags |= VI_MARKED;
}

bool ValueImp::marked() const
{
  return (_flags & VI_MARKED);
}

void ValueImp::setGcAllowed()
{
  //fprintf(stderr,"ValueImp::setGcAllowed %p\n",(void*)this);
  _flags |= VI_GCALLOWED;
}

void* ValueImp::operator new(size_t s)
{
  return Collector::allocate(s);
}

void ValueImp::operator delete(void*)
{
  // Do nothing. So far.
}

// ECMA 9.4
int ValueImp::toInteger(ExecState *exec) const
{
  return int(roundValue(exec, Value(const_cast<ValueImp*>(this))));
}

int ValueImp::toInt32(ExecState *exec) const
{
  double d = roundValue(exec, Value(const_cast<ValueImp*>(this)));
  double d32 = fmod(d, D32);

  if (d32 >= D32 / 2.0)
    d32 -= D32;

  return static_cast<int>(d32);
}

unsigned int ValueImp::toUInt32(ExecState *exec) const
{
  double d = roundValue(exec, Value(const_cast<ValueImp*>(this)));
  double d32 = fmod(d, D32);

  return static_cast<unsigned int>(d32);
}

unsigned short ValueImp::toUInt16(ExecState *exec) const
{
  double d = roundValue(exec, Value(const_cast<ValueImp*>(this)));
  double d16 = fmod(d, D16);

  return static_cast<unsigned short>(d16);
}

// ECMA 8.7.1
Value ValueImp::getBase(ExecState *exec) const
{
  if (type() != ReferenceType) {
    Object err = Error::create(exec, ReferenceError, I18N_NOOP("Invalid reference base"));
    exec->setException(err);
    return err;
  }

  return (static_cast<const ReferenceImp*>(this))->getBase();
}

// ECMA 8.7.2
UString ValueImp::getPropertyName(ExecState * /*exec*/) const
{
  if (type() != ReferenceType)
    // the spec wants a runtime error here. But getValue() and putValue()
    // will catch this case on their own earlier. When returning a Null
    // string we should be on the safe side.
    return UString();

  return (static_cast<const ReferenceImp*>(this))->getPropertyName();
}

// ECMA 8.7.1
Value ValueImp::getValue(ExecState *exec) const
{
  if (type() != ReferenceType)
    return Value(const_cast<ValueImp*>(this));

  Value o = getBase(exec);

  if (o.isNull() || o.type() == NullType) {
    UString m = I18N_NOOP("Can't find variable: ") + getPropertyName(exec);
    Object err = Error::create(exec, ReferenceError, m.ascii());
    exec->setException(err);
    return err;
  }

  if (o.type() != ObjectType) {
    UString m = I18N_NOOP("Base is not an object");
    Object err = Error::create(exec, ReferenceError, m.ascii());
    exec->setException(err);
    return err;
  }

  return static_cast<ObjectImp*>(o.imp())->get(exec,getPropertyName(exec));
}

void ValueImp::putValue(ExecState *exec, const Value w)
{
  if (type() != ReferenceType) {
    Object err = Error::create(exec,ReferenceError);
    exec->setException(err);
    return;
  }

#ifdef KJS_VERBOSE
  printInfo(exec,(UString("setting property ")+getPropertyName(exec)).cstring().c_str(),w);
#endif
  Value o = getBase(exec);
  if (o.type() == NullType)
    exec->interpreter()->globalObject().put(exec,getPropertyName(exec), w);
  else {
    static_cast<ObjectImp*>(o.imp())->put(exec,getPropertyName(exec), w);
  }

  return;
}

bool KJS::operator==(const Value &v1, const Value &v2)
{
  return (v1.imp() == v2.imp());
}

bool KJS::operator!=(const Value &v1, const Value &v2)
{
  return (v1.imp() != v2.imp());
}




// ------------------------------ Value ----------------------------------------

Value::Value()
{
  rep = 0;
}

Value::Value(ValueImp *v)
{
  rep = v;
  if (rep)
  {
    rep->ref();
    //fprintf(stderr, "Value::Value(%p) imp=%p ref=%d\n", this, rep, rep->refcount);
    v->setGcAllowed();
  }
}

Value::Value(const Value &v)
{
  rep = v.imp();
  if (rep)
  {
    rep->ref();
    //fprintf(stderr, "Value::Value(%p)(copying %p) imp=%p ref=%d\n", this, &v, rep, rep->refcount);
  }
}

Value::~Value()
{
  if (rep)
  {
    rep->deref();
    //fprintf(stderr, "Value::~Value(%p) imp=%p ref=%d\n", this, rep, rep->refcount);
  }
}

Value& Value::operator=(const Value &v)
{
  if (rep) {
    rep->deref();
    //fprintf(stderr, "Value::operator=(%p)(copying %p) old imp=%p ref=%d\n", this, &v, rep, rep->refcount);
  }
  rep = v.imp();
  if (rep)
  {
    rep->ref();
    //fprintf(stderr, "Value::operator=(%p)(copying %p) imp=%p ref=%d\n", this, &v, rep, rep->refcount);
  }
  return *this;
}

bool Value::isNull() const
{
  return (rep == 0);
}

ValueImp *Value::imp() const
{
  return rep;
}

Type Value::type() const
{
  return rep->type();
}

bool Value::isA(Type t) const
{
  return (type() == t);
}

Value Value::toPrimitive(ExecState *exec, Type preferredType) const
{
  return rep->toPrimitive(exec,preferredType);
}

bool Value::toBoolean(ExecState *exec) const
{
  return rep->toBoolean(exec);
}

double Value::toNumber(ExecState *exec) const
{
  return rep->toNumber(exec);
}

int Value::toInteger(ExecState *exec) const
{
  return rep->toInteger(exec);
}

int Value::toInt32(ExecState *exec) const
{
  return rep->toInt32(exec);
}

unsigned int Value::toUInt32(ExecState *exec) const
{
  return rep->toUInt32(exec);
}

unsigned short Value::toUInt16(ExecState *exec) const
{
  return rep->toUInt16(exec);
}

UString Value::toString(ExecState *exec) const
{
  return rep->toString(exec);
}

Object Value::toObject(ExecState *exec) const
{
  return rep->toObject(exec);
}

Value Value::getBase(ExecState *exec) const
{
  return rep->getBase(exec);
}

UString Value::getPropertyName(ExecState *exec) const
{
  return rep->getPropertyName(exec);
}

Value Value::getValue(ExecState *exec) const
{
  return rep->getValue(exec);
}

void Value::putValue(ExecState *exec, const Value w)
{
  rep->putValue(exec,w);
}

// ------------------------------ Undefined ------------------------------------

Undefined::Undefined() : Value(UndefinedImp::staticUndefined)
{
}

Undefined::~Undefined() {
}

Undefined::Undefined(UndefinedImp *v) : Value(v)
{
}

Undefined::Undefined(const Undefined &v) : Value(v)
{
}

Undefined& Undefined::operator=(const Undefined &v)
{
  Value::operator=(v);
  return *this;
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

Null::~Null() {
}


Null::Null(NullImp *v) : Value(v)
{
}

Null::Null(const Null &v) : Value(v)
{
}

Null& Null::operator=(const Null &v)
{
  Value::operator=(v);
  return *this;
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

Boolean::~Boolean() { }



Boolean::Boolean(BooleanImp *v) : Value(v)
{
}

Boolean::Boolean(const Boolean &v) : Value(v)
{
}

Boolean& Boolean::operator=(const Boolean &v)
{
  Value::operator=(v);
  return *this;
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

String::String(const UString &s) : Value(new StringImp(UString(s)))
{
}

String::~String() { }

String::String(StringImp *v) : Value(v)
{
}

String::String(const String &v) : Value(v)
{
}

String& String::operator=(const String &v)
{
  Value::operator=(v);
  return *this;
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
  : Value(new NumberImp(static_cast<double>(i))) { }

Number::Number(unsigned int u)
  : Value(new NumberImp(static_cast<double>(u))) { }

Number::Number(double d)
  : Value(new NumberImp(d)) { }

Number::Number(long int l)
  : Value(new NumberImp(static_cast<double>(l))) { }

Number::Number(long unsigned int l)
  : Value(new NumberImp(static_cast<double>(l))) { }

Number::~Number() { }

Number::Number(NumberImp *v) : Value(v)
{
}

Number::Number(const Number &v) : Value(v)
{
}

Number& Number::operator=(const Number &v)
{
  Value::operator=(v);
  return *this;
}

Number Number::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != NumberType)
    return Number((NumberImp*)0);

  return Number(static_cast<NumberImp*>(v.imp()));
}

double Number::value() const
{
  assert(rep);
  return ((NumberImp*)rep)->value();
}

int Number::intValue() const
{
  return int(value());
}

bool Number::isNaN() const
{
  return KJS::isNaN(value());
}

bool Number::isInf() const
{
  return KJS::isInf(value());
}

