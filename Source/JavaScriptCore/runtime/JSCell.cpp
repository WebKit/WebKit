/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2007, 2008 Apple Inc. All rights reserved.
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
#include "JSCell.h"

#include "JSFunction.h"
#include "JSString.h"
#include "JSObject.h"
#include "NumberObject.h"
#include <wtf/MathExtras.h>

namespace JSC {

ASSERT_HAS_TRIVIAL_DESTRUCTOR(JSCell);

void JSCell::destroy(JSCell* cell)
{
    cell->JSCell::~JSCell();
}

bool JSCell::getString(ExecState* exec, UString&stringValue) const
{
    if (!isString())
        return false;
    stringValue = static_cast<const JSString*>(this)->value(exec);
    return true;
}

UString JSCell::getString(ExecState* exec) const
{
    return isString() ? static_cast<const JSString*>(this)->value(exec) : UString();
}

JSObject* JSCell::getObject()
{
    return isObject() ? asObject(this) : 0;
}

const JSObject* JSCell::getObject() const
{
    return isObject() ? static_cast<const JSObject*>(this) : 0;
}

CallType JSCell::getCallData(JSCell*, CallData&)
{
    return CallTypeNone;
}

ConstructType JSCell::getConstructData(JSCell*, ConstructData&)
{
    return ConstructTypeNone;
}

bool JSCell::getOwnPropertySlot(JSCell* cell, ExecState* exec, const Identifier& identifier, PropertySlot& slot)
{
    // This is not a general purpose implementation of getOwnPropertySlot.
    // It should only be called by JSValue::get.
    // It calls getPropertySlot, not getOwnPropertySlot.
    JSObject* object = cell->toObject(exec, exec->lexicalGlobalObject());
    slot.setBase(object);
    if (!object->getPropertySlot(exec, identifier, slot))
        slot.setUndefined();
    return true;
}

bool JSCell::getOwnPropertySlotByIndex(JSCell* cell, ExecState* exec, unsigned identifier, PropertySlot& slot)
{
    // This is not a general purpose implementation of getOwnPropertySlot.
    // It should only be called by JSValue::get.
    // It calls getPropertySlot, not getOwnPropertySlot.
    JSObject* object = cell->toObject(exec, exec->lexicalGlobalObject());
    slot.setBase(object);
    if (!object->getPropertySlot(exec, identifier, slot))
        slot.setUndefined();
    return true;
}

void JSCell::put(JSCell* cell, ExecState* exec, const Identifier& identifier, JSValue value, PutPropertySlot& slot)
{
    JSObject* thisObject = cell->toObject(exec, exec->lexicalGlobalObject());
    thisObject->methodTable()->put(thisObject, exec, identifier, value, slot);
}

void JSCell::putByIndex(JSCell* cell, ExecState* exec, unsigned identifier, JSValue value)
{
    JSObject* thisObject = cell->toObject(exec, exec->lexicalGlobalObject());
    thisObject->methodTable()->putByIndex(thisObject, exec, identifier, value);
}

bool JSCell::deleteProperty(JSCell* cell, ExecState* exec, const Identifier& identifier)
{
    JSObject* thisObject = cell->toObject(exec, exec->lexicalGlobalObject());
    return thisObject->methodTable()->deleteProperty(thisObject, exec, identifier);
}

bool JSCell::deletePropertyByIndex(JSCell* cell, ExecState* exec, unsigned identifier)
{
    JSObject* thisObject = cell->toObject(exec, exec->lexicalGlobalObject());
    return thisObject->methodTable()->deletePropertyByIndex(thisObject, exec, identifier);
}

JSObject* JSCell::toThisObject(JSCell* cell, ExecState* exec)
{
    return cell->toObject(exec, exec->lexicalGlobalObject());
}

JSValue JSCell::toPrimitive(ExecState* exec, PreferredPrimitiveType preferredType) const
{
    if (isString())
        return static_cast<const JSString*>(this)->toPrimitive(exec, preferredType);
    return static_cast<const JSObject*>(this)->toPrimitive(exec, preferredType);
}

bool JSCell::getPrimitiveNumber(ExecState* exec, double& number, JSValue& value) const
{
    if (isString())
        return static_cast<const JSString*>(this)->getPrimitiveNumber(exec, number, value);
    return static_cast<const JSObject*>(this)->getPrimitiveNumber(exec, number, value);
}

double JSCell::toNumber(ExecState* exec) const
{ 
    if (isString())
        return static_cast<const JSString*>(this)->toNumber(exec);
    return static_cast<const JSObject*>(this)->toNumber(exec);
}

JSObject* JSCell::toObject(ExecState* exec, JSGlobalObject* globalObject) const
{
    if (isString())
        return static_cast<const JSString*>(this)->toObject(exec, globalObject);
    ASSERT(isObject());
    return static_cast<JSObject*>(const_cast<JSCell*>(this));
}

void slowValidateCell(JSCell* cell)
{
    ASSERT_GC_OBJECT_LOOKS_VALID(cell);
}

void JSCell::defineGetter(JSObject*, ExecState*, const Identifier&, JSObject*, unsigned)
{
    ASSERT_NOT_REACHED();
}

void JSCell::defineSetter(JSObject*, ExecState*, const Identifier&, JSObject*, unsigned)
{
    ASSERT_NOT_REACHED();
}

JSValue JSCell::defaultValue(const JSObject*, ExecState*, PreferredPrimitiveType)
{
    ASSERT_NOT_REACHED();
    return jsUndefined();
}

void JSCell::getOwnPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode)
{
    ASSERT_NOT_REACHED();
}

UString JSCell::className(const JSObject*)
{
    ASSERT_NOT_REACHED();
    return UString();
}

void JSCell::getPropertyNames(JSObject*, ExecState*, PropertyNameArray&, EnumerationMode)
{
    ASSERT_NOT_REACHED();
}

bool JSCell::hasInstance(JSObject*, ExecState*, JSValue, JSValue)
{
    ASSERT_NOT_REACHED();
    return false;
}

void JSCell::putDirectVirtual(JSObject*, ExecState*, const Identifier&, JSValue, unsigned)
{
    ASSERT_NOT_REACHED();
}

bool JSCell::defineOwnProperty(JSObject*, ExecState*, const Identifier&, PropertyDescriptor&, bool)
{
    ASSERT_NOT_REACHED();
    return false;
}

bool JSCell::getOwnPropertyDescriptor(JSObject*, ExecState*, const Identifier&, PropertyDescriptor&)
{
    ASSERT_NOT_REACHED();
    return false;
}

} // namespace JSC
