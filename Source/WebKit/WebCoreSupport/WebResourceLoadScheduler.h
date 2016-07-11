/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2006-2008, 2015 Apple Inc. All rights reserved.
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

#ifndef WebResourceLoadScheduler_h
#define WebResourceLoadScheduler_h

#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/LoaderStrategy.h>
#include <WebCore/ResourceLoadPriority.h>
#include <WebCore/ResourceLoaderOptions.h>
#include <WebCore/Timer.h>
#include <array>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

class WebResourceLoadScheduler;

WebResourceLoadScheduler& webResourceLoadScheduler();

class WebResourceLoadScheduler : public WebCore::LoaderStrategy {
    WTF_MAKE_NONCOPYABLE(WebResourceLoadScheduler); WTF_MAKE_FAST_ALLOCATED;
public:
    WebResourceLoadScheduler();

    RefPtr<WebCore::SubresourceLoader> loadResource(WebCore::Frame&, WebCore::CachedResource&, const WebCore::ResourceRequest&, const WebCore::ResourceLoaderOptions&) override;
    void loadResourceSynchronously(WebCore::NetworkingContext*, unsigned long, const WebCore::ResourceRequest&, WebCore::StoredCredentials, WebCore::ClientCredentialPolicy, WebCore::ResourceError&, WebCore::ResourceResponse&, Vector<char>&) override;
    void remove(WebCore::ResourceLoader*) override;
    void setDefersLoading(WebCore::ResourceLoader*, bool) override;
    void crossOriginRedirectReceived(WebCore::ResourceLoader*, const WebCore::URL& redirectURL) override;
    
    void servePendingRequests(WebCore::ResourceLoadPriority minimumPriority = WebCore::ResourceLoadPriority::VeryLow) override;
    void suspendPendingRequests() override;
    void resumePendingRequests() override;

    void createPingHandle(WebCore::NetworkingContext*, WebCore::ResourceRequest&, bool shouldUseCredentialStorage) override;

    bool isSerialLoadingEnabled() const { return m_isSerialLoadingEnabled; }
    void setSerialLoadingEnabled(bool b) { m_isSerialLoadingEnabled = b; }

    RefPtr<WebCore::NetscapePlugInStreamLoader> schedulePluginStreamLoad(WebCore::Frame&, WebCore::NetscapePlugInStreamLoaderClient&, const WebCore::ResourceRequest&);

protected:
    virtual ~WebResourceLoadScheduler();

private:
    void scheduleLoad(WebCore::ResourceLoader*);
    void scheduleServePendingRequests();
    void requestTimerFired();

    bool isSuspendingPendingRequests() const { return !!m_suspendPendingRequestsCount; }

    class HostInformation {
        WTF_MAKE_NONCOPYABLE(HostInformation); WTF_MAKE_FAST_ALLOCATED;
    public:
        HostInformation(const String&, unsigned);
        ~HostInformation();
        
        const String& name() const { return m_name; }
        void schedule(WebCore::ResourceLoader*, WebCore::ResourceLoadPriority = WebCore::ResourceLoadPriority::VeryLow);
        void addLoadInProgress(WebCore::ResourceLoader*);
        void remove(WebCore::ResourceLoader*);
        bool hasRequests() const;
        bool limitRequests(WebCore::ResourceLoadPriority) const;

        typedef Deque<RefPtr<WebCore::ResourceLoader>> RequestQueue;
        RequestQueue& requestsPending(WebCore::ResourceLoadPriority priority) { return m_requestsPending[priorityToIndex(priority)]; }

    private:
        static unsigned priorityToIndex(WebCore::ResourceLoadPriority);

        std::array<RequestQueue, WebCore::resourceLoadPriorityCount> m_requestsPending;
        typedef HashSet<RefPtr<WebCore::ResourceLoader>> RequestMap;
        RequestMap m_requestsLoading;
        const String m_name;
        const unsigned m_maxRequestsInFlight;
    };

    enum CreateHostPolicy {
        CreateIfNotFound,
        FindOnly
    };
    
    HostInformation* hostForURL(const WebCore::URL&, CreateHostPolicy = FindOnly);
    WEBCORE_EXPORT void servePendingRequests(HostInformation*, WebCore::ResourceLoadPriority);

    typedef HashMap<String, HostInformation*, StringHash> HostMap;
    HostMap m_hosts;
    HostInformation* m_nonHTTPProtocolHost;
        
    WebCore::Timer m_requestTimer;

    unsigned m_suspendPendingRequestsCount;
    bool m_isSerialLoadingEnabled;
};

#endif
