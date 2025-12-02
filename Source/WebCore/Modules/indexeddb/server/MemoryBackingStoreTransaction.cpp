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

#include "config.h"
#include "MemoryBackingStoreTransaction.h"

#include "IDBKeyRangeData.h"
#include "IDBValue.h"
#include "IndexedDB.h"
#include "Logging.h"
#include "MemoryIDBBackingStore.h"
#include "MemoryObjectStore.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {
namespace IDBServer {

Ref<MemoryBackingStoreTransaction> MemoryBackingStoreTransaction::create(MemoryIDBBackingStore& backingStore, const IDBTransactionInfo& info)
{
    return adoptRef(*new MemoryBackingStoreTransaction(backingStore, info));
}

MemoryBackingStoreTransaction::MemoryBackingStoreTransaction(MemoryIDBBackingStore& backingStore, const IDBTransactionInfo& info)
    : m_backingStore(backingStore)
    , m_info(info)
{
    if (m_info.mode() == IDBTransactionMode::Versionchange) {
        IDBDatabaseInfo info;
        auto error = m_backingStore->getOrEstablishDatabaseInfo(info);
        if (error.isNull())
            m_originalDatabaseInfo = makeUnique<IDBDatabaseInfo>(info);
    }
}

MemoryBackingStoreTransaction::~MemoryBackingStoreTransaction()
{
    ASSERT(!m_inProgress);
}

void MemoryBackingStoreTransaction::addNewObjectStore(MemoryObjectStore& objectStore)
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::addNewObjectStore()");

    ASSERT(isVersionChange());
    m_versionChangeAddedObjectStores.add(&objectStore);

    addExistingObjectStore(objectStore);
}

void MemoryBackingStoreTransaction::addNewIndex(MemoryIndex& index)
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::addNewIndex()");

    ASSERT(isVersionChange());
    m_versionChangeAddedIndexes.add(&index);

    addExistingIndex(index);
}

void MemoryBackingStoreTransaction::removeNewIndex(MemoryIndex& index)
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::removeNewIndex()");

    ASSERT(isVersionChange());

    m_originalIndexNames.remove(&index);
    m_versionChangeAddedIndexes.remove(&index);
    m_indexes.remove(&index);
}

void MemoryBackingStoreTransaction::addExistingIndex(MemoryIndex& index)
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::addExistingIndex");

    ASSERT(isWriting());

    ASSERT(!m_indexes.contains(&index));
    m_indexes.add(&index);
}

void MemoryBackingStoreTransaction::indexDeleted(Ref<MemoryIndex>&& index)
{
    m_indexes.remove(&index.get());
    m_deletedIndexes.add(WTFMove(index));
}

void MemoryBackingStoreTransaction::addExistingObjectStore(MemoryObjectStore& objectStore)
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::addExistingObjectStore");

    ASSERT(isWriting());

    ASSERT(!m_objectStores.contains(&objectStore));
    m_objectStores.add(&objectStore);

    objectStore.writeTransactionStarted(*this);
}

