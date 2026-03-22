/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#pragma once

#include "JSDOMConvert.h"
#include "JSDOMIterator.h"
#include "JSDOMPromise.h"
#include "JSDOMPromiseDeferred.h"
#include <JavaScriptCore/AsyncIteratorPrototype.h>
#include <JavaScriptCore/IteratorOperations.h>
#include <JavaScriptCore/JSBoundFunction.h>
#include <JavaScriptCore/JSPromiseConstructor.h>
#include <JavaScriptCore/PropertySlot.h>
#include <type_traits>
#include <wtf/CompletionHandler.h>

namespace WebCore {
template<typename Iterator>
concept HasAsyncIteratorReturn = requires(Iterator iterator, JSDOMGlobalObject& globalObject)
{
    { iterator.returnSteps(globalObject, JSC::JSValue { }) } -> std::same_as<Ref<DOMPromise>>;
};

template<typename Iterator>
concept IsAsyncIteratorNextReturningPromise = requires(Iterator iterator, JSDOMGlobalObject& globalObject)
{
    { iterator.next(globalObject) } -> std::same_as<Ref<DOMPromise>>;
};

// https://webidl.spec.whatwg.org/#es-asynchronous-iterator-prototype-object
template<typename JSWrapper, typename IteratorTraits> class JSDOMAsyncIteratorPrototype final : public JSC::JSNonFinalObject {
public:
    using Base = JSC::JSNonFinalObject;
    using DOMWrapped = typename JSWrapper::DOMWrapped;

    template<typename CellType, JSC::SubspaceAccess>
    static JSC::GCClient::IsoSubspace* subspaceFor(JSC::VM& vm)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSDOMAsyncIteratorPrototype, Base);
        return &vm.plainObjectSpace();
    }

    static JSDOMAsyncIteratorPrototype* create(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::Structure* structure)
    {
        STATIC_ASSERT_ISO_SUBSPACE_SHARABLE(JSDOMAsyncIteratorPrototype, JSDOMAsyncIteratorPrototype::Base);
        JSDOMAsyncIteratorPrototype* prototype = new (NotNull, JSC::allocateCell<JSDOMAsyncIteratorPrototype>(vm)) JSDOMAsyncIteratorPrototype(vm, structure);
        prototype->finishCreation(vm, globalObject);
        return prototype;
    }

    DECLARE_INFO;

    static JSC::Structure* createStructure(JSC::VM& vm, JSC::JSGlobalObject* globalObject, JSC::JSValue prototype)
    {
        return JSC::Structure::create(vm, globalObject, prototype, JSC::TypeInfo(JSC::ObjectType, StructureFlags), info());
    }

    static JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES next(JSC::JSGlobalObject*, JSC::CallFrame*);
    static JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES returnMethod(JSC::JSGlobalObject*, JSC::CallFrame*);

private:
    JSDOMAsyncIteratorPrototype(JSC::VM& vm, JSC::Structure* structure)
        : Base(vm, structure)
    {
    }

    void finishCreation(JSC::VM&, JSC::JSGlobalObject*);
    template<typename Iterator> void addReturnMethodIfNeeded(JSC::VM&, JSC::JSGlobalObject*) { }
    template<HasAsyncIteratorReturn Iterator> void addReturnMethodIfNeeded(JSC::VM&, JSC::JSGlobalObject*);
};

