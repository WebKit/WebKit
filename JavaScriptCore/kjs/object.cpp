/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2000 Harri Porten (porten@kde.org)
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
 */

#include "object.h"

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "kjs.h"
#include "types.h"
#include "internal.h"
#include "operations.h"
#include "collector.h"
#include "error_object.h"

namespace KJS {

#ifdef WORDS_BIGENDIAN
  unsigned char NaN_Bytes[] = { 0x7f, 0xf8, 0, 0, 0, 0, 0, 0 };
  unsigned char Inf_Bytes[] = { 0x7f, 0xf0, 0, 0, 0, 0, 0, 0 };
#elif defined(arm)
  unsigned char NaN_Bytes[] = { 0, 0, 0xf8, 0x7f, 0, 0, 0, 0 };
  unsigned char Inf_Bytes[] = { 0, 0, 0xf0, 0x7f, 0, 0, 0, 0 };
#else
  unsigned char NaN_Bytes[] = { 0, 0, 0, 0, 0, 0, 0xf8, 0x7f };
  unsigned char Inf_Bytes[] = { 0, 0, 0, 0, 0, 0, 0xf0, 0x7f };
#endif

  const double NaN = *(const double*) NaN_Bytes;
  const double Inf = *(const double*) Inf_Bytes;
  const double D16 = 65536.0;
  const double D31 = 2147483648.0; 	/* TODO: remove in next version */
  const double D32 = 4294967296.0;

  // TODO: -0
};

using namespace KJS;

const TypeInfo Imp::info = { "Imp", AbstractType, 0, 0, 0 };

namespace KJS {
  struct Property {
    UString name;
    Imp *object;
    int attribute;
    Property *next;
  };
}

KJSO::KJSO()
  : rep(0)
{
#ifdef KJS_DEBUG_MEM
  count++;
#endif
}

KJSO::KJSO(Imp *d)
  : rep(d)
{
#ifdef KJS_DEBUG_MEM
  count++;
#endif

  if (rep) {
    rep->ref();
    rep->setGcAllowed(true);
  }
}

KJSO::KJSO(const KJSO &o)
{
#ifdef KJS_DEBUG_MEM
  count++;
#endif

  rep = o.rep;
  if (rep) {
    rep->ref();
    rep->setGcAllowed(true);
  }
}

KJSO& KJSO::operator=(const KJSO &o)
{
  if (rep)
    rep->deref();
  rep = o.rep;
  if (rep) {
    rep->ref();
    rep->setGcAllowed(true);
  }

  return *this;
}

KJSO::~KJSO()
{
#ifdef KJS_DEBUG_MEM
  count--;
#endif

  if (rep)
    rep->deref();
}

bool KJSO::isDefined() const
{
  return !isA(UndefinedType);
}

bool KJSO::isNull() const
{
  return !rep;
}

Type KJSO::type() const
{
#ifdef KJS_VERBOSE
  if (!rep)
    fprintf(stderr, "requesting type of null object\n");
#endif

  return rep ? rep->typeInfo()->type : UndefinedType;
}

bool KJSO::isObject() const
{
  return (type() >= ObjectType);
}

bool KJSO::isA(const char *s) const
{
  assert(rep);
  const TypeInfo *info = rep->typeInfo();

  if (!s || !info || !info->name)
    return false;

  if (info->type == HostType && strcmp(s, "HostObject") == 0)
    return true;

  return (strcmp(s, info->name) == 0);
}

bool KJSO::derivedFrom(const char *s) const
{
  if (!s)
    return false;

  assert(rep);
  const TypeInfo *info = rep->typeInfo();
  while (info) {
    if (info->name && strcmp(s, info->name) == 0)
      return true;
    info = info->base;
  }

  return false;
}

KJSO KJSO::toPrimitive(Type preferred) const
{
  assert(rep);
  return rep->toPrimitive(preferred);
}

Boolean KJSO::toBoolean() const
{
  assert(rep);
  return rep->toBoolean();
}

