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
#include "lookup.h"
#include "reference_list.h"

#include <assert.h>
#include <math.h>
#include <stdio.h>

#include "internal.h"
#include "collector.h"
#include "operations.h"
#include "error_object.h"
#include "nodes.h"
#include "property_map.h"

namespace KJS {

extern const UString lengthPropertyName("length");
extern const UString prototypePropertyName("prototype");
extern const UString toStringPropertyName("toString");
extern const UString valueOfPropertyName("valueOf");

// ------------------------------ Object ---------------------------------------

Object Object::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != ObjectType)
    return Object(0);

  return Object(static_cast<ObjectImp*>(v.imp()));
}

// ------------------------------ ObjectImp ------------------------------------

ObjectImp::ObjectImp(const Object &proto)
  : _prop(0), _proto(static_cast<ObjectImp*>(proto.imp())), _internalValue(0L), _scope(0)
{
  //fprintf(stderr,"ObjectImp::ObjectImp %p\n",(void*)this);
  _scope = ListImp::empty();
  _prop = new PropertyMap();
}

ObjectImp::ObjectImp()
{
  //fprintf(stderr,"ObjectImp::ObjectImp %p\n",(void*)this);
  _prop = 0;
  _proto = NullImp::staticNull;
  _internalValue = 0L;
  _scope = ListImp::empty();
  _scope->setGcAllowed();
  _prop = new PropertyMap();
}

ObjectImp::~ObjectImp()
{
  //fprintf(stderr,"ObjectImp::~ObjectImp %p\n",(void*)this);
  delete _prop;
}

void ObjectImp::mark()
{
  //fprintf(stderr,"ObjectImp::mark() %p\n",(void*)this);
  ValueImp::mark();

  if (_proto && !_proto->marked())
    _proto->mark();

  PropertyMapNode *node = _prop->first();
  while (node) {
    if (!node->value->marked())
      node->value->mark();
    node = node->next();
  }

  if (_internalValue && !_internalValue->marked())
    _internalValue->mark();
  if (_scope && !_scope->marked())
    _scope->mark();
}

const ClassInfo *ObjectImp::classInfo() const
{
  return 0;
}

bool ObjectImp::inherits(const ClassInfo *info) const
{
  if (!info)
    return false;

  const ClassInfo *ci = classInfo();
  if (!ci)
    return false;

  while (ci && ci != info)
    ci = ci->parentClass;

  return (ci == info);
}

Type ObjectImp::type() const
{
  return ObjectType;
}

Value ObjectImp::prototype() const
{
  return Value(_proto);
}

void ObjectImp::setPrototype(const Value &proto)
{
  _proto = proto.imp();
}

UString ObjectImp::className() const
{
  const ClassInfo *ci = classInfo();
  if ( ci )
    return ci->className;
  return "Object";
}

Value ObjectImp::get(ExecState *exec, const UString &propertyName) const
{
  if (propertyName == "__proto__") {
    Object proto = Object::dynamicCast(prototype());
    // non-standard netscape extension
    if (proto.isNull())
      return Null();
    else
      return proto;
  }

  ValueImp *imp = getDirect(propertyName);
  if ( imp )
    return Value(imp);

  Object proto = Object::dynamicCast(prototype());
  if (proto.isNull())
    return Undefined();

  return proto.get(exec,propertyName);
}

Value ObjectImp::get(ExecState *exec, unsigned propertyName) const
{
  return get(exec, UString::from(propertyName));
}

// This get method only looks at the property map.
// A bit like hasProperty(recursive=false), this doesn't go to the prototype.
// This is used e.g. by lookupOrCreateFunction (to cache a function, we don't want
// to look up in the prototype, it might already exist there)
ValueImp* ObjectImp::getDirect(const UString& propertyName) const
{
  return _prop->get(propertyName);
}

// ECMA 8.6.2.2
void ObjectImp::put(ExecState *exec, const UString &propertyName,
                     const Value &value, int attr)
{
  assert(!value.isNull());
  assert(value.type() != ListType);

  /* TODO: check for write permissions directly w/o this call */
  /* Doesn't look very easy with the PropertyMap API - David */
  // putValue() is used for JS assignemnts. It passes no attribute.
  // Assume that a C++ implementation knows what it is doing
  // and let it override the canPut() check.
  if ((attr == None || attr == DontDelete) && !canPut(exec,propertyName)) {
#ifdef KJS_VERBOSE
    fprintf( stderr, "WARNING: canPut %s said NO\n", propertyName.ascii() );
#endif
    return;
  }

  if (propertyName == "__proto__") {
    // non-standard netscape extension
    setPrototype(value);
    return;
  }

  _prop->put(propertyName,value.imp(),attr);
}

