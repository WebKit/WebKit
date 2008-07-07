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
#include "ErrorConstructor.h"
#include "GetterSetter.h"
#include "JSGlobalObject.h"
#include "NativeErrorConstructor.h"
#include "ObjectPrototype.h"
#include "PropertyNameArray.h"
#include "lookup.h"
#include "nodes.h"
#include "operations.h"
#include <math.h>
#include <profiler/Profiler.h>
#include <wtf/Assertions.h>

#define JAVASCRIPT_MARK_TRACING 0

namespace KJS {

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

    JSValue* prototype = m_prototype;
    if (!prototype->marked())
        prototype->mark();

    m_propertyMap.mark();

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
    const ClassInfo* info = classInfo();
    if (info)
        return info->className;
    return "Object";
}

bool JSObject::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    return getOwnPropertySlot(exec, Identifier::from(exec, propertyName), slot);
}

static void throwSetterError(ExecState* exec)
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
    for (JSObject* obj = this; !obj->m_propertyMap.hasGetterSetterProperties(); obj = static_cast<JSObject*>(prototype)) {
        prototype = obj->m_prototype;
        if (prototype == jsNull()) {
            m_propertyMap.put(propertyName, value, 0, true);
            return;
        }
    }
    
    unsigned attributes;
    if (m_propertyMap.get(propertyName, attributes) && attributes & ReadOnly)
        return;

    for (JSObject* obj = this; ; obj = static_cast<JSObject*>(prototype)) {
        if (JSValue* gs = obj->m_propertyMap.get(propertyName, attributes)) {
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

        prototype = obj->m_prototype;
        if (prototype == jsNull())
            break;
    }
    
    m_propertyMap.put(propertyName, value, 0, true);
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

bool JSObject::hasProperty(ExecState* exec, const Identifier& propertyName) const
{
    PropertySlot slot;
    return const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot);
}

bool JSObject::hasProperty(ExecState* exec, unsigned propertyName) const
{
    PropertySlot slot;
    return const_cast<JSObject*>(this)->getPropertySlot(exec, propertyName, slot);
}

// ECMA 8.6.2.5
bool JSObject::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    unsigned attributes;
    JSValue* v = m_propertyMap.get(propertyName, attributes);
    if (v) {
        if ((attributes & DontDelete))
            return false;
        m_propertyMap.remove(propertyName);
        if (attributes & IsGetterSetter) 
            m_propertyMap.setHasGetterSetterProperties(m_propertyMap.containsGettersOrSetters());
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

bool JSObject::deleteProperty(ExecState* exec, unsigned propertyName)
{
    return deleteProperty(exec, Identifier::from(exec, propertyName));
}

static ALWAYS_INLINE JSValue* callDefaultValueFunction(ExecState* exec, const JSObject* object, const Identifier& propertyName)
{
    JSValue* function = object->get(exec, propertyName);
    CallData callData;
    CallType callType = function->getCallData(callData);
    if (callType == CallTypeNone)
        return exec->exception();

    // Prevent "toString" and "valueOf" from observing execution if an exception
    // is pending.
    if (exec->hadException())
        return exec->exception();

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
    // Must call toString first for Date objects.
    if ((hint == StringType) || (hint != NumberType && m_prototype == exec->lexicalGlobalObject()->datePrototype())) {
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
            if (const HashEntry* entry = propHashTable->entry(exec, propertyName))
                return entry;
        }
    }
    return 0;
}

void JSObject::defineGetter(ExecState* exec, const Identifier& propertyName, JSObject* getterFunction)
{
    JSValue* object = getDirect(propertyName);
    GetterSetter* getterSetter;
    if (object && object->type() == GetterSetterType)
        getterSetter = static_cast<GetterSetter*>(object);
    else {
        getterSetter = new (exec) GetterSetter;
        putDirect(propertyName, getterSetter, IsGetterSetter);
    }

    m_propertyMap.setHasGetterSetterProperties(true);
    getterSetter->setGetter(getterFunction);
}

void JSObject::defineSetter(ExecState* exec, const Identifier& propertyName, JSObject* setterFunction)
{
    JSValue* object = getDirect(propertyName);
    GetterSetter* getterSetter;
    if (object && object->type() == GetterSetterType)
        getterSetter = static_cast<GetterSetter*>(object);
    else {
        getterSetter = new (exec) GetterSetter;
        putDirect(propertyName, getterSetter, IsGetterSetter);
    }
    
    m_propertyMap.setHasGetterSetterProperties(true);
    getterSetter->setSetter(setterFunction);
}

