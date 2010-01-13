/*
 *  Copyright (C) 1999-2002 Harri Porten (porten@kde.org)
 *  Copyright (C) 2001 Peter Kelly (pmk@post.com)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
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

bool JSFunction::isHostFunctionNonInline() const
{
    return isHostFunction();
}

JSFunction::JSFunction(NonNullPassRefPtr<Structure> structure)
    : Base(structure)
    , m_executable(adoptRef(new VPtrHackExecutable()))
{
}

JSFunction::JSFunction(ExecState* exec, NonNullPassRefPtr<Structure> structure, int length, const Identifier& name, NativeFunction func)
    : Base(&exec->globalData(), structure, name)
#if ENABLE(JIT)
    , m_executable(adoptRef(new NativeExecutable(exec)))
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

JSFunction::JSFunction(ExecState* exec, NonNullPassRefPtr<FunctionExecutable> executable, ScopeChainNode* scopeChainNode)
    : Base(&exec->globalData(), exec->lexicalGlobalObject()->functionStructure(), executable->name())
    , m_executable(executable)
{
    setScopeChain(scopeChainNode);
}

JSFunction::~JSFunction()
{
    ASSERT(vptr() == JSGlobalData::jsFunctionVPtr);

    // JIT code for other functions may have had calls linked directly to the code for this function; these links
    // are based on a check for the this pointer value for this JSFunction - which will no longer be valid once
    // this memory is freed and may be reused (potentially for another, different JSFunction).
    if (!isHostFunction()) {
#if ENABLE(JIT_OPTIMIZE_CALL)
        ASSERT(m_executable);
        if (jsExecutable()->isGenerated())
            jsExecutable()->generatedBytecode().unlinkCallers();
#endif
        scopeChain().~ScopeChain(); // FIXME: Don't we need to do this in the interpreter too?
    }
}

void JSFunction::markChildren(MarkStack& markStack)
{
    Base::markChildren(markStack);
    if (!isHostFunction()) {
        jsExecutable()->markAggregate(markStack);
        scopeChain().markAggregate(markStack);
    }
}

CallType JSFunction::getCallData(CallData& callData)
{
    if (isHostFunction()) {
        callData.native.function = nativeFunction();
        return CallTypeHost;
    }
    callData.js.functionExecutable = jsExecutable();
    callData.js.scopeChain = scopeChain().node();
    return CallTypeJS;
}

JSValue JSFunction::call(ExecState* exec, JSValue thisValue, const ArgList& args)
{
    ASSERT(!isHostFunction());
    return exec->interpreter()->execute(jsExecutable(), exec, this, thisValue.toThisObject(exec), args, scopeChain().node(), exec->exceptionSlot());
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
    return jsNumber(exec, thisObj->jsExecutable()->parameterCount());
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

    bool JSFunction::getOwnPropertyDescriptor(ExecState* exec, const Identifier& propertyName, PropertyDescriptor& descriptor)
    {
        if (isHostFunction())
            return Base::getOwnPropertyDescriptor(exec, propertyName, descriptor);
        
        if (propertyName == exec->propertyNames().prototype) {
            PropertySlot slot;
            getOwnPropertySlot(exec, propertyName, slot);
            return Base::getOwnPropertyDescriptor(exec, propertyName, descriptor);
        }
        
        if (propertyName == exec->propertyNames().arguments) {
            descriptor.setDescriptor(exec->interpreter()->retrieveArguments(exec, this), ReadOnly | DontEnum | DontDelete);
            return true;
        }
        
        if (propertyName == exec->propertyNames().length) {
            descriptor.setDescriptor(jsNumber(exec, jsExecutable()->parameterCount()), ReadOnly | DontEnum | DontDelete);
            return true;
        }
        
        if (propertyName == exec->propertyNames().caller) {
            descriptor.setDescriptor(exec->interpreter()->retrieveCaller(exec, this), ReadOnly | DontEnum | DontDelete);
            return true;
        }
        
        return Base::getOwnPropertyDescriptor(exec, propertyName, descriptor);
    }
    
void JSFunction::getOwnPropertyNames(ExecState* exec, PropertyNameArray& propertyNames, EnumerationMode mode)
{
    if (!isHostFunction() && (mode == IncludeDontEnumProperties)) {
        propertyNames.add(exec->propertyNames().arguments);
        propertyNames.add(exec->propertyNames().callee);
        propertyNames.add(exec->propertyNames().caller);
        propertyNames.add(exec->propertyNames().length);
    }
    Base::getOwnPropertyNames(exec, propertyNames, mode);
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
    constructData.js.functionExecutable = jsExecutable();
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

    JSValue result = exec->interpreter()->execute(jsExecutable(), exec, this, thisObj, args, scopeChain().node(), exec->exceptionSlot());
    if (exec->hadException() || !result.isObject())
        return thisObj;
    return asObject(result);
}

} // namespace JSC