template<typename JSWrapper, typename IteratorTraits> class JSDOMAsyncIteratorBase : public JSDOMObject {
public:
    using Base = JSDOMObject;

    using Wrapper = JSWrapper;
    using Traits = IteratorTraits;

    using DOMWrapped = typename Wrapper::DOMWrapped;
    using InternalIterator = Ref<typename DOMWrapped::Iterator>;
    using Prototype = JSDOMAsyncIteratorPrototype<Wrapper, Traits>;

    DECLARE_INFO;

    static Prototype* createPrototype(JSC::VM& vm, JSC::JSGlobalObject& globalObject)
    {
        auto* structure = Prototype::createStructure(vm, &globalObject, globalObject.asyncIteratorPrototype());
        structure->setMayBePrototype(true);
        return Prototype::create(vm, &globalObject, structure);
    }

    static void createStructure(JSC::VM&, JSC::JSGlobalObject*, JSC::JSValue); // Make use of createStructure for this compile-error.
    
    JSC::JSValue next(JSC::JSGlobalObject&);
    JSC::JSPromise* runNextSteps(JSC::JSGlobalObject&);

    template<typename Iterator>
    JSC::JSPromise* getNextIterationResult(JSC::JSGlobalObject&);
    template<IsAsyncIteratorNextReturningPromise Iterator>
    JSC::JSPromise* getNextIterationResult(JSC::JSGlobalObject&);

    JSC::JSValue returnMethod(JSC::JSGlobalObject&, JSC::JSValue);
    JSC::JSPromise* runReturnSteps(JSC::JSGlobalObject&, JSC::JSValue);
    JSC::JSPromise* getReturnResult(JSC::JSGlobalObject&, JSC::JSValue);

protected:
    JSDOMAsyncIteratorBase(JSC::Structure* structure, JSWrapper& iteratedObject, IterationKind kind, InternalIterator&& iterator)
        : Base(structure, *iteratedObject.realm())
        , m_iterator(WTF::move(iterator))
        , m_kind(kind)
        , m_isFinished(IsFinished::create())
    {
    }

    template<typename Iterator> JSC::JSValue settle(JSC::JSGlobalObject&, JSC::CallFrame&);
    template<HasAsyncIteratorReturn Iterator> JSC::JSValue settle(JSC::JSGlobalObject&, JSC::CallFrame&);
    static JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES onPromiseSettled(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSBoundFunction* createOnSettledFunction(JSC::JSGlobalObject*, JSC::EncodedJSValue* = nullptr);

    JSC::EncodedJSValue fulfill(JSC::JSGlobalObject*, JSC::JSValue);
    static JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES onPromiseFulFilled(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSBoundFunction* createOnFulfilledFunction(JSC::JSGlobalObject*);

    JSC::EncodedJSValue reject(JSC::JSGlobalObject*, JSC::JSValue);
    static JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES onPromiseRejected(JSC::JSGlobalObject*, JSC::CallFrame*);
    JSC::JSBoundFunction* createOnRejectedFunction(JSC::JSGlobalObject*);

    static void destroy(JSC::JSCell*);

    class IsFinished : public RefCountedAndCanMakeWeakPtr<IsFinished> {
    public:
        static Ref<IsFinished> create() { return adoptRef(*new IsFinished); }

        operator bool() { return m_value; }
        void markAsFinished() { m_value = true; }

    private:
        IsFinished() = default;

        bool m_value { false };
    };

    RefPtr<typename DOMWrapped::Iterator> m_iterator;
    IterationKind m_kind;
    RefPtr<DOMPromise> m_ongoingPromise;
    const Ref<IsFinished> m_isFinished;
};

template<typename IteratorValue, typename IteratorTraits>
inline EnableIfMap<IteratorTraits, JSC::JSValue> convertToJS(JSC::JSGlobalObject& globalObject, JSDOMGlobalObject& domGlobalObject, IteratorValue& value, IteratorTraits, IterationKind kind)
{
    JSC::VM& vm = globalObject.vm();
    Locker<JSC::JSLock> locker(vm.apiLock());

    switch (kind) {
    case IterationKind::Keys:
        return toJS<typename IteratorTraits::KeyType>(globalObject, domGlobalObject, value.key);
    case IterationKind::Values:
        return toJS<typename IteratorTraits::ValueType>(globalObject, domGlobalObject, value.value);
    case IterationKind::Entries:
        return jsPair<typename IteratorTraits::KeyType, typename IteratorTraits::ValueType>(globalObject, domGlobalObject, value.key, value.value);
    };

    ASSERT_NOT_REACHED();
    return { };
}

template<typename IteratorValue, typename IteratorTraits>
inline EnableIfSet<IteratorTraits, JSC::JSValue> convertToJS(JSC::JSGlobalObject& globalObject, JSDOMGlobalObject& domGlobalObject, IteratorValue& value, IteratorTraits, IterationKind kind)
{
    JSC::VM& vm = globalObject.vm();
    Locker<JSC::JSLock> locker(vm.apiLock());

    auto result = toJS<typename IteratorTraits::ValueType>(globalObject, domGlobalObject, value);
    switch (kind) {
    case IterationKind::Keys:
    case IterationKind::Values:
        return result;
    case IterationKind::Entries:
        return jsPair(globalObject, domGlobalObject, result, result);
    };

    ASSERT_NOT_REACHED();
    return { };
}

template<typename JSWrapper, typename IteratorTraits>
void JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::destroy(JSCell* cell)
{
    // We cannot rely on jsCast() during JSObject destruction.
    SUPPRESS_MEMORY_UNSAFE_CAST auto* thisObject = static_cast<JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>*>(cell);
    thisObject->JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::~JSDOMAsyncIteratorBase();
}

template<typename JSWrapper, typename IteratorTraits>
JSC::JSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::next(JSC::JSGlobalObject& lexicalGlobalObject)
{
    JSC::VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!m_ongoingPromise || !m_ongoingPromise->promise()) {
        auto* promise = runNextSteps(lexicalGlobalObject);
        RETURN_IF_EXCEPTION(scope, { });

        m_ongoingPromise = DOMPromise::create(*this->realm(), *promise);
        return m_ongoingPromise->promise();
    }

    auto afterOngoingPromiseCapability = JSC::JSPromise::createNewPromiseCapability(&lexicalGlobalObject, lexicalGlobalObject.promiseConstructor());
    RETURN_IF_EXCEPTION(scope, { });

    auto* promise = jsDynamicCast<JSC::JSPromise*>(afterOngoingPromiseCapability.get(&lexicalGlobalObject, vm.propertyNames->promise));
    RETURN_IF_EXCEPTION(scope, { });

    auto onSettled = createOnSettledFunction(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto* ongoingPromise = m_ongoingPromise->promise();
    ongoingPromise->performPromiseThenExported(vm, &lexicalGlobalObject, onSettled, onSettled, afterOngoingPromiseCapability);

    m_ongoingPromise = DOMPromise::create(*this->realm(), *promise);
    return m_ongoingPromise->promise();
}

template<typename JSWrapper, typename IteratorTraits>
JSC::JSPromise* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::runNextSteps(JSC::JSGlobalObject& globalObject)
{
    JSC::VM& vm = globalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto nextPromiseCapability = JSC::JSPromise::createNewPromiseCapability(&globalObject, globalObject.promiseConstructor());
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto* promise = jsDynamicCast<JSC::JSPromise*>(nextPromiseCapability.get(&globalObject, vm.propertyNames->promise));
    RETURN_IF_EXCEPTION(scope, { });

    if (m_isFinished.get()) {
        promise->resolve(&globalObject, vm, JSC::createIteratorResultObject(&globalObject, JSC::jsUndefined(), true));
        RETURN_IF_EXCEPTION(scope, nullptr);

        return promise;
    }

    auto nextPromise = getNextIterationResult<typename DOMWrapped::Iterator>(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto onFulfilled = createOnFulfilledFunction(&globalObject);
    auto onRejected = createOnRejectedFunction(&globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    nextPromise->performPromiseThenExported(vm, &globalObject, onFulfilled, onRejected, nextPromiseCapability);

    return promise;
}

template<typename JSWrapper, typename IteratorTraits>
template<typename Iterator>
JSC::JSPromise* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::getNextIterationResult(JSC::JSGlobalObject& globalObject)
{
    JSC::VM& vm = JSC::getVM(&globalObject);

    auto* promise = JSC::JSPromise::create(vm, globalObject.promiseStructure());
    auto deferred = DeferredPromise::create(*this->realm(), *promise);
    if (!m_iterator) {
        deferred->resolve();
        return promise;
    }

    Ref { *m_iterator }->next([deferred = WTF::move(deferred), traits = IteratorTraits { }, kind = m_kind, weakIsFinished = WeakPtr(m_isFinished)](auto result) mutable {
        auto* globalObject = deferred->globalObject();
        if (!globalObject)
            return;

        if (result.hasException())
            return deferred->reject(result.releaseException());

        auto resultValue = result.releaseReturnValue();
        if (!resultValue) {
            if (auto* isFinished = weakIsFinished.get())
                isFinished->markAsFinished();

            return deferred->resolve();
        }

        deferred->resolveWithJSValue(convertToJS(*globalObject, *globalObject, *resultValue, traits, kind));
    });

    return promise;
}

template<typename JSWrapper, typename IteratorTraits>
template<IsAsyncIteratorNextReturningPromise Iterator>
JSC::JSPromise* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::getNextIterationResult(JSC::JSGlobalObject& globalObject)
{
    Ref promise = Ref { *m_iterator }->next(*JSC::jsCast<JSDOMGlobalObject*>(&globalObject));
    promise->whenSettled([weakIterator = WeakPtr { *m_iterator }, weakIsFinished = WeakPtr(m_isFinished)] {
        auto* isFinished = weakIsFinished.get();
        if (!isFinished)
            return;
        auto* iterator = weakIterator.get();
        if (iterator && iterator->isFinished())
            isFinished->markAsFinished();
    });
    return promise->promise();
}

template<typename JSWrapper, typename IteratorTraits>
template<typename Iterator> JSC::JSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::settle(JSC::JSGlobalObject& globalObject, JSC::CallFrame&)
{
    return runNextSteps(globalObject);
}

template<typename JSWrapper, typename IteratorTraits>
template<HasAsyncIteratorReturn Iterator> JSC::JSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::settle(JSC::JSGlobalObject& globalObject, JSC::CallFrame& callFrame)
{
    if (callFrame.argumentCount() > 1) {
        ASSERT(callFrame.argumentCount() == 2);
        return runReturnSteps(globalObject, callFrame.argument(1));
    }

    return runNextSteps(globalObject);
}

template<typename JSWrapper, typename IteratorTraits>
JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::onPromiseSettled(JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame)
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto castedThis = JSC::jsDynamicCast<JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>*>(callFrame->thisValue());
    if (!castedThis)
        return throwThisTypeError(*globalObject, scope, JSWrapper::info()->className, "onPromiseSettled");

    return JSC::JSValue::encode(castedThis->template settle<typename DOMWrapped::Iterator>(*globalObject, *callFrame));
}

template<typename JSWrapper, typename IteratorTraits>
JSC::JSBoundFunction* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::createOnSettledFunction(JSC::JSGlobalObject* globalObject, JSC::EncodedJSValue* returnStepsValue)
{
    JSC::VM& vm = globalObject->vm();

    JSC::ArgList boundArgs;
    if (returnStepsValue)
        boundArgs = { returnStepsValue, 1 };

    auto onSettled = JSC::JSFunction::create(vm, globalObject, 0, String(), onPromiseSettled, JSC::ImplementationVisibility::Public);
    return JSC::JSBoundFunction::create(vm, globalObject, onSettled, this, WTF::move(boundArgs), 1, jsEmptyString(vm), JSC::makeSource("createOnSettledFunction"_s, JSC::SourceOrigin(), JSC::SourceTaintedOrigin::Untainted));
}

template<typename JSWrapper, typename IteratorTraits>
JSC::EncodedJSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::fulfill(JSC::JSGlobalObject* globalObject, JSC::JSValue result)
{
    m_ongoingPromise = nullptr;
    if (m_isFinished.get())
        m_iterator = nullptr;

    return JSC::JSValue::encode(JSC::createIteratorResultObject(globalObject, result, m_isFinished.get()));
}

template<typename JSWrapper, typename IteratorTraits>
JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::onPromiseFulFilled(JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame)
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto castedThis = JSC::jsDynamicCast<JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>*>(callFrame->thisValue());
    if (!castedThis)
        return throwThisTypeError(*globalObject, scope, JSWrapper::info()->className, "onPromiseFulfilled");

    return castedThis->fulfill(globalObject, callFrame->argument(0));
}

template<typename JSWrapper, typename IteratorTraits>
JSC::JSBoundFunction* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::createOnFulfilledFunction(JSC::JSGlobalObject* globalObject)
{
    JSC::VM& vm = globalObject->vm();
    auto onFulfilled = JSC::JSFunction::create(vm, globalObject, 0, String(), onPromiseFulFilled, JSC::ImplementationVisibility::Public);
    return JSC::JSBoundFunction::create(vm, globalObject, onFulfilled, this, { }, 1, jsEmptyString(vm), JSC::makeSource("createOnFulfilledFunction"_s, JSC::SourceOrigin(), JSC::SourceTaintedOrigin::Untainted));
}

template<typename JSWrapper, typename IteratorTraits>
JSC::EncodedJSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::reject(JSC::JSGlobalObject* globalObject, JSC::JSValue reason)
{
    m_ongoingPromise = nullptr;
    m_iterator = nullptr;
    m_isFinished->markAsFinished();

    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMError(globalObject, scope, reason);
}

template<typename JSWrapper, typename IteratorTraits>
JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::onPromiseRejected(JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame)
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto castedThis = JSC::jsDynamicCast<JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>*>(callFrame->thisValue());
    if (!castedThis)
        return throwThisTypeError(*globalObject, scope, JSWrapper::info()->className, "onPromiseRejected");

    return castedThis->reject(globalObject, callFrame->argument(0));
}

template<typename JSWrapper, typename IteratorTraits>
JSC::JSBoundFunction* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::createOnRejectedFunction(JSC::JSGlobalObject* globalObject)
{
    JSC::VM& vm = globalObject->vm();
    auto onRejected = JSC::JSFunction::create(vm, globalObject, 0, String(), onPromiseRejected, JSC::ImplementationVisibility::Public);
    return JSC::JSBoundFunction::create(vm, globalObject, onRejected, this, { }, 1, jsEmptyString(vm), JSC::makeSource("createOnRejectedFunction"_s, JSC::SourceOrigin(), JSC::SourceTaintedOrigin::Untainted));
}

template<typename JSWrapper, typename IteratorTraits>
JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES JSDOMAsyncIteratorPrototype<JSWrapper, IteratorTraits>::next(JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame)
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto iterator = JSC::jsDynamicCast<JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>*>(callFrame->thisValue());
    if (!iterator)
        return throwVMTypeError(globalObject, scope, "Cannot call next() on a non-Iterator object"_s);

    return JSC::JSValue::encode(iterator->next(*globalObject));
}

