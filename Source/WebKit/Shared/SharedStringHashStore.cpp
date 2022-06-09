/*
 * Copyright (C) 2010-2017 Apple Inc. All rights reserved.
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

#include "config.h"
#include "SharedStringHashStore.h"

#include <algorithm>

namespace WebKit {

using namespace WebCore;

const unsigned sharedStringHashTableMaxLoad = 2;

static unsigned nextPowerOf2(unsigned v)
{
    // Taken from http://www.cs.utk.edu/~vose/c-stuff/bithacks.html
    // Devised by Sean Anderson, Sepember 14, 2001

    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;

    return v;
}

static unsigned tableSizeForKeyCount(unsigned keyCount)
{
    // We want the table to be at least half empty.
    unsigned tableSize = nextPowerOf2(keyCount * sharedStringHashTableMaxLoad);

    // Ensure that the table size is at least the size of a page.
    size_t minimumTableSize = SharedMemory::systemPageSize() / sizeof(SharedStringHash);
    if (tableSize < minimumTableSize)
        return minimumTableSize;

    return tableSize;
}

SharedStringHashStore::SharedStringHashStore(Client& client)
    : m_client(client)
    , m_pendingOperationsTimer(RunLoop::main(), this, &SharedStringHashStore::processPendingOperations)
{
}

bool SharedStringHashStore::createSharedMemoryHandle(SharedMemory::Handle& handle)
{
    return m_table.sharedMemory()->createHandle(handle, SharedMemory::Protection::ReadOnly);
}

void SharedStringHashStore::scheduleAddition(SharedStringHash sharedStringHash)
{
    m_pendingOperations.append({ Operation::Add, sharedStringHash });

    if (!m_pendingOperationsTimer.isActive())
        m_pendingOperationsTimer.startOneShot(0_s);
}

void SharedStringHashStore::scheduleRemoval(WebCore::SharedStringHash sharedStringHash)
{
    m_pendingOperations.append({ Operation::Remove, sharedStringHash });

    if (!m_pendingOperationsTimer.isActive())
        m_pendingOperationsTimer.startOneShot(0_s);
}

bool SharedStringHashStore::contains(WebCore::SharedStringHash sharedStringHash)
{
    flushPendingChanges();
    return m_table.contains(sharedStringHash);
}

void SharedStringHashStore::clear()
{
    m_pendingOperationsTimer.stop();
    m_pendingOperations.clear();
    m_keyCount = 0;
    m_tableSize = 0;
    m_table.clear();
}

void SharedStringHashStore::flushPendingChanges()
{
    if (!m_pendingOperationsTimer.isActive())
        return;

    m_pendingOperationsTimer.stop();
    processPendingOperations();
}

void SharedStringHashStore::resizeTable(unsigned newTableSize)
{
    auto newTableMemory = SharedMemory::allocate(newTableSize * sizeof(SharedStringHash));
    if (!newTableMemory) {
        LOG_ERROR("Could not allocate shared memory for SharedStringHash table");
        return;
    }

    memset(newTableMemory->data(), 0, newTableMemory->size());

    RefPtr<SharedMemory> currentTableMemory = m_table.sharedMemory();
    unsigned currentTableSize = m_tableSize;

    m_table.setSharedMemory(newTableMemory.releaseNonNull());
    m_tableSize = newTableSize;

    if (currentTableMemory) {
        ASSERT_UNUSED(currentTableSize, currentTableMemory->size() == currentTableSize * sizeof(SharedStringHash));

        // Go through the current hash table and re-add all entries to the new hash table.
        const SharedStringHash* currentSharedStringHashes = static_cast<const SharedStringHash*>(currentTableMemory->data());
        for (unsigned i = 0; i < currentTableSize; ++i) {
            auto sharedStringHash = currentSharedStringHashes[i];
            if (!sharedStringHash)
                continue;

            bool didAddSharedStringHash = m_table.add(sharedStringHash);

            // It should always be possible to add the SharedStringHash to a new table.
            ASSERT_UNUSED(didAddSharedStringHash, didAddSharedStringHash);
        }
    }

    for (auto& operation : m_pendingOperations) {
        switch (operation.type) {
        case Operation::Add:
            if (m_table.add(operation.sharedStringHash))
                ++m_keyCount;
            break;
        case Operation::Remove:
            if (m_table.remove(operation.sharedStringHash))
                --m_keyCount;
            break;
        }
    }
    m_pendingOperations.clear();

    m_client.didInvalidateSharedMemory();
}

void SharedStringHashStore::processPendingOperations()
{
    unsigned currentTableSize = m_tableSize;
    unsigned approximateNewHashCount = std::count_if(m_pendingOperations.begin(), m_pendingOperations.end(), [](auto& operation) {
        return operation.type == Operation::Add;
    });
    // FIXME: The table can currently only grow. We should probably support shrinking it to save memory.
    unsigned newTableSize = tableSizeForKeyCount(m_keyCount + approximateNewHashCount);

    newTableSize = std::max(currentTableSize, newTableSize);

    if (currentTableSize != newTableSize) {
        resizeTable(newTableSize);
        return;
    }

    Vector<SharedStringHash> addedSharedStringHashes;
    Vector<SharedStringHash> removedSharedStringHashes;
    addedSharedStringHashes.reserveInitialCapacity(approximateNewHashCount);
    removedSharedStringHashes.reserveInitialCapacity(m_pendingOperations.size() - approximateNewHashCount);
    for (auto& operation : m_pendingOperations) {
        switch (operation.type) {
        case Operation::Add:
            if (m_table.add(operation.sharedStringHash)) {
                addedSharedStringHashes.uncheckedAppend(operation.sharedStringHash);
                ++m_keyCount;
            }
            break;
        case Operation::Remove:
            if (m_table.remove(operation.sharedStringHash)) {
                removedSharedStringHashes.uncheckedAppend(operation.sharedStringHash);
                --m_keyCount;
            }
            break;
        }
    }

    m_pendingOperations.clear();

    if (!addedSharedStringHashes.isEmpty() || !removedSharedStringHashes.isEmpty())
        m_client.didUpdateSharedStringHashes(addedSharedStringHashes, removedSharedStringHashes);
}

} // namespace WebKit
