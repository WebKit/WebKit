/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2004, 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSString.h"

#include "ExecState.h"
#include "ArrayPrototype.h"
#include "BooleanObject.h"
#include "collector.h"
#include "date_object.h"
#include "debugger.h"
#include "error_object.h"
#include "FunctionPrototype.h"
#include "lexer.h"
#include "MathObject.h"
#include "nodes.h"
#include "NumberObject.h"
#include "JSObject.h"
#include "object_object.h"
#include "operations.h"
#include "RegExpObject.h"
#include "string_object.h"
#include <math.h>
#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>

namespace KJS {

// ------------------------------ JSString ------------------------------------

JSValue* JSString::toPrimitive(ExecState*, JSType) const
{
  return const_cast<JSString*>(this);
}

bool JSString::getPrimitiveNumber(ExecState*, double& number, JSValue*& value)
{
    value = this;
    number = m_value.toDouble();
    return false;
}

bool JSString::toBoolean(ExecState*) const
{
    return !m_value.isEmpty();
}

double JSString::toNumber(ExecState*) const
{
    return m_value.toDouble();
}

UString JSString::toString(ExecState*) const
{
    return m_value;
}

inline StringObject* StringObject::create(ExecState* exec, JSString* string)
{
    return new StringObject(exec->lexicalGlobalObject()->stringPrototype(), string);
}

JSObject* JSString::toObject(ExecState* exec) const
{
    return StringObject::create(exec, const_cast<JSString*>(this));
}

JSObject* JSString::toThisObject(ExecState* exec) const
{
    return StringObject::create(exec, const_cast<JSString*>(this));
}

JSValue* JSString::lengthGetter(ExecState*, const Identifier&, const PropertySlot& slot)
{
    return jsNumber(static_cast<JSString*>(slot.slotBase())->value().size());
}

JSValue* JSString::indexGetter(ExecState*, const Identifier&, const PropertySlot& slot)
{
    return jsString(static_cast<JSString*>(slot.slotBase())->value().substr(slot.index(), 1));
}

JSValue* JSString::indexNumericPropertyGetter(ExecState*, unsigned index, const PropertySlot& slot)
{
    return jsString(static_cast<JSString*>(slot.slotBase())->value().substr(index, 1));
}

bool JSString::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    // The semantics here are really getPropertySlot, not getOwnPropertySlot.
    // This function should only be called by JSValue::get.
    if (getStringPropertySlot(exec, propertyName, slot))
        return true;
    JSObject* object = StringObject::create(exec, this);
    slot.setBase(object);
    if (object->JSObject::getOwnPropertySlot(exec, propertyName, slot))
        return true;
    while (true) {
        JSValue* proto = object->prototype();
        if (!proto->isObject()) {
            slot.setUndefined();
            return true;
        }
        object = static_cast<JSObject*>(proto);
        if (object->getOwnPropertySlot(exec, propertyName, slot))
            return true;
    }
}

bool JSString::getOwnPropertySlot(ExecState* exec, unsigned propertyName, PropertySlot& slot)
{
    // The semantics here are really getPropertySlot, not getOwnPropertySlot.
    // This function should only be called by JSValue::get.
    if (getStringPropertySlot(propertyName, slot))
        return true;
    return JSString::getOwnPropertySlot(exec, Identifier::from(exec, propertyName), slot);
}

// ------------------------------ JSNumberCell ------------------------------------

JSType JSNumberCell::type() const
{
    return NumberType;
}

JSValue* JSNumberCell::toPrimitive(ExecState*, JSType) const
{
    return const_cast<JSNumberCell*>(this);
}

bool JSNumberCell::getPrimitiveNumber(ExecState*, double& number, JSValue*& value)
{
    number = val;
    value = this;
    return true;
}

bool JSNumberCell::toBoolean(ExecState *) const
{
  return val < 0.0 || val > 0.0; // false for NaN
}

double JSNumberCell::toNumber(ExecState *) const
{
  return val;
}

