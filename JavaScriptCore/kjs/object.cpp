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

#include "config.h"
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
//#define JAVASCRIPT_CALL_TRACING 1
#endif

#if JAVASCRIPT_CALL_TRACING
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

JSValue *JSObject::call(ExecState *exec, JSObject *thisObj, const List &args)
{
  assert(implementsCall());

#if KJS_MAX_STACK > 0
  static int depth = 0; // sum of all concurrent interpreters

#if JAVASCRIPT_CALL_TRACING
    static bool tracing = false;
    if (traceJavaScript() && !tracing) {
        tracing = true;
        for (int i = 0; i < depth; i++)
            putchar (' ');
        printf ("*** calling:  %s\n", toString(exec).ascii());
        for (int j = 0; j < args.size(); j++) {
            for (int i = 0; i < depth; i++)
                putchar (' ');
            printf ("*** arg[%d] = %s\n", j, args[j]->toString(exec).ascii());
        }
        tracing = false;
    }
#endif

  if (++depth > KJS_MAX_STACK) {
    --depth;
    return throwError(exec, RangeError, "Maximum call stack size exceeded.");
  }
#endif

  JSValue *ret = callAsFunction(exec,thisObj,args); 

#if KJS_MAX_STACK > 0
  --depth;
#endif

#if JAVASCRIPT_CALL_TRACING
    if (traceJavaScript() && !tracing) {
        tracing = true;
        for (int i = 0; i < depth; i++)
            putchar (' ');
        printf ("*** returning:  %s\n", ret->toString(exec).ascii());
        tracing = false;
    }
#endif

  return ret;
}

// ------------------------------ JSObject ------------------------------------

void JSObject::mark()
{
  JSCell::mark();

  JSValue *proto = _proto;
  if (!proto->marked())
    proto->mark();

  _prop.mark();

  if (_internalValue && !_internalValue->marked())
    _internalValue->mark();

  _scope.mark();
}

Type JSObject::type() const
{
  return ObjectType;
}

const ClassInfo *JSObject::classInfo() const
{
  return 0;
}

UString JSObject::className() const
{
  const ClassInfo *ci = classInfo();
  if ( ci )
    return ci->className;
  return "Object";
}

JSValue *JSObject::get(ExecState *exec, const Identifier &propertyName) const
{
  PropertySlot slot;

  if (const_cast<JSObject *>(this)->getPropertySlot(exec, propertyName, slot))
    return slot.getValue(exec, const_cast<JSObject *>(this), propertyName);
    
  return jsUndefined();
}

JSValue *JSObject::get(ExecState *exec, unsigned propertyName) const
{
  PropertySlot slot;
  if (const_cast<JSObject *>(this)->getPropertySlot(exec, propertyName, slot))
    return slot.getValue(exec, const_cast<JSObject *>(this), propertyName);
    
  return jsUndefined();
}

bool JSObject::getPropertySlot(ExecState *exec, unsigned propertyName, PropertySlot& slot)
{
  JSObject *imp = this;
  
  while (true) {
    if (imp->getOwnPropertySlot(exec, propertyName, slot))
      return true;
    
    JSValue *proto = imp->_proto;
    if (!proto->isObject())
      break;
    
    imp = static_cast<JSObject *>(proto);
  }
  
  return false;
}

bool JSObject::getOwnPropertySlot(ExecState *exec, unsigned propertyName, PropertySlot& slot)
{
  return getOwnPropertySlot(exec, Identifier::from(propertyName), slot);
}

