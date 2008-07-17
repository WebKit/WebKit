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
#include "InternalFunction.h"

#include "FunctionPrototype.h"

namespace KJS {

const ClassInfo InternalFunction::info = { "Function", 0, 0, 0 };

InternalFunction::InternalFunction()
{
}

InternalFunction::InternalFunction(FunctionPrototype* prototype, const Identifier& name)
    : JSObject(prototype)
    , m_name(name)
{
}

bool InternalFunction::implementsHasInstance() const
{
    return true;
}

bool InternalFunction::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().name) {
        slot.setCustom(this, nameGetter);
        return true;
    }

    return JSObject::getOwnPropertySlot(exec, propertyName, slot);
}

void InternalFunction::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    if (propertyName == exec->propertyNames().name)
        return;
    JSObject::put(exec, propertyName, value);
}

bool InternalFunction::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (propertyName == exec->propertyNames().name)
        return false;
    return JSObject::deleteProperty(exec, propertyName);
}

JSValue* InternalFunction::nameGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    InternalFunction* thisObj = static_cast<InternalFunction*>(slot.slotBase());
    return jsString(exec, thisObj->functionName().ustring());
}

} // namespace KJS
