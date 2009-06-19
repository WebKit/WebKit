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
#include "CallFrame.h"
#include "FunctionPrototype.h"
#include "JSGlobalObject.h"
#include "Interpreter.h"
#include "ObjectPrototype.h"
#include "Parser.h"
#include "PropertyNameArray.h"
#include "ScopeChainMark.h"

using namespace WTF;
using namespace Unicode;

namespace JSC {

ASSERT_CLASS_FITS_IN_CELL(JSFunction);

const ClassInfo JSFunction::info = { "Function", &InternalFunction::info, 0, 0 };

JSFunction::JSFunction(ExecState* exec, PassRefPtr<Structure> structure, int length, const Identifier& name, NativeFunction func)
    : Base(&exec->globalData(), structure, name)
#if ENABLE(JIT)
    , m_body(FunctionBodyNode::createNativeThunk(&exec->globalData()))
#else
    , m_body(0)
#endif
{
#if ENABLE(JIT)
    setNativeFunction(func);
    putDirect(exec->propertyNames().length, jsNumber(exec, length), DontDelete | ReadOnly | DontEnum);
#else
    UNUSED_PARAM(length);
    UNUSED_PARAM(func);
    ASSERT_NOT_REACHED();
#endif
}

JSFunction::JSFunction(ExecState* exec, const Identifier& name, FunctionBodyNode* body, ScopeChainNode* scopeChainNode)
    : Base(&exec->globalData(), exec->lexicalGlobalObject()->functionStructure(), name)
    , m_body(body)
{
    setScopeChain(scopeChainNode);
}

JSFunction::~JSFunction()
{
#if ENABLE(JIT) 
    // JIT code for other functions may have had calls linked directly to the code for this function; these links
    // are based on a check for the this pointer value for this JSFunction - which will no longer be valid once
    // this memory is freed and may be reused (potentially for another, different JSFunction).
    if (m_body && m_body->isGenerated())
        m_body->generatedBytecode().unlinkCallers();
    if (!isHostFunction())
        scopeChain().~ScopeChain();
#endif
}

void JSFunction::mark()
{
    Base::mark();
    m_body->mark();
    if (!isHostFunction())
        scopeChain().mark();
}

CallType JSFunction::getCallData(CallData& callData)
{
    if (isHostFunction()) {
        callData.native.function = nativeFunction();
        return CallTypeHost;
    }
    callData.js.functionBody = m_body.get();
    callData.js.scopeChain = scopeChain().node();
    return CallTypeJS;
}

JSValue JSFunction::call(ExecState* exec, JSValue thisValue, const ArgList& args)
{
    ASSERT(!isHostFunction());
    return exec->interpreter()->execute(m_body.get(), exec, this, thisValue.toThisObject(exec), args, scopeChain().node(), exec->exceptionSlot());
}

JSValue JSFunction::argumentsGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSFunction* thisObj = asFunction(slot.slotBase());
    ASSERT(!thisObj->isHostFunction());
    return exec->interpreter()->retrieveArguments(exec, thisObj);
}

JSValue JSFunction::callerGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSFunction* thisObj = asFunction(slot.slotBase());
    ASSERT(!thisObj->isHostFunction());
    return exec->interpreter()->retrieveCaller(exec, thisObj);
}

JSValue JSFunction::lengthGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSFunction* thisObj = asFunction(slot.slotBase());
    ASSERT(!thisObj->isHostFunction());
    return jsNumber(exec, thisObj->m_body->parameterCount());
}

bool JSFunction::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (isHostFunction())
        return Base::getOwnPropertySlot(exec, propertyName, slot);

    if (propertyName == exec->propertyNames().prototype) {
        JSValue* location = getDirectLocation(propertyName);

        if (!location) {
            JSObject* prototype = new (exec) JSObject(scopeChain().globalObject()->emptyObjectStructure());
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

void JSFunction::put(ExecState* exec, const Identifier& propertyName, JSValue value, PutPropertySlot& slot)
{
    if (isHostFunction()) {
        Base::put(exec, propertyName, value, slot);
        return;
    }
    if (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().length)
        return;
    Base::put(exec, propertyName, value, slot);
}

bool JSFunction::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (isHostFunction())
        return Base::deleteProperty(exec, propertyName);
    if (propertyName == exec->propertyNames().arguments || propertyName == exec->propertyNames().length)
        return false;
    return Base::deleteProperty(exec, propertyName);
}

// ECMA 13.2.2 [[Construct]]
ConstructType JSFunction::getConstructData(ConstructData& constructData)
{
    if (isHostFunction())
        return ConstructTypeNone;
    constructData.js.functionBody = m_body.get();
    constructData.js.scopeChain = scopeChain().node();
    return ConstructTypeJS;
}

JSObject* JSFunction::construct(ExecState* exec, const ArgList& args)
{
    ASSERT(!isHostFunction());
    Structure* structure;
    JSValue prototype = get(exec, exec->propertyNames().prototype);
    if (prototype.isObject())
        structure = asObject(prototype)->inheritorID();
    else
        structure = exec->lexicalGlobalObject()->emptyObjectStructure();
    JSObject* thisObj = new (exec) JSObject(structure);

    JSValue result = exec->interpreter()->execute(m_body.get(), exec, this, thisObj, args, scopeChain().node(), exec->exceptionSlot());
    if (exec->hadException() || !result.isObject())
        return thisObj;
    return asObject(result);
}

} // namespace JSC
