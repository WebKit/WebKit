/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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
#include "JSPromise.h"

#include "BuiltinNames.h"
#include "DeferredWorkTimer.h"
#include "ErrorInstance.h"
#include "ErrorInstanceInlines.h"
#include "GlobalObjectMethodTable.h"
#include "JSCInlines.h"
#include "JSFunctionWithFields.h"
#include "JSMicrotask.h"
#include "JSPromiseCombinatorsContext.h"
#include "JSPromiseConstructor.h"
#include "JSPromisePrototype.h"
#include "JSPromiseReaction.h"
#include "Microtask.h"
#include "ObjectConstructor.h"
#include "TopExceptionScope.h"
#include "VMInlines.h"

namespace JSC {

const ClassInfo JSPromise::s_info = { "Promise"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSPromise) };

JSPromise* JSPromise::create(VM& vm, Structure* structure)
{
    JSPromise* promise = new (NotNull, allocateCell<JSPromise>(vm)) JSPromise(vm, structure);
    promise->finishCreation(vm);
    return promise;
}

JSPromise* JSPromise::createWithInitialValues(VM& vm, Structure* structure)
{
    return create(vm, structure);
}

Structure* JSPromise::createStructure(VM& vm, JSGlobalObject* globalObject, JSValue prototype)
{
    return Structure::create(vm, globalObject, prototype, TypeInfo(JSPromiseType, StructureFlags), info());
}

JSPromise::JSPromise(VM& vm, Structure* structure)
    : Base(vm, structure)
{
}

template<typename Visitor>
void JSPromise::visitChildrenImpl(JSCell* cell, Visitor& visitor)
{
    auto* thisObject = uncheckedDowncast<JSPromise>(cell);
    ASSERT_GC_OBJECT_INHERITS(thisObject, info());
    Base::visitChildren(thisObject, visitor);
    if (JSCell* payload = thisObject->m_packed.pointer())
        visitor.appendUnbarriered(payload);
    visitor.append(thisObject->m_slot);
}

DEFINE_VISIT_CHILDREN(JSPromise);

JSValue JSPromise::createNewPromiseCapability(JSGlobalObject* globalObject, JSValue constructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto [promise, resolve, reject] = newPromiseCapability(globalObject, constructor);
    RETURN_IF_EXCEPTION(scope, { });
    return createPromiseCapability(vm, globalObject, promise, resolve, reject);
}

JSValue JSPromise::createPromiseCapability(VM& vm, JSGlobalObject* globalObject, JSObject* promise, JSObject* resolve, JSObject* reject)
{
    auto* capability = constructEmptyObject(vm, globalObject->promiseCapabilityObjectStructure());
    capability->putDirectOffset(vm, promiseCapabilityResolvePropertyOffset, resolve);
    capability->putDirectOffset(vm, promiseCapabilityRejectPropertyOffset, reject);
    capability->putDirectOffset(vm, promiseCapabilityPromisePropertyOffset, promise);
    return capability;
}

std::tuple<JSObject*, JSObject*, JSObject*> JSPromise::newPromiseCapability(JSGlobalObject* globalObject, JSValue constructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (constructor == globalObject->promiseConstructor()) {
        auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
        auto [resolve, reject] = promise->createFirstResolvingFunctions(vm, globalObject);
        return { promise, resolve, reject };
    }

    auto* executor = JSFunctionWithFields::create(vm, globalObject, vm.promiseCapabilityExecutorExecutable());
    executor->setField(vm, JSFunctionWithFields::Field::ExecutorResolve, jsUndefined());
    executor->setField(vm, JSFunctionWithFields::Field::ExecutorReject, jsUndefined());

    MarkedArgumentBuffer args;
    args.append(executor);
    ASSERT(!args.hasOverflowed());
    JSObject* newObject = construct(globalObject, constructor, args, "argument is not a constructor"_s);
    RETURN_IF_EXCEPTION(scope, { });

    JSValue resolve = executor->getField(JSFunctionWithFields::Field::ExecutorResolve);
    JSValue reject = executor->getField(JSFunctionWithFields::Field::ExecutorReject);
    if (!resolve.isCallable()) [[unlikely]] {
        throwTypeError(globalObject, scope, "executor did not take a resolve function"_s);
        return { };
    }

    if (!reject.isCallable()) [[unlikely]] {
        throwTypeError(globalObject, scope, "executor did not take a reject function"_s);
        return { };
    }

    return { newObject, asObject(resolve), asObject(reject) };
}

JSPromise::DeferredData JSPromise::createDeferredData(JSGlobalObject* globalObject, JSPromiseConstructor* promiseConstructor)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto [ promiseCapability, resolveCapability, rejectCapability ] = newPromiseCapability(globalObject, promiseConstructor);
    RETURN_IF_EXCEPTION(scope, { });
    auto* promise = dynamicDowncast<JSPromise>(promiseCapability);
    auto* resolve = dynamicDowncast<JSFunction>(resolveCapability);
    auto* reject  = dynamicDowncast<JSFunction>(rejectCapability);
    if (promise && resolve && reject)
        return DeferredData { promise, resolve, reject };

    throwTypeError(globalObject, scope, "constructor is producing a bad value"_s);
    return { };
}

