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
  // FIXNUM: need special case for fixnum, they should act as if
  // always marked.
  return (_flags & VI_MARKED);
}

void ValueImp::setGcAllowed()
{
  // FIXNUM: need special case for fixnum, should be a no-op
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

bool ValueImp::toUInt32(unsigned&) const
{
  return false;
}

// ECMA 9.4
int ValueImp::toInteger(ExecState *exec) const
{
  unsigned i;
  if (toUInt32(i))
    return (int)i;
  return int(roundValue(exec, Value(const_cast<ValueImp*>(this))));
}

int ValueImp::toInt32(ExecState *exec) const
{
  unsigned i;
  if (toUInt32(i))
    return (int)i;

  double d = roundValue(exec, Value(const_cast<ValueImp*>(this)));
  double d32 = fmod(d, D32);

  if (d32 >= D32 / 2.0)
    d32 -= D32;

  return static_cast<int>(d32);
}

unsigned int ValueImp::toUInt32(ExecState *exec) const
{
  unsigned i;
  if (toUInt32(i))
    return i;

  double d = roundValue(exec, Value(const_cast<ValueImp*>(this)));
  double d32 = fmod(d, D32);

  return static_cast<unsigned int>(d32);
}

unsigned short ValueImp::toUInt16(ExecState *exec) const
{
  unsigned i;
  if (toUInt32(i))
    return (unsigned short)i;

  double d = roundValue(exec, Value(const_cast<ValueImp*>(this)));
  double d16 = fmod(d, D16);

  return static_cast<unsigned short>(d16);
}

// ECMA 8.7.1
Value ValueImp::getBase(ExecState *exec) const
{
  Object err = Error::create(exec, ReferenceError, I18N_NOOP("Invalid reference base"));
  exec->setException(err);
  return err;
}

// ECMA 8.7.2
UString ValueImp::getPropertyName(ExecState * /*exec*/) const
{
  // the spec wants a runtime error here. But getValue() and putValue()
  // will catch this case on their own earlier. When returning a Null
  // string we should be on the safe side.
  return UString();
}

// ECMA 8.7.1
Value ValueImp::getValue(ExecState *exec) const
{
  return Value(const_cast<ValueImp*>(this));
}

void ValueImp::putValue(ExecState *exec, const Value& w)
{
  Object err = Error::create(exec,ReferenceError);
  exec->setException(err);
}

bool ValueImp::deleteValue(ExecState *exec)
{
  Object err = Error::create(exec,ReferenceError);
  exec->setException(err);
  return false;
}


// Dispatchers for virtual functions, to special-case fixnums which
// won't be real pointers

Type ValueImp::dispatchType() const
{
  // FIXNUM: need special case for fixnums here 
  return this->type();
}

Value ValueImp::dispatchToPrimitive(ExecState *exec, Type preferredType) const
{
  // FIXNUM: need special case for fixnums here 
  return this->toPrimitive(exec, preferredType);
}

bool ValueImp::dispatchToBoolean(ExecState *exec) const
{
  // FIXNUM: need special case for fixnums here 
  return this->toBoolean(exec);
}

double ValueImp::dispatchToNumber(ExecState *exec) const
{
  // FIXNUM: need special case for fixnums here 
  return this->toNumber(exec);
}

UString ValueImp::dispatchToString(ExecState *exec) const
{
  // FIXNUM: need special case for fixnums here 
  return this->toString(exec);
}

Object ValueImp::dispatchToObject(ExecState *exec) const
{
  // FIXNUM: need special case for fixnums here 
  return this->toObject(exec);
}

bool ValueImp::dispatchToUInt32(unsigned& result) const
{
  // FIXNUM: need special case for fixnums here 
  return this->toUInt32(result);
}

Value ValueImp::dispatchGetBase(ExecState *exec) const
{
  // FIXNUM: need special case for fixnums here 
  return this->getBase(exec);
}

UString ValueImp::dispatchGetPropertyName(ExecState *exec) const
{
  // FIXNUM: need special case for fixnums here 
  return this->getPropertyName(exec);
}

#if 0
Value ValueImp::dispatchGetValue(ExecState *exec) const
{
  // FIXNUM: need special case for fixnums here 
  return this->getValue(exec);
}
#endif

void ValueImp::dispatchPutValue(ExecState *exec, const Value& w)
{
  // FIXNUM: need special case for fixnums here 
  return this->putValue(exec, w);
}

bool ValueImp::dispatchDeleteValue(ExecState *exec)
{
  // FIXNUM: need special case for fixnums here 
  return this->deleteValue(exec);
}


// ------------------------------ Value ----------------------------------------

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

String::String(const UString &s) : Value(new StringImp(UString(s)))
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

// FIXNUM: need fixnum special case in below constructor
Number::Number(int i)
  : Value(new NumberImp(static_cast<double>(i))) { }

// FIXNUM: need fixnum special case in below constructor
Number::Number(unsigned int u)
  : Value(new NumberImp(static_cast<double>(u))) { }

// FIXNUM: need fixnum special case in below constructor
Number::Number(double d)
  : Value(new NumberImp(d)) { }

// FIXNUM: need fixnum special case in below constructor
Number::Number(long int l)
  : Value(new NumberImp(static_cast<double>(l))) { }

// FIXNUM: need fixnum special case in below constructor
Number::Number(long unsigned int l)
  : Value(new NumberImp(static_cast<double>(l))) { }

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
