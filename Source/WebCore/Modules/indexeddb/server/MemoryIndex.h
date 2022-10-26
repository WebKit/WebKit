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

#include "IDBIndexInfo.h"
#include "IDBResourceIdentifier.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebCore {

class IDBCursorInfo;
class IDBError;
class IDBGetAllResult;
class IDBGetResult;
class IDBKeyData;
class IndexKey;
class ThreadSafeDataBuffer;

struct IDBKeyRangeData;

namespace IndexedDB {
enum class GetAllType : bool;
enum class IndexRecordType : bool;
}

namespace IDBServer {

class IndexValueStore;
class MemoryBackingStoreTransaction;
class MemoryIndexCursor;
class MemoryObjectStore;

class MemoryIndex : public RefCounted<MemoryIndex> {
public:
    static Ref<MemoryIndex> create(const IDBIndexInfo&, MemoryObjectStore&);

    ~MemoryIndex();

    const IDBIndexInfo& info() const { return m_info; }

    void rename(const String& newName) { m_info.rename(newName); }

    IDBGetResult getResultForKeyRange(IndexedDB::IndexRecordType, const IDBKeyRangeData&) const;
    uint64_t countForKeyRange(const IDBKeyRangeData&);
    void getAllRecords(const IDBKeyRangeData&, std::optional<uint32_t> count, IndexedDB::GetAllType, IDBGetAllResult&) const;

    IDBError putIndexKey(const IDBKeyData&, const IndexKey&);

    void removeEntriesWithValueKey(const IDBKeyData&);
    void removeRecord(const IDBKeyData&, const IndexKey&);

    void objectStoreCleared();
    void clearIndexValueStore();
    void replaceIndexValueStore(std::unique_ptr<IndexValueStore>&&);

    MemoryIndexCursor* maybeOpenCursor(const IDBCursorInfo&);

    IndexValueStore* valueStore() { return m_records.get(); }

    MemoryObjectStore& objectStore() { return m_objectStore; }

    void cursorDidBecomeClean(MemoryIndexCursor&);
    void cursorDidBecomeDirty(MemoryIndexCursor&);

    void notifyCursorsOfValueChange(const IDBKeyData& indexKey, const IDBKeyData& primaryKey);

private:
    MemoryIndex(const IDBIndexInfo&, MemoryObjectStore&);

    uint64_t recordCountForKey(const IDBKeyData&) const;

    void notifyCursorsOfAllRecordsChanged();

    IDBIndexInfo m_info;
    MemoryObjectStore& m_objectStore;

    std::unique_ptr<IndexValueStore> m_records;

    HashMap<IDBResourceIdentifier, std::unique_ptr<MemoryIndexCursor>> m_cursors;
    HashSet<MemoryIndexCursor*> m_cleanCursors;
};

} // namespace IDBServer
} // namespace WebCore