JSPromise* JSPromise::resolvedPromise(JSGlobalObject* globalObject, JSValue value)
{
    return uncheckedDowncast<JSPromise>(promiseResolve(globalObject, globalObject->promiseConstructor(), value));
}

JSPromise* JSPromise::rejectedPromise(JSGlobalObject* globalObject, JSValue value)
{
    VM& vm = globalObject->vm();
    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
    promise->reject(vm, value);
    return promise;
}

void JSPromise::resolve(JSGlobalObject* globalObject, VM& vm, JSValue value)
{
    ASSERT(!value.inherits<Exception>());
    if (!isFirstResolvingFunctionCalled()) {
        setFlags(flags() | isFirstResolvingFunctionCalledFlag);
        resolvePromise(globalObject, vm, value);
    }
}

void JSPromise::reject(VM& vm, JSValue value)
{
    ASSERT(!value.inherits<Exception>());
    if (!isFirstResolvingFunctionCalled()) {
        setFlags(flags() | isFirstResolvingFunctionCalledFlag);
        rejectPromise(vm, value);
    }
}

void JSPromise::fulfill(VM& vm, JSValue value)
{
    ASSERT(!value.inherits<Exception>());
    if (!isFirstResolvingFunctionCalled()) {
        setFlags(flags() | isFirstResolvingFunctionCalledFlag);
        fulfillPromise(vm, value);
    }
}

void JSPromise::pipeFrom(VM& vm, JSPromise* from)
{
    if (isFirstResolvingFunctionCalled())
        return;
    setFlags(flags() | isFirstResolvingFunctionCalledFlag);

    from->performPromiseThenWithInternalMicrotask(vm, InternalMicrotask::PromiseFulfillWithoutHandlerJob, this, jsUndefined());
}

void JSPromise::performPromiseThenExported(VM& vm, JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected, JSValue promiseOrCapability)
{
    return performPromiseThen(vm, globalObject, onFulfilled, onRejected, promiseOrCapability);
}

void JSPromise::rejectAsHandled(VM& vm, JSValue value)
{
    // Setting isHandledFlag before calling reject since this removes round-trip between JSC and PromiseRejectionTracker, and it does not show an user-observable behavior.
    if (!isFirstResolvingFunctionCalled()) {
        markAsHandled();
        reject(vm, value);
    }
}

void JSPromise::reject(VM& vm, Exception* reason)
{
    reject(vm, reason->value());
}

void JSPromise::rejectAsHandled(VM& vm, Exception* reason)
{
    rejectAsHandled(vm, reason->value());
}

JSPromise* JSPromise::rejectWithCaughtException(VM& vm, ThrowScope& scope)
{
    Exception* exception = scope.exception();
    ASSERT(exception);
    TRY_CLEAR_EXCEPTION(scope, nullptr);
    scope.release();
    reject(vm, exception->value());
    return this;
}

void JSPromise::setInlineMicrotaskReaction(VM& vm, InternalMicrotask task, JSCell* cell, JSValue context)
{
    ASSERT(status() == Status::Pending);
    ASSERT(inlineReactionKind() == InlineReactionKind::None);
    ASSERT(!payloadCell());
    ASSERT(task != InternalMicrotask::None);
    // The inline reaction always implies markAsHandled; fold both into one flag update.
    uint16_t newFlags = flags()
        | isHandledFlag
        | (static_cast<uint16_t>(InlineReactionKind::InternalMicrotask) << inlineReactionKindShift)
        | (static_cast<uint16_t>(task) << inlineReactionMicrotaskShift);
    setSlot(vm, context);
    setPackedCell(vm, newFlags, cell);
}

void JSPromise::setInlineHandlerReaction(VM& vm, InlineReactionKind kind, JSPromise* resultPromise, JSValue handler)
{
    ASSERT(status() == Status::Pending);
    ASSERT(inlineReactionKind() == InlineReactionKind::None);
    ASSERT(!payloadCell());
    ASSERT(kind == InlineReactionKind::FulfillHandler || kind == InlineReactionKind::RejectHandler);
    ASSERT(resultPromise);
    uint16_t newFlags = flags()
        | isHandledFlag
        | (static_cast<uint16_t>(kind) << inlineReactionKindShift);
    setSlot(vm, handler);
    setPackedCell(vm, newFlags, resultPromise);
}

JSPromiseReaction* JSPromise::spillInlineReaction(VM& vm)
{
    auto kind = inlineReactionKind();
    ASSERT(kind != InlineReactionKind::None);
    JSSlimPromiseReaction* reaction = nullptr;
    switch (kind) {
    case InlineReactionKind::InternalMicrotask: {
        InternalMicrotask task = inlineReactionMicrotask();
        JSValue context = m_slot.get();
        JSCell* cell = payloadCell();
        reaction = JSSlimPromiseReaction::create(vm, cell ? JSValue(cell) : jsUndefined(), task, context, nullptr);
        break;
    }
    case InlineReactionKind::FulfillHandler:
    case InlineReactionKind::RejectHandler: {
        JSPromise* resultPromise = uncheckedDowncast<JSPromise>(payloadCell());
        JSValue handler = m_slot.get();
        bool isFulfill = kind == InlineReactionKind::FulfillHandler;
        reaction = JSSlimPromiseReaction::create(vm, resultPromise, handler, isFulfill, nullptr);
        break;
    }
    case InlineReactionKind::None:
        RELEASE_ASSERT_NOT_REACHED();
    }
    clearSlot();
    uint16_t newFlags = flags() & ~(inlineReactionKindMask | inlineReactionMicrotaskMask);
    setPackedCell(vm, newFlags, reaction);
    return reaction;
}

