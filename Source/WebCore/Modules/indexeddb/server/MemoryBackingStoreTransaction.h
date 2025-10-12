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

#include <WebCore/IDBDatabaseInfo.h>
#include <WebCore/IDBKeyData.h>
#include <WebCore/IDBTransactionInfo.h>
#include <WebCore/IndexValueStore.h>
#include <WebCore/ThreadSafeDataBuffer.h>
#include <wtf/CheckedRef.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {
namespace IDBServer {

class MemoryCursor;
class MemoryIDBBackingStore;
class MemoryIndex;
class MemoryObjectStore;

typedef HashMap<IDBKeyData, ThreadSafeDataBuffer, IDBKeyDataHash, IDBKeyDataHashTraits> KeyValueMap;

class MemoryBackingStoreTransaction final : public RefCountedAndCanMakeWeakPtr<MemoryBackingStoreTransaction> {
public:
    static Ref<MemoryBackingStoreTransaction> create(MemoryIDBBackingStore&, const IDBTransactionInfo&);

    ~MemoryBackingStoreTransaction();

    bool isVersionChange() const { return m_info.mode() == IDBTransactionMode::Versionchange; }
    bool isWriting() const { return m_info.mode() != IDBTransactionMode::Readonly; }
    bool isAborting() const { return m_isAborting; }

    const IDBDatabaseInfo& originalDatabaseInfo() const;

    void addNewObjectStore(MemoryObjectStore&);
    void addExistingObjectStore(MemoryObjectStore&);

    void objectStoreDeleted(Ref<MemoryObjectStore>&&);
    void objectStoreCleared(MemoryObjectStore&, KeyValueMap&&, std::unique_ptr<IDBKeyDataSet>&&);
    void objectStoreRenamed(MemoryObjectStore&, const String& oldName);
    void indexRenamed(MemoryIndex&, const String& oldName);

    void addNewIndex(MemoryIndex&);
    void removeNewIndex(MemoryIndex&);
    void addExistingIndex(MemoryIndex&);
    void indexDeleted(Ref<MemoryIndex>&&);

    void abort();
    void commit();

    IDBTransactionInfo info() const { return m_info; }

    MemoryCursor* cursor(const IDBResourceIdentifier&) const;
    void addCursor(MemoryCursor&);

private:
    MemoryBackingStoreTransaction(MemoryIDBBackingStore&, const IDBTransactionInfo&);
    void finish();

    const CheckedRef<MemoryIDBBackingStore> m_backingStore;
    IDBTransactionInfo m_info;

    std::unique_ptr<IDBDatabaseInfo> m_originalDatabaseInfo;

    bool m_inProgress { true };
    bool m_isAborting { false };

    HashSet<RefPtr<MemoryObjectStore>> m_objectStores;
    HashSet<RefPtr<MemoryObjectStore>> m_versionChangeAddedObjectStores;
    HashSet<RefPtr<MemoryIndex>> m_indexes;
    HashSet<RefPtr<MemoryIndex>> m_versionChangeAddedIndexes;

    HashMap<String, RefPtr<MemoryObjectStore>> m_deletedObjectStores;
    HashSet<RefPtr<MemoryIndex>> m_deletedIndexes;
    HashMap<MemoryObjectStore*, String> m_originalObjectStoreNames;
    HashMap<MemoryIndex*, String> m_originalIndexNames;

    HashMap<IDBResourceIdentifier, WeakPtr<MemoryCursor>> m_cursors;
};

} // namespace IDBServer
} // namespace WebCore
