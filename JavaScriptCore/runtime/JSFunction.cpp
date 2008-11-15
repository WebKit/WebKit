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

#include "CodeBlock.h"
#include "CommonIdentifiers.h"
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

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSFunction);

const ClassInfo JSFunction::info = { "Function", 0, 0, 0 };

JSFunction::JSFunction(ExecState* exec, const Identifier& name, FunctionBodyNode* body, ScopeChainNode* scopeChainNode)
    : Base(&exec->globalData(), exec->lexicalGlobalObject()->functionStructure(), name)
    , m_body(body)
    , m_scopeChain(scopeChainNode)
{
}

JSFunction::~JSFunction()
{
#if ENABLE(CTI) 
    // JIT code for other functions may have had calls linked directly to the code for this function; these links
    // are based on a check for the this pointer value for this JSFunction - which will no longer be valid once
    // this memory is freed and may be reused (potentially for another, different JSFunction).
    if (m_body.get() && m_body->isGenerated())
        m_body->generatedBytecode().unlinkCallers();
#endif
}

void JSFunction::mark()
{
    Base::mark();
    m_body->mark();
    m_scopeChain.mark();
}

CallType JSFunction::getCallData(CallData& callData)
{
    callData.js.functionBody = m_body.get();
    callData.js.scopeChain = m_scopeChain.node();
    return CallTypeJS;
}

JSValue* JSFunction::call(ExecState* exec, JSValue* thisValue, const ArgList& args)
{
    return exec->interpreter()->execute(m_body.get(), exec, this, thisValue->toThisObject(exec), args, m_scopeChain.node(), exec->exceptionSlot());
}

JSValue* JSFunction::argumentsGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSFunction* thisObj = asFunction(slot.slotBase());
    return exec->interpreter()->retrieveArguments(exec, thisObj);
}

JSValue* JSFunction::callerGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSFunction* thisObj = asFunction(slot.slotBase());
    return exec->interpreter()->retrieveCaller(exec, thisObj);
}

JSValue* JSFunction::lengthGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSFunction* thisObj = asFunction(slot.slotBase());
    return jsNumber(exec, thisObj->m_body->parameterCount());
}

bool JSFunction::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (propertyName == exec->propertyNames().prototype) {
        JSValue** location = getDirectLocation(propertyName);

        if (!location) {
            JSObject* prototype = new (exec) JSObject(m_scopeChain.globalObject()->emptyObjectStructure());
            prototype->putDirect(exec->propertyNames().constructor, this, DontEnum);
            putDirect(exec->propertyNames().prototype, prototype, DontDelete);
            location = getDirectLocation(propertyName);
        }

        slot.setValueSlot(this, location, offsetForLocation(location));
    }

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

    return Base::getOwnPropertySlot(exec, propertyName, slot);
}

void JSFunction::put(ExecState* exec, const Identifier& propertyName, JSValue* value, PutPropertySlot& slot)
{
    if (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().length)
        return;
    Base::put(exec, propertyName, value, slot);
}

bool JSFunction::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().length)
        return false;
    return Base::deleteProperty(exec, propertyName);
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
    const Identifier* parameters = m_body->parameters();

    if (static_cast<size_t>(index) >= m_body->parameterCount())
        return m_scopeChain.globalObject()->globalData()->propertyNames->nullIdentifier;
  
    const Identifier& name = parameters[index];

    // Are there any subsequent parameters with the same name?
    size_t size = m_body->parameterCount();
    for (size_t i = index + 1; i < size; ++i) {
        if (parameters[i] == name)
            return m_scopeChain.globalObject()->globalData()->propertyNames->nullIdentifier;
    }

    return name;
}

// ECMA 13.2.2 [[Construct]]
ConstructType JSFunction::getConstructData(ConstructData& constructData)
{
    constructData.js.functionBody = m_body.get();
    constructData.js.scopeChain = m_scopeChain.node();
    return ConstructTypeJS;
}

JSObject* JSFunction::construct(ExecState* exec, const ArgList& args)
{
    StructureID* structure;
    JSValue* prototype = get(exec, exec->propertyNames().prototype);
    if (prototype->isObject())
        structure = asObject(prototype)->inheritorID();
    else
        structure = exec->lexicalGlobalObject()->emptyObjectStructure();
    JSObject* thisObj = new (exec) JSObject(structure);

    JSValue* result = exec->interpreter()->execute(m_body.get(), exec, this, thisObj, args, m_scopeChain.node(), exec->exceptionSlot());
    if (exec->hadException() || !result->isObject())
        return thisObj;
    return asObject(result);
}

} // namespace JSC
