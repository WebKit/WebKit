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
#include "JSPromiseConstructor.h"

#if ENABLE(PROMISES)

#include "Error.h"
#include "JSCJSValueInlines.h"
#include "JSCellInlines.h"
#include "JSPromise.h"
#include "JSPromiseDeferred.h"
#include "JSPromiseFunctions.h"
#include "JSPromisePrototype.h"
#include "Lookup.h"
#include "NumberObject.h"
#include "StructureInlines.h"

namespace JSC {

STATIC_ASSERT_IS_TRIVIALLY_DESTRUCTIBLE(JSPromiseConstructor);

static EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncCast(ExecState*);
static EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncResolve(ExecState*);
static EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncReject(ExecState*);
static EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncRace(ExecState*);
static EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncAll(ExecState*);
}

#include "JSPromiseConstructor.lut.h"

namespace JSC {

const ClassInfo JSPromiseConstructor::s_info = { "Function", &InternalFunction::s_info, 0, ExecState::promiseConstructorTable, CREATE_METHOD_TABLE(JSPromiseConstructor) };

/* Source for JSPromiseConstructor.lut.h
@begin promiseConstructorTable
  cast            JSPromiseConstructorFuncCast                DontEnum|Function 1
  resolve         JSPromiseConstructorFuncResolve             DontEnum|Function 1
  reject          JSPromiseConstructorFuncReject              DontEnum|Function 1
  race            JSPromiseConstructorFuncRace                DontEnum|Function 1
  all             JSPromiseConstructorFuncAll                 DontEnum|Function 1
@end
*/

JSPromiseConstructor* JSPromiseConstructor::create(VM& vm, Structure* structure, JSPromisePrototype* promisePrototype)
{
    JSPromiseConstructor* constructor = new (NotNull, allocateCell<JSPromiseConstructor>(vm.heap)) JSPromiseConstructor(vm, structure);
    constructor->finishCreation(vm, promisePrototype);
    return constructor;
}

Structure* JSPromiseConstructor::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(ObjectType, StructureFlags), info());
}

JSPromiseConstructor::JSPromiseConstructor(VM& vm, Structure* structure)
    : InternalFunction(vm, structure)
{
}

void JSPromiseConstructor::finishCreation(VM& vm, JSPromisePrototype* promisePrototype)
{
    Base::finishCreation(vm, "Promise");
    putDirectWithoutTransition(vm, vm.propertyNames->prototype, promisePrototype, DontEnum | DontDelete | ReadOnly);
    putDirectWithoutTransition(vm, vm.propertyNames->length, jsNumber(1), ReadOnly | DontEnum | DontDelete);
}

static EncodedJSValue JSC_HOST_CALL constructPromise(ExecState* exec)
{
    // NOTE: We ignore steps 1-4 as they only matter if you support subclassing, which we do not yet.
    // 1. Let promise be the this value.
    // 2. If Type(promise) is not Object, then throw a TypeError exception.
    // 3. If promise does not have a [[PromiseStatus]] internal slot, then throw a TypeError exception.
    // 4. If promise's [[PromiseStatus]] internal slot is not undefined, then throw a TypeError exception.

    JSValue resolver = exec->argument(0);

    // 5. IsCallable(resolver) is false, then throw a TypeError exception
    CallData callData;
    CallType callType = getCallData(resolver, callData);
    if (callType == CallTypeNone)
        return JSValue::encode(throwTypeError(exec, ASCIILiteral("Promise constructor takes a function argument")));

    VM& vm = exec->vm();
    JSGlobalObject* globalObject = exec->callee()->globalObject();

    JSPromise* promise = JSPromise::create(vm, globalObject, jsCast<JSPromiseConstructor*>(exec->callee()));

    // NOTE: Steps 6-8 are handled by JSPromise::create().
    // 6. Set promise's [[PromiseStatus]] internal slot to "unresolved".
    // 7. Set promise's [[ResolveReactions]] internal slot to a new empty List.
    // 8. Set promise's [[RejectReactions]] internal slot to a new empty List.
    
    // 9. Let 'resolve' be a new built-in function object as defined in Resolve Promise Functions.
    JSFunction* resolve = createResolvePromiseFunction(vm, globalObject);

    // 10. Set the [[Promise]] internal slot of 'resolve' to 'promise'.
    resolve->putDirect(vm, vm.propertyNames->promisePrivateName, promise);
    
    // 11. Let 'reject' be a new built-in function object as defined in Reject Promise Functions
    JSFunction* reject = createRejectPromiseFunction(vm, globalObject);

    // 12. Set the [[Promise]] internal slot of 'reject' to 'promise'.
    reject->putDirect(vm, vm.propertyNames->promisePrivateName, promise);

    // 13. Let 'result' be the result of calling the [[Call]] internal method of resolver with
    //     undefined as thisArgument and a List containing resolve and reject as argumentsList.
    MarkedArgumentBuffer arguments;
    arguments.append(resolve);
    arguments.append(reject);
    call(exec, resolver, callType, callData, jsUndefined(), arguments);

    // 14. If result is an abrupt completion, call PromiseReject(promise, result.[[value]]).
    if (exec->hadException()) {
        JSValue exception = exec->exception();
        exec->clearException();

        promise->reject(vm, exception);
    }

    // 15. Return promise.
    return JSValue::encode(promise);
}

