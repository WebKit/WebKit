// -*- c-basic-offset: 2 -*-
/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
#include "JSObject.h"

#include "DatePrototype.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "error_object.h"
#include "nodes.h"
#include "operations.h"
#include <math.h>
#include <profiler/Profiler.h>
#include <wtf/Assertions.h>

#define JAVASCRIPT_MARK_TRACING 0

namespace KJS {

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

UString JSObject::className() const
{
  const ClassInfo *ci = classInfo();
  if ( ci )
    return ci->className;
  return "Object";
}

bool JSObject::getOwnPropertySlot(ExecState *exec, unsigned propertyName, PropertySlot& slot)
{
  return getOwnPropertySlot(exec, Identifier::from(exec, propertyName), slot);
}

static void throwSetterError(ExecState *exec)
{
  throwError(exec, TypeError, "setting a property that has only a getter");
}

// ECMA 8.6.2.2
void JSObject::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
  ASSERT(value);
  ASSERT(!Heap::heap(value) || Heap::heap(value) == Heap::heap(this));

  if (propertyName == exec->propertyNames().underscoreProto) {
    JSObject* proto = value->getObject();

    // Setting __proto__ to a non-object, non-null value is silently ignored to match Mozilla.
    if (!proto && value != jsNull())
      return;

    while (proto) {
      if (proto == this) {
        throwError(exec, GeneralError, "cyclic __proto__ value");
        return;
      }
      proto = proto->prototype() ? proto->prototype()->getObject() : 0;
    }

    setPrototype(value);
    return;
  }

  // Check if there are any setters or getters in the prototype chain
  JSValue* prototype;
  for (JSObject* obj = this; !obj->_prop.hasGetterSetterProperties(); obj = static_cast<JSObject*>(prototype)) {
    prototype = obj->_proto;
    if (prototype == jsNull()) {
      _prop.put(propertyName, value, 0, true);
      return;
    }
  }

  unsigned attributes;
  if (_prop.get(propertyName, attributes) && attributes & ReadOnly)
    return;

  for (JSObject* obj = this; ; obj = static_cast<JSObject*>(prototype)) {
    if (JSValue* gs = obj->_prop.get(propertyName, attributes)) {
      if (attributes & IsGetterSetter) {
        JSObject* setterFunc = static_cast<GetterSetter*>(gs)->setter();        
        if (!setterFunc) {
          throwSetterError(exec);
          return;
        }

        CallData callData;
        CallType callType = setterFunc->getCallData(callData);
        ArgList args;
        args.append(value);
        call(exec, setterFunc, callType, callData, this, args);
        return;
      }

      // If there's an existing property on the object or one of its 
      // prototypes it should be replaced, so break here.
      break;
    }

    prototype = obj->_proto;
    if (prototype == jsNull())
      break;
  }

  _prop.put(propertyName, value, 0, true);
}

void JSObject::put(ExecState* exec, unsigned propertyName, JSValue* value)
{
    put(exec, Identifier::from(exec, propertyName), value);
}

void JSObject::putWithAttributes(ExecState*, const Identifier& propertyName, JSValue* value, unsigned attributes)
{
    putDirect(propertyName, value, attributes);
}

void JSObject::putWithAttributes(ExecState* exec, unsigned propertyName, JSValue* value, unsigned attributes)
{
    putWithAttributes(exec, Identifier::from(exec, propertyName), value, attributes);
}

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
bool JSObject::deleteProperty(ExecState* exec, const Identifier &propertyName)
{
  unsigned attributes;
  JSValue *v = _prop.get(propertyName, attributes);
  if (v) {
    if ((attributes & DontDelete))
      return false;
    _prop.remove(propertyName);
    if (attributes & IsGetterSetter) 
        _prop.setHasGetterSetterProperties(_prop.containsGettersOrSetters());
    return true;
  }

  // Look in the static hashtable of properties
  const HashEntry* entry = findPropertyHashEntry(exec, propertyName);
  if (entry && entry->attributes & DontDelete)
    return false; // this builtin property can't be deleted
  // FIXME: Should the code here actually do some deletion?
  return true;
}

bool JSObject::hasOwnProperty(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot;
    return const_cast<JSObject*>(this)->getOwnPropertySlot(exec, propertyName, slot);
}

bool JSObject::deleteProperty(ExecState *exec, unsigned propertyName)
{
  return deleteProperty(exec, Identifier::from(exec, propertyName));
}