// ECMA 8.6.2.2
void JSObject::put(ExecState *exec, const Identifier &propertyName, JSValue *value, int attr)
{
  assert(value);

  // non-standard netscape extension
  if (propertyName == exec->dynamicInterpreter()->specialPrototypeIdentifier()) {
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

  JSObject *obj = this;
  while (true) {
    if (JSValue *gs = obj->_prop.get(propertyName)) {
      if (gs->type() == GetterSetterType) {
        JSObject *setterFunc = static_cast<GetterSetterImp *>(gs)->getSetter();
            
        if (!setterFunc) {
          throwError(exec, TypeError, "setting a property that has only a getter");
          return;
        }
            
        List args;
        args.append(value);
        
        setterFunc->call(exec, this, args);
        return;
      } else  {
        // If there's an existing property on the object or one of its 
        // prototype it should be replaced, so we just break here.
        break;
      }
    }
     
    if (!obj->_proto || !obj->_proto->isObject())
      break;
        
    obj = static_cast<JSObject *>(obj->_proto);
  }

  _prop.put(propertyName,value,attr);
}

void JSObject::put(ExecState *exec, unsigned propertyName,
                     JSValue *value, int attr)
{
  put(exec, Identifier::from(propertyName), value, attr);
}

// ECMA 8.6.2.3
bool JSObject::canPut(ExecState *, const Identifier &propertyName) const
{
  int attributes;
    
  // Don't look in the prototype here. We can always put an override
  // in the object, even if the prototype has a ReadOnly property.

  if (!getPropertyAttributes(propertyName, attributes))
    return true;
  else
    return !(attributes & ReadOnly);
}

// ECMA 8.6.2.4
bool JSObject::hasProperty(ExecState *exec, const Identifier &propertyName) const
{
  PropertySlot slot;
  return const_cast<JSObject *>(this)->getPropertySlot(exec, propertyName, slot);
}

bool JSObject::hasProperty(ExecState *exec, unsigned propertyName) const
{
  PropertySlot slot;
  return const_cast<JSObject *>(this)->getPropertySlot(exec, propertyName, slot);
}

// ECMA 8.6.2.5
bool JSObject::deleteProperty(ExecState */*exec*/, const Identifier &propertyName)
{
  int attributes;
  JSValue *v = _prop.get(propertyName, attributes);
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

bool JSObject::deleteProperty(ExecState *exec, unsigned propertyName)
{
  return deleteProperty(exec, Identifier::from(propertyName));
}

static inline
#ifdef __GNUC__
__attribute__((always_inline))
#endif
JSValue *tryGetAndCallProperty(ExecState *exec, const JSObject *object, const Identifier &propertyName) {
  JSValue *v = object->get(exec, propertyName);
  if (v->isObject()) {
    JSObject *o = static_cast<JSObject*>(v);
    if (o->implementsCall()) { // spec says "not primitive type" but ...
      JSObject *thisObj = const_cast<JSObject*>(object);
      JSValue *def = o->call(exec, thisObj, List::empty());
      Type defType = def->type();
      if (defType == UnspecifiedType || defType == UndefinedType ||
          defType == NullType || defType == BooleanType ||
          defType == StringType || defType == NumberType) {
        return def;
      }
    }
  }
  return NULL;
}

// ECMA 8.6.2.6
JSValue *JSObject::defaultValue(ExecState *exec, Type hint) const
{
  Identifier firstPropertyName;
  Identifier secondPropertyName;
  /* Prefer String for Date objects */
  if ((hint == StringType) || (hint != StringType) && (hint != NumberType) && (_proto == exec->lexicalInterpreter()->builtinDatePrototype())) {
    firstPropertyName = toStringPropertyName;
    secondPropertyName = valueOfPropertyName;
  } else {
    firstPropertyName = valueOfPropertyName;
    secondPropertyName = toStringPropertyName;
  }

  JSValue *v;
  if ((v = tryGetAndCallProperty(exec, this, firstPropertyName)))
    return v;
  if ((v = tryGetAndCallProperty(exec, this, secondPropertyName)))
    return v;

  if (exec->hadException())
    return exec->exception();

  return throwError(exec, TypeError, "No default value");
}

const HashEntry* JSObject::findPropertyHashEntry(const Identifier& propertyName) const
{
  for (const ClassInfo *info = classInfo(); info; info = info->parentClass) {
    if (const HashTable *propHashTable = info->propHashTable) {
      if (const HashEntry *e = Lookup::findEntry(propHashTable, propertyName))
        return e;
    }
  }
  return 0;
}

void JSObject::defineGetter(ExecState *exec, const Identifier& propertyName, JSObject *getterFunc)
{
    JSValue *o = getDirect(propertyName);
    GetterSetterImp *gs;
    
    if (o && o->type() == GetterSetterType) {
        gs = static_cast<GetterSetterImp *>(o);
    } else {
        gs = new GetterSetterImp;
        putDirect(propertyName, gs);
    }
    
    gs->setGetter(getterFunc);
}

void JSObject::defineSetter(ExecState *exec, const Identifier& propertyName, JSObject *setterFunc)
{
    JSValue *o = getDirect(propertyName);
    GetterSetterImp *gs;
    
    if (o && o->type() == GetterSetterType) {
        gs = static_cast<GetterSetterImp *>(o);
    } else {
        gs = new GetterSetterImp;
        putDirect(propertyName, gs);
    }
    
    gs->setSetter(setterFunc);
}

bool JSObject::implementsConstruct() const
{
  return false;
}

JSObject *JSObject::construct(ExecState */*exec*/, const List &/*args*/)
{
  assert(false);
  return NULL;
}

JSObject *JSObject::construct(ExecState *exec, const List &args, const UString &/*sourceURL*/, int /*lineNumber*/)
{
  return construct(exec, args);
}

bool JSObject::implementsCall() const
{
  return false;
}

JSValue *JSObject::callAsFunction(ExecState */*exec*/, JSObject */*thisObj*/, const List &/*args*/)
{
  assert(false);
  return NULL;
}

bool JSObject::implementsHasInstance() const
{
  return false;
}

bool JSObject::hasInstance(ExecState */*exec*/, JSValue */*value*/)
{
  assert(false);
  return false;
}

bool JSObject::propertyIsEnumerable(ExecState *exec, const Identifier &propertyName) const
{
  int attributes;
 
  if (!getPropertyAttributes(propertyName, attributes))
    return false;
  else
    return !(attributes & DontEnum);
}

bool JSObject::getPropertyAttributes(const Identifier& propertyName, int& attributes) const
{
  if (_prop.get(propertyName, attributes))
    return true;
    
  // Look in the static hashtable of properties
  const HashEntry* e = findPropertyHashEntry(propertyName);
  if (e) {
    attributes = e->attr;
    return true;
  }
    
  return false;
}

ReferenceList JSObject::propList(ExecState *exec, bool recursive)
{
  ReferenceList list;
  if (_proto->isObject() && recursive)
    list = static_cast<JSObject*>(_proto)->propList(exec,recursive);

  _prop.addEnumerablesToReferenceList(list, this);

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

JSValue *JSObject::toPrimitive(ExecState *exec, Type preferredType) const
{
  return defaultValue(exec,preferredType);
}

bool JSObject::toBoolean(ExecState */*exec*/) const
{
  return true;
}

double JSObject::toNumber(ExecState *exec) const
{
  JSValue *prim = toPrimitive(exec,NumberType);
  if (exec->hadException()) // should be picked up soon in nodes.cpp
    return 0.0;
  return prim->toNumber(exec);
}

UString JSObject::toString(ExecState *exec) const
{
  JSValue *prim = toPrimitive(exec,StringType);
  if (exec->hadException()) // should be picked up soon in nodes.cpp
    return "";
  return prim->toString(exec);
}

JSObject *JSObject::toObject(ExecState */*exec*/) const
{
  return const_cast<JSObject*>(this);
}

void JSObject::putDirect(const Identifier &propertyName, JSValue *value, int attr)
{
    _prop.put(propertyName, value, attr);
}

void JSObject::putDirect(const Identifier &propertyName, int value, int attr)
{
    _prop.put(propertyName, jsNumber(value), attr);
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

JSObject *Error::create(ExecState *exec, ErrorType errtype, const UString &message,
                         int lineno, int sourceId, const UString *sourceURL)
{
  JSObject *cons;
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

  List args;
  if (message.isEmpty())
    args.append(jsString(errorNames[errtype]));
  else
    args.append(jsString(message));
  JSObject *err = static_cast<JSObject *>(cons->construct(exec,args));

  if (lineno != -1)
    err->put(exec, "line", jsNumber(lineno));
  if (sourceId != -1)
    err->put(exec, "sourceId", jsNumber(sourceId));

  if(sourceURL)
   err->put(exec,"sourceURL", jsString(*sourceURL));
 
  return err;

/*
#ifndef NDEBUG
  const char *msg = err->get("message")->toString().value().ascii();
  if (l >= 0)
      fprintf(stderr, "KJS: %s at line %d. %s\n", estr, l, msg);
  else
      fprintf(stderr, "KJS: %s. %s\n", estr, msg);
#endif

  return err;
*/
}

JSObject *Error::create(ExecState *exec, ErrorType type, const char *message)
{
    return create(exec, type, message, -1, -1, NULL);
}

JSObject *throwError(ExecState *exec, ErrorType type)
{
    JSObject *error = Error::create(exec, type, UString(), -1, -1, NULL);
    exec->setException(error);
    return error;
}

JSObject *throwError(ExecState *exec, ErrorType type, const UString &message)
{
    JSObject *error = Error::create(exec, type, message, -1, -1, NULL);
    exec->setException(error);
    return error;
}

JSObject *throwError(ExecState *exec, ErrorType type, const char *message)
{
    JSObject *error = Error::create(exec, type, message, -1, -1, NULL);
    exec->setException(error);
    return error;
}

JSObject *throwError(ExecState *exec, ErrorType type, const UString &message, int line, int sourceId, const UString *sourceURL)
{
    JSObject *error = Error::create(exec, type, message, line, sourceId, sourceURL);
    exec->setException(error);
    return error;
}

} // namespace KJS