ConstructType JSPromiseConstructor::getConstructData(JSCell*, ConstructData& constructData)
{
    constructData.native.function = constructPromise;
    return ConstructTypeHost;
}

CallType JSPromiseConstructor::getCallData(JSCell*, CallData& callData)
{
    callData.native.function = constructPromise;
    return CallTypeHost;
}

bool JSPromiseConstructor::getOwnPropertySlot(JSObject* object, ExecState* exec, PropertyName propertyName, PropertySlot& slot)
{
    return getStaticFunctionSlot<InternalFunction>(exec, ExecState::promiseConstructorTable(exec), jsCast<JSPromiseConstructor*>(object), propertyName, slot);
}

EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncCast(ExecState* exec)
{
    // -- Promise.cast(x) --
    JSValue x = exec->argument(0);

    // 1. Let 'C' be the this value.
    JSValue C = exec->thisValue();

    // 2. If IsPromise(x) is true,
    JSPromise* promise = jsDynamicCast<JSPromise*>(x);
    if (promise) {
        // i. Let 'constructor' be the value of x's [[PromiseConstructor]] internal slot.
        JSValue constructor = promise->constructor();
        // ii. If SameValue(constructor, C) is true, return x.
        if (sameValue(exec, constructor, C))
            return JSValue::encode(x);
    }

    // 3. Let 'deferred' be the result of calling GetDeferred(C).
    JSValue deferredValue = createJSPromiseDeferredFromConstructor(exec, C);
    
    // 4. ReturnIfAbrupt(deferred).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSPromiseDeferred* deferred = jsCast<JSPromiseDeferred*>(deferredValue);

    // 5. Let 'resolveResult' be the result of calling the [[Call]] internal method
    //    of deferred.[[Resolve]] with undefined as thisArgument and a List containing x
    //    as argumentsList.
    performDeferredResolve(exec, deferred, x);

    // 6. ReturnIfAbrupt(resolveResult).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    // 7. Return deferred.[[Promise]].
    return JSValue::encode(deferred->promise());
}

EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncResolve(ExecState* exec)
{
    // -- Promise.resolve(x) --
    JSValue x = exec->argument(0);

    // 1. Let 'C' be the this value.
    JSValue C = exec->thisValue();

    // 2. Let 'deferred' be the result of calling GetDeferred(C).
    JSValue deferredValue = createJSPromiseDeferredFromConstructor(exec, C);

    // 3. ReturnIfAbrupt(deferred).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSPromiseDeferred* deferred = jsCast<JSPromiseDeferred*>(deferredValue);

    // 4. Let 'resolveResult' be the result of calling the [[Call]] internal method
    //    of deferred.[[Resolve]] with undefined as thisArgument and a List containing x
    //    as argumentsList.
    performDeferredResolve(exec, deferred, x);
    
    // 5. ReturnIfAbrupt(resolveResult).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    // 6. Return deferred.[[Promise]].
    return JSValue::encode(deferred->promise());
}

EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncReject(ExecState* exec)
{
    // -- Promise.reject(x) --
    JSValue r = exec->argument(0);

    // 1. Let 'C' be the this value.
    JSValue C = exec->thisValue();

    // 2. Let 'deferred' be the result of calling GetDeferred(C).
    JSValue deferredValue = createJSPromiseDeferredFromConstructor(exec, C);

    // 3. ReturnIfAbrupt(deferred).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSPromiseDeferred* deferred = jsCast<JSPromiseDeferred*>(deferredValue);

    // 4. Let 'rejectResult' be the result of calling the [[Call]] internal method
    //    of deferred.[[Reject]] with undefined as thisArgument and a List containing r
    //    as argumentsList.
    performDeferredReject(exec, deferred, r);

    // 5. ReturnIfAbrupt(resolveResult).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    // 6. Return deferred.[[Promise]].
    return JSValue::encode(deferred->promise());
}

EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncRace(ExecState* exec)
{
    // -- Promise.race(iterable) --
    JSValue iterable = exec->argument(0);
    VM& vm = exec->vm();

    // 1. Let 'C' be the this value.
    JSValue C = exec->thisValue();

    // 2. Let 'deferred' be the result of calling GetDeferred(C).
    JSValue deferredValue = createJSPromiseDeferredFromConstructor(exec, C);

    // 3. ReturnIfAbrupt(deferred).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    JSPromiseDeferred* deferred = jsCast<JSPromiseDeferred*>(deferredValue);

    // 4. Let 'iterator' be the result of calling GetIterator(iterable).
    JSValue iteratorFunction = iterable.get(exec, vm.propertyNames->iteratorPrivateName);
    if (exec->hadException())
        return JSValue::encode(abruptRejection(exec, deferred));

    CallData iteratorFunctionCallData;
    CallType iteratorFunctionCallType = getCallData(iteratorFunction, iteratorFunctionCallData);
    if (iteratorFunctionCallType == CallTypeNone) {
        throwTypeError(exec);
        return JSValue::encode(abruptRejection(exec, deferred));
    }

    ArgList iteratorFunctionArguments;
    JSValue iterator = call(exec, iteratorFunction, iteratorFunctionCallType, iteratorFunctionCallData, iterable, iteratorFunctionArguments);

    // 5. RejectIfAbrupt(iterator, deferred).
    if (exec->hadException())
        return JSValue::encode(abruptRejection(exec, deferred));

    // 6. Repeat
    do {
        // i. Let 'next' be the result of calling IteratorStep(iterator).
        JSValue nextFunction = iterator.get(exec, exec->vm().propertyNames->iteratorNextPrivateName);
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        CallData nextFunctionCallData;
        CallType nextFunctionCallType = getCallData(nextFunction, nextFunctionCallData);
        if (nextFunctionCallType == CallTypeNone) {
            throwTypeError(exec);
            return JSValue::encode(abruptRejection(exec, deferred));
        }

        MarkedArgumentBuffer nextFunctionArguments;
        nextFunctionArguments.append(jsUndefined());
        JSValue next = call(exec, nextFunction, nextFunctionCallType, nextFunctionCallData, iterator, nextFunctionArguments);
        
        // ii. RejectIfAbrupt(next, deferred).
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));
    
        // iii. If 'next' is false, return deferred.[[Promise]].
        // Note: We implement this as an iterationTerminator
        if (next == vm.iterationTerminator.get())
            return JSValue::encode(deferred->promise());
        
        // iv. Let 'nextValue' be the result of calling IteratorValue(next).
        // v. RejectIfAbrupt(nextValue, deferred).
        // Note: 'next' is already the value, so there is nothing to do here.

        // vi. Let 'nextPromise' be the result of calling Invoke(C, "cast", (nextValue)).
        JSValue castFunction = C.get(exec, vm.propertyNames->cast);
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        CallData castFunctionCallData;
        CallType castFunctionCallType = getCallData(castFunction, castFunctionCallData);
        if (castFunctionCallType == CallTypeNone) {
            throwTypeError(exec);
            return JSValue::encode(abruptRejection(exec, deferred));
        }

        MarkedArgumentBuffer castFunctionArguments;
        castFunctionArguments.append(next);
        JSValue nextPromise = call(exec, castFunction, castFunctionCallType, castFunctionCallData, C, castFunctionArguments);

        // vii. RejectIfAbrupt(nextPromise, deferred).
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        // viii. Let 'result' be the result of calling Invoke(nextPromise, "then", (deferred.[[Resolve]], deferred.[[Reject]])).
        JSValue thenFunction = nextPromise.get(exec, vm.propertyNames->then);
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        CallData thenFunctionCallData;
        CallType thenFunctionCallType = getCallData(thenFunction, thenFunctionCallData);
        if (thenFunctionCallType == CallTypeNone) {
            throwTypeError(exec);
            return JSValue::encode(abruptRejection(exec, deferred));
        }

        MarkedArgumentBuffer thenFunctionArguments;
        thenFunctionArguments.append(deferred->resolve());
        thenFunctionArguments.append(deferred->reject());

        call(exec, thenFunction, thenFunctionCallType, thenFunctionCallData, nextPromise, thenFunctionArguments);

        // ix. RejectIfAbrupt(result, deferred).
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));
    } while (true);
}