void ObjectImp::put(ExecState *exec, unsigned propertyName,
                     const Value &value, int attr)
{
  put(exec, UString::from(propertyName), value, attr);
}

// ECMA 8.6.2.3
bool ObjectImp::canPut(ExecState *, const UString &propertyName) const
{
  PropertyMapNode *node = _prop->getNode(propertyName);
  if (node)
    return!(node->attr & ReadOnly);

  // Look in the static hashtable of properties
  const HashEntry* e = findPropertyHashEntry(propertyName);
  if (e)
    return !(e->attr & ReadOnly);

  // Don't look in the prototype here. We can always put an override
  // in the object, even if the prototype has a ReadOnly property.
  return true;
}

// ECMA 8.6.2.4
bool ObjectImp::hasProperty(ExecState *exec, const UString &propertyName) const
{
  if (propertyName == "__proto__")
    return true;
  if (_prop->get(propertyName))
    return true;

  // Look in the static hashtable of properties
  if (findPropertyHashEntry(propertyName))
      return true;

  // Look in the prototype
  Object proto = Object::dynamicCast(prototype());
  return !proto.isNull() && proto.hasProperty(exec,propertyName);
}

bool ObjectImp::hasProperty(ExecState *exec, unsigned propertyName) const
{
  return hasProperty(exec, UString::from(propertyName));
}

// ECMA 8.6.2.5
bool ObjectImp::deleteProperty(ExecState */*exec*/, const UString &propertyName)
{
  PropertyMapNode *node = _prop->getNode(propertyName);
  if (node) {
    if ((node->attr & DontDelete))
      return false;
    _prop->remove(propertyName);
    return true;
  }

  // Look in the static hashtable of properties
  const HashEntry* entry = findPropertyHashEntry(propertyName);
  if (entry && entry->attr & DontDelete)
    return false; // this builtin property can't be deleted
  return true;
}

bool ObjectImp::deleteProperty(ExecState *exec, unsigned propertyName)
{
  return deleteProperty(exec, UString::from(propertyName));
}

void ObjectImp::deleteAllProperties( ExecState * )
{
  _prop->clear();
}

// ECMA 8.6.2.6
Value ObjectImp::defaultValue(ExecState *exec, Type hint) const
{
  if (hint != StringType && hint != NumberType) {
    /* Prefer String for Date objects */
    if (_proto == exec->interpreter()->builtinDatePrototype().imp())
      hint = StringType;
    else
      hint = NumberType;
  }

  Value v;
  if (hint == StringType)
    v = get(exec,toStringPropertyName);
  else
    v = get(exec,valueOfPropertyName);

  if (v.type() == ObjectType) {
    Object o = Object(static_cast<ObjectImp*>(v.imp()));
    if (o.implementsCall()) { // spec says "not primitive type" but ...
      Object thisObj = Object(const_cast<ObjectImp*>(this));
      Value def = o.call(exec,thisObj,List::empty());
      Type defType = def.type();
      if (defType == UnspecifiedType || defType == UndefinedType ||
          defType == NullType || defType == BooleanType ||
          defType == StringType || defType == NumberType) {
        return def;
      }
    }
  }

  if (hint == StringType)
    v = get(exec,valueOfPropertyName);
  else
    v = get(exec,toStringPropertyName);

  if (v.type() == ObjectType) {
    Object o = Object(static_cast<ObjectImp*>(v.imp()));
    if (o.implementsCall()) { // spec says "not primitive type" but ...
      Object thisObj = Object(const_cast<ObjectImp*>(this));
      Value def = o.call(exec,thisObj,List::empty());
      Type defType = def.type();
      if (defType == UnspecifiedType || defType == UndefinedType ||
          defType == NullType || defType == BooleanType ||
          defType == StringType || defType == NumberType) {
        return def;
      }
    }
  }

  Object err = Error::create(exec, TypeError, I18N_NOOP("No default value"));
  exec->setException(err);
  return err;
}

const HashEntry* ObjectImp::findPropertyHashEntry( const UString& propertyName ) const
{
  const ClassInfo *info = classInfo();
  while (info) {
    if (info->propHashTable) {
      const HashEntry *e = Lookup::findEntry(info->propHashTable, propertyName);
      if (e)
        return e;
    }
    info = info->parentClass;
  }
  return 0L;
}

bool ObjectImp::implementsConstruct() const
{
  return false;
}

Object ObjectImp::construct(ExecState */*exec*/, const List &/*args*/)
{
  assert(false);
  return Object(0);
}