Number KJSO::toNumber() const
{
  assert(rep);
  return rep->toNumber();
}

// helper function for toInteger, toInt32, toUInt32 and toUInt16
double KJSO::round() const
{
  if (isA(UndefinedType)) /* TODO: see below */
    return 0.0;
  Number n = toNumber();
  if (n.value() == 0.0)   /* TODO: -0, NaN, Inf */
    return 0.0;
  double d = floor(fabs(n.value()));
  if (n.value() < 0)
    d *= -1;

  return d;
}

// ECMA 9.4
Number KJSO::toInteger() const
{
  return Number(round());
}

// ECMA 9.5
int KJSO::toInt32() const
{
  double d = round();
  double d32 = fmod(d, D32);

  if (d32 >= D32 / 2.0)
    d32 -= D32;

  return static_cast<int>(d32);
}

// ECMA 9.6
unsigned int KJSO::toUInt32() const
{
  double d = round();
  double d32 = fmod(d, D32);

  return static_cast<unsigned int>(d32);
}

// ECMA 9.7
unsigned short KJSO::toUInt16() const
{
  double d = round();
  double d16 = fmod(d, D16);

  return static_cast<unsigned short>(d16);
}

String KJSO::toString() const
{
  assert(rep);
  return rep->toString();
}

Object KJSO::toObject() const
{
  assert(rep);
  if (isObject())
    return Object(rep);

  return rep->toObject();
}

bool KJSO::implementsCall() const
{
  return (type() == FunctionType ||
	  type() == InternalFunctionType ||
	  type() == ConstructorType ||
	  type() == DeclaredFunctionType ||
	  type() == AnonymousFunctionType);
}

// [[call]]
KJSO KJSO::executeCall(const KJSO &thisV, const List *args)
{
  assert(rep);
  assert(implementsCall());
  return (static_cast<FunctionImp*>(rep))->executeCall(thisV.imp(), args);
}

KJSO KJSO::executeCall(const KJSO &thisV, const List *args, const List *extraScope) const
{
  assert(rep);
  assert(implementsCall());
  return (static_cast<FunctionImp*>(rep))->executeCall(thisV.imp(), args, extraScope);
}

void KJSO::setConstructor(KJSO c)
{
  put("constructor", c, DontEnum | DontDelete | ReadOnly);
}

// ECMA 8.7.1
KJSO KJSO::getBase() const
{
  if (!isA(ReferenceType))
    return Error::create(ReferenceError, I18N_NOOP("Invalid reference base"));

  return ((ReferenceImp*)rep)->getBase();
}

// ECMA 8.7.2
UString KJSO::getPropertyName() const
{
  if (!isA(ReferenceType))
    // the spec wants a runtime error here. But getValue() and putValue()
    // will catch this case on their own earlier. When returning a Null
    // string we should be on the safe side.
    return UString();

  return ((ReferenceImp*)rep)->getPropertyName();
}

// ECMA 8.7.1
KJSO KJSO::getValue() const
{
  if (!isA(ReferenceType)) {
    return *this;
  }
  KJSO o = getBase();
  if (o.isNull() || o.isA(NullType)) {
    UString m = I18N_NOOP("Can't find variable: ") + getPropertyName();
    return Error::create(ReferenceError, m.ascii());
  }

  return o.get(getPropertyName());
}

/* TODO: remove in next version */
KJSO KJSO::getValue()
{
    return const_cast<const KJSO*>(this)->getValue();
}

// ECMA 8.7.2
ErrorType KJSO::putValue(const KJSO& v)
{
  if (!isA(ReferenceType))
    return ReferenceError;

  KJSO o = getBase();
  if (o.isA(NullType))
    Global::current().put(getPropertyName(), v);
  else {
    // are we writing into an array ?
    if (o.isA(ObjectType) && (o.toObject().getClass() == ArrayClass))
      o.putArrayElement(getPropertyName(), v);
    else
      o.put(getPropertyName(), v);
  }

  return NoError;
}