static ALWAYS_INLINE JSValue* callDefaultValueFunction(ExecState* exec, const JSObject* object, const Identifier& propertyName)
{
    JSValue* function = object->get(exec, propertyName);
    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone)
        return 0;
    JSValue* result = call(exec, function, callType, callData, const_cast<JSObject*>(object), exec->emptyList());
    ASSERT(result->type() != GetterSetterType);
    if (exec->hadException())
        return exec->exception();
    if (result->isObject())
        return 0;
    return result;
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
    // We need this check to guard against the case where this object is rhs of
    // a binary expression where lhs threw an exception in its conversion to
    // primitive.
    if (exec->hadException())
        return exec->exception();

    // Must call toString first for Date objects.
    if ((hint == StringType) || (hint != NumberType && _proto == exec->lexicalGlobalObject()->datePrototype())) {
        if (JSValue* value = callDefaultValueFunction(exec, this, exec->propertyNames().toString))
            return value;
        if (JSValue* value = callDefaultValueFunction(exec, this, exec->propertyNames().valueOf))
            return value;
    } else {
        if (JSValue* value = callDefaultValueFunction(exec, this, exec->propertyNames().valueOf))
            return value;
        if (JSValue* value = callDefaultValueFunction(exec, this, exec->propertyNames().toString))
            return value;
    }

    ASSERT(!exec->hadException());

    return throwError(exec, TypeError, "No default value");
}

const HashEntry* JSObject::findPropertyHashEntry(ExecState* exec, const Identifier& propertyName) const
{
    for (const ClassInfo* info = classInfo(); info; info = info->parentClass) {
        if (const HashTable* propHashTable = info->propHashTable(exec)) {
            if (const HashEntry* e = propHashTable->entry(exec, propertyName))
                return e;
        }
    }
    return 0;
}

void JSObject::defineGetter(ExecState* exec, const Identifier& propertyName, JSObject* getterFunc)
{
    JSValue *o = getDirect(propertyName);
    GetterSetter *gs;
    
    if (o && o->type() == GetterSetterType) {
        gs = static_cast<GetterSetter *>(o);
    } else {
        gs = new (exec) GetterSetter;
        putDirect(propertyName, gs, IsGetterSetter);
    }
    
    _prop.setHasGetterSetterProperties(true);
    gs->setGetter(getterFunc);
}

void JSObject::defineSetter(ExecState* exec, const Identifier& propertyName, JSObject* setterFunc)
{
    JSValue *o = getDirect(propertyName);
    GetterSetter *gs;
    
    if (o && o->type() == GetterSetterType) {
        gs = static_cast<GetterSetter *>(o);
    } else {
        gs = new (exec) GetterSetter;
        putDirect(propertyName, gs, IsGetterSetter);
    }
    
    _prop.setHasGetterSetterProperties(true);
    gs->setSetter(setterFunc);
}

JSValue* JSObject::lookupGetter(ExecState*, const Identifier& propertyName)
{
    JSObject* obj = this;
    while (true) {
        JSValue* v = obj->getDirect(propertyName);
        if (v) {
            if (v->type() != GetterSetterType)
                return jsUndefined();
            JSObject* funcObj = static_cast<GetterSetter*>(v)->getter();
            if (!funcObj)
                return jsUndefined();
            return funcObj;
        }

        if (!obj->prototype() || !obj->prototype()->isObject())
            return jsUndefined();
        obj = static_cast<JSObject*>(obj->prototype());
    }
}

JSValue* JSObject::lookupSetter(ExecState*, const Identifier& propertyName)
{
    JSObject* obj = this;
    while (true) {
        JSValue* v = obj->getDirect(propertyName);
        if (v) {
            if (v->type() != GetterSetterType)
                return jsUndefined();
            JSObject* funcObj = static_cast<GetterSetter*>(v)->setter();
            if (!funcObj)
                return jsUndefined();
            return funcObj;
        }

        if (!obj->prototype() || !obj->prototype()->isObject())
            return jsUndefined();
        obj = static_cast<JSObject*>(obj->prototype());
    }
}

bool JSObject::implementsHasInstance() const
{
  return false;
}