bool ObjectImp::implementsCall() const
{
  return false;
}

Value ObjectImp::call(ExecState */*exec*/, Object &/*thisObj*/, const List &/*args*/)
{
  assert(false);
  return Object(0);
}

bool ObjectImp::implementsHasInstance() const
{
  return false;
}

Boolean ObjectImp::hasInstance(ExecState */*exec*/, const Value &/*value*/)
{
  assert(false);
  return Boolean(false);
}

const List ObjectImp::scope() const
{
  return _scope;
}

void ObjectImp::setScope(const List &s)
{
  if (_scope) _scope->setGcAllowed();
  _scope = static_cast<ListImp*>(s.imp());
}

ReferenceList ObjectImp::propList(ExecState *exec, bool recursive)
{
  ReferenceList list;
  if (_proto && _proto->dispatchType() == ObjectType && recursive)
    list = static_cast<ObjectImp*>(_proto)->propList(exec,recursive);


  PropertyMapNode *node = _prop->first();
  while (node) {
    if (!(node->attr & DontEnum))
      list.append(Reference(Object(this), node->name));
    node = node->next();
  }

  // Add properties from the static hashtable of properties
  const ClassInfo *info = classInfo();
  while (info) {
    if (info->propHashTable) {
      int size = info->propHashTable->size;
      const HashEntry *e = info->propHashTable->entries;
      for (int i = 0; i < size; ++i, ++e) {
        if ( e->s && !(e->attr & DontEnum) )
          list.append(Reference(Object(this), e->s)); /// ######### check for duplicates with the propertymap
      }
    }
    info = info->parentClass;
  }

  return list;
}

Value ObjectImp::internalValue() const
{
  return Value(_internalValue);
}

void ObjectImp::setInternalValue(const Value &v)
{
  _internalValue = v.imp();
}

Value ObjectImp::toPrimitive(ExecState *exec, Type preferredType) const
{
  return defaultValue(exec,preferredType);
}

bool ObjectImp::toBoolean(ExecState */*exec*/) const
{
  return true;
}

double ObjectImp::toNumber(ExecState *exec) const
{
  Value prim = toPrimitive(exec,NumberType);
  if (exec->hadException()) // should be picked up soon in nodes.cpp
    return 0.0;
  return prim.toNumber(exec);
}

UString ObjectImp::toString(ExecState *exec) const
{
  Value prim = toPrimitive(exec,StringType);
  if (exec->hadException()) // should be picked up soon in nodes.cpp
    return "";
  return prim.toString(exec);
}

Object ObjectImp::toObject(ExecState */*exec*/) const
{
  return Object(const_cast<ObjectImp*>(this));
}


// ------------------------------ Error ----------------------------------------

const char * const errorNamesArr[] = {
  I18N_NOOP("Error"), // GeneralError
  I18N_NOOP("Evaluation error"), // EvalError
  I18N_NOOP("Range error"), // RangeError
  I18N_NOOP("Reference error"), // ReferenceError
  I18N_NOOP("Syntax error"), // SyntaxError
  I18N_NOOP("Type error"), // TypeError
  I18N_NOOP("URI error"), // URIError
};

const char * const * const Error::errorNames = errorNamesArr;

Object Error::create(ExecState *exec, ErrorType errtype, const char *message,
                     int lineno, int sourceId)
{
  Object cons;

  switch (errtype) {
  case EvalError:
    cons = exec->interpreter()->builtinEvalError();
    break;
  case RangeError:
    cons = exec->interpreter()->builtinRangeError();
    break;
  case ReferenceError:
    cons = exec->interpreter()->builtinReferenceError();
    break;
  case SyntaxError:
    cons = exec->interpreter()->builtinSyntaxError();
    break;
  case TypeError:
    cons = exec->interpreter()->builtinTypeError();
    break;
  case URIError:
    cons = exec->interpreter()->builtinURIError();
    break;
  default:
    cons = exec->interpreter()->builtinError();
    break;
  }

  if (!message)
    message = errorNames[errtype];
  List args;
  args.append(String(message));
  Object err = Object::dynamicCast(cons.construct(exec,args));

  if (lineno != -1)
    err.put(exec, "line", Number(lineno));
  if (sourceId != -1)
    err.put(exec, "sourceId", Number(sourceId));

  return err;

/*
#ifndef NDEBUG
  const char *msg = err.get("message").toString().value().ascii();
  if (l >= 0)
      fprintf(stderr, "KJS: %s at line %d. %s\n", estr, l, msg);
  else
      fprintf(stderr, "KJS: %s. %s\n", estr, msg);
#endif

  return err;
*/
}

} // namespace KJS
