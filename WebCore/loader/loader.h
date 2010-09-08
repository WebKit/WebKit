/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2006, 2007, 2008 Apple Inc. All rights reserved.

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

#ifndef loader_h
#define loader_h

#include "FrameLoaderTypes.h"
#include "PlatformString.h"
#include "SubresourceLoaderClient.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringImpl.h>

namespace WebCore {

    class CachedResource;
    class CachedResourceLoader;
    class KURL;
    class Request;

    class Loader : public Noncopyable {
    public:
        Loader();
        ~Loader();

        void load(CachedResourceLoader*, CachedResource*, bool incremental = true, SecurityCheckPolicy = DoSecurityCheck, bool sendResourceLoadCallbacks = true);

        void cancelRequests(CachedResourceLoader*);
        
        enum Priority { VeryLow, Low, Medium, High };
        void servePendingRequests(Priority minimumPriority = VeryLow);

        bool isSuspendingPendingRequests() { return m_isSuspendingPendingRequests; }
        void suspendPendingRequests();
        void resumePendingRequests();
        
        void nonCacheRequestInFlight(const KURL&);
        void nonCacheRequestComplete(const KURL&);

    private:
        Priority determinePriority(const CachedResource*) const;
        void scheduleServePendingRequests();
        
        void requestTimerFired(Timer<Loader>*);

        class Host : public RefCounted<Host>, private SubresourceLoaderClient {
        public:
            static PassRefPtr<Host> create(const AtomicString& name, unsigned maxRequestsInFlight) 
            {
                return adoptRef(new Host(name, maxRequestsInFlight));
            }
            ~Host();
            
            const AtomicString& name() const { return m_name; }
            void addRequest(Request*, Priority);
            void nonCacheRequestInFlight();
            void nonCacheRequestComplete();
            void servePendingRequests(Priority minimumPriority = VeryLow);
            void cancelRequests(CachedResourceLoader*);
            bool hasRequests() const;

            bool processingResource() const { return m_numResourcesProcessing != 0 || m_nonCachedRequestsInFlight !=0; }

        private:
            Host(const AtomicString&, unsigned);

            virtual void didReceiveResponse(SubresourceLoader*, const ResourceResponse&);
            virtual void didReceiveData(SubresourceLoader*, const char*, int);
            virtual void didReceiveCachedMetadata(SubresourceLoader*, const char*, int);
            virtual void didFinishLoading(SubresourceLoader*);
            virtual void didFail(SubresourceLoader*, const ResourceError&);
            
            typedef Deque<Request*> RequestQueue;
            void servePendingRequests(RequestQueue& requestsPending, bool& serveLowerPriority);
            void didFail(SubresourceLoader*, bool cancelled = false);
            void cancelPendingRequests(RequestQueue& requestsPending, CachedResourceLoader*);
            
            RequestQueue m_requestsPending[High + 1];
            typedef HashMap<RefPtr<SubresourceLoader>, Request*> RequestMap;
            RequestMap m_requestsLoading;
            const AtomicString m_name;
            const int m_maxRequestsInFlight;
            int m_numResourcesProcessing;
            int m_nonCachedRequestsInFlight;
        };
        typedef HashMap<AtomicStringImpl*, RefPtr<Host> > HostMap;
        HostMap m_hosts;
        RefPtr<Host> m_nonHTTPProtocolHost;
        
        Timer<Loader> m_requestTimer;

        bool m_isSuspendingPendingRequests;
    };

}

#endif
