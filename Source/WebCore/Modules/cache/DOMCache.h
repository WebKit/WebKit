/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "CacheStorageConnection.h"
#include "CacheStorageRecord.h"

namespace WebCore {

class ScriptExecutionContext;

class DOMCache final : public RefCounted<DOMCache>, public ActiveDOMObject {
public:
    static Ref<DOMCache> create(ScriptExecutionContext& context, String&& name, uint64_t identifier, Ref<CacheStorageConnection>&& connection) { return adoptRef(*new DOMCache(context, WTFMove(name), identifier, WTFMove(connection))); }
    ~DOMCache();

    using RequestInfo = FetchRequest::Info;

    using KeysPromise = DOMPromiseDeferred<IDLSequence<IDLInterface<FetchRequest>>>;

    void match(RequestInfo&&, CacheQueryOptions&&, Ref<DeferredPromise>&&);

    using MatchAllPromise = DOMPromiseDeferred<IDLSequence<IDLInterface<FetchResponse>>>;
    void matchAll(Optional<RequestInfo>&&, CacheQueryOptions&&, MatchAllPromise&&);
    void add(RequestInfo&&, DOMPromiseDeferred<void>&&);

    void addAll(Vector<RequestInfo>&&, DOMPromiseDeferred<void>&&);
    void put(RequestInfo&&, Ref<FetchResponse>&&, DOMPromiseDeferred<void>&&);
    void remove(RequestInfo&&, CacheQueryOptions&&, DOMPromiseDeferred<IDLBoolean>&&);
    void keys(Optional<RequestInfo>&&, CacheQueryOptions&&, KeysPromise&&);

    const String& name() const { return m_name; }
    uint64_t identifier() const { return m_identifier; }

    using MatchCallback = WTF::Function<void(ExceptionOr<FetchResponse*>)>;
    void doMatch(RequestInfo&&, CacheQueryOptions&&, MatchCallback&&);

private:
    DOMCache(ScriptExecutionContext&, String&& name, uint64_t identifier, Ref<CacheStorageConnection>&&);

    ExceptionOr<Ref<FetchRequest>> requestFromInfo(RequestInfo&&, bool ignoreMethod);

    // ActiveDOMObject
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    void putWithResponseData(DOMPromiseDeferred<void>&&, Ref<FetchRequest>&&, Ref<FetchResponse>&&, ExceptionOr<RefPtr<SharedBuffer>>&&);

    void retrieveRecords(const URL&, WTF::Function<void(Optional<Exception>&&)>&&);
    Vector<CacheStorageRecord> queryCacheWithTargetStorage(const FetchRequest&, const CacheQueryOptions&, const Vector<CacheStorageRecord>&);
    void queryCache(Ref<FetchRequest>&&, CacheQueryOptions&&, WTF::Function<void(ExceptionOr<Vector<CacheStorageRecord>>&&)>&&);
    void batchDeleteOperation(const FetchRequest&, CacheQueryOptions&&, WTF::Function<void(ExceptionOr<bool>&&)>&&);
    void batchPutOperation(const FetchRequest&, FetchResponse&, DOMCacheEngine::ResponseBody&&, WTF::Function<void(ExceptionOr<void>&&)>&&);
    void batchPutOperation(Vector<DOMCacheEngine::Record>&&, WTF::Function<void(ExceptionOr<void>&&)>&&);

    void updateRecords(Vector<DOMCacheEngine::Record>&&);
    Vector<Ref<FetchResponse>> cloneResponses(const Vector<CacheStorageRecord>&);
    DOMCacheEngine::Record toConnectionRecord(const FetchRequest&, FetchResponse&, DOMCacheEngine::ResponseBody&&);

    String m_name;
    uint64_t m_identifier;
    Ref<CacheStorageConnection> m_connection;

    Vector<CacheStorageRecord> m_records;
    bool m_isStopped { false };
};

} // namespace WebCore
