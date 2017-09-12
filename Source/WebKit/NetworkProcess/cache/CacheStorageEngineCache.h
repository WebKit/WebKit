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

#include "NetworkCacheKey.h"
#include <WebCore/DOMCacheEngine.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

namespace CacheStorage {

class Caches;

struct RecordData {
    NetworkCache::Key key;
    double insertionTime { 0 };

    uint64_t identifier { 0 };
    uint64_t updateResponseCounter { 0 };

    WebCore::URL url;
    bool hasVaryStar { false };
    HashMap<String, String> varyHeaders;

    struct Data {
        WebCore::FetchHeaders::Guard requestHeadersGuard;
        WebCore::ResourceRequest request;
        WebCore::FetchOptions options;
        String referrer;

        WebCore::FetchHeaders::Guard responseHeadersGuard;
        WebCore::ResourceResponse response;
        WebCore::DOMCacheEngine::ResponseBody responseBody;
    };

    std::optional<Data> data;
};

class Cache {
public:
    enum class State { Uninitialized, Opening, Open };
    Cache(Caches&, uint64_t identifier, State, String&& name);

    void open(WebCore::DOMCacheEngine::CompletionCallback&&);

    uint64_t identifier() const { return m_identifier; }
    const String& name() const { return m_name; }
    bool isActive() const { return m_state != State::Uninitialized; }

    Vector<WebCore::DOMCacheEngine::Record> retrieveRecords(const WebCore::URL&) const;
    WebCore::DOMCacheEngine::CacheInfo info() const { return { m_identifier, m_name }; }

    void put(Vector<WebCore::DOMCacheEngine::Record>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void remove(WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);

    void dispose();
    void clearMemoryRepresentation();
 
private:
    Vector<RecordData>* recordsFromURL(const WebCore::URL&);
    const Vector<RecordData>* recordsFromURL(const WebCore::URL&) const;
    RecordData& addRecord(Vector<RecordData>*, WebCore::DOMCacheEngine::Record&&);

    void finishOpening(WebCore::DOMCacheEngine::CompletionCallback&&, std::optional<WebCore::DOMCacheEngine::Error>&&);

    void readRecordsList(WebCore::DOMCacheEngine::CompletionCallback&&);
    void writeRecordsList(WebCore::DOMCacheEngine::CompletionCallback&&);
    void writeRecordToDisk(RecordData&);
    void removeRecordFromDisk(RecordData&);

    Caches& m_caches;
    State m_state;
    uint64_t m_identifier { 0 };
    String m_name;
    HashMap<String, Vector<RecordData>> m_records;
    uint64_t m_nextRecordIdentifier { 0 };
    Vector<WebCore::DOMCacheEngine::CompletionCallback> m_pendingOpeningCallbacks;
};

} // namespace CacheStorage

} // namespace WebKit