template<typename JSWrapper, typename IteratorTraits>
JSC::JSPromise* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::runReturnSteps(JSC::JSGlobalObject& globalObject, JSC::JSValue value)
{
    JSC::VM& vm = globalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto returnPromiseCapability = JSC::JSPromise::createNewPromiseCapability(&globalObject, globalObject.promiseConstructor());
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto* returnPromise = jsDynamicCast<JSC::JSPromise*>(returnPromiseCapability.get(&globalObject, vm.propertyNames->promise));
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (m_isFinished.get()) {
        returnPromise->resolve(&globalObject, vm, JSC::createIteratorResultObject(&globalObject, value, true));
        RETURN_IF_EXCEPTION(scope, nullptr);

        return returnPromise;
    }

    m_isFinished->markAsFinished();

    auto returnResultPromise = getReturnResult(globalObject, value);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto encodedValue = JSC::JSValue::encode(value);
    JSC::ArgList boundArgs { &encodedValue, 1 };
    auto onFulfilled = JSC::JSFunction::create(vm, &globalObject, 0, String(), onPromiseFulFilled, JSC::ImplementationVisibility::Public);
    auto onFulfilledFunction = JSC::JSBoundFunction::create(vm, &globalObject, onFulfilled, this, WTF::move(boundArgs), 1, jsEmptyString(vm), JSC::makeSource("createOnReturnFulfilledFunction"_s, JSC::SourceOrigin(), JSC::SourceTaintedOrigin::Untainted));
    RETURN_IF_EXCEPTION(scope, nullptr);

    returnResultPromise->performPromiseThenExported(vm, &globalObject, onFulfilledFunction, JSC::jsUndefined(), returnPromiseCapability);

    return returnPromise;
}

