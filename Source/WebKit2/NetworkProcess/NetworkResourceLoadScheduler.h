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

#ifndef NetworkResourceLoadScheduler_h
#define NetworkResourceLoadScheduler_h

#include "NetworkResourceLoader.h"
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/Timer.h>
#include <wtf/HashSet.h>

#if ENABLE(NETWORK_PROCESS)

namespace WebKit {

class HostRecord;
class NetworkResourceLoadParameters;
class NetworkConnectionToWebProcess;
typedef uint64_t ResourceLoadIdentifier;

class NetworkResourceLoadScheduler {
    WTF_MAKE_NONCOPYABLE(NetworkResourceLoadScheduler); WTF_MAKE_FAST_ALLOCATED;

public:
    NetworkResourceLoadScheduler();
    
    // Adds the request to the queue for its host and create a unique identifier for it.
    ResourceLoadIdentifier scheduleResourceLoad(const NetworkResourceLoadParameters&, NetworkConnectionToWebProcess*);
    
    // Creates a unique identifier for an already-in-progress load.
    ResourceLoadIdentifier addLoadInProgress(const WebCore::KURL&);

    // Called by the WebProcess when a ResourceLoader is being cleaned up.
    void removeLoadIdentifier(ResourceLoadIdentifier);

    // Called within the NetworkProcess on a background thread when a resource load has finished.
    void scheduleRemoveLoadIdentifier(ResourceLoadIdentifier);

    void receivedRedirect(ResourceLoadIdentifier, const WebCore::KURL& redirectURL);
    void servePendingRequests(WebCore::ResourceLoadPriority = WebCore::ResourceLoadPriorityVeryLow);
    
    NetworkResourceLoader* networkResourceLoaderForIdentifier(ResourceLoadIdentifier);

private:
    enum CreateHostPolicy {
        CreateIfNotFound,
        FindOnly
    };
    
    HostRecord* hostForURL(const WebCore::KURL&, CreateHostPolicy = FindOnly);
    
    void scheduleServePendingRequests();
    void requestTimerFired(WebCore::Timer<NetworkResourceLoadScheduler>*);

    void servePendingRequestsForHost(HostRecord*, WebCore::ResourceLoadPriority);

    unsigned platformInitializeMaximumHTTPConnectionCountPerHost();

    static void removeScheduledLoadIdentifiers(void* context);
    void removeScheduledLoadIdentifiers();

    HashMap<ResourceLoadIdentifier, RefPtr<NetworkResourceLoader> > m_resourceLoaders;

    typedef HashMap<String, HostRecord*, StringHash> HostMap;
    HostMap m_hosts;

    typedef HashMap<ResourceLoadIdentifier, HostRecord*> IdentifierHostMap;
    IdentifierHostMap m_identifiers;

    HostRecord* m_nonHTTPProtocolHost;

    bool m_isSerialLoadingEnabled;

    WebCore::Timer<NetworkResourceLoadScheduler> m_requestTimer;
    
    Mutex m_identifiersToRemoveMutex;
    HashSet<ResourceLoadIdentifier> m_identifiersToRemove;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)

#endif // NetworkResourceLoadScheduler_h