EncodedJSValue JSC_HOST_CALL JSPromiseConstructorFuncAll(ExecState* exec)
{
    // -- Promise.all(iterable) --

    JSValue iterable = exec->argument(0);
    VM& vm = exec->vm();

    // 1. Let 'C' be the this value.
    JSValue C = exec->thisValue();

    // 2. Let 'deferred' be the result of calling GetDeferred(C).
    JSValue deferredValue = createJSPromiseDeferredFromConstructor(exec, C);

    // 3. ReturnIfAbrupt(deferred).
    if (exec->hadException())
        return JSValue::encode(jsUndefined());

    // NOTE: A non-abrupt completion of createJSPromiseDeferredFromConstructor implies that
    // C and deferredValue are objects.
    JSObject* thisObject = asObject(C);
    JSPromiseDeferred* deferred = jsCast<JSPromiseDeferred*>(deferredValue);

    // 4. Let 'iterator' be the result of calling GetIterator(iterable).
    JSValue iteratorFunction = iterable.get(exec, vm.propertyNames->iteratorPrivateName);
    if (exec->hadException())
        return JSValue::encode(abruptRejection(exec, deferred));

    CallData iteratorFunctionCallData;
    CallType iteratorFunctionCallType = getCallData(iteratorFunction, iteratorFunctionCallData);
    if (iteratorFunctionCallType == CallTypeNone) {
        throwTypeError(exec);
        return JSValue::encode(abruptRejection(exec, deferred));
    }

    ArgList iteratorFunctionArguments;
    JSValue iterator = call(exec, iteratorFunction, iteratorFunctionCallType, iteratorFunctionCallData, iterable, iteratorFunctionArguments);

    // 5. RejectIfAbrupt(iterator, deferred).
    if (exec->hadException())
        return JSValue::encode(abruptRejection(exec, deferred));

    // 6. Let 'values' be the result of calling ArrayCreate(0).
    JSArray* values = constructEmptyArray(exec, nullptr, thisObject->globalObject());
    
    // 7. Let 'countdownHolder' be Record { [[Countdown]]: 0 }.
    NumberObject* countdownHolder = constructNumber(exec, thisObject->globalObject(), JSValue(0));
    
    // 8. Let 'index' be 0.
    unsigned index = 0;
    
    // 9. Repeat.
    do {
        // i. Let 'next' be the result of calling IteratorStep(iterator).
        JSValue nextFunction = iterator.get(exec, exec->vm().propertyNames->iteratorNextPrivateName);
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        CallData nextFunctionCallData;
        CallType nextFunctionCallType = getCallData(nextFunction, nextFunctionCallData);
        if (nextFunctionCallType == CallTypeNone) {
            throwTypeError(exec);
            return JSValue::encode(abruptRejection(exec, deferred));
        }

        MarkedArgumentBuffer nextFunctionArguments;
        nextFunctionArguments.append(jsUndefined());
        JSValue next = call(exec, nextFunction, nextFunctionCallType, nextFunctionCallData, iterator, nextFunctionArguments);
        
        // ii. RejectIfAbrupt(next, deferred).
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        // iii. If 'next' is false,
        // Note: We implement this as an iterationTerminator
        if (next == vm.iterationTerminator.get()) {
            // a. If 'index' is 0,
            if (!index) {
                // a. Let 'resolveResult' be the result of calling the [[Call]] internal method
                //    of deferred.[[Resolve]] with undefined as thisArgument and a List containing
                //    values as argumentsList.
                performDeferredResolve(exec, deferred, values);

                // b. ReturnIfAbrupt(resolveResult).
                if (exec->hadException())
                    return JSValue::encode(jsUndefined());
            }
            
            // b. Return deferred.[[Promise]].
            return JSValue::encode(deferred->promise());
        }
        
        // iv. Let 'nextValue' be the result of calling IteratorValue(next).
        // v. RejectIfAbrupt(nextValue, deferred).
        // Note: 'next' is already the value, so there is nothing to do here.

        // vi. Let 'nextPromise' be the result of calling Invoke(C, "cast", (nextValue)).
        JSValue castFunction = C.get(exec, vm.propertyNames->cast);
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        CallData castFunctionCallData;
        CallType castFunctionCallType = getCallData(castFunction, castFunctionCallData);
        if (castFunctionCallType == CallTypeNone) {
            throwTypeError(exec);
            return JSValue::encode(abruptRejection(exec, deferred));
        }

        MarkedArgumentBuffer castFunctionArguments;
        castFunctionArguments.append(next);
        JSValue nextPromise = call(exec, castFunction, castFunctionCallType, castFunctionCallData, C, castFunctionArguments);

        // vii. RejectIfAbrupt(nextPromise, deferred).
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        // viii. Let 'countdownFunction' be a new built-in function object as defined in Promise.all Countdown Functions.
        JSFunction* countdownFunction = createPromiseAllCountdownFunction(vm, thisObject->globalObject());
        
        // ix. Set the [[Index]] internal slot of 'countdownFunction' to 'index'.
        countdownFunction->putDirect(vm, vm.propertyNames->indexPrivateName, JSValue(index));

        // x. Set the [[Values]] internal slot of 'countdownFunction' to 'values'.
        countdownFunction->putDirect(vm, vm.propertyNames->valuesPrivateName, values);

        // xi. Set the [[Deferred]] internal slot of 'countdownFunction' to 'deferred'.
        countdownFunction->putDirect(vm, vm.propertyNames->deferredPrivateName, deferred);

        // xii. Set the [[CountdownHolder]] internal slot of 'countdownFunction' to 'countdownHolder'.
        countdownFunction->putDirect(vm, vm.propertyNames->countdownHolderPrivateName, countdownHolder);

        // xiii. Let 'result' be the result of calling Invoke(nextPromise, "then", (countdownFunction, deferred.[[Reject]])).
        JSValue thenFunction = nextPromise.get(exec, vm.propertyNames->then);
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        CallData thenFunctionCallData;
        CallType thenFunctionCallType = getCallData(thenFunction, thenFunctionCallData);
        if (thenFunctionCallType == CallTypeNone) {
            throwTypeError(exec);
            return JSValue::encode(abruptRejection(exec, deferred));
        }

        MarkedArgumentBuffer thenFunctionArguments;
        thenFunctionArguments.append(countdownFunction);
        thenFunctionArguments.append(deferred->reject());

        call(exec, thenFunction, thenFunctionCallType, thenFunctionCallData, nextPromise, thenFunctionArguments);

        // xiv. RejectIfAbrupt(result, deferred).
        if (exec->hadException())
            return JSValue::encode(abruptRejection(exec, deferred));

        // xv. Set index to index + 1.
        index++;

        // xvi. Set countdownHolder.[[Countdown]] to countdownHolder.[[Countdown]] + 1.
        uint32_t newCountdownValue = countdownHolder->internalValue().asUInt32() + 1;
        countdownHolder->setInternalValue(vm, JSValue(newCountdownValue));
    } while (true);
}

JSPromise* constructPromise(ExecState* exec, JSGlobalObject* globalObject, JSFunction* resolver)
{
    JSPromiseConstructor* promiseConstructor = globalObject->promiseConstructor();

    ConstructData constructData;
    ConstructType constructType = getConstructData(promiseConstructor, constructData);
    ASSERT(constructType != ConstructTypeNone);

    MarkedArgumentBuffer arguments;
    arguments.append(resolver);

    return jsCast<JSPromise*>(construct(exec, promiseConstructor, constructType, constructData, arguments));
}

} // namespace JSC

#endif // ENABLE(PROMISES)