JSPromiseReaction* JSPromise::reactionHead(VM& vm)
{
    ASSERT(status() == Status::Pending);
    if (inlineReactionKind() != InlineReactionKind::None) [[unlikely]]
        return spillInlineReaction(vm);
    return uncheckedDowncast<JSPromiseReaction>(payloadCell());
}

JSValue JSPromise::asyncStackTraceContext() const
{
    if (status() != Status::Pending)
        return { };
    switch (inlineReactionKind()) {
    case InlineReactionKind::None: {
        auto* head = uncheckedDowncast<JSPromiseReaction>(payloadCell());
        return head ? JSPromiseReaction::tryGetContext(head) : JSValue();
    }
    case InlineReactionKind::InternalMicrotask: {
        if (promiseReactionPacksGlobalContextAndIndex(inlineReactionMicrotask())) {
            ASSERT(payloadCell());
            return JSValue(payloadCell());
        }
        return m_slot.get();
    }
    case InlineReactionKind::FulfillHandler:
    case InlineReactionKind::RejectHandler:
        return { };
    }
    return { };
}

void JSPromise::performPromiseThen(VM& vm, JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected, JSValue promiseOrCapability)
{
    bool fulfilledCallable = onFulfilled.isCallable();
    bool rejectedCallable = onRejected.isCallable();

    switch (status()) {
    case JSPromise::Status::Pending: {
        bool onlyFulfill = fulfilledCallable && !rejectedCallable;
        bool onlyReject = !fulfilledCallable && rejectedCallable;
        if (inlineReactionKind() == InlineReactionKind::None && !payloadCell()) {
            if ((onlyFulfill || onlyReject) && promiseOrCapability.inherits<JSPromise>()) [[likely]] {
                auto* resultPromise = uncheckedDowncast<JSPromise>(promiseOrCapability);
                setInlineHandlerReaction(vm, onlyFulfill ? InlineReactionKind::FulfillHandler : InlineReactionKind::RejectHandler, resultPromise, onlyFulfill ? onFulfilled : onRejected);
                break;
            }
        }
        JSPromiseReaction* existing = reactionHead(vm);
        JSPromiseReaction* reaction;
        if (onlyFulfill)
            reaction = JSSlimPromiseReaction::create(vm, promiseOrCapability, onFulfilled, true, existing);
        else if (onlyReject)
            reaction = JSSlimPromiseReaction::create(vm, promiseOrCapability, onRejected, false, existing);
        else if (fulfilledCallable) {
            ASSERT(rejectedCallable);
            reaction = JSFullPromiseReaction::create(vm, promiseOrCapability, onFulfilled, onRejected, jsUndefined(), existing);
        } else
            reaction = JSSlimPromiseReaction::create(vm, promiseOrCapability, InternalMicrotask::PromiseResolveWithoutHandlerJob, jsUndefined(), existing);
        setPackedCell(vm, flags() | isHandledFlag, reaction);
        break;
    }
    case JSPromise::Status::Rejected: {
        JSValue settled = settlementValue();
        if (!isHandled())
            globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Handle);
        if (rejectedCallable)
            globalObject->queueMicrotask(vm, InternalMicrotask::PromiseReactionJob, static_cast<uint8_t>(Status::Rejected), promiseOrCapability, onRejected, settled);
        else
            globalObject->queueMicrotask(vm, InternalMicrotask::PromiseResolveWithoutHandlerJob, static_cast<uint8_t>(Status::Rejected), promiseOrCapability, settled, jsUndefined());
        markAsHandled();
        break;
    }
    case JSPromise::Status::Fulfilled: {
        JSValue settled = settlementValue();
        if (fulfilledCallable)
            globalObject->queueMicrotask(vm, InternalMicrotask::PromiseReactionJob, static_cast<uint8_t>(Status::Fulfilled), promiseOrCapability, onFulfilled, settled);
        else
            globalObject->queueMicrotask(vm, InternalMicrotask::PromiseResolveWithoutHandlerJob, static_cast<uint8_t>(Status::Fulfilled), promiseOrCapability, settled, jsUndefined());
        break;
    }
    }
}

void JSPromise::performPromiseThenWithInternalMicrotask(VM& vm, InternalMicrotask task, JSCell* cell, JSValue context)
{
    JSValue cellValue = cell ? JSValue(cell) : jsUndefined();
    switch (status()) {
    case JSPromise::Status::Pending: {
        if (inlineReactionKind() == InlineReactionKind::None && !payloadCell()) [[likely]] {
            setInlineMicrotaskReaction(vm, task, cell, context);
            break;
        }
        JSPromiseReaction* existing = reactionHead(vm);
        auto* reaction = JSSlimPromiseReaction::create(vm, cellValue, task, context, existing);
        setPackedCell(vm, flags() | isHandledFlag, reaction);
        break;
    }
    case JSPromise::Status::Rejected: {
        JSGlobalObject* globalObject = realm();
        JSValue settled = settlementValue();
        if (!isHandled())
            globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Handle);
        globalObject->queueMicrotask(vm, task, static_cast<uint8_t>(Status::Rejected), cellValue, settled, context);
        markAsHandled();
        break;
    }
    case JSPromise::Status::Fulfilled: {
        JSGlobalObject* globalObject = realm();
        JSValue settled = settlementValue();
        globalObject->queueMicrotask(vm, task, static_cast<uint8_t>(Status::Fulfilled), cellValue, settled, context);
        break;
    }
    }
}