UString JSNumberCell::toString(ExecState *) const
{
  if (val == 0.0) // +0.0 or -0.0
    return "0";
  return UString::from(val);
}

JSObject *JSNumberCell::toObject(ExecState *exec) const
{
  ArgList args;
  args.append(const_cast<JSNumberCell*>(this));
  return static_cast<JSObject *>(exec->lexicalGlobalObject()->numberConstructor()->construct(exec,args));
}

JSObject* JSNumberCell::toThisObject(ExecState* exec) const
{
    ArgList args;
    args.append(const_cast<JSNumberCell*>(this));
    return static_cast<JSObject*>(exec->lexicalGlobalObject()->numberConstructor()->construct(exec, args));
}

bool JSNumberCell::getUInt32(uint32_t& uint32) const
{
    uint32 = static_cast<uint32_t>(val);
    return uint32 == val;
}

bool JSNumberCell::getTruncatedInt32(int32_t& int32) const
{
    if (!(val >= -2147483648.0 && val < 2147483648.0))
        return false;
    int32 = static_cast<int32_t>(val);
    return true;
}

bool JSNumberCell::getTruncatedUInt32(uint32_t& uint32) const
{
    if (!(val >= 0.0 && val < 4294967296.0))
        return false;
    uint32 = static_cast<uint32_t>(val);
    return true;
}

// --------------------------- GetterSetter ---------------------------------
void GetterSetter::mark()
{
    JSCell::mark();
    
    if (getter && !getter->marked())
        getter->mark();
    if (setter && !setter->marked())
        setter->mark();
}

JSValue* GetterSetter::toPrimitive(ExecState*, JSType) const
{
    ASSERT(false);
    return jsNull();
}

bool GetterSetter::getPrimitiveNumber(ExecState*, double& number, JSValue*& value)
{
    ASSERT_NOT_REACHED();
    number = 0;
    value = 0;
    return true;
}

bool GetterSetter::toBoolean(ExecState*) const
{
    ASSERT(false);
    return false;
}

double GetterSetter::toNumber(ExecState *) const
{
    ASSERT(false);
    return 0.0;
}

UString GetterSetter::toString(ExecState *) const
{
    ASSERT(false);
    return UString::null();
}

JSObject *GetterSetter::toObject(ExecState *exec) const
{
    ASSERT(false);
    return jsNull()->toObject(exec);
}

bool GetterSetter::getOwnPropertySlot(ExecState*, const Identifier&, PropertySlot&)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool GetterSetter::getOwnPropertySlot(ExecState*, unsigned, PropertySlot&)
{
    ASSERT_NOT_REACHED();
    return false;
}

void GetterSetter::put(ExecState*, const Identifier&, JSValue*)
{
    ASSERT_NOT_REACHED();
}

void GetterSetter::put(ExecState*, unsigned, JSValue*)
{
    ASSERT_NOT_REACHED();
}

JSObject* GetterSetter::toThisObject(ExecState*) const
{
    ASSERT_NOT_REACHED();
    return 0;
}

// ------------------------------ LabelStack -----------------------------------

bool LabelStack::push(const Identifier &id)
{
  if (contains(id))
    return false;

  StackElem *newtos = new StackElem;
  newtos->id = id;
  newtos->prev = tos;
  tos = newtos;
  return true;
}

bool LabelStack::contains(const Identifier &id) const
{
  if (id.isEmpty())
    return true;

  for (StackElem *curr = tos; curr; curr = curr->prev)
    if (curr->id == id)
      return true;

  return false;
}

// ------------------------------ InternalFunction --------------------------

const ClassInfo InternalFunction::info = { "Function", 0, 0, 0 };

InternalFunction::InternalFunction()
{
}

InternalFunction::InternalFunction(FunctionPrototype* funcProto, const Identifier& name)
  : JSObject(funcProto)
  , m_name(name)
{
}

CallType InternalFunction::getCallData(CallData&)
{
    return CallTypeNative;
}

bool InternalFunction::implementsHasInstance() const
{
  return true;
}

}
