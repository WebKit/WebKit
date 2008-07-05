/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2007 Cameron Zwarich (cwzwarich@uwaterloo.ca)
 *  Copyright (C) 2007 Maks Orlovich
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
#include "JSFunction.h"

#include "ExecState.h"
#include "FunctionPrototype.h"
#include "JSGlobalObject.h"
#include "Machine.h"
#include "ObjectPrototype.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "ScopeChainMark.h"

using namespace WTF;
using namespace Unicode;

namespace KJS {

const ClassInfo JSFunction::info = { "Function", &InternalFunction::info, 0, 0 };

JSFunction::JSFunction(ExecState* exec, const Identifier& name, FunctionBodyNode* b, ScopeChainNode* scopeChain)
  : InternalFunction(exec->lexicalGlobalObject()->functionPrototype(), name)
  , body(b)
  , _scope(scopeChain)
{
}

void JSFunction::mark()
{
    InternalFunction::mark();
    body->mark();
    _scope.mark();
}

CallType JSFunction::getCallData(CallData& callData)
{
    callData.js.functionBody = body.get();
    callData.js.scopeChain = _scope.node();
    return CallTypeJS;
}

JSValue* JSFunction::call(ExecState* exec, JSValue* thisValue, const ArgList& args)
{
    return exec->machine()->execute(body.get(), exec, this, thisValue->toThisObject(exec), args, _scope.node(), exec->exceptionSlot());
}

JSValue* JSFunction::argumentsGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSFunction* thisObj = static_cast<JSFunction*>(slot.slotBase());
    return exec->machine()->retrieveArguments(exec, thisObj);
}

JSValue* JSFunction::callerGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSFunction* thisObj = static_cast<JSFunction*>(slot.slotBase());
    return exec->machine()->retrieveCaller(exec, thisObj);
}

JSValue* JSFunction::lengthGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSFunction* thisObj = static_cast<JSFunction*>(slot.slotBase());
    return jsNumber(exec, thisObj->body->parameters().size());
}

bool JSFunction::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().arguments) {
        slot.setCustom(this, argumentsGetter);
        return true;
    }

    if (propertyName == exec->propertyNames().length) {
        slot.setCustom(this, lengthGetter);
        return true;
    }

    if (propertyName == exec->propertyNames().caller) {
        slot.setCustom(this, callerGetter);
        return true;
    }

    return InternalFunction::getOwnPropertySlot(exec, propertyName, slot);
}

void JSFunction::put(ExecState* exec, const Identifier& propertyName, JSValue* value)
{
    if (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().length)
        return;
    InternalFunction::put(exec, propertyName, value);
}

bool JSFunction::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().length)
        return false;
    return InternalFunction::deleteProperty(exec, propertyName);
}

/* Returns the parameter name corresponding to the given index. eg:
 * function f1(x, y, z): getParameterName(0) --> x
 *
 * If a name appears more than once, only the last index at which
 * it appears associates with it. eg:
 * function f2(x, x): getParameterName(0) --> null
 */
const Identifier& JSFunction::getParameterName(int index)
{
    Vector<Identifier>& parameters = body->parameters();

    if (static_cast<size_t>(index) >= body->parameters().size())
        return _scope.globalObject()->globalData()->propertyNames->nullIdentifier;
  
    const Identifier& name = parameters[index];

    // Are there any subsequent parameters with the same name?
    size_t size = parameters.size();
    for (size_t i = index + 1; i < size; ++i)
        if (parameters[i] == name)
            return _scope.globalObject()->globalData()->propertyNames->nullIdentifier;

    return name;
}

// ECMA 13.2.2 [[Construct]]
ConstructType JSFunction::getConstructData(ConstructData& constructData)
{
    constructData.js.functionBody = body.get();
    constructData.js.scopeChain = _scope.node();
    return ConstructTypeJS;
}

JSObject* JSFunction::construct(ExecState* exec, const ArgList& args)
{
    JSObject* proto;
    JSValue* p = get(exec, exec->propertyNames().prototype);
    if (p->isObject())
        proto = static_cast<JSObject*>(p);
    else
        proto = exec->lexicalGlobalObject()->objectPrototype();

    JSObject* thisObj = new (exec) JSObject(proto);

    JSValue* result = exec->machine()->execute(body.get(), exec, this, thisObj, args, _scope.node(), exec->exceptionSlot());
    if (exec->hadException() || !result->isObject())
        return thisObj;
    return static_cast<JSObject*>(result);
}

} // namespace KJS