void KJSO::setPrototype(const KJSO& p)
{
  assert(rep);
  rep->setPrototype(p);
}

void KJSO::setPrototypeProperty(const KJSO& p)
{
  assert(rep);
  put("prototype", p, DontEnum | DontDelete | ReadOnly);
}

KJSO KJSO::prototype() const
{
  if (rep)
    return KJSO(rep->proto);
  else
    return KJSO();
}

// ECMA 8.6.2.1
KJSO KJSO::get(const UString &p) const
{
  assert(rep);
  return rep->get(p);
}

// ECMA 8.6.2.2
void KJSO::put(const UString &p, const KJSO& v)
{
  assert(rep);
  rep->put(p, v);
}

// ECMA 8.6.2.2
void KJSO::put(const UString &p, const KJSO& v, int attr)
{
  static_cast<Imp*>(rep)->put(p, v, attr);
}

// provided for convenience.
void KJSO::put(const UString &p, double d, int attr)
{
  put(p, Number(d), attr);
}

// provided for convenience.
void KJSO::put(const UString &p, int i, int attr)
{
  put(p, Number(i), attr);
}

// provided for convenience.
void KJSO::put(const UString &p, unsigned int u, int attr)
{
  put(p, Number(u), attr);
}

// ECMA 15.4.5.1
void KJSO::putArrayElement(const UString &p, const KJSO& v)
{
  assert(rep);
  rep->putArrayElement(p, v);
}

// ECMA 8.6.2.3
bool KJSO::canPut(const UString &p) const
{
  assert(rep);
  return rep->canPut(p);
}

// ECMA 8.6.2.4
bool KJSO::hasProperty(const UString &p, bool recursive) const
{
  assert(rep);
  return rep->hasProperty(p, recursive);
}

// ECMA 8.6.2.5
bool KJSO::deleteProperty(const UString &p)
{
  assert(rep);
  return rep->deleteProperty(p);
}

Object::Object(Imp *d) : KJSO(d) { }

Object::Object(Class c) : KJSO(new ObjectImp(c)) { }

Object::Object(Class c, const KJSO& v) : KJSO(new ObjectImp(c, v)) { }

Object::Object(Class c, const KJSO& v, const Object& p)
  : KJSO(new ObjectImp(c, v))
{
  setPrototype(p);
}

Object::~Object() { }

void Object::setClass(Class c)
{
  static_cast<ObjectImp*>(rep)->cl = c;
}

Class Object::getClass() const
{
  assert(rep);
  return static_cast<ObjectImp*>(rep)->cl;
}

void Object::setInternalValue(const KJSO& v)
{
  assert(rep);
  static_cast<ObjectImp*>(rep)->val = v.imp();
}

KJSO Object::internalValue()
{
  assert(rep);
  return KJSO(static_cast<ObjectImp*>(rep)->val);
}

Object Object::create(Class c)
{
  return Object::create(c, KJSO());
}

// factory
Object Object::create(Class c, const KJSO& val)
{
  Global global(Global::current());
  Object obj = Object();
  obj.setClass(c);
  obj.setInternalValue(val);

  UString p = "[[";
  switch (c) {
  case UndefClass:
  case ObjectClass:
    p += "Object";
    break;
  case FunctionClass:
    p += "Function";
    break;
  case ArrayClass:
    p += "Array";
    obj.put("length", Number(0), DontEnum | DontDelete);
    break;
  case StringClass:
    p += "String";
    obj.put("length", val.toString().value().size());
    break;
  case BooleanClass:
    p += "Boolean";
    break;
  case NumberClass:
    p += "Number";
    break;
  case DateClass:
    p += "Date";
    break;
  case RegExpClass:
    p += "RegExp";
    break;
  case ErrorClass:
    p += "Error";
    break;
  }
  p += ".prototype]]";

  //  KJSO prot = global.get(p).get("prototype");
  KJSO prot = global.get(p);
  assert(prot.isDefined());

  obj.setPrototype(prot);
  return obj;
}

