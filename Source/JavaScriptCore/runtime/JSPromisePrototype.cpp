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
#include "JSPromisePrototype.h"

#if ENABLE(PROMISES)

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSGlobalObject.h"
#include "JSPromise.h"
#include "JSPromiseCallback.h"
#include "JSPromiseResolver.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromisePrototype);

// Promise then([TreatUndefinedAs=Missing] optional AnyCallback fulfillCallback, [TreatUndefinedAs=Missing] optional AnyCallback rejectCallback);
static EncodedJSValue JSC_HOST_CALL JSPromisePrototypeFuncThen(ExecState*);
// Promise catch([TreatUndefinedAs=Missing] optional AnyCallback rejectCallback);
static EncodedJSValue JSC_HOST_CALL JSPromisePrototypeFuncCatch(ExecState*);

}

#include "JSPromisePrototype.lut.h"

namespace JSC {

const ClassInfo JSPromisePrototype::s_info = { "PromisePrototype", &JSNonFinalObject::s_info, 0, ExecState::promisePrototypeTable, CREATE_METHOD_TABLE(JSPromisePrototype) };

/* Source for JSPromisePrototype.lut.h
@begin promisePrototypeTable
  then         JSPromisePrototypeFuncThen             DontEnum|Function 0
  catch        JSPromisePrototypeFuncCatch            DontEnum|Function 0
@end
*/

JSPromisePrototype* JSPromisePrototype::create(ExecState* exec, JSGlobalObject*, Structure* structure)
{
    JSPromisePrototype* object = new (NotNull, allocateCell<JSPromisePrototype>(*exec->heap())) JSPromisePrototype(exec, structure);
    object->finishCreation(exec, structure);
    return object;
}

Structure* JSPromisePrototype::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromisePrototype::JSPromisePrototype(ExecState* exec, Structure* structure)
    : JSNonFinalObject(exec->vm(), structure)
{
}

void JSPromisePrototype::finishCreation(ExecState* exec, Structure*)
{
    Base::finishCreation(exec->vm());
}

bool JSPromisePrototype::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, ExecState::promisePrototypeTable(exec), jsCast<JSPromisePrototype*>(object), propertyName, slot);
}

static InternalFunction* wrapCallback(ExecState* exec, JSGlobalObject* globalObject, JSValue callback, JSPromiseResolver* resolver, JSPromiseCallback::Algorithm algorithm)
{
    if (!callback.isUndefined())
        return JSPromiseWrapperCallback::create(exec, globalObject, globalObject->promiseWrapperCallbackStructure(), resolver, callback);
    return JSPromiseCallback::create(exec, globalObject, globalObject->promiseCallbackStructure(), resolver, algorithm);
}

EncodedJSValue JSC_HOST_CALL JSPromisePrototypeFuncThen(ExecState* exec)
{
    JSPromise* thisObject = jsDynamicCast<JSPromise*>(exec->thisValue());
    if (!thisObject)
        return throwVMError(exec, createTypeError(exec, "Receiver of then must be a Promise"));

    JSValue fulfillCallback = exec->argument(0);
    if (!fulfillCallback.isUndefined()) {
        CallData callData;
        CallType callType = getCallData(fulfillCallback, callData);
        if (callType == CallTypeNone)
            return throwVMError(exec, createTypeError(exec, "Expected function or undefined as as first argument"));
    }
    
    JSValue rejectCallback = exec->argument(1);
    if (!rejectCallback.isUndefined()) {
        CallData callData;
        CallType callType = getCallData(rejectCallback, callData);
        if (callType == CallTypeNone)
            return throwVMError(exec, createTypeError(exec, "Expected function or undefined as as second argument"));
    }

    JSFunction* callee = jsCast<JSFunction*>(exec->callee());
    JSGlobalObject* globalObject = callee->globalObject();

    // 1. Let promise be a new promise.
    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject);

    // 2. Let resolver be promise's associated resolver.
    JSPromiseResolver* resolver = promise->resolver();

    // 3. Let fulfillWrapper be a promise wrapper callback for resolver and fulfillCallback if fulfillCallback is
    //    not omitted and a promise callback for resolver and its fulfill algorithm otherwise.
    InternalFunction* fulfillWrapper = wrapCallback(exec, globalObject, fulfillCallback, resolver, JSPromiseCallback::Fulfill);

    // 4. Let rejectWrapper be a promise wrapper callback for resolver and rejectCallback if rejectCallback is
    //    not omitted and a promise callback for resolver and its reject algorithm otherwise.
    InternalFunction* rejectWrapper = wrapCallback(exec, globalObject, rejectCallback, resolver, JSPromiseCallback::Reject);

    // 5. Append fulfillWrapper and rejectWrapper to the context object.
    thisObject->appendCallbacks(exec, fulfillWrapper, rejectWrapper);

    // 6. Return promise.
    return JSValue::encode(promise);
}

EncodedJSValue JSC_HOST_CALL JSPromisePrototypeFuncCatch(ExecState* exec)
{
    JSPromise* thisObject = jsDynamicCast<JSPromise*>(exec->thisValue());
    if (!thisObject)
        return throwVMError(exec, createTypeError(exec, "Receiver of catch must be a Promise"));
    
    JSValue rejectCallback = exec->argument(0);
    if (!rejectCallback.isUndefined()) {
        CallData callData;
        CallType callType = getCallData(rejectCallback, callData);
        if (callType == CallTypeNone)
            return throwVMError(exec, createTypeError(exec, "Expected function or undefined as as first argument"));
    }

    JSFunction* callee = jsCast<JSFunction*>(exec->callee());
    JSGlobalObject* globalObject = callee->globalObject();

    // 1. Let promise be a new promise.
    JSPromise* promise = JSPromise::createWithResolver(exec->vm(), globalObject);

    // 2. Let resolver be promise's associated resolver.
    JSPromiseResolver* resolver = promise->resolver();

    // 3. Let fulfillCallback be a new promise callback for resolver and its fulfill algorithm.
    InternalFunction* fulfillWrapper = JSPromiseCallback::create(exec, globalObject, globalObject->promiseCallbackStructure(), resolver, JSPromiseCallback::Fulfill);

    // 4. Let rejectWrapper be a promise wrapper callback for resolver and rejectCallback if rejectCallback is
    //    not omitted and a promise callback for resolver and its reject algorithm otherwise.
    InternalFunction* rejectWrapper = wrapCallback(exec, globalObject, rejectCallback, resolver, JSPromiseCallback::Reject);

    // 5. Append fulfillWrapper and rejectWrapper to the context object.
    thisObject->appendCallbacks(exec, fulfillWrapper, rejectWrapper);

    // 6. Return promise.
    return JSValue::encode(promise);
}

} // namespace JSC

#endif // ENABLE(PROMISES)
