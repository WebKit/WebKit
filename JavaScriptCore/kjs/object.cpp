// -*- c-basic-offset: 2 -*-
/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
 *  Copyright (C) 2007 Eric Seidel (eric@webkit.org)
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
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "object.h"

#include "date_object.h"
#include "error_object.h"
#include "lookup.h"
#include "nodes.h"
#include "operations.h"
#include "PropertyNameArray.h"
#include <math.h>
#include <wtf/Assertions.h>

// maximum global call stack size. Protects against accidental or
// malicious infinite recursions. Define to -1 if you want no limit.
// In real-world testing it appears ok to bump the stack depth count to 500.
// This of course is dependent on stack frame size.
#define KJS_MAX_STACK 500

#define JAVASCRIPT_CALL_TRACING 0
#define JAVASCRIPT_MARK_TRACING 0

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
  ASSERT(implementsCall());

#if KJS_MAX_STACK > 0
  static int depth = 0; // sum of all extant function calls

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

#if JAVASCRIPT_MARK_TRACING
  static int markStackDepth = 0;
  markStackDepth++;
  for (int i = 0; i < markStackDepth; i++)
    putchar('-');
  
  printf("%s (%p)\n", className().UTF8String().c_str(), this);
#endif
  
  JSValue *proto = _proto;
  if (!proto->marked())
    proto->mark();

  _prop.mark();
  
#if JAVASCRIPT_MARK_TRACING
  markStackDepth--;
#endif
}

JSType JSObject::type() const
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

static void throwSetterError(ExecState *exec)
{
  throwError(exec, TypeError, "setting a property that has only a getter");
}

// ECMA 8.6.2.2
void JSObject::put(ExecState* exec, const Identifier &propertyName, JSValue *value, int attr)
{
  ASSERT(value);

  if (propertyName == exec->propertyNames().underscoreProto) {
    JSObject* proto = value->getObject();
    while (proto) {
      if (proto == this)
        throwError(exec, GeneralError, "cyclic __proto__ value");
      proto = proto->prototype() ? proto->prototype()->getObject() : 0;
    }
    
    setPrototype(value);
    return;
  }

  // The put calls from JavaScript execution either have no attributes set, or in some cases
  // have DontDelete set. For those calls, respect the ReadOnly flag.
  bool checkReadOnly = !(attr & ~DontDelete);

  // Check if there are any setters or getters in the prototype chain
  JSObject *obj = this;
  bool hasGettersOrSetters = false;
  while (true) {
    if (obj->_prop.hasGetterSetterProperties()) {
      hasGettersOrSetters = true;
      break;
    }
      
    if (!obj->_proto->isObject())
      break;
      
    obj = static_cast<JSObject *>(obj->_proto);
  }
  
  if (hasGettersOrSetters) {
    if (checkReadOnly && !canPut(exec, propertyName))
      return;

    obj = this;
    while (true) {
      unsigned attributes;
      if (JSValue *gs = obj->_prop.get(propertyName, attributes)) {
        if (attributes & GetterSetter) {
          JSObject *setterFunc = static_cast<GetterSetterImp *>(gs)->getSetter();
        
          if (!setterFunc) {
            throwSetterError(exec);
            return;
          }
            
          List args;
          args.append(value);
        
          setterFunc->call(exec, this, args);
          return;
        } else {
          // If there's an existing property on the object or one of its 
          // prototype it should be replaced, so we just break here.
          break;
        }
      }
     
      if (!obj->_proto->isObject())
        break;
        
      obj = static_cast<JSObject *>(obj->_proto);
    }
  }
  
  _prop.put(propertyName, value, attr, checkReadOnly);
}

void JSObject::put(ExecState *exec, unsigned propertyName,
                     JSValue *value, int attr)
{
  put(exec, Identifier::from(propertyName), value, attr);
}

