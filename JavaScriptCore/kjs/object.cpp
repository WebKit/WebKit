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

#ifndef NDEBUG
#define JAVASCRIPT_CALL_TRACING Yes
#endif

#ifdef JAVASCRIPT_CALL_TRACING
static bool _traceJavaScript = false;

extern "C" {
    void setTraceJavaScript(bool f)
    {
        _traceJavaScript = f;
    }

    static bool traceJavaScript()
    {
        return _traceJavaScript;
    }
}
#endif

namespace KJS {

// ------------------------------ Object ---------------------------------------

Object Object::dynamicCast(const Value &v)
{
  if (v.isNull() || v.type() != ObjectType)
    return Object(0);

  return Object(static_cast<ObjectImp*>(v.imp()));
}


Value Object::call(ExecState *exec, Object &thisObj, const List &args)
{ 
#if KJS_MAX_STACK > 0
  static int depth = 0; // sum of all concurrent interpreters

#ifdef JAVASCRIPT_CALL_TRACING
    static bool tracing = false;
    if (traceJavaScript() && !tracing) {
        tracing = true;
        for (int i = 0; i < depth; i++)
            putchar (' ');
        printf ("*** calling:  %s\n", toString(exec).ascii());
        for (int j = 0; j < args.size(); j++) {
            for (int i = 0; i < depth; i++)
                putchar (' ');
            printf ("*** arg[%d] = %s\n", j, args[j].toString(exec).ascii());
        }
        tracing = false;
    }
#endif

  if (++depth > KJS_MAX_STACK) {
    --depth;
    Object err = Error::create(exec, RangeError,
                               "Maximum call stack size exceeded.");
    exec->setException(err);
    return err;
  }
#endif

  Value ret = imp()->call(exec,thisObj,args); 

#if KJS_MAX_STACK > 0
  --depth;
#endif

#ifdef JAVASCRIPT_CALL_TRACING
    if (traceJavaScript() && !tracing) {
        tracing = true;
        for (int i = 0; i < depth; i++)
            putchar (' ');
        printf ("*** returning:  %s\n", ret.toString(exec).ascii());
        tracing = false;
    }
#endif

  return ret;
}

// ------------------------------ ObjectImp ------------------------------------

ObjectImp::ObjectImp(const Object &proto)
  : _proto(static_cast<ObjectImp*>(proto.imp())), _internalValue(0L)
{
  //fprintf(stderr,"ObjectImp::ObjectImp %p\n",(void*)this);
}

ObjectImp::ObjectImp(ObjectImp *proto)
  : _proto(proto), _internalValue(0L)
{
  //fprintf(stderr,"ObjectImp::ObjectImp %p\n",(void*)this);
}

ObjectImp::ObjectImp()
{
  //fprintf(stderr,"ObjectImp::ObjectImp %p\n",(void*)this);
  _proto = NullImp::staticNull;
  _internalValue = 0L;
}

ObjectImp::~ObjectImp()
{
  //fprintf(stderr,"ObjectImp::~ObjectImp %p\n",(void*)this);
}

void ObjectImp::mark()
{
  //fprintf(stderr,"ObjectImp::mark() %p\n",(void*)this);
  ValueImp::mark();

  if (_proto && !_proto->marked())
    _proto->mark();

  _prop.mark();

  if (_internalValue && !_internalValue->marked())
    _internalValue->mark();

  _scope.mark();
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

Value ObjectImp::get(ExecState *exec, const Identifier &propertyName) const
{
  Value result;

  const ObjectImp *imp = this;

  while (true) {
    if (imp->getOwnProperty(exec, propertyName, result))
      return result;

    const ValueImp *proto = imp->_proto;
    if (proto->dispatchType() != ObjectType)
      break;

    imp = static_cast<const ObjectImp *>(proto);
  }

  return Undefined();
}

bool ObjectImp::getOwnProperty(ExecState *exec, unsigned propertyName, Value& result) const
{
  return getOwnProperty(exec, Identifier::from(propertyName), result);
}

Value ObjectImp::get(ExecState *exec, unsigned propertyName) const
{
  Value result;

  const ObjectImp *imp = this;

  while (imp) {
    if (imp->getOwnProperty(exec, propertyName, result))
      return result;

    const ValueImp *proto = imp->_proto;
    if (proto->dispatchType() != ObjectType)
      break;

    imp = static_cast<const ObjectImp *>(proto);
  }

  return Undefined();
}

bool ObjectImp::getProperty(ExecState *exec, unsigned propertyName, Value& result) const
{
  const ObjectImp *imp = this;
  
  while (true) {
    if (imp->getOwnProperty(exec, propertyName, result))
      return true;
    
    const ValueImp *proto = imp->_proto;
      if (proto->dispatchType() != ObjectType)
        break;
      
      imp = static_cast<const ObjectImp *>(proto);
  }
  
  return false;
}

// ECMA 8.6.2.2
void ObjectImp::put(ExecState *exec, const Identifier &propertyName,
                     const Value &value, int attr)
{
  assert(!value.isNull());

  // non-standard netscape extension
  if (propertyName == specialPrototypePropertyName) {
    setPrototype(value);
    return;
  }

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

  _prop.put(propertyName,value.imp(),attr);
}

void ObjectImp::put(ExecState *exec, unsigned propertyName,
                     const Value &value, int attr)
{
  put(exec, Identifier::from(propertyName), value, attr);
}

// ECMA 8.6.2.3
bool ObjectImp::canPut(ExecState *, const Identifier &propertyName) const
{
  int attributes;
  ValueImp *v = _prop.get(propertyName, attributes);
  if (v)
    return!(attributes & ReadOnly);

  // Look in the static hashtable of properties
  const HashEntry* e = findPropertyHashEntry(propertyName);
  if (e)
    return !(e->attr & ReadOnly);

  // Don't look in the prototype here. We can always put an override
  // in the object, even if the prototype has a ReadOnly property.
  return true;
}

// ECMA 8.6.2.4
bool ObjectImp::hasProperty(ExecState *exec, const Identifier &propertyName) const
{
  if (hasOwnProperty(exec, propertyName))
    return true;

  if (_proto->dispatchType() != ObjectType) {
    return false;
  }

  // Look in the prototype
  return static_cast<ObjectImp *>(_proto)->hasProperty(exec, propertyName);
}

bool ObjectImp::hasProperty(ExecState *exec, unsigned propertyName) const
{
    if (hasOwnProperty(exec, propertyName))
      return true;

    if (_proto->dispatchType() != ObjectType) {
      return false;
    }

    // Look in the prototype
    return static_cast<ObjectImp *>(_proto)->hasProperty(exec, propertyName);
}

bool ObjectImp::hasOwnProperty(ExecState *exec, const Identifier &propertyName) const
{
  if (_prop.get(propertyName))
    return true;

  // Look in the static hashtable of properties
  if (findPropertyHashEntry(propertyName))
    return true;

  // non-standard netscape extension
  if (propertyName == specialPrototypePropertyName)
    return true;

  return false;
}

bool ObjectImp::hasOwnProperty(ExecState *exec, unsigned propertyName) const
{
  return hasOwnProperty(exec, Identifier::from(propertyName));
}


// ECMA 8.6.2.5
bool ObjectImp::deleteProperty(ExecState */*exec*/, const Identifier &propertyName)
{
  int attributes;
  ValueImp *v = _prop.get(propertyName, attributes);
  if (v) {
    if ((attributes & DontDelete))
      return false;
    _prop.remove(propertyName);
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
  return deleteProperty(exec, Identifier::from(propertyName));
}

void ObjectImp::deleteAllProperties( ExecState * )
{
  _prop.clear();
}

// ECMA 8.6.2.6
Value ObjectImp::defaultValue(ExecState *exec, Type hint) const
{
  if (hint != StringType && hint != NumberType) {
    /* Prefer String for Date objects */
    if (_proto == exec->lexicalInterpreter()->builtinDatePrototype().imp())
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

  if (exec->hadException())
    return exec->exception();

  Object err = Error::create(exec, TypeError, I18N_NOOP("No default value"));
  exec->setException(err);
  return err;
}

const HashEntry* ObjectImp::findPropertyHashEntry( const Identifier& propertyName ) const
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

Object ObjectImp::construct(ExecState *exec, const List &args, const UString &/*sourceURL*/, int /*lineNumber*/)
{
  return construct(exec, args);
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

ReferenceList ObjectImp::propList(ExecState *exec, bool recursive)
{
  ReferenceList list;
  if (_proto && _proto->dispatchType() == ObjectType && recursive)
    list = static_cast<ObjectImp*>(_proto)->propList(exec,recursive);

  _prop.addEnumerablesToReferenceList(list, Object(this));

  // Add properties from the static hashtable of properties
  const ClassInfo *info = classInfo();
  while (info) {
    if (info->propHashTable) {
      int size = info->propHashTable->size;
      const HashEntry *e = info->propHashTable->entries;
      for (int i = 0; i < size; ++i, ++e) {
        if ( e->s && !(e->attr & DontEnum) )
          list.append(Reference(this, e->s)); /// ######### check for duplicates with the propertymap
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

void ObjectImp::setInternalValue(ValueImp *v)
{
  _internalValue = v;
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

void ObjectImp::putDirect(const Identifier &propertyName, ValueImp *value, int attr)
{
    _prop.put(propertyName, value, attr);
}

void ObjectImp::putDirect(const Identifier &propertyName, int value, int attr)
{
    _prop.put(propertyName, NumberImp::create(value), attr);
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
                     int lineno, int sourceId, const UString *sourceURL)
{
  Object cons;
  switch (errtype) {
  case EvalError:
    cons = exec->lexicalInterpreter()->builtinEvalError();
    break;
  case RangeError:
    cons = exec->lexicalInterpreter()->builtinRangeError();
    break;
  case ReferenceError:
    cons = exec->lexicalInterpreter()->builtinReferenceError();
    break;
  case SyntaxError:
    cons = exec->lexicalInterpreter()->builtinSyntaxError();
    break;
  case TypeError:
    cons = exec->lexicalInterpreter()->builtinTypeError();
    break;
  case URIError:
    cons = exec->lexicalInterpreter()->builtinURIError();
    break;
  default:
    cons = exec->lexicalInterpreter()->builtinError();
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

  if(sourceURL)
   err.put(exec,"sourceURL", String(*sourceURL));
 
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

ObjectImp *error(ExecState *exec, ErrorType type, const char *message, int line, int sourceId, const UString *sourceURL)
{
    return Error::create(exec, type, message, line, sourceId, sourceURL).imp();
}

} // namespace KJS