template<typename JSWrapper, typename IteratorTraits>
JSC::JSPromise* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::getReturnResult(JSC::JSGlobalObject& globalObject, JSC::JSValue value)
{
    ASSERT(m_iterator);
    auto iterator = std::exchange(m_iterator, { });
    return iterator->returnSteps(*JSC::jsCast<JSDOMGlobalObject*>(&globalObject), value)->promise();
}

template<typename JSWrapper, typename IteratorTraits>
JSC::JSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>::returnMethod(JSC::JSGlobalObject& lexicalGlobalObject, JSC::JSValue value)
{
    JSC::VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto returnPromiseCapability = JSC::JSPromise::createNewPromiseCapability(&lexicalGlobalObject, lexicalGlobalObject.promiseConstructor());
    RETURN_IF_EXCEPTION(scope, { });

    auto* returnPromise = jsDynamicCast<JSC::JSPromise*>(returnPromiseCapability.get(&lexicalGlobalObject, vm.propertyNames->promise));
    RETURN_IF_EXCEPTION(scope, { });

    if (m_ongoingPromise) {
        auto afterOngoingPromiseCapability = JSC::JSPromise::createNewPromiseCapability(&lexicalGlobalObject, lexicalGlobalObject.promiseConstructor());
        RETURN_IF_EXCEPTION(scope, { });

        auto* promise = jsDynamicCast<JSC::JSPromise*>(afterOngoingPromiseCapability.get(&lexicalGlobalObject, vm.propertyNames->promise));
        RETURN_IF_EXCEPTION(scope, { });

        auto returnStepsValue = JSC::JSValue::encode(value);
        auto onSettled = createOnSettledFunction(&lexicalGlobalObject, &returnStepsValue);
        RETURN_IF_EXCEPTION(scope, { });

        auto* ongoingPromise = m_ongoingPromise->promise();
        ongoingPromise->performPromiseThenExported(vm, &lexicalGlobalObject, onSettled, onSettled, afterOngoingPromiseCapability);

        m_ongoingPromise = DOMPromise::create(*this->realm(), *promise);
    } else {
        auto* promise = runReturnSteps(lexicalGlobalObject, value);
        RETURN_IF_EXCEPTION(scope, { });

        m_ongoingPromise = DOMPromise::create(*this->realm(), *promise);
        return m_ongoingPromise->promise();
    }

    auto encodedValue = JSC::JSValue::encode(value);
    JSC::ArgList boundArgs { &encodedValue, 1 };
    auto onFulfilled = JSC::JSFunction::create(vm, &lexicalGlobalObject, 0, String(), onPromiseFulFilled, JSC::ImplementationVisibility::Public);
    auto onFulfilledFunction = JSC::JSBoundFunction::create(vm, &lexicalGlobalObject, onFulfilled, this, WTF::move(boundArgs), 1, jsEmptyString(vm), JSC::makeSource("createOnReturnFulfilledFunction"_s, JSC::SourceOrigin(), JSC::SourceTaintedOrigin::Untainted));

    m_ongoingPromise->promise()->performPromiseThenExported(vm, &lexicalGlobalObject, onFulfilledFunction, JSC::jsUndefined(), returnPromiseCapability);

    return returnPromise;
}

