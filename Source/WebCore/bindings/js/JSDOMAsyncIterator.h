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
#include <JavaScriptCore/PropertySlot.h>
#include <type_traits>
#include <wtf/CompletionHandler.h>

namespace WebCore {

// https://webidl.spec.whatwg.org/#es-asynchronous-iterator-prototype-object
template<typename JSWrapper, typename IteratorTraits, typename JSIterator> class JSDOMAsyncIteratorPrototype final : public JSC::JSNonFinalObject {
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

private:
    JSDOMAsyncIteratorPrototype(JSC::VM& vm, JSC::Structure* structure)
        : Base(vm, structure)
    {
    }

    void finishCreation(JSC::VM&, JSC::JSGlobalObject*);
};

template<typename JSWrapper, typename IteratorTraits, typename JSIterator> class JSDOMAsyncIteratorBase : public JSDOMObject {
public:
    using Base = JSDOMObject;

    using Wrapper = JSWrapper;
    using Traits = IteratorTraits;

    using DOMWrapped = typename Wrapper::DOMWrapped;
    using Prototype = JSDOMAsyncIteratorPrototype<Wrapper, Traits, JSIterator>;

    DECLARE_INFO;

    static Prototype* createPrototype(JSC::VM& vm, JSC::JSGlobalObject& globalObject)
    {
        return Prototype::create(vm, &globalObject, Prototype::createStructure(vm, &globalObject, globalObject.asyncIteratorPrototype()));
    }

    static void createStructure(JSC::VM&, JSC::JSGlobalObject*, JSC::JSValue); // Make use of createStructure for this compile-error.
    
    JSC::JSValue next(JSC::JSGlobalObject&);
    JSC::JSPromise* runNextSteps(JSC::JSGlobalObject&);
    JSC::JSPromise* getNextIterationResult(JSC::JSGlobalObject&);
    JSC::EncodedJSValue settle(JSC::JSGlobalObject*);
    JSC::EncodedJSValue fulfill(JSC::JSGlobalObject*, JSC::JSValue);
    JSC::EncodedJSValue reject(JSC::JSGlobalObject*, JSC::JSValue);

protected:
    JSDOMAsyncIteratorBase(JSC::Structure* structure, JSWrapper& iteratedObject, IterationKind kind)
        : Base(structure, *iteratedObject.globalObject())
        , m_iterator(iteratedObject.wrapped().createIterator(iteratedObject.globalObject()->scriptExecutionContext()))
        , m_kind(kind)
    {
    }

    static void destroy(JSC::JSCell*);

    RefPtr<typename DOMWrapped::Iterator> m_iterator;
    IterationKind m_kind;
    RefPtr<DOMPromise> m_ongoingPromise;
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

template<typename JSWrapper, typename IteratorTraits, typename JSIterator>
void JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>::destroy(JSCell* cell)
{
    JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>* thisObject = static_cast<JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>*>(cell);
    thisObject->JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>::~JSDOMAsyncIteratorBase();
}

template<typename JSWrapper, typename IteratorTraits, typename JSIterator>
JSC::JSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>::next(JSC::JSGlobalObject& lexicalGlobalObject)
{
    JSC::VM& vm = lexicalGlobalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    
    if (!m_ongoingPromise || !m_ongoingPromise->promise()) {
        auto* promise = runNextSteps(lexicalGlobalObject);
        RETURN_IF_EXCEPTION(scope, { });

        m_ongoingPromise = DOMPromise::create(*this->globalObject(), *promise);
        return m_ongoingPromise->promise();
    }

    auto afterOngoingPromiseCapability = JSC::JSPromise::createNewPromiseCapability(&lexicalGlobalObject, lexicalGlobalObject.promiseConstructor());
    RETURN_IF_EXCEPTION(scope, { });
    
    auto data = JSC::JSPromise::convertCapabilityToDeferredData(&lexicalGlobalObject, afterOngoingPromiseCapability);
    RETURN_IF_EXCEPTION(scope, { });

    auto* castedThis = JSC::jsDynamicCast<JSIterator*>(this);
    RETURN_IF_EXCEPTION(scope, { });

    auto onSettled = castedThis->createOnSettledFunction(&lexicalGlobalObject);
    RETURN_IF_EXCEPTION(scope, { });

    auto* ongoingPromise = m_ongoingPromise->promise();
    ongoingPromise->performPromiseThen(&lexicalGlobalObject, onSettled, onSettled, afterOngoingPromiseCapability);
    RETURN_IF_EXCEPTION(scope, { });

    m_ongoingPromise = DOMPromise::create(*this->globalObject(), *data.promise);
    return m_ongoingPromise->promise();
}

template<typename JSWrapper, typename IteratorTraits, typename JSIterator>
JSC::JSPromise* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>::runNextSteps(JSC::JSGlobalObject& globalObject)
{
    JSC::VM& vm = globalObject.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto nextPromiseCapability = JSC::JSPromise::createNewPromiseCapability(&globalObject, globalObject.promiseConstructor());
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto data = JSC::JSPromise::convertCapabilityToDeferredData(&globalObject, nextPromiseCapability);
    RETURN_IF_EXCEPTION(scope, nullptr);

    if (!m_iterator) {
        data.promise->resolve(&globalObject, JSC::createIteratorResultObject(&globalObject, JSC::jsUndefined(), true));
        RETURN_IF_EXCEPTION(scope, nullptr);

        return data.promise;
    }

    auto nextPromise = getNextIterationResult(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto* castedThis = JSC::jsDynamicCast<JSIterator*>(this);
    RETURN_IF_EXCEPTION(scope, { });

    auto onFulfilled = castedThis->createOnFulfilledFunction(&globalObject);
    auto onRejected = castedThis->createOnRejectedFunction(&globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    nextPromise->performPromiseThen(&globalObject, onFulfilled, onRejected, nextPromiseCapability);
    RETURN_IF_EXCEPTION(scope, nullptr);

    return data.promise;
}

template<typename JSWrapper, typename IteratorTraits, typename JSIterator>
JSC::JSPromise* JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>::getNextIterationResult(JSC::JSGlobalObject& globalObject)
{
    JSC::VM& vm = JSC::getVM(&globalObject);

    auto* promise = JSC::JSPromise::create(vm, globalObject.promiseStructure());
    auto deferred = DeferredPromise::create(*this->globalObject(), *promise);
    if (!m_iterator) {
        deferred->resolve();
        return promise;
    }

    m_iterator->next([deferred = WTFMove(deferred), traits = IteratorTraits { }, kind = m_kind](auto result) mutable {
        auto* globalObject = deferred->globalObject();
        if (!globalObject)
            return;

        if (result.hasException())
            return deferred->reject(result.releaseException());

        auto resultValue = result.releaseReturnValue();
        if (!resultValue)
            return deferred->resolve();

        deferred->resolveWithJSValue(convertToJS(*globalObject, *globalObject, *resultValue, traits, kind));
    });

    return promise;
}

template<typename JSWrapper, typename IteratorTraits, typename JSIterator>
JSC::EncodedJSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>::settle(JSC::JSGlobalObject* globalObject)
{
    return JSC::JSValue::encode(runNextSteps(*globalObject));
}

template<typename JSWrapper, typename IteratorTraits, typename JSIterator>
JSC::EncodedJSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>::fulfill(JSC::JSGlobalObject* globalObject, JSC::JSValue result)
{
    m_ongoingPromise = nullptr;
    if (result.isUndefined()) {
        m_iterator = nullptr;
        return JSC::JSValue::encode(JSC::createIteratorResultObject(globalObject, result, true));
    }

    return JSC::JSValue::encode(JSC::createIteratorResultObject(globalObject, result, false));
}

template<typename JSWrapper, typename IteratorTraits, typename JSIterator>
JSC::EncodedJSValue JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>::reject(JSC::JSGlobalObject* globalObject, JSC::JSValue reason)
{
    m_ongoingPromise = nullptr;
    m_iterator = nullptr;

    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    return throwVMError(globalObject, scope, reason);
}

template<typename JSWrapper, typename IteratorTraits, typename JSIterator>
JSC::EncodedJSValue JSC_HOST_CALL_ATTRIBUTES JSDOMAsyncIteratorPrototype<JSWrapper, IteratorTraits, JSIterator>::next(JSC::JSGlobalObject* globalObject, JSC::CallFrame* callFrame)
{
    JSC::VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto iterator = JSC::jsDynamicCast<JSDOMAsyncIteratorBase<JSWrapper, IteratorTraits, JSIterator>*>(callFrame->thisValue());
    if (!iterator)
        return throwVMTypeError(globalObject, scope, "Cannot call next() on a non-Iterator object"_s);

    return JSC::JSValue::encode(iterator->next(*globalObject));
}

template<typename JSWrapper, typename IteratorTraits, typename JSIterator>
void JSDOMAsyncIteratorPrototype<JSWrapper, IteratorTraits, JSIterator>::finishCreation(JSC::VM& vm, JSC::JSGlobalObject* globalObject)
{
    Base::finishCreation(vm);
    ASSERT(inherits(info()));

    JSC_NATIVE_FUNCTION_WITHOUT_TRANSITION(vm.propertyNames->next, next, 0, 0, JSC::ImplementationVisibility::Public);
    JSC_TO_STRING_TAG_WITHOUT_TRANSITION();
}

}
