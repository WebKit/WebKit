/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "VisitedLinkProvider.h"

#include "SharedMemory.h"
#include "VisitedLinkProviderMessages.h"
#include "VisitedLinkTable.h"
#include "VisitedLinkTableControllerMessages.h"
#include "WebContext.h"
#include "WebProcessMessages.h"

using namespace WebCore;

namespace WebKit {

static const int visitedLinkTableMaxLoad = 2;

static uint64_t generateIdentifier()
{
    static uint64_t identifier;

    return ++identifier;
}

PassRefPtr<VisitedLinkProvider> VisitedLinkProvider::create()
{
    return adoptRef(new VisitedLinkProvider);
}

VisitedLinkProvider::~VisitedLinkProvider()
{
    ASSERT(m_processes.isEmpty());
}

VisitedLinkProvider::VisitedLinkProvider()
    : m_identifier(generateIdentifier())
    , m_keyCount(0)
    , m_tableSize(0)
    , m_pendingVisitedLinksTimer(RunLoop::main(), this, &VisitedLinkProvider::pendingVisitedLinksTimerFired)
{
}

void VisitedLinkProvider::addProcess(WebProcessProxy& process)
{
    ASSERT(process.state() == WebProcessProxy::State::Running);

    if (!m_processes.add(&process).isNewEntry)
        return;

    process.addMessageReceiver(Messages::VisitedLinkProvider::messageReceiverName(), m_identifier, *this);

    if (!m_keyCount)
        return;

    ASSERT(m_table.sharedMemory());

    sendTable(process);
}

void VisitedLinkProvider::removeProcess(WebProcessProxy& process)
{
    ASSERT(m_processes.contains(&process));

    if (m_processes.remove(&process))
        process.removeMessageReceiver(Messages::VisitedLinkProvider::messageReceiverName(), m_identifier);
}

void VisitedLinkProvider::addVisitedLinkHash(LinkHash linkHash)
{
    m_pendingVisitedLinks.add(linkHash);

    if (!m_pendingVisitedLinksTimer.isActive())
        m_pendingVisitedLinksTimer.startOneShot(0);
}

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
    unsigned tableSize = nextPowerOf2(keyCount * visitedLinkTableMaxLoad);

    // Ensure that the table size is at least the size of a page.
    size_t minimumTableSize = SharedMemory::systemPageSize() / sizeof(LinkHash);
    if (tableSize < minimumTableSize)
        return minimumTableSize;
    
    return tableSize;
}

void VisitedLinkProvider::pendingVisitedLinksTimerFired()
{
    unsigned currentTableSize = m_tableSize;
    unsigned newTableSize = tableSizeForKeyCount(m_keyCount + m_pendingVisitedLinks.size());

    newTableSize = std::max(currentTableSize, newTableSize);

    if (currentTableSize != newTableSize) {
        resizeTable(newTableSize);
        return;
    }

    Vector<WebCore::LinkHash> addedVisitedLinks;

    for (auto linkHash : m_pendingVisitedLinks) {
        if (m_table.addLinkHash(linkHash)) {
            addedVisitedLinks.append(linkHash);
            m_keyCount++;
        }
    }

    m_pendingVisitedLinks.clear();

    if (addedVisitedLinks.isEmpty())
        return;

    for (auto& processAndCount : m_processes) {
        if (addedVisitedLinks.size() > 20)
            processAndCount.key->connection()->send(Messages::VisitedLinkTableController::AllVisitedLinkStateChanged(), m_identifier);
        else
            processAndCount.key->connection()->send(Messages::VisitedLinkTableController::VisitedLinkStateChanged(addedVisitedLinks), m_identifier);
    }
}

void VisitedLinkProvider::resizeTable(unsigned newTableSize)
{
    RefPtr<SharedMemory> newTableMemory = SharedMemory::create(newTableSize * sizeof(LinkHash));

    if (!newTableMemory) {
        LOG_ERROR("Could not allocate shared memory for visited link table");
        return;
    }

    memset(newTableMemory->data(), 0, newTableMemory->size());

    RefPtr<SharedMemory> currentTableMemory = m_table.sharedMemory();
    unsigned currentTableSize = m_tableSize;

    m_table.setSharedMemory(newTableMemory);
    m_tableSize = newTableSize;

    if (currentTableMemory) {
        ASSERT_UNUSED(currentTableSize, currentTableMemory->size() == currentTableSize * sizeof(LinkHash));

        // Go through the current hash table and re-add all entries to the new hash table.
        const LinkHash* currentLinkHashes = static_cast<const LinkHash*>(currentTableMemory->data());
        for (unsigned i = 0; i < m_tableSize; ++i) {
            LinkHash linkHash = currentLinkHashes[i];

            if (!linkHash)
                continue;

            bool didAddLinkHash = m_table.addLinkHash(linkHash);

            // It should always be possible to add the link hash to a new table.
            ASSERT_UNUSED(didAddLinkHash, didAddLinkHash);
        }
    }

    for (auto linkHash : m_pendingVisitedLinks) {
        if (m_table.addLinkHash(linkHash))
            m_keyCount++;
    }
    m_pendingVisitedLinks.clear();

    for (auto& processAndCount : m_processes)
        sendTable(*processAndCount.key);
}

void VisitedLinkProvider::sendTable(WebProcessProxy& process)
{
    SharedMemory::Handle handle;
    if (!m_table.sharedMemory()->createHandle(handle, SharedMemory::ReadOnly))
        return;

    process.connection()->send(Messages::VisitedLinkTableController::SetVisitedLinkTable(handle), m_identifier);
}

} // namespace WebKit
