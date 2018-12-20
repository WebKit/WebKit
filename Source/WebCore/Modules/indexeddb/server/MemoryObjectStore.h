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

#pragma once

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyData.h"
#include "IDBObjectStoreInfo.h"
#include "MemoryIndex.h"
#include "MemoryObjectStoreCursor.h"
#include "ThreadSafeDataBuffer.h"
#include <wtf/HashMap.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class IDBCursorInfo;
class IDBError;
class IDBGetAllResult;
class IDBKeyData;
class IDBValue;

struct IDBKeyRangeData;

namespace IndexedDB {
enum class GetAllType;
enum class IndexRecordType;
}

namespace IDBServer {

class MemoryBackingStoreTransaction;

typedef HashMap<IDBKeyData, ThreadSafeDataBuffer, IDBKeyDataHash, IDBKeyDataHashTraits> KeyValueMap;

class MemoryObjectStore : public RefCounted<MemoryObjectStore> {
public:
    static Ref<MemoryObjectStore> create(const IDBObjectStoreInfo&);

    ~MemoryObjectStore();

    void writeTransactionStarted(MemoryBackingStoreTransaction&);
    void writeTransactionFinished(MemoryBackingStoreTransaction&);
    MemoryBackingStoreTransaction* writeTransaction() { return m_writeTransaction; }

    IDBError createIndex(MemoryBackingStoreTransaction&, const IDBIndexInfo&);
    IDBError deleteIndex(MemoryBackingStoreTransaction&, uint64_t indexIdentifier);
    void deleteAllIndexes(MemoryBackingStoreTransaction&);
    void registerIndex(Ref<MemoryIndex>&&);

    bool containsRecord(const IDBKeyData&);
    void deleteRecord(const IDBKeyData&);
    void deleteRange(const IDBKeyRangeData&);
    IDBError addRecord(MemoryBackingStoreTransaction&, const IDBKeyData&, const IDBValue&);

    uint64_t currentKeyGeneratorValue() const { return m_keyGeneratorValue; }
    void setKeyGeneratorValue(uint64_t value) { m_keyGeneratorValue = value; }

    void clear();
    void replaceKeyValueStore(std::unique_ptr<KeyValueMap>&&, std::unique_ptr<IDBKeyDataSet>&&);

    ThreadSafeDataBuffer valueForKey(const IDBKeyData&) const;
    ThreadSafeDataBuffer valueForKeyRange(const IDBKeyRangeData&) const;
    IDBKeyData lowestKeyWithRecordInRange(const IDBKeyRangeData&) const;
    IDBGetResult indexValueForKeyRange(uint64_t indexIdentifier, IndexedDB::IndexRecordType, const IDBKeyRangeData&) const;
    uint64_t countForKeyRange(uint64_t indexIdentifier, const IDBKeyRangeData&) const;

    void getAllRecords(const IDBKeyRangeData&, Optional<uint32_t> count, IndexedDB::GetAllType, IDBGetAllResult&) const;

    const IDBObjectStoreInfo& info() const { return m_info; }

    MemoryObjectStoreCursor* maybeOpenCursor(const IDBCursorInfo&);

    IDBKeyDataSet* orderedKeys() { return m_orderedKeys.get(); }

    MemoryIndex* indexForIdentifier(uint64_t);

    void maybeRestoreDeletedIndex(Ref<MemoryIndex>&&);

    void rename(const String& newName) { m_info.rename(newName); }
    void renameIndex(MemoryIndex&, const String& newName);

private:
    MemoryObjectStore(const IDBObjectStoreInfo&);

    IDBKeyDataSet::iterator lowestIteratorInRange(const IDBKeyRangeData&, bool reverse) const;

    IDBError populateIndexWithExistingRecords(MemoryIndex&);
    IDBError updateIndexesForPutRecord(const IDBKeyData&, const ThreadSafeDataBuffer& value);
    void updateIndexesForDeleteRecord(const IDBKeyData& value);
    void updateCursorsForPutRecord(IDBKeyDataSet::iterator);
    void updateCursorsForDeleteRecord(const IDBKeyData&);

    RefPtr<MemoryIndex> takeIndexByIdentifier(uint64_t indexIdentifier);

    IDBObjectStoreInfo m_info;

    MemoryBackingStoreTransaction* m_writeTransaction { nullptr };
    uint64_t m_keyGeneratorValue { 1 };

    std::unique_ptr<KeyValueMap> m_keyValueStore;
    std::unique_ptr<IDBKeyDataSet> m_orderedKeys;

    void unregisterIndex(MemoryIndex&);
    HashMap<uint64_t, RefPtr<MemoryIndex>> m_indexesByIdentifier;
    HashMap<String, RefPtr<MemoryIndex>> m_indexesByName;
    HashMap<IDBResourceIdentifier, std::unique_ptr<MemoryObjectStoreCursor>> m_cursors;
};

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
