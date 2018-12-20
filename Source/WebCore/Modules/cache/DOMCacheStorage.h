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

#include "CacheStorageConnection.h"
#include "DOMCache.h"
#include "FetchRequest.h"
#include <wtf/Forward.h>

namespace WebCore {

class DOMCacheStorage : public RefCounted<DOMCacheStorage>, public ActiveDOMObject {
public:
    static Ref<DOMCacheStorage> create(ScriptExecutionContext& context, Ref<CacheStorageConnection>&& connection) { return adoptRef(*new DOMCacheStorage(context, WTFMove(connection))); }

    using KeysPromise = DOMPromiseDeferred<IDLSequence<IDLDOMString>>;

    void match(DOMCache::RequestInfo&&, CacheQueryOptions&&, Ref<DeferredPromise>&&);
    void has(const String&, DOMPromiseDeferred<IDLBoolean>&&);
    void open(const String&, DOMPromiseDeferred<IDLInterface<DOMCache>>&&);
    void remove(const String&, DOMPromiseDeferred<IDLBoolean>&&);
    void keys(KeysPromise&&);

private:
    DOMCacheStorage(ScriptExecutionContext&, Ref<CacheStorageConnection>&&);

    // ActiveDOMObject
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    void doOpen(const String& name, DOMPromiseDeferred<IDLInterface<DOMCache>>&&);
    void doRemove(const String&, DOMPromiseDeferred<IDLBoolean>&&);
    void retrieveCaches(WTF::Function<void(Optional<Exception>&&)>&&);
    Ref<DOMCache> findCacheOrCreate(DOMCacheEngine::CacheInfo&&);
    Optional<ClientOrigin> origin() const;

    Vector<Ref<DOMCache>> m_caches;
    uint64_t m_updateCounter { 0 };
    Ref<CacheStorageConnection> m_connection;
    bool m_isStopped { false };
};

} // namespace WebCore