void MemoryBackingStoreTransaction::objectStoreDeleted(Ref<MemoryObjectStore>&& objectStore)
{
    ASSERT(m_objectStores.contains(&objectStore.get()));
    m_objectStores.remove(&objectStore.get());
    if (m_originalObjectStoreNames.contains(&objectStore.get()))
        m_originalObjectStoreNames.remove(&objectStore.get());
    objectStore->deleteAllIndexes(*this);

    // If the store removed is previously added in this transaction, we don't need to
    // keep it for transaction abort.
    if (auto addedObjectStore = m_versionChangeAddedObjectStores.take(&objectStore.get())) {
        // We don't need to track its indexes either.
        m_deletedIndexes.removeIf([identifier = objectStore->info().identifier()](auto& index) {
            return index->objectStore()->info().identifier() == identifier;
        });
        return;
    }

    auto addResult = m_deletedObjectStores.add(objectStore->info().name(), nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = WTFMove(objectStore);
}

void MemoryBackingStoreTransaction::objectStoreRenamed(MemoryObjectStore& objectStore, const String& oldName)
{
    ASSERT(m_objectStores.contains(&objectStore));
    ASSERT(m_info.mode() == IDBTransactionMode::Versionchange);

    // We only care about the first rename in a given transaction, because if the transaction is aborted we want
    // to go back to the first 'oldName'
    m_originalObjectStoreNames.add(&objectStore, oldName);
}

void MemoryBackingStoreTransaction::indexRenamed(MemoryIndex& index, const String& oldName)
{
    ASSERT(!index.objectStore() || m_objectStores.contains(index.objectStore()));
    ASSERT(m_info.mode() == IDBTransactionMode::Versionchange);

    // We only care about the first rename in a given transaction, because if the transaction is aborted we want
    // to go back to the first 'oldName'
    m_originalIndexNames.add(&index, oldName);
}

void MemoryBackingStoreTransaction::abort()
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::abort()");
    SetForScope change(m_isAborting, true);

    // Restore renamed indexes.
    for (const auto& iterator : m_originalIndexNames) {
        RefPtr index = iterator.key;
        auto originalName = iterator.value;
        auto identifier = index->info().identifier();

        // If a new index was added with the original name of an index being renamed in this transaction, we need to delete it.
        RefPtr<MemoryIndex> indexToDelete;
        for (auto addedIndex : m_indexes) {
            if (addedIndex->info().name() == originalName && addedIndex->info().identifier() != identifier) {
                indexToDelete = addedIndex;
                break;
            }
        }
        if (indexToDelete) {
            if (RefPtr objectStore = indexToDelete->objectStore())
                objectStore->deleteIndex(*this, indexToDelete->info().identifier());
        }

        if (RefPtr objectStore = index->objectStore()) {
            auto indexToReRegister = objectStore->takeIndexByIdentifier(identifier).releaseNonNull();
            objectStore->info().deleteIndex(identifier);
            index->rename(originalName);
            objectStore->info().addExistingIndex(index->info());
            objectStore->registerIndex(WTFMove(indexToReRegister));
        }
    }
    m_originalIndexNames.clear();

    // Restore renamed object stores.
    for (const auto& iterator : m_originalObjectStoreNames)
        m_backingStore->renameObjectStoreForVersionChangeAbort(Ref { *iterator.key }, iterator.value);
    m_originalObjectStoreNames.clear();

    // Restore added object stores.
    for (const auto& objectStore : m_versionChangeAddedObjectStores)
        m_backingStore->removeObjectStoreForVersionChangeAbort(*objectStore);
    m_deletedIndexes.removeIf([&](auto& index) {
        return m_versionChangeAddedObjectStores.contains(index->objectStore());
    });
    m_versionChangeAddedObjectStores.clear();

    // Restore deleted object stores.
    for (auto& objectStore : m_deletedObjectStores.values()) {
        m_backingStore->restoreObjectStoreForVersionChangeAbort(*objectStore);
        ASSERT(!m_objectStores.contains(objectStore.get()));
        m_objectStores.add(objectStore);
    }
    m_deletedObjectStores.clear();

    // Restore database info.
    if (m_originalDatabaseInfo) {
        ASSERT(m_info.mode() == IDBTransactionMode::Versionchange);
        m_backingStore->setDatabaseInfo(*m_originalDatabaseInfo);
    }

    for (auto& index : m_deletedIndexes) {
        RELEASE_ASSERT(m_backingStore->hasObjectStore(index->info().objectStoreIdentifier()));
        index->protectedObjectStore()->maybeRestoreDeletedIndex(*index);
    }
    m_deletedIndexes.clear();

    // Restore object store records.
    for (auto& objectStore : m_objectStores)
        objectStore->transactionAborted(*this);

    finish();
}

void MemoryBackingStoreTransaction::commit()
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::commit()");

    finish();
}

void MemoryBackingStoreTransaction::finish()
{
    m_inProgress = false;

    if (!isWriting()) {
        // Read-only transaction does not track object stores, so get it from backing store.
        for (auto objectStoreName : m_info.objectStores()) {
            if (RefPtr objectStore = m_backingStore->objectStoreForName(objectStoreName))
                objectStore->transactionFinished(*this);
        }
        return;
    }

    for (auto& objectStore : m_objectStores)
        objectStore->transactionFinished(*this);
    for (auto& objectStore : m_deletedObjectStores.values())
        objectStore->transactionFinished(*this);
}

MemoryCursor* MemoryBackingStoreTransaction::cursor(const IDBResourceIdentifier& identifier) const
{
    return m_cursors.get(identifier);
}

void MemoryBackingStoreTransaction::addCursor(MemoryCursor& cursor)
{
    m_cursors.add(cursor.info().identifier(), &cursor);
}

} // namespace IDBServer
} // namespace WebCore
