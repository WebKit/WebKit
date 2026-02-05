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

#pragma once

#include "ExceptionOr.h"
#include "IDLTypes.h"
#include "JSValueInWrappedObject.h"
#include "ScriptWrappable.h"
#include "WebCoreOpaqueRoot.h"
#include <wtf/Deque.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/UniqueRef.h>

namespace WebCore {

class DOMPromise;
class DeferredPromise;
class ReadableStream;
class ReadableStreamReadIntoRequest;

template<typename IDLType> class DOMPromiseProxy;

class ReadableStreamBYOBReader : public ScriptWrappable, public RefCountedAndCanMakeWeakPtr<ReadableStreamBYOBReader> {
    WTF_MAKE_TZONE_ALLOCATED(ReadableStreamBYOBReader);
public:
    static ExceptionOr<Ref<ReadableStreamBYOBReader>> create(JSDOMGlobalObject&, ReadableStream&);
    ~ReadableStreamBYOBReader();

    struct ReadOptions {
        uint64_t min { 1 };
    };

    void readForBindings(JSDOMGlobalObject&, JSC::ArrayBufferView&, ReadOptions, Ref<DeferredPromise>&&);
    void releaseLock(JSDOMGlobalObject&);

    DOMPromise& closedPromise();

    Ref<DOMPromise> cancel(JSDOMGlobalObject&, JSC::JSValue);

    Ref<ReadableStreamReadIntoRequest> takeFirstReadIntoRequest();
    size_t readIntoRequestsSize() const { return m_readIntoRequests.size(); }
    void addReadIntoRequest(Ref<ReadableStreamReadIntoRequest>&&);

    void resolveClosedPromise();
    void rejectClosedPromise(JSC::JSValue);
    void errorReadIntoRequests(JSC::JSValue);

    using ClosedCallback = Function<void(JSDOMGlobalObject&, JSC::JSValue)>;
    void onClosedPromiseRejection(ClosedCallback&&);

    void read(JSDOMGlobalObject&, JSC::ArrayBufferView&, uint64_t, Ref<ReadableStreamReadIntoRequest>&&);

    bool isReachableFromOpaqueRoots() const;
    template<typename Visitor> void visitAdditionalChildren(Visitor&);

private:
    explicit ReadableStreamBYOBReader(Ref<DOMPromise>&&, Ref<DeferredPromise>&&);

    ExceptionOr<void> setupBYOBReader(JSDOMGlobalObject&, ReadableStream&);
    void initialize(JSDOMGlobalObject&, ReadableStream&);
    void genericRelease(JSDOMGlobalObject&);
    void errorReadIntoRequests(Exception&&);

    Ref<DOMPromise> genericCancel(JSDOMGlobalObject&, JSC::JSValue);

    Ref<DOMPromise> m_closedPromise;
    Ref<DeferredPromise> m_closedDeferred;
    RefPtr<ReadableStream> m_stream;
    Deque<Ref<ReadableStreamReadIntoRequest>> m_readIntoRequests;

    ClosedCallback m_closedCallback;
};

WebCoreOpaqueRoot root(ReadableStreamBYOBReader*);

} // namespace WebCore
