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
#include <pal/SessionID.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class CacheStorageProvider : public RefCounted<CacheStorageProvider> {
public:
    class DummyCacheStorageConnection final : public WebCore::CacheStorageConnection {
    public:
        static Ref<DummyCacheStorageConnection> create(PAL::SessionID sessionID) { return adoptRef(*new DummyCacheStorageConnection(sessionID)); }

    private:
        explicit DummyCacheStorageConnection(PAL::SessionID sessionID)
            : m_sessionID(sessionID)
        {
        }

        void open(const ClientOrigin&, const String&, DOMCacheEngine::CacheIdentifierCallback&&) final { }
        void remove(uint64_t, DOMCacheEngine::CacheIdentifierCallback&&) final { }
        void retrieveCaches(const ClientOrigin&, uint64_t, DOMCacheEngine::CacheInfosCallback&&) final { }
        void retrieveRecords(uint64_t, const URL&, DOMCacheEngine::RecordsCallback&&) final { }
        void batchDeleteOperation(uint64_t, const ResourceRequest&, CacheQueryOptions&&, DOMCacheEngine::RecordIdentifiersCallback&&) final { }
        void batchPutOperation(uint64_t, Vector<DOMCacheEngine::Record>&&, DOMCacheEngine::RecordIdentifiersCallback&&) final { }
        void reference(uint64_t) final { }
        void dereference(uint64_t) final { }
        PAL::SessionID sessionID() const final { return m_sessionID; }

        PAL::SessionID m_sessionID;
    };

    static Ref<CacheStorageProvider> create() { return adoptRef(*new CacheStorageProvider); }
    virtual Ref<CacheStorageConnection> createCacheStorageConnection(PAL::SessionID sessionID) { return DummyCacheStorageConnection::create(sessionID); }
    virtual ~CacheStorageProvider() { };

protected:
    CacheStorageProvider() = default;
};

}
