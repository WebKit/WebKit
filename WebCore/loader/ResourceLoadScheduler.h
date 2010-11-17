/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.
    Copyright (C) 2010 Google Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
 */

#ifndef ResourceLoadScheduler_h
#define ResourceLoadScheduler_h

#include "FrameLoaderTypes.h"
#include "PlatformString.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Frame;
class KURL;
class NetscapePlugInStreamLoader;
class NetscapePlugInStreamLoaderClient;
class ResourceLoader;
class ResourceRequest;
class SubresourceLoader;
class SubresourceLoaderClient;

class ResourceLoadScheduler : public Noncopyable {
public:
    friend ResourceLoadScheduler* resourceLoadScheduler();

    enum Priority { VeryLow, Low, Medium, High, LowestPriority = VeryLow, HighestPriority = High };
    PassRefPtr<SubresourceLoader> scheduleSubresourceLoad(Frame*, SubresourceLoaderClient*, const ResourceRequest&, Priority = Low, SecurityCheckPolicy = DoSecurityCheck, bool sendResourceLoadCallbacks = true, bool shouldContentSniff = true);
    PassRefPtr<NetscapePlugInStreamLoader> schedulePluginStreamLoad(Frame*, NetscapePlugInStreamLoaderClient*, const ResourceRequest&);
    void addMainResourceLoad(ResourceLoader*);
    void remove(ResourceLoader*);
    void crossOriginRedirectReceived(ResourceLoader*, const KURL& redirectURL);
    
    void servePendingRequests(Priority minimumPriority = VeryLow);
    void suspendPendingRequests();
    void resumePendingRequests();

private:
    ResourceLoadScheduler();
    ~ResourceLoadScheduler();

    void scheduleLoad(ResourceLoader*, Priority);
    void scheduleServePendingRequests();
    void requestTimerFired(Timer<ResourceLoadScheduler>*);

    class HostInformation : public Noncopyable {
    public:
        HostInformation(const String&, unsigned);
        ~HostInformation();
        
        const String& name() const { return m_name; }
        void schedule(ResourceLoader*, Priority = VeryLow);
        void addLoadInProgress(ResourceLoader*);
        void remove(ResourceLoader*);
        bool hasRequests() const;
        bool limitRequests() const { return m_requestsLoading.size() >= m_maxRequestsInFlight; }

        typedef Deque<RefPtr<ResourceLoader> > RequestQueue;
        RequestQueue& requestsPending(Priority priority) { return m_requestsPending[priority]; }

    private:                    
        RequestQueue m_requestsPending[HighestPriority + 1];
        typedef HashSet<RefPtr<ResourceLoader> > RequestMap;
        RequestMap m_requestsLoading;
        const String m_name;
        const int m_maxRequestsInFlight;
    };

    enum CreateHostPolicy {
        CreateIfNotFound,
        FindOnly
    };
    
    HostInformation* hostForURL(const KURL&, CreateHostPolicy = FindOnly);
    void servePendingRequests(HostInformation*, Priority);

    typedef HashMap<String, HostInformation*, StringHash> HostMap;
    HostMap m_hosts;
    HostInformation* m_nonHTTPProtocolHost;
        
    Timer<ResourceLoadScheduler> m_requestTimer;

    bool m_isSuspendingPendingRequests;
};

ResourceLoadScheduler* resourceLoadScheduler();

}

#endif
