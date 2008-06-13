// -*- c-basic-offset: 4 -*-
/*
 *  Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
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
#include "property_slot.h"

#include "JSGlobalObject.h"
#include "object.h"
#include "RegisterFileStack.h"

namespace KJS {

JSValue* PropertySlot::undefinedGetter(ExecState*, const Identifier&, const PropertySlot&)
{
    return jsUndefined();
}

JSValue* PropertySlot::functionGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    CallData data;
    CallType callType = slot.m_data.getterFunc->getCallData(data);
    if (callType == CallTypeNative)
        return slot.m_data.getterFunc->callAsFunction(exec, static_cast<JSObject*>(slot.slotBase()), exec->emptyList());
    ASSERT(callType == CallTypeJS);
    RegisterFileStack* stack = &exec->dynamicGlobalObject()->registerFileStack();
    stack->pushFunctionRegisterFile();
    JSValue* result = slot.m_data.getterFunc->callAsFunction(exec, static_cast<JSObject*>(slot.slotBase()), exec->emptyList());
    stack->popFunctionRegisterFile();
    return result;    
}

}
