/*
    This file is part of the KDE libraries

    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef CachedResource_h
#define CachedResource_h

#include "DeprecatedString.h"
#include "CachePolicy.h"
#include "PlatformString.h"
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <time.h>

#ifdef __OBJC__
@class NSData;
@class NSURLResponse;
#else
class NSData;
class NSURLResponse;
#endif

namespace WebCore
{
    class CachedResourceClient;
    class Request;
    
    /**
     *
     * A cached object. Classes who want to use this object should derive
     * from CachedResourceClient, to get the function calls in case the requested data has arrived.
     *
     * This class also does the actual communication with kio and loads the file.
     */
    class CachedResource
    {
    public:
        enum Type {
            ImageResource,
            CSSStyleSheet,
            Script
#ifdef KHTML_XSLT
            , XSLStyleSheet
#endif
#ifndef KHTML_NO_XBL
            , XBL
#endif
        };

        enum Status {
            NotCached,    // this URL is not cached
            Unknown,      // let cache decide what to do with it
            New,          // inserting new item
            Pending,      // only partially loaded
            Persistent,   // never delete this
            Cached,       // regular case
            Uncacheable   // too big to be cached, will be destroyed as soon as possible
        };

        CachedResource(const String& URL, Type type, CachePolicy cachePolicy, time_t expireDate, int size = 0)
        {
            m_url = URL;
            m_type = type;
            m_status = Pending;
            m_size = size;
            m_free = false;
            m_cachePolicy = cachePolicy;
            m_request = 0;
#if __APPLE__
            m_response = 0;
            m_allData = 0;
#endif
            m_expireDate = expireDate;
            m_deleted = false;
            m_expireDateChanged = false;
            
            m_accessCount = 0;
            
            m_nextInLRUList = 0;
            m_prevInLRUList = 0;
        }
        virtual ~CachedResource();

        virtual void setCharset(const DeprecatedString&) { }
        virtual Vector<char>& bufferData(const char* bytes, int addedSize, Request*);
        virtual void data(Vector<char>&, bool allDataReceived) = 0;
        virtual void error() = 0;

        const String &url() const { return m_url; }
        Type type() const { return m_type; }

        virtual void ref(CachedResourceClient*);
        virtual void deref(CachedResourceClient*);

        int count() const { return m_clients.size(); }

        Status status() const { return m_status; }

        int size() const { return m_size; }

        bool isLoaded() const { return !m_loading; }

        virtual bool isImage() const { return false; }

        int accessCount() const { return m_accessCount; }
        void increaseAccessCount() { m_accessCount++; }
    
        /**
         * computes the status of an object after loading.
         * the result depends on the objects size and the size of the cache
         * also updates the expire date on the cache entry file
         */
        void finish();

        /**
         * Called by the cache if the object has been removed from the cache dict
         * while still being referenced. This means the object should kill itself
         * if its reference counter drops down to zero.
         */
        void setFree(bool b) { m_free = b; }

        CachePolicy cachePolicy() const { return m_cachePolicy; }

        void setRequest(Request*);

#if __APPLE__
        NSURLResponse* response() const { return m_response; }
        void setResponse(NSURLResponse*);
        NSData* allData() const { return m_allData; }
        void setAllData(NSData*);
#endif

        bool canDelete() const { return m_clients.isEmpty() && !m_request; }

        void setExpireDate(time_t expireDate, bool changeHttpCache);

        bool isExpired() const;

        virtual bool schedule() const { return false; }

        // List of acceptable MIME types seperated by ",".
        // A MIME type may contain a wildcard, e.g. "text/*".
        DeprecatedString accept() const { return m_accept; }
        void setAccept(const DeprecatedString& accept) { m_accept = accept; }

    protected:
        void setSize(int size);

        HashSet<CachedResourceClient*> m_clients;

        String m_url;
        DeprecatedString m_accept;
        Request *m_request;
#if __APPLE__
        NSURLResponse *m_response;
        NSData *m_allData;
#endif
        Type m_type;
        Status m_status;
    private:
        int m_size;
        int m_accessCount;
    
    protected:
        time_t m_expireDate;
        CachePolicy m_cachePolicy;
        bool m_free : 1;
        bool m_deleted : 1;
        bool m_loading : 1;
        bool m_expireDateChanged : 1;

    private:
        bool allowInLRUList() const { return canDelete() && status() != Persistent; }

        CachedResource *m_nextInLRUList;
        CachedResource *m_prevInLRUList;
        friend class Cache;
    };

}

#endif