Object Object::create(Class c, const KJSO& val, const Object& p)
{
  Global global(Global::current());
  Object obj = Object();
  Object prot;
  obj.setClass(c);
  obj.setInternalValue(val);

  prot = p;
  obj.setPrototype(prot);
  return obj;
}

Object Object::dynamicCast(const KJSO &obj)
{
  // return null object on type mismatch
  if (!obj.isA(ObjectType))
    return Object(0L);

  return Object(obj.imp());

}

#ifdef KJS_DEBUG_MEM
int KJSO::count = 0;
int Imp::count = 0;
int List::count = 0;
#endif

Imp::Imp()
  : refcount(0), prop(0), proto(0)
{
  setCreated(true);
#ifdef KJS_DEBUG_MEM
  count++;
#endif
}

Imp::~Imp()
{
#ifdef KJS_DEBUG_MEM
  assert(Collector::collecting);
  count--;
#endif

// dangling pointer during garbage collection !
//   if (proto)
//     proto->deref();

  // delete attached properties
  Property *tmp, *p = prop;
  while (p) {
    tmp = p;
    p = p->next;
    delete tmp;
  }
}

KJSO Imp::toPrimitive(Type preferred) const
{
  return defaultValue(preferred);
  /* TODO: is there still any need to throw a runtime error _here_ ? */
}

Boolean Imp::toBoolean() const
{
  return Boolean();
}

Number Imp::toNumber() const
{
  return Number();
}

String Imp::toString() const
{
  return String();
}

Object Imp::toObject() const
{
  return Object(Error::create(TypeError).imp());
}


PropList* Imp::propList(PropList *first, PropList *last, bool recursive) const
{
  Property *pr = prop;
  while(pr) {
    if (!(pr->attribute & DontEnum) && !first->contains(pr->name)) {
      if(last) {
        last->next = new PropList();
	last = last->next;
      } else {
        first = new PropList();
	last = first;
      }
      last->name = pr->name;
    }
    pr = pr->next;
  }
  if (proto && recursive)
    proto->propList(first, last);

  return first;
}

KJSO Imp::get(const UString &p) const
{
  Property *pr = prop;
  while (pr) {
    if (pr->name == p) {
      return pr->object;
    }
    pr = pr->next;
  }

  if (!proto)
    return Undefined();

  return proto->get(p);
}

// may be overriden
void Imp::put(const UString &p, const KJSO& v)
{
  put(p, v, None);
}

// ECMA 8.6.2.2
void Imp::put(const UString &p, const KJSO& v, int attr)
{
  /* TODO: check for write permissions directly w/o this call */
  // putValue() is used for JS assignemnts. It passes no attribute.
  // Assume that a C++ implementation knows what it is doing
  // and let it override the canPut() check.
  if (attr == None && !canPut(p))
    return;

  Property *pr;

  if (prop) {
    pr = prop;
    while (pr) {
      if (pr->name == p) {
	// replace old value
	pr->object = v.imp();
	pr->attribute = attr;
	return;
      }
      pr = pr->next;
    }
  }

  // add new property
  pr = new Property;
  pr->name = p;
  pr->object = v.imp();
  pr->attribute = attr;
  pr->next = prop;
  prop = pr;
}

// ECMA 8.6.2.3
bool Imp::canPut(const UString &p) const
{
  if (prop) {
    const Property *pr = prop;
    while (pr) {
      if (pr->name == p)
	return !(pr->attribute & ReadOnly);
      pr = pr->next;
    }
  }
  if (!proto)
    return true;

  return proto->canPut(p);
}

// ECMA 8.6.2.4
bool Imp::hasProperty(const UString &p, bool recursive) const
{
  const Property *pr = prop;
  while (pr) {
    if (pr->name == p)
      return true;
    pr = pr->next;
  }

  if (!proto || !recursive)
    return false;

  return proto->hasProperty(p);
}