// ECMA 8.6.2.3
bool JSObject::canPut(ExecState *, const Identifier &propertyName) const
{
  unsigned attributes;
    
  // Don't look in the prototype here. We can always put an override
  // in the object, even if the prototype has a ReadOnly property.
  // Also, there is no need to check the static property table, as this
  // would have been done by the subclass already.

  if (!_prop.get(propertyName, attributes))
    return true;

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
bool JSObject::deleteProperty(ExecState* /*exec*/, const Identifier &propertyName)
{
  unsigned attributes;
  JSValue *v = _prop.get(propertyName, attributes);
  if (v) {
    if ((attributes & DontDelete))
      return false;
    _prop.remove(propertyName);
    if (attributes & GetterSetter) 
        _prop.setHasGetterSetterProperties(_prop.containsGettersOrSetters());
    return true;
  }

  // Look in the static hashtable of properties
  const HashEntry* entry = findPropertyHashEntry(propertyName);
  if (entry && entry->attr & DontDelete)
    return false; // this builtin property can't be deleted
  return true;
}

bool JSObject::hasOwnProperty(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot;
    return const_cast<JSObject*>(this)->getOwnPropertySlot(exec, propertyName, slot);
}

bool JSObject::deleteProperty(ExecState *exec, unsigned propertyName)
{
  return deleteProperty(exec, Identifier::from(propertyName));
}

static ALWAYS_INLINE JSValue *tryGetAndCallProperty(ExecState *exec, const JSObject *object, const Identifier &propertyName) {
  JSValue *v = object->get(exec, propertyName);
  if (v->isObject()) {
    JSObject *o = static_cast<JSObject*>(v);
    if (o->implementsCall()) { // spec says "not primitive type" but ...
      JSObject *thisObj = const_cast<JSObject*>(object);
      JSValue* def = o->call(exec, thisObj, exec->emptyList());
      JSType defType = def->type();
      ASSERT(defType != GetterSetterType);
      if (defType != ObjectType)
        return def;
    }
  }
  return NULL;
}

bool JSObject::getPrimitiveNumber(ExecState* exec, double& number, JSValue*& result)
{
    result = defaultValue(exec, NumberType);
    number = result->toNumber(exec);
    return !result->isString();
}

// ECMA 8.6.2.6
JSValue* JSObject::defaultValue(ExecState* exec, JSType hint) const
{
  /* Prefer String for Date objects */
  if ((hint == StringType) || (hint != NumberType && _proto == exec->lexicalGlobalObject()->datePrototype())) {
    if (JSValue* v = tryGetAndCallProperty(exec, this, exec->propertyNames().toString))
      return v;
    if (JSValue* v = tryGetAndCallProperty(exec, this, exec->propertyNames().valueOf))
      return v;
  } else {
    if (JSValue* v = tryGetAndCallProperty(exec, this, exec->propertyNames().valueOf))
      return v;
    if (JSValue* v = tryGetAndCallProperty(exec, this, exec->propertyNames().toString))
      return v;
  }

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

void JSObject::defineGetter(ExecState*, const Identifier& propertyName, JSObject* getterFunc)
{
    JSValue *o = getDirect(propertyName);
    GetterSetterImp *gs;
    
    if (o && o->type() == GetterSetterType) {
        gs = static_cast<GetterSetterImp *>(o);
    } else {
        gs = new GetterSetterImp;
        putDirect(propertyName, gs, GetterSetter);
    }
    
    _prop.setHasGetterSetterProperties(true);
    gs->setGetter(getterFunc);
}

void JSObject::defineSetter(ExecState*, const Identifier& propertyName, JSObject* setterFunc)
{
    JSValue *o = getDirect(propertyName);
    GetterSetterImp *gs;
    
    if (o && o->type() == GetterSetterType) {
        gs = static_cast<GetterSetterImp *>(o);
    } else {
        gs = new GetterSetterImp;
        putDirect(propertyName, gs, GetterSetter);
    }
    
    _prop.setHasGetterSetterProperties(true);
    gs->setSetter(setterFunc);
}

bool JSObject::implementsConstruct() const
{
  return false;
}

JSObject* JSObject::construct(ExecState*, const List& /*args*/)
{
  ASSERT(false);
  return NULL;
}

JSObject* JSObject::construct(ExecState* exec, const List& args, const Identifier& /*functionName*/, const UString& /*sourceURL*/, int /*lineNumber*/)
{
  return construct(exec, args);
}

bool JSObject::implementsCall() const
{
  return false;
}

JSValue *JSObject::callAsFunction(ExecState* /*exec*/, JSObject* /*thisObj*/, const List &/*args*/)
{
  ASSERT(false);
  return NULL;
}

bool JSObject::implementsHasInstance() const
{
  return false;
}

bool JSObject::hasInstance(ExecState* exec, JSValue* value)
{
    JSValue* proto = get(exec, exec->propertyNames().prototype);
    if (!proto->isObject()) {
        throwError(exec, TypeError, "intanceof called on an object with an invalid prototype property.");
        return false;
    }
    
    if (!value->isObject())
        return false;
    
    JSObject* o = static_cast<JSObject*>(value);
    while ((o = o->prototype()->getObject())) {
        if (o == proto)
            return true;
    }
    return false;
}

bool JSObject::propertyIsEnumerable(ExecState*, const Identifier& propertyName) const
{
  unsigned attributes;
 
  if (!getPropertyAttributes(propertyName, attributes))
    return false;
  else
    return !(attributes & DontEnum);
}

bool JSObject::getPropertyAttributes(const Identifier& propertyName, unsigned& attributes) const
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

void JSObject::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
   _prop.getEnumerablePropertyNames(propertyNames);

  // Add properties from the static hashtable of properties
  const ClassInfo *info = classInfo();
  while (info) {
    if (info->propHashTable) {
      int size = info->propHashTable->size;
      const HashEntry *e = info->propHashTable->entries;
      for (int i = 0; i < size; ++i, ++e) {
        if (e->s && !(e->attr & DontEnum))
          propertyNames.add(e->s);
      }
    }
    info = info->parentClass;
  }
  if (_proto->isObject())
     static_cast<JSObject*>(_proto)->getPropertyNames(exec, propertyNames);
}

bool JSObject::toBoolean(ExecState*) const
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

JSObject *JSObject::toObject(ExecState*) const
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

void JSObject::removeDirect(const Identifier &propertyName)
{
    _prop.remove(propertyName);
}

void JSObject::putDirectFunction(InternalFunctionImp* func, int attr)
{
    putDirect(func->functionName(), func, attr); 
}

void JSObject::fillGetterPropertySlot(PropertySlot& slot, JSValue **location)
{
    GetterSetterImp *gs = static_cast<GetterSetterImp *>(*location);
    JSObject *getterFunc = gs->getGetter();
    if (getterFunc)
        slot.setGetterSlot(this, getterFunc);
    else
        slot.setUndefined(this);
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
                         int lineno, int sourceId, const UString &sourceURL)
{
  JSObject *cons;
  switch (errtype) {
  case EvalError:
    cons = exec->lexicalGlobalObject()->evalErrorConstructor();
    break;
  case RangeError:
    cons = exec->lexicalGlobalObject()->rangeErrorConstructor();
    break;
  case ReferenceError:
    cons = exec->lexicalGlobalObject()->referenceErrorConstructor();
    break;
  case SyntaxError:
    cons = exec->lexicalGlobalObject()->syntaxErrorConstructor();
    break;
  case TypeError:
    cons = exec->lexicalGlobalObject()->typeErrorConstructor();
    break;
  case URIError:
    cons = exec->lexicalGlobalObject()->URIErrorConstructor();
    break;
  default:
    cons = exec->lexicalGlobalObject()->errorConstructor();
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

  if(!sourceURL.isNull())
    err->put(exec, "sourceURL", jsString(sourceURL));
 
  return err;
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

JSObject *throwError(ExecState *exec, ErrorType type, const UString &message, int line, int sourceId, const UString &sourceURL)
{
    JSObject *error = Error::create(exec, type, message, line, sourceId, sourceURL);
    exec->setException(error);
    return error;
}

} // namespace KJS
