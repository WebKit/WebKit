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
#include "JSPromiseDeferred.h"
#include "JSPromiseFunctions.h"
#include "JSPromiseReaction.h"
#include "Microtask.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromisePrototype);

static EncodedJSValue JSC_HOST_CALL JSPromisePrototypeFuncThen(ExecState*);
static EncodedJSValue JSC_HOST_CALL JSPromisePrototypeFuncCatch(ExecState*);

}

#include "JSPromisePrototype.lut.h"

namespace JSC {

const ClassInfo JSPromisePrototype::s_info = { "PromisePrototype", &JSNonFinalObject::s_info, 0, ExecState::promisePrototypeTable, CREATE_METHOD_TABLE(JSPromisePrototype) };

/* Source for JSPromisePrototype.lut.h
@begin promisePrototypeTable
  then         JSPromisePrototypeFuncThen             DontEnum|Function 2
  catch        JSPromisePrototypeFuncCatch            DontEnum|Function 1
@end
*/

JSPromisePrototype* JSPromisePrototype::create(ExecState* exec, JSGlobalObject*, Structure* structure)
{
    VM& vm = exec->vm();
    JSPromisePrototype* object = new (NotNull, allocateCell<JSPromisePrototype>(vm.heap)) JSPromisePrototype(exec, structure);
    object->finishCreation(vm, structure);
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

void JSPromisePrototype::finishCreation(VM& vm, Structure*)
{
    Base::finishCreation(vm);
}

bool JSPromisePrototype::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<JSObject>(exec, ExecState::promisePrototypeTable(exec->vm()), jsCast<JSPromisePrototype*>(object), propertyName, slot);
}

EncodedJSValue JSC_HOST_CALL JSPromisePrototypeFuncThen(ExecState* exec)
{
    // -- Promise.prototype.then(onFulfilled, onRejected) --

    // 1. Let promise be the this value.
    // 2. If IsPromise(promise) is false, throw a TypeError exception.
    JSPromise* promise = jsDynamicCast<JSPromise*>(exec->thisValue());
    if (!promise)
        return JSValue::encode(throwTypeError(exec));

    // 3. Let 'C' be the result of calling Get(promise, "constructor").
    JSValue C = promise->get(exec, exec->propertyNames().constructor);
    
    // 4. ReturnIfAbrupt(C).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());
    
    // 5. Let 'deferred' be the result of calling GetDeferred(C).
    JSValue deferred = createJSPromiseDeferredFromConstructor(exec, C);

    // 6. ReturnIfAbrupt(deferred).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    VM& vm = exec->vm();
    JSGlobalObject* globalObject = promise->globalObject();

    // 7. Let 'rejectionHandler' be a new built-in function object as defined in Thrower Functions
    // 8. If IsCallable(onRejected), set rejectionHandler to onRejected.
    JSValue onRejected = exec->argument(1);
    CallData onRejectedCallData;
    CallType onRejectedCallType = getCallData(onRejected, onRejectedCallData);
    JSObject* rejectionHandler = (onRejectedCallType == CallTypeNone) ? createThrowerFunction(vm, globalObject) : asObject(onRejected);
    
    // 9. Let 'fulfillmentHandler' be a new built-in function object as defined in Identity Functions
    // 10. If IsCallable(onFulfilled), set fulfillmentHandler to onFulfilled
    JSValue onFulfilled = exec->argument(0);
    CallData onFulfilledCallData;
    CallType onFulfilledCallType = getCallData(onFulfilled, onFulfilledCallData);
    JSObject* fulfillmentHandler = (onFulfilledCallType == CallTypeNone) ? createIdentifyFunction(vm, globalObject) : asObject(onFulfilled);

    // 11. Let 'resolutionHandler' be a new built-in function object as defined in Promise Resolution Handler Functions
    JSObject* resolutionHandler = createPromiseResolutionHandlerFunction(vm, globalObject);

    // 12. Set the [[Promise]] internal slot of resolutionHandler to promise.
    resolutionHandler->putDirect(vm, vm.propertyNames->promisePrivateName, promise);

    // 13. Set the [[FulfillmentHandler]] internal slot of resolutionHandler to fulfillmentHandler.
    resolutionHandler->putDirect(vm, vm.propertyNames->fulfillmentHandlerPrivateName, fulfillmentHandler);

    // 14. Set the [[RejectionHandler]] internal slot of resolutionHandler to rejectionHandler.
    resolutionHandler->putDirect(vm, vm.propertyNames->rejectionHandlerPrivateName, rejectionHandler);

    // 15. Let 'resolveReaction' be the PromiseReaction { [[Deferred]]: deferred, [[Handler]]: resolutionHandler }.
    JSPromiseReaction* resolveReaction = JSPromiseReaction::create(vm, jsCast<JSPromiseDeferred*>(deferred), resolutionHandler);

    // 16. Let 'rejectReaction' be the PromiseReaction { [[Deferred]]: deferred, [[Handler]]: rejectionHandler }.
    JSPromiseReaction* rejectReaction = JSPromiseReaction::create(vm, jsCast<JSPromiseDeferred*>(deferred), rejectionHandler);

    switch (promise->status()) {
    case JSPromise::Status::Unresolved: {
        // 17. If the value of promise's [[PromiseStatus]] internal slot is "unresolved",

        // i. Append resolveReaction as the last element of promise's [[ResolveReactions]] internal slot.
        promise->appendResolveReaction(vm, resolveReaction);

        // ii. Append rejectReaction as the last element of promise's [[RejectReactions]] internal slot.
        promise->appendRejectReaction(vm, rejectReaction);
        break;
    }

    case JSPromise::Status::HasResolution: {
        // 18. If the value of promise's [[PromiseStatus]] internal slot is "has-resolution",

        // i. Let 'resolution' be the value of promise's [[Result]] internal slot.
        JSValue resolution = promise->result();

        // ii. Call QueueMicrotask(ExecutePromiseReaction, (resolveReaction, resolution)).
        globalObject->queueMicrotask(createExecutePromiseReactionMicrotask(vm, resolveReaction, resolution));
        break;
    }

    case JSPromise::Status::HasRejection: {
        // 19. If the value of promise's [[PromiseStatus]] internal slot is "has-rejection",

        // i. Let reason be the value of promise's [[Result]] internal slot.
        JSValue reason = promise->result();

        // ii. Call QueueMicrotask(ExecutePromiseReaction, (rejectReaction, reason)).
        globalObject->queueMicrotask(createExecutePromiseReactionMicrotask(vm, rejectReaction, reason));
        break;
    }
    }

    // 20. Return deferred.[[Promise]].
    return JSValue::encode(jsCast<JSPromiseDeferred*>(deferred)->promise());
}

EncodedJSValue JSC_HOST_CALL JSPromisePrototypeFuncCatch(ExecState* exec)
{
    // -- Promise.prototype.catch(onRejected) --

    // 1. Let 'promise' be the this value.
    JSValue promise = exec->thisValue();
    
    // 2. Return the result of calling Invoke(promise, "then", (undefined, onRejected)).
    // NOTE: Invoke does not seem to be defined anywhere, so I am guessing here.
    JSValue thenValue = promise.get(exec, exec->vm().propertyNames->then);
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    CallData thenCallData;
    CallType thenCallType = getCallData(thenValue, thenCallData);
    if (thenCallType == CallTypeNone)
        return JSValue::encode(throwTypeError(exec));

    MarkedArgumentBuffer arguments;
    arguments.append(jsUndefined());
    arguments.append(exec->argument(0));

    return JSValue::encode(call(exec, thenValue, thenCallType, thenCallData, promise, arguments));
}

} // namespace JSC

#endif // ENABLE(PROMISES)
