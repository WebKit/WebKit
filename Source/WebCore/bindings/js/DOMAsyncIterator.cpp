/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "DOMAsyncIterator.h"

#include "JSDOMGlobalObjectInlines.h"
#include "JSDOMPromise.h"
#include <JavaScriptCore/IteratorOperations.h>
#include <JavaScriptCore/JSPromise.h>
#include <JavaScriptCore/TopExceptionScope.h>

namespace WebCore {

ExceptionOr<Ref<DOMAsyncIterator>> DOMAsyncIterator::create(JSDOMGlobalObject& globalObject, JSC::JSValue iterable)
{
    Ref vm = globalObject.vm();
    auto throwScope = DECLARE_THROW_SCOPE(vm);
    auto iteratorRecord = JSC::getAsyncIteratorExported(globalObject, iterable);
    RETURN_IF_EXCEPTION(throwScope, Exception { ExceptionCode::ExistingExceptionError });

    auto* iteratorObject = iteratorRecord.iterator.getObject();
    if (!iteratorObject)
        return Exception { ExceptionCode::TypeError, "iterator should be an object"_s };
    if (!iteratorRecord.nextMethod.isCell())
        return Exception { ExceptionCode::TypeError, "iterator next should be callable"_s };

    return adoptRef(*new DOMAsyncIterator(globalObject, *iteratorObject, *iteratorRecord.nextMethod.asCell()));
}

void DOMAsyncIterator::handleCallbackWithPromise(JSDOMGlobalObject& globalObject, Callback&& callback, JSC::JSPromise& promise)
{
    bool shouldStoreCallback = DOMPromise::whenPromiseIsSettled(&globalObject, &promise, [weakThis = WeakPtr { *this }](auto* globalObject, bool isOK, auto value) {
        if (RefPtr protectedThis = weakThis.get())
            std::exchange(protectedThis->m_callback, { })(globalObject, isOK, value);
    }) == DOMPromise::IsCallbackRegistered::Yes;

    if (!shouldStoreCallback) {
        callback(&globalObject, false, { });
        return;
    }

    m_callback = WTF::move(callback);
}

void DOMAsyncIterator::callNext(Callback&& callback)
{
    ASSERT(!m_callback);
    auto* globalObject = this->globalObject();
    if (!globalObject) {
        callback(nullptr, false, { });
        return;
    }

    Ref vm = globalObject->vm();

    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    auto result = JSC::iteratorNextExported(globalObject, { m_iterator->guardedObject(), guardedObject() }, { });
    if (auto* exception = scope.exception()) {
        scope.clearException();
        callback(globalObject, false, exception->value());
        return;
    }

    auto* promise = JSC::JSPromise::resolvedPromise(globalObject, result);
    if (auto* exception = scope.exception()) {
        scope.clearException();
        callback(globalObject, false, exception->value());
        return;
    }

    // FIXME: Is it needed?
    if (!promise) {
        callback(globalObject, false, { });
        return;
    }

    handleCallbackWithPromise(*globalObject, WTF::move(callback), *promise);
}

void DOMAsyncIterator::callReturn(JSC::JSValue reason, Callback&& callback)
{
    ASSERT(!m_callback);
    auto* globalObject = this->globalObject();
    if (!globalObject) {
        callback(nullptr, false, { });
        return;
    }

    Ref vm = globalObject->vm();

    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    JSC::CallData callData;
    auto returnMethod = m_iterator->object()->getMethod(globalObject, callData, vm->propertyNames->returnKeyword, "return property should be callable"_s);
    if (auto* exception = scope.exception()) {
        scope.clearException();
        callback(globalObject, false, exception->value());
        return;
    }

    if (!returnMethod || returnMethod.isUndefined()) {
        // FIXME: We should queue a microtask to call the callback.
        callback(globalObject, true, { });
        return;
    }

    JSC::MarkedArgumentBuffer arguments;
    arguments.append(reason);

    callData = JSC::getCallData(returnMethod);
    auto result = JSC::call(globalObject, returnMethod, callData, m_iterator->guardedObject(), arguments);
    if (auto* exception = scope.exception()) {
        scope.clearException();
        callback(globalObject, false, exception->value());
        return;
    }

    auto* promise = JSC::JSPromise::resolvedPromise(globalObject, result);
    if (auto* exception = scope.exception()) {
        scope.clearException();
        callback(globalObject, false, exception->value());
        return;
    }

    // FIXME: Is it needed?
    if (!promise) {
        callback(globalObject, false, { });
        return;
    }

    handleCallbackWithPromise(*globalObject, WTF::move(callback), *promise);
}

}