// ECMA 8.6.2.5
bool Imp::deleteProperty(const UString &p)
{
  Property *pr = prop;
  Property **prev = &prop;
  while (pr) {
    if (pr->name == p) {
      if ((pr->attribute & DontDelete))
	return false;
      *prev = pr->next;
      delete pr;
      return true;
    }
    prev = &(pr->next);
    pr = pr->next;
  }
  return true;
}

// ECMA 15.4.5.1
void Imp::putArrayElement(const UString &p, const KJSO& v)
{
  if (!canPut(p))
    return;

  if (hasProperty(p)) {
    if (p == "length") {
      KJSO len = get("length");
      unsigned int oldLen = len.toUInt32();
      unsigned int newLen = v.toUInt32();
      // shrink array
      for (unsigned int u = newLen; u < oldLen; u++) {
	UString p = UString::from(u);
	if (hasProperty(p, false))
	  deleteProperty(p);
      }
      put("length", Number(newLen), DontEnum | DontDelete);
      return;
    }
    //    put(p, v);
    } //  } else
    put(p, v);

  // array index ?
  unsigned int idx;
  if (!sscanf(p.cstring().c_str(), "%u", &idx)) /* TODO */
    return;

  // do we need to update/create the length property ?
  if (hasProperty("length", false)) {
    KJSO len = get("length");
    if (idx < len.toUInt32())
      return;
  }

  put("length", Number(idx+1), DontDelete | DontEnum);
}

bool Imp::implementsCall() const
{
  return (type() == FunctionType ||
	  type() == InternalFunctionType ||
	  type() == ConstructorType ||
	  type() == DeclaredFunctionType ||
	  type() == AnonymousFunctionType);
}

// ECMA 8.6.2.6 (new draft)
KJSO Imp::defaultValue(Type hint) const
{
  KJSO o;

  /* TODO String on Date object */
  if (hint != StringType && hint != NumberType)
    hint = NumberType;

  if (hint == StringType)
    o = get("toString");
  else
    o = get("valueOf");

  Imp *that = const_cast<Imp*>(this);
  if (o.implementsCall()) { // spec says "not primitive type" but ...
    FunctionImp *f = static_cast<FunctionImp*>(o.imp());
    KJSO s = f->executeCall(that, 0L);
    if (!s.isObject())
      return s;
  }

  if (hint == StringType)
    o = get("valueOf");
  else
    o = get("toString");

  if (o.implementsCall()) {
    FunctionImp *f = static_cast<FunctionImp*>(o.imp());
    KJSO s = f->executeCall(that, 0L);
    if (!s.isObject())
      return s;
  }

  return Error::create(TypeError, I18N_NOOP("No default value"));
}

void Imp::mark(Imp*)
{
  setMarked(true);

  if (proto && !proto->marked())
    proto->mark();

  struct Property *p = prop;
  while (p) {
    if (p->object && !p->object->marked())
      p->object->mark();
    p = p->next;
  }
}

bool Imp::marked() const
{
  return prev;
}

void Imp::setPrototype(const KJSO& p)
{
  if (proto)
    proto->deref();
  proto = p.imp();
  if (proto)
    proto->ref();
}

void Imp::setPrototypeProperty(const KJSO &p)
{
  put("prototype", p, DontEnum | DontDelete | ReadOnly);
}

void Imp::setConstructor(const KJSO& c)
{
  put("constructor", c, DontEnum | DontDelete | ReadOnly);
}

void* Imp::operator new(size_t s)
{
  return Collector::allocate(s);
}

void Imp::operator delete(void*, size_t)
{
  // deprecated. a mistake.
}

void Imp::operator delete(void*)
{
  // Do nothing. So far.
}

void Imp::setMarked(bool m)
{
  prev = m ? this : 0L;
}

void Imp::setGcAllowed(bool a)
{
  next = this;
  if (a)
    next++;
}

bool Imp::gcAllowed() const
{
  return (next && next != this);
}

void Imp::setCreated(bool c)
{
  next = c ? this : 0L;
}

