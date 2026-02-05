/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "ReadableStreamReadRequest.h"

#include "JSDOMExceptionHandling.h"
#include "JSDOMGuardedObject.h"
#include "JSDOMPromiseDeferred.h"
#include "JSReadableStreamReadResult.h"
#include <JavaScriptCore/TopExceptionScope.h>

namespace WebCore {

class ReadableStreamDefaultReadRequest : public ReadableStreamReadRequest {
public:
    static Ref<ReadableStreamDefaultReadRequest> create(Ref<DeferredPromise>&& promise) { return adoptRef(*new ReadableStreamDefaultReadRequest(WTF::move(promise))); }

private:
    explicit ReadableStreamDefaultReadRequest(Ref<DeferredPromise>&& promise)
        : m_promise(WTF::move(promise))
    {
    }

    void runChunkSteps(JSC::JSValue value) final
    {
        m_promise->resolve<IDLDictionary<ReadableStreamReadResult>>(ReadableStreamReadResult { value, false });
    }

    void runCloseSteps() final
    {
        m_promise->resolve<IDLDictionary<ReadableStreamReadResult>>(ReadableStreamReadResult { JSC::jsUndefined(), true });
    }

    void runErrorSteps(JSC::JSValue value) final
    {
        m_promise->rejectWithCallback([&](auto&) {
            return value;
        });
    }

    void runErrorSteps(Exception&& exception) final
    {
        m_promise->reject(WTF::move(exception));
    }

    JSDOMGlobalObject* globalObject() final
    {
        return m_promise->globalObject();
    }

    const Ref<DeferredPromise> m_promise;
};

class ReadableStreamDefaultReadIntoRequest : public ReadableStreamReadIntoRequest {
public:
    static Ref<ReadableStreamDefaultReadIntoRequest> create(Ref<DeferredPromise>&& promise) { return adoptRef(*new ReadableStreamDefaultReadIntoRequest(WTF::move(promise))); }

private:
    explicit ReadableStreamDefaultReadIntoRequest(Ref<DeferredPromise>&& promise)
        : m_promise(WTF::move(promise))
    {
    }

    void runChunkSteps(JSC::JSValue value) final
    {
        m_promise->resolve<IDLDictionary<ReadableStreamReadResult>>(ReadableStreamReadResult { value, false });
    }

    void runCloseSteps(JSC::JSValue value) final
    {
        m_promise->resolve<IDLDictionary<ReadableStreamReadResult>>(ReadableStreamReadResult { value, true });
    }

    void runErrorSteps(JSC::JSValue value) final
    {
        m_promise->rejectWithCallback([&](auto&) {
            return value;
        });
    }

    void runErrorSteps(Exception&& exception) final
    {
        m_promise->reject(WTF::move(exception));
    }

    JSDOMGlobalObject* globalObject() final
    {
        return m_promise->globalObject();
    }

    const Ref<DeferredPromise> m_promise;
};

void ReadableStreamReadRequestBase::runErrorSteps(Exception&& exception)
{
    auto* globalObject = this->globalObject();
    if (!globalObject)
        return;

    Ref vm = globalObject->vm();
    JSC::JSLockHolder locker(vm);
    auto scope = DECLARE_TOP_EXCEPTION_SCOPE(vm);
    auto jsException = createDOMException(*globalObject, WTF::move(exception));
    if (scope.exception()) [[unlikely]] {
        scope.clearException();
        return;
    }
    runErrorSteps(jsException);
}

Ref<ReadableStreamReadRequest> ReadableStreamReadRequest::create(Ref<DeferredPromise>&& promise)
{
    return ReadableStreamDefaultReadRequest::create(WTF::move(promise));
}

Ref<ReadableStreamReadIntoRequest> ReadableStreamReadIntoRequest::create(Ref<DeferredPromise>&& promise)
{
    return ReadableStreamDefaultReadIntoRequest::create(WTF::move(promise));
}

} // namespace WebCore
