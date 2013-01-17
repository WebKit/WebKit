/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "HostRecord.h"

#include "Logging.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkProcess.h"
#include "NetworkResourceLoadParameters.h"
#include "NetworkResourceLoadScheduler.h"
#include "NetworkResourceLoader.h"
#include "SyncNetworkResourceLoader.h"

#if ENABLE(NETWORK_PROCESS)

using namespace WebCore;

namespace WebKit {

HostRecord::HostRecord(const String& name, int maxRequestsInFlight)
    : m_name(name)
    , m_maxRequestsInFlight(maxRequestsInFlight)
{
}

HostRecord::~HostRecord()
{
#ifndef NDEBUG
    ASSERT(m_resourceIdentifiersLoading.isEmpty());
    for (unsigned p = 0; p <= ResourceLoadPriorityHighest; p++)
        ASSERT(m_loadersPending[p].isEmpty());
#endif
}

void HostRecord::schedule(PassRefPtr<NetworkResourceLoader> loader)
{
    m_loadersPending[loader->priority()].append(loader);
}

void HostRecord::addLoadInProgress(ResourceLoadIdentifier identifier)
{
    m_resourceIdentifiersLoading.add(identifier);
}

void HostRecord::remove(ResourceLoadIdentifier identifier)
{
    // FIXME (NetworkProcess): Due to IPC race conditions, it's possible this HostRecord will be asked to remove the same identifer twice.
    // It would be nice to know the identifier has already been removed and treat it as a no-op.

    if (m_resourceIdentifiersLoading.contains(identifier)) {
        m_resourceIdentifiersLoading.remove(identifier);
        return;
    }
    
    for (int priority = ResourceLoadPriorityHighest; priority >= ResourceLoadPriorityLowest; --priority) {  
        LoaderQueue::iterator end = m_loadersPending[priority].end();
        for (LoaderQueue::iterator it = m_loadersPending[priority].begin(); it != end; ++it) {
            if (it->get()->identifier() == identifier) {
                m_loadersPending[priority].remove(it);
                return;
            }
        }
    }
}

bool HostRecord::hasRequests() const
{
    if (!m_resourceIdentifiersLoading.isEmpty())
        return true;

    for (unsigned p = 0; p <= ResourceLoadPriorityHighest; p++) {
        if (!m_loadersPending[p].isEmpty())
            return true;
    }

    return false;
}

bool HostRecord::limitRequests(ResourceLoadPriority priority, bool serialLoadingEnabled) const
{
    if (priority == ResourceLoadPriorityVeryLow && !m_resourceIdentifiersLoading.isEmpty())
        return true;

    return m_resourceIdentifiersLoading.size() >= (serialLoadingEnabled ? 1 : m_maxRequestsInFlight);
}

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)
