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
#include "IDBKeyData.h"
#include "IDBResourceIdentifier.h"
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/WeakHashSet.h>

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

class MemoryIndex : public RefCounted<MemoryIndex>, public CanMakeThreadSafeCheckedPtr<MemoryIndex> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(MemoryIndex);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MemoryIndex);
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

    MemoryIndexCursor* maybeOpenCursor(const IDBCursorInfo&, MemoryBackingStoreTransaction&);
    IndexValueStore* valueStore() { return m_records.get(); }

    MemoryObjectStore* objectStore();

    void cursorDidBecomeClean(MemoryIndexCursor&);
    void cursorDidBecomeDirty(MemoryIndexCursor&);

    void notifyCursorsOfValueChange(const IDBKeyData& indexKey, const IDBKeyData& primaryKey);
    void transactionFinished(MemoryBackingStoreTransaction&);

    void writeTransactionStarted(MemoryBackingStoreTransaction&);
    void writeTransactionFinished(MemoryBackingStoreTransaction&);
    void transactionAborted(MemoryBackingStoreTransaction&);

private:
    MemoryIndex(const IDBIndexInfo&, MemoryObjectStore&);

    uint64_t recordCountForKey(const IDBKeyData&) const;

    void notifyCursorsOfAllRecordsChanged();
    IDBError addIndexRecord(const IDBKeyData& indexKey, const IDBKeyData& valueKey);
    void removeIndexRecord(const IDBKeyData& indexKey, const IDBKeyData& valueKey);
    void removeIndexRecord(const IDBKeyData& indexKey);

    IDBIndexInfo m_info;
    WeakPtr<MemoryObjectStore> m_objectStore;

    WeakPtr<MemoryBackingStoreTransaction> m_writeTransaction;
    HashMap<IDBKeyData, Vector<IDBKeyData>, DefaultHash<IDBKeyData>, IDBKeyDataHashTraits> m_transactionModifiedRecords;
    std::unique_ptr<IndexValueStore> m_records;

    HashMap<IDBResourceIdentifier, RefPtr<MemoryIndexCursor>> m_cursors;
    WeakHashSet<MemoryIndexCursor> m_cleanCursors;
};

} // namespace IDBServer
} // namespace WebCore
