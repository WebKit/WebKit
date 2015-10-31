/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#ifndef MemoryObjectStore_h
#define MemoryObjectStore_h

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyData.h"
#include "IDBObjectStoreInfo.h"
#include "ThreadSafeDataBuffer.h"
#include <set>
#include <wtf/HashMap.h>

namespace WebCore {

class IDBKeyData;

struct IDBKeyRangeData;

namespace IDBServer {

class MemoryBackingStoreTransaction;

typedef HashMap<IDBKeyData, ThreadSafeDataBuffer, IDBKeyDataHash, IDBKeyDataHashTraits> KeyValueMap;

class MemoryObjectStore {
    friend std::unique_ptr<MemoryObjectStore> std::make_unique<MemoryObjectStore>(const WebCore::IDBObjectStoreInfo&);
public:
    static std::unique_ptr<MemoryObjectStore> create(const IDBObjectStoreInfo&);

    ~MemoryObjectStore();

    void writeTransactionStarted(MemoryBackingStoreTransaction&);
    void writeTransactionFinished(MemoryBackingStoreTransaction&);

    bool containsRecord(const IDBKeyData&);
    void deleteRecord(const IDBKeyData&);
    void putRecord(MemoryBackingStoreTransaction&, const IDBKeyData&, const ThreadSafeDataBuffer& value);

    void setKeyValue(const IDBKeyData&, const ThreadSafeDataBuffer& value);

    uint64_t currentKeyGeneratorValue() const { return m_keyGeneratorValue; }
    void setKeyGeneratorValue(uint64_t value) { m_keyGeneratorValue = value; }

    void clear();
    void replaceKeyValueStore(std::unique_ptr<KeyValueMap>&&);

    ThreadSafeDataBuffer valueForKeyRange(const IDBKeyRangeData&) const;

    const IDBObjectStoreInfo& info() const { return m_info; }

private:
    MemoryObjectStore(const IDBObjectStoreInfo&);

    IDBObjectStoreInfo m_info;

    MemoryBackingStoreTransaction* m_writeTransaction { nullptr };
    uint64_t m_keyGeneratorValue { 1 };

    std::unique_ptr<KeyValueMap> m_keyValueStore;
    std::unique_ptr<std::set<IDBKeyData>> m_orderedKeys;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
#endif // MemoryObjectStore_h