template<typename JSWrapper, typename IteratorTraits>
JSC::EncodedJSValue JSDOMAsyncIteratorPrototype<JSWrapper, IteratorTraits>::returnMethod(JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame)
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto iterator = JSC::jsDynamicCast<JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits>*>(callFrame->thisValue());
    if (!iterator) {
        auto returnPromiseCapability = JSC::JSPromise::createNewPromiseCapability(globalObject, globalObject->promiseConstructor());
        RETURN_IF_EXCEPTION(scope, { });

        auto* promise = jsDynamicCast<JSC::JSPromise*>(returnPromiseCapability.get(globalObject, vm.propertyNames->promise));
        RETURN_IF_EXCEPTION(scope, { });

        auto deferred = DeferredPromise::create(*JSC::jsCast<JSDOMGlobalObject*>(globalObject), *promise);
        rejectPromiseWithThisTypeError(deferred, JSWrapper::info()->className, "return");
        return JSC::JSValue::encode(promise);
    }

    return JSC::JSValue::encode(iterator->returnMethod(*globalObject, callFrame->argument(0)));
}

template<typename JSWrapper, typename IteratorTraits>
void JSDOMAsyncIteratorPrototype<JSWrapper, IteratorTraits>::finishCreation(JSC::VM& vm, JSC::JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->next, next, 0, 0, JSC::ImplementationVisibility::Public);
    addReturnMethodIfNeeded<typename DOMWrapped::Iterator>(vm, globalObject);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

template<typename JSWrapper, typename IteratorTraits>
template<HasAsyncIteratorReturn Iterator>
void JSDOMAsyncIteratorPrototype<JSWrapper, IteratorTraits>::addReturnMethodIfNeeded(JSC::VM& vm, JSC::JSGlobalObject* globalObject)
{
    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->returnKeyword, returnMethod, 0, 1, JSC::ImplementationVisibility::Public);
}

}