bool isDefinitelyNonThenable(JSObject* object, JSGlobalObject* globalObject)
{
    if (!globalObject->promiseThenWatchpointSet().isStillValid()) [[unlikely]]
        return false;

    auto* structure = object->structure();
    auto state = structure->definitelyNonThenableState();
    if (state == Structure::DefinitelyNonThenableState::NonThenable && structure->realm() == globalObject)
        return true;
    if (state == Structure::DefinitelyNonThenableState::MaybeThenable)
        return false;

    bool result = true;
    auto* current = structure;
    while (current) {
        if (current->hasSpecialProperties()
            || current->typeInfo().getOwnPropertySlotIsImpureForPropertyAbsence()
            || current->typeInfo().overridesGetPrototype()
            || !current->hasMonoProto()) {
            result = false;
            break;
        }
        current = current->storedPrototypeStructure();
    }

    // Dictionary structures are mutated in place when properties are added or removed,
    // so the cached state could become stale (e.g. caching NonThenable, then adding `then`).
    // Give up caching entirely for them; the per-call walk above remains correct because
    // `hasSpecialProperties` is updated in place even for dictionaries.
    if (state == Structure::DefinitelyNonThenableState::NotComputed && !structure->isDictionary()) [[unlikely]] {
        if (!result) {
            // Always safe: a stale `false` only loses the optimization, never miscompiles.
            structure->setDefinitelyNonThenableState(Structure::DefinitelyNonThenableState::MaybeThenable);
        } else {
            // A `true` result is cacheable only when the entire prototype chain stays
            // under the protection of promiseThenWatchpointSet, which watches `then`
            // absence on Object.prototype. That limits the cacheable chain to [self]
            // (null proto) or [self, Object.prototype]. Mark anything else Uncacheable
            // so subsequent calls skip this check and go straight to the walk.
            JSValue proto = structure->storedPrototype();
            if (!proto.isObject() || asObject(proto) == structure->realm()->objectPrototype())
                structure->setDefinitelyNonThenableState(Structure::DefinitelyNonThenableState::NonThenable);
            else
                structure->setDefinitelyNonThenableState(Structure::DefinitelyNonThenableState::Uncacheable);
        }
    }
    return result;
}

ALWAYS_INLINE void JSPromise::settleInlineInternalMicrotask(VM& vm, JSGlobalObject* globalObject, Status newStatus, JSValue argument, uint16_t flagsSnapshot)
{
    ASSERT((flagsSnapshot & inlineReactionKindMask) == (static_cast<uint16_t>(InlineReactionKind::InternalMicrotask) << inlineReactionKindShift));
    ASSERT(flagsSnapshot & isHandledFlag);
    InternalMicrotask task = static_cast<InternalMicrotask>((flagsSnapshot & inlineReactionMicrotaskMask) >> inlineReactionMicrotaskShift);
    JSValue context = m_slot.get();
    JSCell* cell = payloadCell();
    JSValue cellValue = cell ? JSValue(cell) : jsUndefined();
    uint16_t settledFlags = (flagsSnapshot & ~(inlineReactionKindMask | inlineReactionMicrotaskMask)) | static_cast<uint16_t>(newStatus);
    setSlot(vm, argument);
    setPackedCell(vm, settledFlags, nullptr);
    globalObject->queueMicrotask(vm, task, static_cast<uint8_t>(newStatus), cellValue, argument, context);
}

ALWAYS_INLINE void JSPromise::settleInlineHandler(VM& vm, JSGlobalObject* globalObject, Status newStatus, JSValue argument, uint16_t flagsSnapshot)
{
    ASSERT(flagsSnapshot & isHandledFlag);
    InlineReactionKind kind = static_cast<InlineReactionKind>((flagsSnapshot & inlineReactionKindMask) >> inlineReactionKindShift);
    ASSERT(kind == InlineReactionKind::FulfillHandler || kind == InlineReactionKind::RejectHandler);
    bool settledIsFulfilled = newStatus == Status::Fulfilled;
    bool handlerIsFulfill = kind == InlineReactionKind::FulfillHandler;
    JSPromise* resultPromise = uncheckedDowncast<JSPromise>(payloadCell());
    JSValue handler = m_slot.get();
    uint16_t settledFlags = (flagsSnapshot & ~(inlineReactionKindMask | inlineReactionMicrotaskMask)) | static_cast<uint16_t>(newStatus);
    setSlot(vm, argument);
    setPackedCell(vm, settledFlags, nullptr);
    if (settledIsFulfilled == handlerIsFulfill)
        globalObject->queueMicrotask(vm, InternalMicrotask::PromiseReactionJob, static_cast<uint8_t>(newStatus), resultPromise, handler, argument);
    else
        globalObject->queueMicrotask(vm, InternalMicrotask::PromiseResolveWithoutHandlerJob, static_cast<uint8_t>(newStatus), resultPromise, argument, jsUndefined());
}

