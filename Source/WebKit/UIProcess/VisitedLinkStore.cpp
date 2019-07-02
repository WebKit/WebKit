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
#include "VisitedLinkStore.h"

#include "VisitedLinkStoreMessages.h"
#include "VisitedLinkTableControllerMessages.h"
#include "WebPageProxy.h"
#include "WebProcessMessages.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"

namespace WebKit {
using namespace WebCore;

Ref<VisitedLinkStore> VisitedLinkStore::create()
{
    return adoptRef(*new VisitedLinkStore);
}

VisitedLinkStore::~VisitedLinkStore()
{
    RELEASE_ASSERT(m_processes.isEmpty());
}

VisitedLinkStore::VisitedLinkStore()
    : m_linkHashStore(*this)
{
}

void VisitedLinkStore::addProcess(WebProcessProxy& process)
{
    ASSERT(process.state() == WebProcessProxy::State::Running);
    ASSERT(!m_processes.contains(&process));

    if (!m_processes.add(&process).isNewEntry)
        return;

    process.addMessageReceiver(Messages::VisitedLinkStore::messageReceiverName(), identifier(), *this);

    if (m_linkHashStore.isEmpty())
        return;

    sendStoreHandleToProcess(process);
}

void VisitedLinkStore::removeProcess(WebProcessProxy& process)
{
    if (!m_processes.remove(&process))
        return;

    process.removeMessageReceiver(Messages::VisitedLinkStore::messageReceiverName(), identifier());
}

void VisitedLinkStore::addVisitedLinkHash(SharedStringHash linkHash)
{
    m_linkHashStore.scheduleAddition(linkHash);
}

bool VisitedLinkStore::containsVisitedLinkHash(WebCore::SharedStringHash linkHash)
{
    return m_linkHashStore.contains(linkHash);
}

void VisitedLinkStore::removeVisitedLinkHash(WebCore::SharedStringHash linkHash)
{
    m_linkHashStore.scheduleRemoval(linkHash);
}

void VisitedLinkStore::removeAll()
{
    m_linkHashStore.clear();

    for (WebProcessProxy* process : m_processes) {
        ASSERT(process->processPool().processes().contains(process));
        process->send(Messages::VisitedLinkTableController::RemoveAllVisitedLinks(), identifier());
    }
}

void VisitedLinkStore::addVisitedLinkHashFromPage(PageIdentifier pageID, SharedStringHash linkHash)
{
    if (auto* webPageProxy = WebProcessProxy::webPage(pageID)) {
        if (!webPageProxy->addsVisitedLinks())
            return;
    }

    addVisitedLinkHash(linkHash);
}

void VisitedLinkStore::sendStoreHandleToProcess(WebProcessProxy& process)
{
    ASSERT(process.processPool().processes().contains(&process));

    SharedMemory::Handle handle;
    if (!m_linkHashStore.createSharedMemoryHandle(handle))
        return;

    process.send(Messages::VisitedLinkTableController::SetVisitedLinkTable(handle), identifier());
}

void VisitedLinkStore::didInvalidateSharedMemory()
{
    for (WebProcessProxy* process : m_processes)
        sendStoreHandleToProcess(*process);
}

void VisitedLinkStore::didUpdateSharedStringHashes(const Vector<WebCore::SharedStringHash>& addedHashes, const Vector<WebCore::SharedStringHash>& removedHashes)
{
    ASSERT(!addedHashes.isEmpty() || !removedHashes.isEmpty());

    for (auto* process : m_processes) {
        ASSERT(process->processPool().processes().contains(process));

        if (addedHashes.size() > 20 || !removedHashes.isEmpty())
            process->send(Messages::VisitedLinkTableController::AllVisitedLinkStateChanged(), identifier());
        else
            process->send(Messages::VisitedLinkTableController::VisitedLinkStateChanged(addedHashes), identifier());
    }
}

} // namespace WebKit
