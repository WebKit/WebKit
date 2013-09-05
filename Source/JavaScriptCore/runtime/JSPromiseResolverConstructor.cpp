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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "JSPromiseResolverConstructor.h"

#if ENABLE(PROMISES)

#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSPromiseResolverPrototype.h"
#include "Lookup.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseResolverConstructor);

const ClassInfo JSPromiseResolverConstructor::s_info = { "Function", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSPromiseResolverConstructor) };

JSPromiseResolverConstructor* JSPromiseResolverConstructor::create(ExecState* exec, JSGlobalObject* globalObject, Structure* structure, JSPromiseResolverPrototype* promisePrototype)
{
    JSPromiseResolverConstructor* constructor = new (NotNull, allocateCell<JSPromiseResolverConstructor>(*exec->heap())) JSPromiseResolverConstructor(globalObject, structure);
    constructor->finishCreation(exec, promisePrototype);
    return constructor;
}

Structure* JSPromiseResolverConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromiseResolverConstructor::JSPromiseResolverConstructor(JSGlobalObject* globalObject, Structure* structure)
    : InternalFunction(globalObject, structure) 
{
}

void JSPromiseResolverConstructor::finishCreation(ExecState* exec, JSPromiseResolverPrototype* promiseResolverPrototype)
{
    Base::finishCreation(exec->vm(), "PromiseResolver");
    putDirectWithoutTransition(exec->vm(), exec->propertyNames().prototype, promiseResolverPrototype, DontEnum | DontDelete | ReadOnly);
}

ConstructType JSPromiseResolverConstructor::getConstructData(JSCell*, ConstructData&)
{
    return ConstructTypeNone;
}

CallType JSPromiseResolverConstructor::getCallData(JSCell*, CallData&)
{
    return CallTypeNone;
}

} // namespace JSC

#endif // ENABLE(PROMISES)