JSValue* JSObject::lookupGetter(ExecState*, const Identifier& propertyName)
{
    JSObject* object = this;
    while (true) {
        JSValue* value = object->getDirect(propertyName);
        if (value) {
            if (value->type() != GetterSetterType)
                return jsUndefined();
            JSObject* functionObject = static_cast<GetterSetter*>(value)->getter();
            if (!functionObject)
                return jsUndefined();
            return functionObject;
        }

        if (!object->prototype() || !object->prototype()->isObject())
            return jsUndefined();
        object = static_cast<JSObject*>(object->prototype());
    }
}

JSValue* JSObject::lookupSetter(ExecState*, const Identifier& propertyName)
{
    JSObject* object = this;
    while (true) {
        JSValue* value = object->getDirect(propertyName);
        if (value) {
            if (value->type() != GetterSetterType)
                return jsUndefined();
            JSObject* functionObject = static_cast<GetterSetter*>(value)->setter();
            if (!functionObject)
                return jsUndefined();
            return functionObject;
        }

        if (!object->prototype() || !object->prototype()->isObject())
            return jsUndefined();
        object = static_cast<JSObject*>(object->prototype());
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

    JSObject* object = static_cast<JSObject*>(value);
    while ((object = object->prototype()->getObject())) {
        if (object == proto)
            return true;
    }
    return false;
}

bool JSObject::propertyIsEnumerable(ExecState* exec, const Identifier& propertyName) const
{
    unsigned attributes;
    if (!getPropertyAttributes(exec, propertyName, attributes))
        return false;
    return !(attributes & DontEnum);
}

bool JSObject::getPropertyAttributes(ExecState* exec, const Identifier& propertyName, unsigned& attributes) const
{
    if (m_propertyMap.get(propertyName, attributes))
        return true;
    
    // Look in the static hashtable of properties
    const HashEntry* entry = findPropertyHashEntry(exec, propertyName);
    if (entry) {
        attributes = entry->attributes;
        return true;
    }
    
    return false;
}

void JSObject::getPropertyNames(ExecState* exec, PropertyNameArray& propertyNames)
{
    m_propertyMap.getEnumerablePropertyNames(propertyNames);

    // Add properties from the static hashtables of properties
    for (const ClassInfo* info = classInfo(); info; info = info->parentClass) {
        const HashTable* table = info->propHashTable(exec);
        if (!table)
            continue;
        table->initializeIfNeeded(exec);
        ASSERT(table->table);
        int hashSizeMask = table->hashSizeMask;
        const HashEntry* entry = table->table;
        for (int i = 0; i <= hashSizeMask; ++i, ++entry) {
            if (entry->key && !(entry->attributes & DontEnum))
                propertyNames.add(entry->key);
        }
    }

    if (m_prototype->isObject())
        static_cast<JSObject*>(m_prototype)->getPropertyNames(exec, propertyNames);
}

bool JSObject::toBoolean(ExecState*) const
{
    return true;
}

double JSObject::toNumber(ExecState* exec) const
{
    JSValue* primitive = toPrimitive(exec, NumberType);
    if (exec->hadException()) // should be picked up soon in nodes.cpp
        return 0.0;
    return primitive->toNumber(exec);
}

UString JSObject::toString(ExecState* exec) const
{
    JSValue* primitive = toPrimitive(exec, StringType);
    if (exec->hadException())
        return "";
    return primitive->toString(exec);
}

JSObject* JSObject::toObject(ExecState*) const
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

void JSObject::removeDirect(const Identifier& propertyName)
{
    m_propertyMap.remove(propertyName);
}

void JSObject::putDirectFunction(InternalFunction* function, unsigned attr)
{
    putDirect(function->functionName(), function, attr);
}

void JSObject::fillGetterPropertySlot(PropertySlot& slot, JSValue** location)
{
    if (JSObject* getterFunction = static_cast<GetterSetter*>(*location)->getter())
        slot.setGetterSlot(getterFunction);
    else
        slot.setUndefined();
}

JSObject* constructEmptyObject(ExecState* exec)
{
    return new (exec) JSObject(exec->lexicalGlobalObject()->objectPrototype());
}

} // namespace KJS