bool Imp::created() const
{
  return next;
}

ObjectImp::ObjectImp(Class c) : cl(c), val(0L) { }

ObjectImp::ObjectImp(Class c, const KJSO &v) : cl(c), val(v.imp()) { }

ObjectImp::ObjectImp(Class c, const KJSO &v, const KJSO &p)
  : cl(c), val(v.imp())
{
  setPrototype(p);
}

ObjectImp::~ObjectImp() { }

Boolean ObjectImp::toBoolean() const
{
  return Boolean(true);
}

Number ObjectImp::toNumber() const
{
  return toPrimitive(NumberType).toNumber();
}

String ObjectImp::toString() const
{
  KJSO tmp;
  String res;

  if (hasProperty("toString") && (tmp = get("toString")).implementsCall()) {
    // TODO live w/o hack
    res = tmp.executeCall(KJSO(const_cast<ObjectImp*>(this)), 0L).toString();
  } else {
    tmp = toPrimitive(StringType);
    res = tmp.toString();
  }

  return res;
}

const TypeInfo ObjectImp::info = { "Object", ObjectType, 0, 0, 0 };

Object ObjectImp::toObject() const
{
  return Object(const_cast<ObjectImp*>(this));
}

KJSO ObjectImp::toPrimitive(Type preferred) const
{
  // ### Imp already does that now. Remove in KDE 3.0.
  return defaultValue(preferred);
  /* TODO: is there still any need to throw a runtime error _here_ ? */
}

void ObjectImp::mark(Imp*)
{
  // mark objects from the base
  Imp::mark();

  // mark internal value, if any and it has not been visited yet
  if (val && !val->marked())
    val->mark();
}

HostImp::HostImp()
{
    setPrototype(Global::current().objectPrototype());
    //printf("HostImp::HostImp() %p\n",this);
}

HostImp::~HostImp() { }

Boolean HostImp::toBoolean() const
{
  return Boolean(true);
}

String HostImp::toString() const
{
  // Exact copy of ObjectImp::toString....
  KJSO tmp;
  String res;

  if (hasProperty("toString") && (tmp = get("toString")).implementsCall()) {
    // TODO live w/o hack
    res = tmp.executeCall(KJSO(const_cast<HostImp*>(this)), 0L).toString();
  } else {
    tmp = toPrimitive(StringType);
    res = tmp.toString();
  }

  return res;
}

const TypeInfo HostImp::info = { "HostObject", HostType, 0, 0, 0 };

Object Error::createObject(ErrorType e, const char *m, int l)
{
  Context *context = Context::current();
  if (!context)
    return Object();

  Object err = ErrorObject::create(e, m, l);

  if (!KJScriptImp::hadException())
    KJScriptImp::setException(err.imp());

  const struct ErrorStruct {
      ErrorType e;
      const char *s;
  } errtab[] = {
      { GeneralError, I18N_NOOP("General error") },
      { EvalError, I18N_NOOP("Evaluation error") },
      { RangeError, I18N_NOOP("Range error") },
      { ReferenceError, I18N_NOOP("Reference error") },
      { SyntaxError, I18N_NOOP("Syntax error") },
      { TypeError, I18N_NOOP("Type error") },
      { URIError, I18N_NOOP("URI error") },
      { (ErrorType)0, 0 }
  };

  const char *estr = I18N_NOOP("Unknown error");
  const ErrorStruct *estruct = errtab;
  while (estruct->e) {
      if (estruct->e == e) {
	  estr = estruct->s;
	  break;
      }
      estruct++;
  }

#ifndef NDEBUG
  const char *msg = err.get("message").toString().value().ascii();
  if (l >= 0)
      fprintf(stderr, "JS: %s at line %d. %s\n", estr, l, msg);
  else
      fprintf(stderr, "JS: %s. %s\n", estr, msg);
#endif

  return err;
}

KJSO Error::create(ErrorType e, const char *m, int l)
{
  return KJSO(createObject(e, m, l).imp());
}
