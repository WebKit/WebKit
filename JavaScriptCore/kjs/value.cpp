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

using namespace KJS;

// ----------------------------- ValueImp -------------------------------------

#if !USE_CONSERVATIVE_GC
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
}
#endif

#if TEST_CONSERVATIVE_GC
static bool conservativeMark = false;

void ValueImp::useConservativeMark(bool use)
{
  conservativeMark = use;
}
#endif

void ValueImp::mark()
{
  //fprintf(stderr,"ValueImp::mark %p\n",(void*)this);
#if USE_CONSERVATIVE_GC
  _marked = true;
#elif TEST_CONSERVATIVE_GC
  if (conservativeMark) {
    _flags |= VI_CONSERVATIVE_MARKED;
  } else {
    if (!(_flags & VI_CONSERVATIVE_MARKED)) {
      printf("Conservative collector missed ValueImp 0x%x. refcount %d, protect count %d\n", (int)this, refcount, ProtectedValues::getProtectCount(this));
    }
    _flags |= VI_MARKED;
  }
#else
  _flags |= VI_MARKED;
#endif
}

#if !USE_CONSERVATIVE_GC
void ValueImp::setGcAllowed()
{
  //fprintf(stderr,"ValueImp::setGcAllowed %p\n",(void*)this);
  // simple numbers are never seen by the collector so setting this
  // flag is irrelevant
  if (!SimpleNumber::is(this))
    _flags |= VI_GCALLOWED;
}
#endif

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

// ------------------------------ Value ----------------------------------------

#if !USE_CONSERVATIVE_GC

Value::Value(ValueImp *v)
{
  rep = v;
#if DEBUG_COLLECTOR
  assert (!(rep && !SimpleNumber::is(rep) && *((uint32_t *)rep) == 0 ));
  assert (!(rep && !SimpleNumber::is(rep) && rep->_flags & ValueImp::VI_MARKED));
#endif
  if (v)
  {
    v->ref();
    //fprintf(stderr, "Value::Value(%p) imp=%p ref=%d\n", this, rep, rep->refcount);
    v->setGcAllowed();
  }
}

Value::Value(const Value &v)
{
  rep = v.imp();
#if DEBUG_COLLECTOR
  assert (!(rep && !SimpleNumber::is(rep) && *((uint32_t *)rep) == 0 ));
  assert (!(rep && !SimpleNumber::is(rep) && rep->_flags & ValueImp::VI_MARKED));
#endif
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
#endif

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
