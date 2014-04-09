/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SetConstructor.h"

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSGlobalObject.h"
#include "JSSet.h"
#include "MapData.h"
#include "SetPrototype.h"

namespace JSC {

const ClassInfo SetConstructor::s_info = { "Function", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(SetConstructor) };

void SetConstructor::finishCreation(VM& vm, SetPrototype* setPrototype)
{
    Base::finishCreation(vm, setPrototype->classInfo()->className);
    putDirectPrototypePropertyWithoutTransitions(vm, setPrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum | DontDelete);
}

static EncodedJSValue JSC_HOST_CALL callSet(CallFrame* callFrame)
{
    // Until we have iterators we throw if we've been given
    // any arguments that could require us to throw.
    if (!callFrame->argument(0).isUndefinedOrNull())
        return JSValue::encode(throwTypeError(callFrame, ASCIILiteral("Set does not accept arguments when called as a function")));
    if (!callFrame->argument(1).isUndefined())
        return throwVMError(callFrame, createRangeError(callFrame, WTF::ASCIILiteral("Invalid comparator function")));

    JSGlobalObject* globalObject = asInternalFunction(callFrame->callee())->globalObject();
    Structure* setStructure = globalObject->setStructure();
    return JSValue::encode(JSSet::create(callFrame, setStructure));
}

static EncodedJSValue JSC_HOST_CALL constructSet(CallFrame* callFrame)
{
    JSGlobalObject* globalObject = asInternalFunction(callFrame->callee())->globalObject();
    Structure* setStructure = globalObject->setStructure();
    JSSet* set = JSSet::create(callFrame, setStructure);
    MapData* mapData = set->mapData();
    size_t count = callFrame->argumentCount();
    for (size_t i = 0; i < count; i++) {
        JSValue item = callFrame->uncheckedArgument(i);
        mapData->set(callFrame, item, item);
    }
    return JSValue::encode(set);
}

ConstructType SetConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructSet;
    return ConstructTypeHost;
}

CallType SetConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callSet;
    return CallTypeHost;
}

}