bool JSObject::hasInstance(ExecState* exec, JSValue* value)
{
    JSValue* proto = get(exec, exec->propertyNames().prototype);
    if (!proto->isObject()) {
        throwError(exec, TypeError, "instanceof called on an object with an invalid prototype property.");
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

bool JSObject::propertyIsEnumerable(ExecState* exec, const Identifier& propertyName) const
{
  unsigned attributes;
 
  if (!getPropertyAttributes(exec, propertyName, attributes))
    return false;
  else
    return !(attributes & DontEnum);
}

bool JSObject::getPropertyAttributes(ExecState* exec, const Identifier& propertyName, unsigned& attributes) const
{
  if (_prop.get(propertyName, attributes))
    return true;
    
  // Look in the static hashtable of properties
  const HashEntry* e = findPropertyHashEntry(exec, propertyName);
  if (e) {
    attributes = e->attributes;
    return true;
  }
    
  return false;
}

void JSObject::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    _prop.getEnumerablePropertyNames(propertyNames);

    // Add properties from the static hashtables of properties
    for (const ClassInfo* info = classInfo(); info; info = info->parentClass) {
        const HashTable* table = info->propHashTable(exec);
        if (!table)
            continue;
        table->initializeIfNeeded(exec);
        ASSERT(table->table);
        int hashSizeMask = table->hashSizeMask;
        const HashEntry* e = table->table;
        for (int i = 0; i <= hashSizeMask; ++i, ++e) {
            if (e->key && !(e->attributes & DontEnum))
                propertyNames.add(e->key);
        }
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

UString JSObject::toString(ExecState* exec) const
{
    JSValue* primitive = toPrimitive(exec, StringType);
    if (exec->hadException())
        return "";
    return primitive->toString(exec);
}

JSObject *JSObject::toObject(ExecState*) const
{
  return const_cast<JSObject*>(this);
}

JSObject* JSObject::toThisObject(ExecState*) const
{
    return const_cast<JSObject*>(this);
}

JSGlobalObject* JSObject::toGlobalObject(ExecState*) const
{
    return 0;
}

void JSObject::removeDirect(const Identifier &propertyName)
{
    _prop.remove(propertyName);
}

void JSObject::putDirectFunction(InternalFunction* func, int attr)
{
    putDirect(func->functionName(), func, attr); 
}

void JSObject::fillGetterPropertySlot(PropertySlot& slot, JSValue** location)
{
    if (JSObject* getterFunc = static_cast<GetterSetter*>(*location)->getter())
        slot.setGetterSlot(getterFunc);
    else
        slot.setUndefined();
}

// ------------------------------ Error ----------------------------------------

JSObject* Error::create(ExecState* exec, ErrorType errtype, const UString& message,
    int lineno, int sourceId, const UString& sourceURL)
{
  JSObject* cons;
  const char* name;
  switch (errtype) {
  case EvalError:
    cons = exec->lexicalGlobalObject()->evalErrorConstructor();
    name = "Evaluation error";
    break;
  case RangeError:
    cons = exec->lexicalGlobalObject()->rangeErrorConstructor();
    name = "Range error";
    break;
  case ReferenceError:
    cons = exec->lexicalGlobalObject()->referenceErrorConstructor();
    name = "Reference error";
    break;
  case SyntaxError:
    cons = exec->lexicalGlobalObject()->syntaxErrorConstructor();
    name = "Syntax error";
    break;
  case TypeError:
    cons = exec->lexicalGlobalObject()->typeErrorConstructor();
    name = "Type error";
    break;
  case URIError:
    cons = exec->lexicalGlobalObject()->URIErrorConstructor();
    name = "URI error";
    break;
  default:
    cons = exec->lexicalGlobalObject()->errorConstructor();
    name = "Error";
    break;
  }

  ArgList args;
  if (message.isEmpty())
    args.append(jsString(exec, name));
  else
    args.append(jsString(exec, message));
  ConstructData constructData;
  ConstructType constructType = cons->getConstructData(constructData);
  JSObject* err = construct(exec, cons, constructType, constructData, args);

  if (lineno != -1)
    err->put(exec, Identifier(exec, "line"), jsNumber(exec, lineno));
  if (sourceId != -1)
    err->put(exec, Identifier(exec, "sourceId"), jsNumber(exec, sourceId));

  if(!sourceURL.isNull())
    err->put(exec, Identifier(exec, "sourceURL"), jsString(exec, sourceURL));
 
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

JSObject* constructEmptyObject(ExecState* exec)
{
    return new (exec) JSObject(exec->lexicalGlobalObject()->objectPrototype());
}

} // namespace KJS