void JSPromise::rejectPromise(VM& vm, JSValue argument)
{
    ASSERT(status() == Status::Pending);
    JSGlobalObject* globalObject = realm();
    uint16_t currentFlags = flags();
    auto kind = static_cast<InlineReactionKind>((currentFlags & inlineReactionKindMask) >> inlineReactionKindShift);
    switch (kind) {
    case InlineReactionKind::InternalMicrotask:
        return settleInlineInternalMicrotask(vm, globalObject, Status::Rejected, argument, currentFlags);

    case InlineReactionKind::FulfillHandler:
    case InlineReactionKind::RejectHandler:
        return settleInlineHandler(vm, globalObject, Status::Rejected, argument, currentFlags);

    case InlineReactionKind::None: {
        JSPromiseReaction* reactions = uncheckedDowncast<JSPromiseReaction>(payloadCell());
        uint16_t settledFlags = currentFlags | static_cast<uint16_t>(Status::Rejected);
        setSlot(vm, argument);
        setPackedCell(vm, settledFlags, nullptr);

        if (!isHandled())
            globalObject->globalObjectMethodTable()->promiseRejectionTracker(globalObject, this, JSPromiseRejectionOperation::Reject);

        if (!reactions)
            return;
        triggerPromiseReactions(vm, globalObject, Status::Rejected, reactions, argument);
        return;
    }
    }
}

void JSPromise::fulfillPromise(VM& vm, JSValue argument)
{
    ASSERT(status() == Status::Pending);
    JSGlobalObject* globalObject = realm();
    uint16_t currentFlags = flags();
    auto kind = static_cast<InlineReactionKind>((currentFlags & inlineReactionKindMask) >> inlineReactionKindShift);
    switch (kind) {
    case InlineReactionKind::InternalMicrotask:
        return settleInlineInternalMicrotask(vm, globalObject, Status::Fulfilled, argument, currentFlags);

    case InlineReactionKind::FulfillHandler:
    case InlineReactionKind::RejectHandler:
        return settleInlineHandler(vm, globalObject, Status::Fulfilled, argument, currentFlags);

    case InlineReactionKind::None: {
        JSPromiseReaction* reactions = uncheckedDowncast<JSPromiseReaction>(payloadCell());
        uint16_t settledFlags = currentFlags | static_cast<uint16_t>(Status::Fulfilled);
        setSlot(vm, argument);
        setPackedCell(vm, settledFlags, nullptr);

        if (!reactions)
            return;
        triggerPromiseReactions(vm, globalObject, Status::Fulfilled, reactions, argument);
        return;
    }
    }
}

