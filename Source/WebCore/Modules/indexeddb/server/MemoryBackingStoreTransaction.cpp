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

#if ENABLE(INDEXED_DATABASE)

#include "IDBKeyRangeData.h"
#include "IDBValue.h"
#include "IndexedDB.h"
#include "Logging.h"
#include "MemoryIDBBackingStore.h"
#include "MemoryObjectStore.h"
#include <wtf/SetForScope.h>

namespace WebCore {
namespace IDBServer {

std::unique_ptr<MemoryBackingStoreTransaction> MemoryBackingStoreTransaction::create(MemoryIDBBackingStore& backingStore, const IDBTransactionInfo& info)
{
    return std::make_unique<MemoryBackingStoreTransaction>(backingStore, info);
}

MemoryBackingStoreTransaction::MemoryBackingStoreTransaction(MemoryIDBBackingStore& backingStore, const IDBTransactionInfo& info)
    : m_backingStore(backingStore)
    , m_info(info)
{
    if (m_info.mode() == IDBTransactionMode::Versionchange) {
        IDBDatabaseInfo info;
        auto error = m_backingStore.getOrEstablishDatabaseInfo(info);
        if (error.isNull())
            m_originalDatabaseInfo = std::make_unique<IDBDatabaseInfo>(info);
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

    // If this MemoryIndex belongs to an object store that will not get restored if this transaction aborts,
    // then we can forget about it altogether.
    auto& objectStore = index->objectStore();
    if (auto deletedObjectStore = m_deletedObjectStores.get(objectStore.info().name())) {
        if (deletedObjectStore != &objectStore)
            return;
    }

    auto addResult = m_deletedIndexes.add(index->info().name(), nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = WTFMove(index);
}

void MemoryBackingStoreTransaction::addExistingObjectStore(MemoryObjectStore& objectStore)
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::addExistingObjectStore");

    ASSERT(isWriting());

    ASSERT(!m_objectStores.contains(&objectStore));
    m_objectStores.add(&objectStore);

    objectStore.writeTransactionStarted(*this);

    m_originalKeyGenerators.add(&objectStore, objectStore.currentKeyGeneratorValue());
}

void MemoryBackingStoreTransaction::objectStoreDeleted(Ref<MemoryObjectStore>&& objectStore)
{
    ASSERT(m_objectStores.contains(&objectStore.get()));
    m_objectStores.remove(&objectStore.get());

    objectStore->deleteAllIndexes(*this);

    auto addResult = m_deletedObjectStores.add(objectStore->info().name(), nullptr);
    if (addResult.isNewEntry)
        addResult.iterator->value = WTFMove(objectStore);
}

void MemoryBackingStoreTransaction::objectStoreCleared(MemoryObjectStore& objectStore, std::unique_ptr<KeyValueMap>&& keyValueMap, std::unique_ptr<IDBKeyDataSet>&& orderedKeys)
{
    ASSERT(m_objectStores.contains(&objectStore));

    auto addResult = m_clearedKeyValueMaps.add(&objectStore, nullptr);

    // If this object store has already been cleared during this transaction, we shouldn't remember this clearing.
    if (!addResult.isNewEntry)
        return;

    addResult.iterator->value = WTFMove(keyValueMap);

    ASSERT(!m_clearedOrderedKeys.contains(&objectStore));
    m_clearedOrderedKeys.add(&objectStore, WTFMove(orderedKeys));
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
    ASSERT(m_objectStores.contains(&index.objectStore()));
    ASSERT(m_info.mode() == IDBTransactionMode::Versionchange);

    // We only care about the first rename in a given transaction, because if the transaction is aborted we want
    // to go back to the first 'oldName'
    m_originalIndexNames.add(&index, oldName);
}

void MemoryBackingStoreTransaction::indexCleared(MemoryIndex& index, std::unique_ptr<IndexValueStore>&& valueStore)
{
    auto addResult = m_clearedIndexValueStores.add(&index, nullptr);

    // If this index has already been cleared during this transaction, we shouldn't remember this clearing.
    if (!addResult.isNewEntry)
        return;

    addResult.iterator->value = WTFMove(valueStore);
}

void MemoryBackingStoreTransaction::recordValueChanged(MemoryObjectStore& objectStore, const IDBKeyData& key, ThreadSafeDataBuffer* value)
{
    ASSERT(m_objectStores.contains(&objectStore));

    if (m_isAborting)
        return;

    // If this object store had been cleared during the transaction, no point in recording this
    // individual key/value change as its entire key/value map will be restored upon abort.
    if (m_clearedKeyValueMaps.contains(&objectStore))
        return;

    auto originalAddResult = m_originalValues.add(&objectStore, nullptr);
    if (originalAddResult.isNewEntry)
        originalAddResult.iterator->value = std::make_unique<KeyValueMap>();

    auto* map = originalAddResult.iterator->value.get();

    auto addResult = map->add(key, ThreadSafeDataBuffer());
    if (!addResult.isNewEntry)
        return;

    if (value)
        addResult.iterator->value = *value;
}

void MemoryBackingStoreTransaction::abort()
{
    LOG(IndexedDB, "MemoryBackingStoreTransaction::abort()");

    SetForScope<bool> change(m_isAborting, true);

    for (const auto& iterator : m_originalIndexNames)
        iterator.key->rename(iterator.value);
    m_originalIndexNames.clear();

    for (const auto& iterator : m_originalObjectStoreNames)
        iterator.key->rename(iterator.value);
    m_originalObjectStoreNames.clear();

    for (const auto& objectStore : m_versionChangeAddedObjectStores)
        m_backingStore.removeObjectStoreForVersionChangeAbort(*objectStore);
    m_versionChangeAddedObjectStores.clear();

    for (auto& objectStore : m_deletedObjectStores.values()) {
        m_backingStore.restoreObjectStoreForVersionChangeAbort(*objectStore);
        ASSERT(!m_objectStores.contains(objectStore.get()));
        m_objectStores.add(objectStore);
    }
    m_deletedObjectStores.clear();

    if (m_originalDatabaseInfo) {
        ASSERT(m_info.mode() == IDBTransactionMode::Versionchange);
        m_backingStore.setDatabaseInfo(*m_originalDatabaseInfo);
    }

    // Restore cleared index value stores before we re-insert values into object stores
    // because inserting those values will regenerate the appropriate index values.
    for (auto& iterator : m_clearedIndexValueStores)
        iterator.key->replaceIndexValueStore(WTFMove(iterator.value));
    m_clearedIndexValueStores.clear();
    
    for (auto& objectStore : m_objectStores) {
        ASSERT(m_originalKeyGenerators.contains(objectStore.get()));
        objectStore->setKeyGeneratorValue(m_originalKeyGenerators.get(objectStore.get()));

        auto clearedKeyValueMap = m_clearedKeyValueMaps.take(objectStore.get());
        if (clearedKeyValueMap) {
            ASSERT(m_clearedOrderedKeys.contains(objectStore.get()));
            objectStore->replaceKeyValueStore(WTFMove(clearedKeyValueMap), m_clearedOrderedKeys.take(objectStore.get()));
        }

        auto keyValueMap = m_originalValues.take(objectStore.get());
        if (!keyValueMap)
            continue;

        for (const auto& entry : *keyValueMap) {
            objectStore->deleteRecord(entry.key);
            objectStore->addRecord(*this, entry.key, { entry.value });
        }
    }

    for (auto& index : m_deletedIndexes.values())
        index->objectStore().maybeRestoreDeletedIndex(*index);
    m_deletedIndexes.clear();

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

    if (!isWriting())
        return;

    for (auto& objectStore : m_objectStores)
        objectStore->writeTransactionFinished(*this);
    for (auto& objectStore : m_deletedObjectStores.values())
        objectStore->writeTransactionFinished(*this);
}

} // namespace IDBServer
} // namespace WebCore

#endif // ENABLE(INDEXED_DATABASE)
