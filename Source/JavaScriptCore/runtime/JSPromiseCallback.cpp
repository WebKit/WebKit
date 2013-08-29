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
#include "JSPromiseCallback.h"

#if ENABLE(PROMISES)

#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSPromise.h"
#include "JSPromiseResolver.h"
#include "SlotVisitorInlines.h"
#include "StructureInlines.h"

namespace JSC {

const ClassInfo JSPromiseCallback::s_info = { "Function", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSPromiseCallback) };

JSPromiseCallback* JSPromiseCallback::create(ExecState* exec, JSGlobalObject* globalObject, Structure* structure, JSPromiseResolver* resolver, Algorithm algorithm)
{
    JSPromiseCallback* constructor = new (NotNull, allocateCell<JSPromiseCallback>(*exec->heap())) JSPromiseCallback(globalObject, structure, algorithm);
    constructor->finishCreation(exec, resolver);
    return constructor;
}

Structure* JSPromiseCallback::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromiseCallback::JSPromiseCallback(JSGlobalObject* globalObject, Structure* structure, Algorithm algorithm)
    : InternalFunction(globalObject, structure)
    , m_algorithm(algorithm)
{
}

void JSPromiseCallback::finishCreation(ExecState* exec, JSPromiseResolver* resolver)
{
    Base::finishCreation(exec->vm(), "PromiseCallback");
    m_resolver.set(exec->vm(), this, resolver);
}

void JSPromiseCallback::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSPromiseCallback* thisObject = jsCast<JSPromiseCallback*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_resolver);
}

EncodedJSValue JSC_HOST_CALL JSPromiseCallback::callPromiseCallback(ExecState* exec)
{
    JSPromiseCallback* promiseCallback = jsCast<JSPromiseCallback*>(exec->callee());
    JSPromiseResolver* resolver = promiseCallback->m_resolver.get();

    // 1. Let value be the first argument that is passed, and undefined otherwise.
    JSValue value = exec->argument(0);

    // 2. Run resolver's algorithm with value and the synchronous flag set.
    switch (promiseCallback->m_algorithm) {
    case JSPromiseCallback::Fulfill:
        resolver->fulfill(exec, value, JSPromiseResolver::ResolveSynchronously);
        break;

    case JSPromiseCallback::Resolve:
        resolver->resolve(exec, value, JSPromiseResolver::ResolveSynchronously);
        break;

    case JSPromiseCallback::Reject:
        resolver->reject(exec, value, JSPromiseResolver::ResolveSynchronously);
        break;
    }

    return JSValue::encode(jsUndefined());
}

CallType JSPromiseCallback::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callPromiseCallback;
    return CallTypeHost;
}




const ClassInfo JSPromiseWrapperCallback::s_info = { "Function", &Base::s_info, 0, 0, CREATE_METHOD_TABLE(JSPromiseWrapperCallback) };

JSPromiseWrapperCallback* JSPromiseWrapperCallback::create(ExecState* exec, JSGlobalObject* globalObject, Structure* structure, JSPromiseResolver* resolver, JSValue callback)
{
    JSPromiseWrapperCallback* constructor = new (NotNull, allocateCell<JSPromiseWrapperCallback>(*exec->heap())) JSPromiseWrapperCallback(globalObject, structure);
    constructor->finishCreation(exec, resolver, callback);
    return constructor;
}

Structure* JSPromiseWrapperCallback::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromiseWrapperCallback::JSPromiseWrapperCallback(JSGlobalObject* globalObject, Structure* structure)
    : InternalFunction(globalObject, structure)
{
}

void JSPromiseWrapperCallback::finishCreation(ExecState* exec, JSPromiseResolver* resolver, JSValue callback)
{
    Base::finishCreation(exec->vm(), "PromiseWrapperCallback");
    m_resolver.set(exec->vm(), this, resolver);
    m_callback.set(exec->vm(), this, callback);
}

void JSPromiseWrapperCallback::visitChildren(JSCell* cell, SlotVisitor& visitor)
{
    JSPromiseWrapperCallback* thisObject = jsCast<JSPromiseWrapperCallback*>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    COMPILE_ASSERT(StructureFlags & OverridesVisitChildren, OverridesVisitChildrenWithoutSettingFlag);
    ASSERT(thisObject->structure()->typeInfo().overridesVisitChildren());

    Base::visitChildren(thisObject, visitor);

    visitor.append(&thisObject->m_resolver);
    visitor.append(&thisObject->m_callback);
}

EncodedJSValue JSC_HOST_CALL JSPromiseWrapperCallback::callPromiseWrapperCallback(ExecState* exec)
{
    JSPromiseWrapperCallback* promiseWrapperCallback = jsCast<JSPromiseWrapperCallback*>(exec->callee());

    JSPromiseResolver* resolver = promiseWrapperCallback->m_resolver.get();
    JSValue callback = promiseWrapperCallback->m_callback.get();

    // 1. Let argument be the first argument that is passed, and undefined otherwise.
    JSValue argument = exec->argument(0);

    // 2. Set callback's callback this value to resolver's associated promise.
    // 3. Let value be the result of invoking callback with argument as argument.
    CallData callData;
    CallType callType = JSC::getCallData(callback, callData);
    ASSERT(callType != CallTypeNone);

    MarkedArgumentBuffer callbackArguments;
    callbackArguments.append(argument);
    JSValue value = JSC::call(exec, callback, callType, callData, resolver->promise(), callbackArguments);

    // 4. If invoking callback threw an exception, catch it and run resolver's reject
    //    with the thrown exception as argument and the synchronous flag set.
    if (exec->hadException()) {
        JSValue exception = exec->exception();
        exec->clearException();

        resolver->reject(exec, exception, JSPromiseResolver::ResolveSynchronously);
        return JSValue::encode(jsUndefined());
    }

    // 5. Otherwise, run resolver's resolve with value and the synchronous flag set.
    resolver->resolve(exec, value, JSPromiseResolver::ResolveSynchronously);

    return JSValue::encode(jsUndefined());
}

CallType JSPromiseWrapperCallback::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = callPromiseWrapperCallback;
    return CallTypeHost;
}

} // namespace JSC

#endif // ENABLE(PROMISES)