void JSPromise::resolvePromise(JSGlobalObject* globalObject, VM& vm, JSValue resolution)
{
    if (resolution == this) [[unlikely]] {
        Structure* errorStructure = globalObject->errorStructure(ErrorType::TypeError);
        auto* error = ErrorInstance::create(vm, errorStructure, "Cannot resolve a promise with itself"_s, jsUndefined(), nullptr, TypeNothing, ErrorType::TypeError, false);
        return rejectPromise(vm, error);
    }

    if (!resolution.isObject())
        return fulfillPromise(vm, resolution);

    auto* resolutionObject = asObject(resolution);
    if (resolutionObject->inherits<JSPromise>()) {
        auto* promise = uncheckedDowncast<JSPromise>(resolutionObject);
        if (promise->isThenFastAndNonObservable())
            return promise->realm()->queueMicrotask(vm, InternalMicrotask::PromiseResolveThenableJobFast, 0, resolutionObject, this, jsUndefined());
    }

    if (isDefinitelyNonThenable(resolutionObject, globalObject))
        return fulfillPromise(vm, resolution);

    JSValue then;
    JSValue error;
    {
        auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        then = resolutionObject->get(globalObject, vm.propertyNames->then);
        if (catchScope.exception()) [[unlikely]] {
            error = catchScope.exception()->value();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                return;
        }
    }
    if (error) [[unlikely]]
        return rejectPromise(vm, error);

    if (!then.isCallable()) [[likely]]
        return fulfillPromise(vm, resolutionObject);

    return globalObject->queueMicrotask(vm, InternalMicrotask::PromiseResolveThenableJob, 0, resolutionObject, then, this);
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionResolve, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    auto* other = dynamicDowncast<JSFunctionWithFields>(callee->getField(JSFunctionWithFields::Field::ResolvingOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingOther, jsNull());

    auto* promise = uncheckedDowncast<JSPromise>(callee->getField(JSFunctionWithFields::Field::ResolvingPromise));
    JSValue argument = callFrame->argument(0);

    promise->resolvePromise(globalObject, vm, argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionReject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    auto* other = dynamicDowncast<JSFunctionWithFields>(callee->getField(JSFunctionWithFields::Field::ResolvingOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingOther, jsNull());

    auto* promise = uncheckedDowncast<JSPromise>(callee->getField(JSFunctionWithFields::Field::ResolvingPromise));
    JSValue argument = callFrame->argument(0);

    promise->rejectPromise(vm, argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseFirstResolvingFunctionResolve, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    auto* promise = uncheckedDowncast<JSPromise>(callee->getField(JSFunctionWithFields::Field::FirstResolvingPromise));
    JSValue argument = callFrame->argument(0);

    promise->resolve(globalObject, globalObject->vm(), argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseFirstResolvingFunctionReject, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    auto* promise = uncheckedDowncast<JSPromise>(callee->getField(JSFunctionWithFields::Field::FirstResolvingPromise));
    JSValue argument = callFrame->argument(0);

    promise->reject(globalObject->vm(), argument);
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionResolveWithInternalMicrotask, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    auto* other = dynamicDowncast<JSFunctionWithFields>(callee->getField(JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, jsNull());

    auto* contextCell = uncheckedDowncast<JSSlimPromiseReaction>(callee->getField(JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskContext));
    JSValue argument = callFrame->argument(0);
    JSPromise::resolveWithInternalMicrotask(globalObject, vm, argument, contextCell->internalMicrotask(), contextCell->handlerOrContext());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseResolvingFunctionRejectWithInternalMicrotask, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();

    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    auto* other = dynamicDowncast<JSFunctionWithFields>(callee->getField(JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther));
    if (!other) [[unlikely]]
        return JSValue::encode(jsUndefined());

    callee->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, jsNull());
    other->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, jsNull());

    auto* contextCell = uncheckedDowncast<JSSlimPromiseReaction>(callee->getField(JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskContext));
    JSValue argument = callFrame->argument(0);
    JSPromise::rejectWithInternalMicrotask(vm, globalObject, argument, contextCell->internalMicrotask(), contextCell->handlerOrContext());
    return JSValue::encode(jsUndefined());
}

JSC_DEFINE_HOST_FUNCTION(promiseCapabilityExecutor, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* callee = uncheckedDowncast<JSFunctionWithFields>(callFrame->jsCallee());
    JSValue resolve = callee->getField(JSFunctionWithFields::Field::ExecutorResolve);
    if (!resolve.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "resolve function is already set"_s);

    JSValue reject = callee->getField(JSFunctionWithFields::Field::ExecutorReject);
    if (!reject.isUndefined()) [[unlikely]]
        return throwVMTypeError(globalObject, scope, "reject function is already set"_s);

    callee->setField(vm, JSFunctionWithFields::Field::ExecutorResolve, callFrame->argument(0));
    callee->setField(vm, JSFunctionWithFields::Field::ExecutorReject, callFrame->argument(1));

    return JSValue::encode(jsUndefined());
}

std::tuple<JSFunction*, JSFunction*> JSPromise::createResolvingFunctions(VM& vm, JSGlobalObject* globalObject)
{
    auto* resolve = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionResolveExecutable());
    auto* reject = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionRejectExecutable());

    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingPromise, this);
    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingOther, reject);

    reject->setField(vm, JSFunctionWithFields::Field::ResolvingPromise, this);
    reject->setField(vm, JSFunctionWithFields::Field::ResolvingOther, resolve);

    return std::tuple { resolve, reject };
}

JSFunction* JSPromise::createFirstResolveFunction(VM& vm, JSGlobalObject* globalObject)
{
    auto* resolve = JSFunctionWithFields::create(vm, globalObject, vm.promiseFirstResolvingFunctionResolveExecutable());
    resolve->setField(vm, JSFunctionWithFields::Field::FirstResolvingPromise, this);
    return resolve;
}

JSFunction* JSPromise::createFirstRejectFunction(VM& vm, JSGlobalObject* globalObject)
{
    auto* reject = JSFunctionWithFields::create(vm, globalObject, vm.promiseFirstResolvingFunctionRejectExecutable());
    reject->setField(vm, JSFunctionWithFields::Field::FirstResolvingPromise, this);
    return reject;
}

std::tuple<JSFunction*, JSFunction*> JSPromise::createFirstResolvingFunctions(VM& vm, JSGlobalObject* globalObject)
{
    return std::tuple { createFirstResolveFunction(vm, globalObject), createFirstRejectFunction(vm, globalObject) };
}

std::tuple<JSFunction*, JSFunction*> JSPromise::createResolvingFunctionsWithInternalMicrotask(VM& vm, JSGlobalObject* globalObject, InternalMicrotask task, JSValue context)
{
    auto* resolve = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionResolveWithInternalMicrotaskExecutable());
    auto* reject = JSFunctionWithFields::create(vm, globalObject, vm.promiseResolvingFunctionRejectWithInternalMicrotaskExecutable());

    auto* contextCell = JSSlimPromiseReaction::create(vm, jsUndefined(), task, context, /* next */ nullptr);

    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskContext, contextCell);
    resolve->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, reject);

    reject->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskContext, contextCell);
    reject->setField(vm, JSFunctionWithFields::Field::ResolvingWithInternalMicrotaskOther, resolve);

    return std::tuple { resolve, reject };
}

