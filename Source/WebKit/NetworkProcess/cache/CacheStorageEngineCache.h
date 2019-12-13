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

#include "NetworkCacheStorage.h"
#include <WebCore/DOMCacheEngine.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

namespace CacheStorage {

class Caches;

struct RecordInformation {
    NetworkCache::Key key;
    double insertionTime { 0 };

    uint64_t identifier { 0 };
    uint64_t updateResponseCounter { 0 };

    uint64_t size { 0 };

    URL url;
    bool hasVaryStar { false };
    HashMap<String, String> varyHeaders;
};

class AsynchronousPutTaskCounter;
class ReadRecordTaskCounter;

class Cache {
public:
    enum class State { Uninitialized, Opening, Open };
    Cache(Caches&, uint64_t identifier, State, String&& name, String&& uniqueName);

    bool isOpened() const { return m_state == State::Open; }
    void open(WebCore::DOMCacheEngine::CompletionCallback&&);

    uint64_t identifier() const { return m_identifier; }
    const String& name() const { return m_name; }
    const String& uniqueName() const { return m_uniqueName; }
    bool isActive() const { return m_state != State::Uninitialized; }

    void retrieveRecords(const URL&, WebCore::DOMCacheEngine::RecordsCallback&&);
    WebCore::DOMCacheEngine::CacheInfo info() const { return { m_identifier, m_name }; }

    void put(Vector<WebCore::DOMCacheEngine::Record>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);
    void remove(WebCore::ResourceRequest&&, WebCore::CacheQueryOptions&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);

    Vector<NetworkCache::Key> keys() const;

    void dispose();
    void clearMemoryRepresentation();

    static Optional<WebCore::DOMCacheEngine::Record> decode(const NetworkCache::Storage::Record&);
    static NetworkCache::Storage::Record encode(const RecordInformation&, const WebCore::DOMCacheEngine::Record&);

    struct DecodedRecord {
        DecodedRecord(double insertionTime, uint64_t size, WebCore::DOMCacheEngine::Record&& record)
            : insertionTime(insertionTime)
            , size(size)
            , record(WTFMove(record))
        { }

        double insertionTime { 0 };
        uint64_t size { 0 };
        WebCore::DOMCacheEngine::Record record;
    };
    static Optional<DecodedRecord> decodeRecordHeader(const NetworkCache::Storage::Record&);

    bool hasPendingOpeningCallbacks() const { return !m_pendingOpeningCallbacks.isEmpty(); }

private:
    Vector<RecordInformation>* recordsFromURL(const URL&);
    const Vector<RecordInformation>* recordsFromURL(const URL&) const;
    RecordInformation& addRecord(Vector<RecordInformation>*, const WebCore::DOMCacheEngine::Record&);

    void storeRecords(Vector<WebCore::DOMCacheEngine::Record>&&, WebCore::DOMCacheEngine::RecordIdentifiersCallback&&);

    RecordInformation toRecordInformation(const WebCore::DOMCacheEngine::Record&);

    void finishOpening(WebCore::DOMCacheEngine::CompletionCallback&&, Optional<WebCore::DOMCacheEngine::Error>&&);
    void retrieveRecord(const RecordInformation&, Ref<ReadRecordTaskCounter>&&);

    void readRecordsList(WebCore::DOMCacheEngine::CompletionCallback&&);
    void writeRecordToDisk(const RecordInformation&, WebCore::DOMCacheEngine::Record&&, Ref<AsynchronousPutTaskCounter>&&, uint64_t previousRecordSize);
    void updateRecordToDisk(RecordInformation&, WebCore::DOMCacheEngine::Record&&, Ref<AsynchronousPutTaskCounter>&&);
    void removeRecordFromDisk(const RecordInformation&);
    void readRecordFromDisk(const RecordInformation&, WTF::Function<void(Expected<WebCore::DOMCacheEngine::Record, WebCore::DOMCacheEngine::Error>&&)>&&);
    void removeFromRecordList(const Vector<uint64_t>&);

    Caches& m_caches;
    State m_state;
    uint64_t m_identifier { 0 };
    String m_name;
    String m_uniqueName;
    HashMap<String, Vector<RecordInformation>> m_records;
    uint64_t m_nextRecordIdentifier { 0 };
    Vector<WebCore::DOMCacheEngine::CompletionCallback> m_pendingOpeningCallbacks;
};

} // namespace CacheStorage

} // namespace WebKit
