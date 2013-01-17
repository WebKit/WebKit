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

#ifndef HostRecord_h
#define HostRecord_h

#if ENABLE(NETWORK_PROCESS)

#include <WebCore/ResourceRequest.h>
#include <wtf/Deque.h>
#include <wtf/HashSet.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class NetworkResourceLoader;
class SyncNetworkResourceLoader;
typedef uint64_t ResourceLoadIdentifier;

class HostRecord {
    WTF_MAKE_NONCOPYABLE(HostRecord); WTF_MAKE_FAST_ALLOCATED;
public:
    HostRecord(const String& name, int maxRequestsInFlight);
    ~HostRecord();
    
    const String& name() const { return m_name; }
    void schedule(PassRefPtr<NetworkResourceLoader>);
    void addLoadInProgress(ResourceLoadIdentifier);
    void remove(ResourceLoadIdentifier);
    bool hasRequests() const;
    bool limitRequests(WebCore::ResourceLoadPriority, bool serialLoadingEnabled) const;

    typedef Deque<RefPtr<NetworkResourceLoader> > LoaderQueue;
    LoaderQueue& loadersPending(WebCore::ResourceLoadPriority priority) { return m_loadersPending[priority]; }

    typedef Deque<RefPtr<SyncNetworkResourceLoader> > SyncLoaderQueue;
    SyncLoaderQueue& syncLoadersPending() { return m_syncLoadersPending; }

private:                    
    LoaderQueue m_loadersPending[WebCore::ResourceLoadPriorityHighest + 1];
    typedef HashSet<ResourceLoadIdentifier> ResourceLoadIdentifierSet;
    ResourceLoadIdentifierSet m_resourceIdentifiersLoading;
    
    SyncLoaderQueue m_syncLoadersPending;
    HashSet<RefPtr<SyncNetworkResourceLoader> > m_syncLoadersLoading;

    const String m_name;
    int m_maxRequestsInFlight;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)

#endif // #ifndef HostRecord_h