void JSPromise::triggerPromiseReactions(VM& vm, JSGlobalObject* globalObject, Status status, JSPromiseReaction* head, JSValue argument)
{
    bool isResolved = status == JSPromise::Status::Fulfilled;

    auto queue = [&](JSPromiseReaction* reaction) ALWAYS_INLINE_LAMBDA {
        JSValue promise = reaction->promise();
        InternalMicrotask task = InternalMicrotask::PromiseReactionJob;
        JSValue handler;
        JSValue arg = argument;

        switch (reaction->type()) {
        case JSSlimPromiseReactionType: {
            auto* slimReaction = uncheckedDowncast<JSSlimPromiseReaction>(reaction);
            if (auto internalTask = slimReaction->internalMicrotask(); internalTask != InternalMicrotask::None) {
                task = internalTask;
                handler = argument;
                arg = slimReaction->handlerOrContext();
            } else if (slimReaction->isFulfillHandler() == isResolved)
                handler = slimReaction->handlerOrContext();
            else {
                task = InternalMicrotask::PromiseResolveWithoutHandlerJob;
                handler = argument;
                arg = jsUndefined();
            }
            break;
        }
        case JSFullPromiseReactionType: {
            auto* fullReaction = uncheckedDowncast<JSFullPromiseReaction>(reaction);
            handler = isResolved ? fullReaction->onFulfilled() : fullReaction->onRejected();
            ASSERT(fullReaction->context().isUndefinedOrNull());
            break;
        }
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }

        globalObject->queueMicrotask(vm, task, static_cast<uint8_t>(status), promise, handler, arg);
    };

    ASSERT(head);
    if (!head->next()) [[likely]] {
        queue(head);
        return;
    }

    // Reverse the order of singly-linked-list.
    JSPromiseReaction* previous = nullptr;
    {
        auto* current = head;
        while (current) {
            auto* next = current->next();
            current->setNext(vm, previous);
            previous = current;
            current = next;
        }
    }
    head = previous;

    auto* current = head;
    do {
        auto* next = current->next();
        queue(current);
        current = next;
    } while (current);
}

void JSPromise::resolveWithInternalMicrotaskForAsyncAwait(JSGlobalObject* globalObject, VM& vm, JSValue resolution, InternalMicrotask task, JSValue context)
{
    if (resolution.inherits<JSPromise>()) {
        auto* promise = uncheckedDowncast<JSPromise>(resolution);
        if (promise->realm() == globalObject && promiseSpeciesWatchpointIsValid(vm, promise)) [[likely]]
            return promise->performPromiseThenWithInternalMicrotask(vm, task, nullptr, context);

        JSValue constructor;
        JSValue error;
        {
            auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
            constructor = promise->get(globalObject, vm.propertyNames->constructor);
            if (catchScope.exception()) [[unlikely]] {
                error = catchScope.exception()->value();
                if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                    return;
            }
        }
        if (error) [[unlikely]] {
            std::array<JSValue, maxMicrotaskArguments> arguments { {
                jsUndefined(),
                error,
                context,
            } };
            runInternalMicrotask(globalObject, vm, task, static_cast<uint8_t>(JSPromise::Status::Rejected), arguments);
            return;
        }

        if (constructor == globalObject->promiseConstructor())
            return promise->performPromiseThenWithInternalMicrotask(vm, task, nullptr, context);
    }

    resolveWithInternalMicrotask(globalObject, vm, resolution, task, context);
}

void JSPromise::resolveWithInternalMicrotask(JSGlobalObject* globalObject, VM& vm, JSValue resolution, InternalMicrotask task, JSValue context)
{
    if (!resolution.isObject())
        return fulfillWithInternalMicrotask(vm, globalObject, resolution, task, context);

    auto* resolutionObject = asObject(resolution);
    if (resolutionObject->inherits<JSPromise>()) {
        auto* promise = uncheckedDowncast<JSPromise>(resolutionObject);
        if (promise->realm() == globalObject && promise->isThenFastAndNonObservable())
            return globalObject->queueMicrotask(vm, InternalMicrotask::PromiseResolveThenableJobWithInternalMicrotaskFast, static_cast<uint8_t>(task), resolutionObject, context, jsUndefined());
    }

    if (isDefinitelyNonThenable(resolutionObject, globalObject))
        return fulfillWithInternalMicrotask(vm, globalObject, resolution, task, context);

    JSValue then;
    JSValue error;
    {
        auto catchScope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
        then = resolutionObject->get(globalObject, vm.propertyNames->then);
        if (catchScope.exception()) [[unlikely]] {
            error = catchScope.exception()->value();
            if (!catchScope.clearExceptionExceptTermination()) [[unlikely]]
                return;
        }
    }
    if (error) [[unlikely]]
        return rejectWithInternalMicrotask(vm, globalObject, error, task, context);

    if (!then.isCallable()) [[likely]]
        return fulfillWithInternalMicrotask(vm, globalObject, resolution, task, context);

    return globalObject->queueMicrotask(vm, InternalMicrotask::PromiseResolveThenableJobWithInternalMicrotask, static_cast<uint8_t>(task), resolutionObject, then, context);
}

void JSPromise::rejectWithInternalMicrotask(VM& vm, JSGlobalObject* globalObject, JSValue argument, InternalMicrotask task, JSValue context)
{
    globalObject->queueMicrotask(vm, task, static_cast<uint8_t>(Status::Rejected), jsUndefined(), argument, context);
}

void JSPromise::fulfillWithInternalMicrotask(VM& vm, JSGlobalObject* globalObject, JSValue argument, InternalMicrotask task, JSValue context)
{
    globalObject->queueMicrotask(vm, task, static_cast<uint8_t>(Status::Fulfilled), jsUndefined(), argument, context);
}

