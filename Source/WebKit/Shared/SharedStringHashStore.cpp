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
    , m_pendingSharedStringHashesTimer(RunLoop::main(), this, &SharedStringHashStore::pendingSharedStringHashesTimerFired)
{
}

bool SharedStringHashStore::createSharedMemoryHandle(SharedMemory::Handle& handle)
{
    return m_table.sharedMemory()->createHandle(handle, SharedMemory::Protection::ReadOnly);
}

void SharedStringHashStore::add(SharedStringHash sharedStringHash)
{
    m_pendingSharedStringHashes.add(sharedStringHash);

    if (!m_pendingSharedStringHashesTimer.isActive())
        m_pendingSharedStringHashesTimer.startOneShot(0_s);
}

void SharedStringHashStore::clear()
{
    m_pendingSharedStringHashesTimer.stop();
    m_pendingSharedStringHashes.clear();
    m_keyCount = 0;
    m_tableSize = 0;
    m_table.clear();
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

    for (auto& sharedStringHash : m_pendingSharedStringHashes) {
        if (m_table.add(sharedStringHash))
            ++m_keyCount;
    }
    m_pendingSharedStringHashes.clear();

    m_client.didInvalidateSharedMemory();
}

void SharedStringHashStore::pendingSharedStringHashesTimerFired()
{
    unsigned currentTableSize = m_tableSize;
    unsigned newTableSize = tableSizeForKeyCount(m_keyCount + m_pendingSharedStringHashes.size());

    newTableSize = std::max(currentTableSize, newTableSize);

    if (currentTableSize != newTableSize) {
        resizeTable(newTableSize);
        return;
    }

    Vector<SharedStringHash> addedSharedStringHashes;
    addedSharedStringHashes.reserveInitialCapacity(m_pendingSharedStringHashes.size());
    for (auto& sharedStringHash : m_pendingSharedStringHashes) {
        if (m_table.add(sharedStringHash)) {
            addedSharedStringHashes.uncheckedAppend(sharedStringHash);
            ++m_keyCount;
        }
    }

    m_pendingSharedStringHashes.clear();

    if (!addedSharedStringHashes.isEmpty())
        m_client.didAddSharedStringHashes(addedSharedStringHashes);
}

} // namespace WebKit
