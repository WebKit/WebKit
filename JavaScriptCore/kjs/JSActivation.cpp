/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
 
#include "config.h"
#include "JSActivation.h"

#include "CodeBlock.h"
#include "Machine.h"
#include "Register.h"
#include "JSFunction.h"

namespace KJS {

const ClassInfo JSActivation::info = { "JSActivation", 0, 0, 0 };

JSActivation::JSActivation(PassRefPtr<FunctionBodyNode> functionBody, Register** registerBase, int registerOffset)
    : Base(new JSActivationData(functionBody, registerBase, registerOffset))
{
}

JSActivation::~JSActivation()
{
    delete d();
}

void JSActivation::copyRegisters()
{
    int numLocals = d()->functionBody->generatedCode().numLocals;
    if (!numLocals)
        return;

    copyRegisterArray(registers() - numLocals, numLocals);
}

bool JSActivation::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    if (symbolTableGet(propertyName, slot))
        return true;

    if (JSValue** location = getDirectLocation(propertyName)) {
        slot.setValueSlot(location);
        return true;
    }

    // Only return the built-in arguments object if it wasn't overridden above.
    if (propertyName == exec->propertyNames().arguments) {
        slot.setCustom(this, getArgumentsGetter());
        return true;
    }

    // We don't call through to JSObject because there's no way to give an 
    // activation object getter properties or a prototype.
    ASSERT(!_prop.hasGetterSetterProperties());
    ASSERT(prototype() == jsNull());
    return false;
}

void JSActivation::put(ExecState*, const Identifier& propertyName, JSValue* value)
{
    if (symbolTablePut(propertyName, value))
        return;

    // We don't call through to JSObject because __proto__ and getter/setter 
    // properties are non-standard extensions that other implementations do not
    // expose in the activation object.
    ASSERT(!_prop.hasGetterSetterProperties());
    _prop.put(propertyName, value, 0, true);
}

// FIXME: Make this function honor ReadOnly (const) and DontEnum
void JSActivation::putWithAttributes(ExecState*, const Identifier& propertyName, JSValue* value, unsigned attributes)
{
    if (symbolTablePutWithAttributes(propertyName, value, attributes))
        return;

    // We don't call through to JSObject because __proto__ and getter/setter 
    // properties are non-standard extensions that other implementations do not
    // expose in the activation object.
    ASSERT(!_prop.hasGetterSetterProperties());
    _prop.put(propertyName, value, attributes, true);
}

bool JSActivation::deleteProperty(ExecState* exec, const Identifier& propertyName)
{
    if (propertyName == exec->propertyNames().arguments)
        return false;

    return Base::deleteProperty(exec, propertyName);
}

JSObject* JSActivation::toThisObject(ExecState* exec) const
{
    return exec->globalThisValue();
}

void JSActivation::mark()
{
    Base::mark();
    
    if (d()->argumentsObject)
        d()->argumentsObject->mark();
}

bool JSActivation::isActivationObject() const
{
    return true;
}

bool JSActivation::isDynamicScope() const
{
    return d()->functionBody->usesEval();
}

JSValue* JSActivation::argumentsGetter(ExecState* exec, const Identifier&, const PropertySlot& slot)
{
    JSActivation* thisObj = static_cast<JSActivation*>(slot.slotBase());
    if (!thisObj->d()->argumentsObject)
        thisObj->d()->argumentsObject = thisObj->createArgumentsObject(exec);

    return thisObj->d()->argumentsObject;
}

// These two functions serve the purpose of isolating the common case from a
// PIC branch.

PropertySlot::GetValueFunc JSActivation::getArgumentsGetter()
{
    return argumentsGetter;
}

JSObject* JSActivation::createArgumentsObject(ExecState* exec)
{
    Register* callFrame = registers() - d()->functionBody->generatedCode().numLocals - RegisterFile::CallFrameHeaderSize;

    JSFunction* function;
    Register* argv;
    int argc;
    exec->machine()->getFunctionAndArguments(registerBase(), callFrame, function, argv, argc);
    ArgList args(reinterpret_cast<JSValue***>(registerBase()), argv - *registerBase(), argc);
    return new (exec) Arguments(exec, function, args, this);
}

} // namespace KJS