bool JSPromise::isThenFastAndNonObservable()
{
    JSGlobalObject* globalObject = this->realm();
    Structure* structure = this->structure();
    if (!globalObject->promiseThenWatchpointSet().isStillValid()) [[unlikely]]
        return false;

    if (structure == globalObject->promiseStructure())
        return true;

    if (getPrototypeDirect() != globalObject->promisePrototype())
        return false;

    VM& vm = globalObject->vm();
    if (getDirectOffset(vm, vm.propertyNames->then) != invalidOffset)
        return false;

    return true;
}

JSObject* promiseSpeciesConstructor(JSGlobalObject* globalObject, JSObject* thisObject)
{
    VM& vm = getVM(globalObject);
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (auto* promise = dynamicDowncast<JSPromise>(thisObject)) [[likely]] {
        if (promiseSpeciesWatchpointIsValid(vm, promise)) [[likely]]
            return globalObject->promiseConstructor();
    }

    JSValue constructor = thisObject->get(globalObject, vm.propertyNames->constructor);
    RETURN_IF_EXCEPTION(scope, { });

    if (constructor.isUndefined())
        return globalObject->promiseConstructor();

    if (!constructor.isObject()) [[unlikely]] {
        throwTypeError(globalObject, scope, "|this|.constructor is not an Object or undefined"_s);
        return { };
    }

    constructor = asObject(constructor)->get(globalObject, vm.propertyNames->speciesSymbol);
    RETURN_IF_EXCEPTION(scope, { });

    if (constructor.isUndefinedOrNull())
        return globalObject->promiseConstructor();

    if (constructor.isConstructor()) [[likely]]
        return asObject(constructor);

    throwTypeError(globalObject, scope, "|this|.constructor[Symbol.species] is not a constructor"_s);
    return { };
}

Structure* createPromiseCapabilityObjectStructure(VM& vm, JSGlobalObject& globalObject)
{
    Structure* structure = globalObject.structureCache().emptyObjectStructureForPrototype(&globalObject, globalObject.objectPrototype(), JSFinalObject::defaultInlineCapacity);
    PropertyOffset offset;
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->resolve, 0, offset);
    RELEASE_ASSERT(offset == promiseCapabilityResolvePropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->reject, 0, offset);
    RELEASE_ASSERT(offset == promiseCapabilityRejectPropertyOffset);
    structure = Structure::addPropertyTransition(vm, structure, vm.propertyNames->promise, 0, offset);
    RELEASE_ASSERT(offset == promiseCapabilityPromisePropertyOffset);
    return structure;
}

JSObject* JSPromise::then(JSGlobalObject* globalObject, JSValue onFulfilled, JSValue onRejected)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    JSObject* resultPromise;
    JSValue resultPromiseCapability;
    if (promiseSpeciesWatchpointIsValid(vm, this)) [[likely]] {
        resultPromise = JSPromise::create(vm, globalObject->promiseStructure());
        resultPromiseCapability = resultPromise;
    } else {
        auto* constructor = promiseSpeciesConstructor(globalObject, this);
        RETURN_IF_EXCEPTION(scope, { });

        auto [promise, resolve, reject] = JSPromise::newPromiseCapability(globalObject, constructor);
        RETURN_IF_EXCEPTION(scope, { });

        resultPromise = promise;
        resultPromiseCapability = JSPromise::createPromiseCapability(vm, globalObject, promise, resolve, reject);
    }

    scope.release();
    performPromiseThen(vm, globalObject, onFulfilled, onRejected, resultPromiseCapability);
    return resultPromise;
}

JSObject* JSPromise::promiseResolve(JSGlobalObject* globalObject, JSObject* constructor, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (argument.inherits<JSPromise>()) {
        auto* promise = uncheckedDowncast<JSPromise>(argument);
        if (promiseSpeciesWatchpointIsValid(vm, promise)) [[likely]] {
            if (constructor == promise->realm()->promiseConstructor())
                return promise;
        } else {
            auto property = promise->get(globalObject, vm.propertyNames->constructor);
            RETURN_IF_EXCEPTION(scope, { });

            if (property == constructor)
                return promise;
        }
    }

    if (constructor == globalObject->promiseConstructor()) [[likely]] {
        JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
        scope.release();
        promise->resolve(globalObject, vm, argument);
        return promise;
    }

    auto [promise, resolve, reject] = newPromiseCapability(globalObject, constructor);
    RETURN_IF_EXCEPTION(scope, { });

    MarkedArgumentBuffer arguments;
    arguments.append(argument);
    ASSERT(!arguments.hasOverflowed());
    scope.release();
    call(globalObject, resolve, jsUndefined(), arguments, "resolve is not a function"_s);
    return promise;
}

JSObject* JSPromise::promiseReject(JSGlobalObject* globalObject, JSObject* constructor, JSValue argument)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (constructor == globalObject->promiseConstructor()) [[likely]] {
        JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());
        promise->reject(vm, argument);
        return promise;
    }

    auto [promise, resolve, reject] = newPromiseCapability(globalObject, constructor);
    RETURN_IF_EXCEPTION(scope, { });

    MarkedArgumentBuffer arguments;
    arguments.append(argument);
    ASSERT(!arguments.hasOverflowed());
    scope.release();
    call(globalObject, reject, jsUndefined(), arguments, "reject is not a function"_s);
    return promise;
}

} // namespace JSC
