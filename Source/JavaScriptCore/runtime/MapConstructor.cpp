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
#include "MapConstructor.h"

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSGlobalObject.h"
#include "JSMap.h"
#include "MapPrototype.h"

namespace JSC {

const ClassInfo MapConstructor::s_info = { "Function", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(MapConstructor) };

void MapConstructor::finishCreation(VM& vm, MapPrototype* mapPrototype)
{
    Base::finishCreation(vm, mapPrototype->classInfo()->className);
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, mapPrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(0), ReadOnly | DontEnum | DontDelete);
}

static EncodedJSValue JSC_HOST_CALL constructMap(ExecState* exec)
{
    // Until we have iterators we throw if we've been given
    // any arguments that could require us to throw.
    if (!exec->argument(0).isUndefinedOrNull())
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Map constructor does not accept arguments")));
    if (!exec->argument(1).isUndefined())
        return throwVMError(exec, createRangeError(exec, WTF::ASCIILiteral("Invalid comparator function")));

    JSGlobalObject* globalObject = asInternalFunction(exec->callee())->globalObject();
    Structure* mapStructure = globalObject->mapStructure();
    return JSValue::encode(JSMap::create(exec, mapStructure));
}

ConstructType MapConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructMap;
    return ConstructTypeHost;
}

CallType MapConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = constructMap;
    return CallTypeHost;
}

}
