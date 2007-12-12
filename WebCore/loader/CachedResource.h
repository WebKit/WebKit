/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.

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

#ifndef CachedResource_h
#define CachedResource_h

#include "PlatformString.h"
#include "ResourceResponse.h"
#include "SharedBuffer.h"
#include <wtf/HashCountedSet.h>
#include <wtf/Vector.h>
#include <time.h>

namespace WebCore {

class Cache;
class CachedResourceClient;
class DocLoader;
class Request;

// A resource that is held in the cache. Classes who want to use this object should derive
// from CachedResourceClient, to get the function calls in case the requested data has arrived.
// This class also does the actual communication with the loader to obtain the resource from the network.
class CachedResource {
    friend class Cache;
    
public:
    enum Type {
        ImageResource,
        CSSStyleSheet,
        Script,
        FontResource
#if ENABLE(XSLT)
        , XSLStyleSheet
#endif
#if ENABLE(XBL)
        , XBL
#endif
    };

    enum Status {
        NotCached,    // this URL is not cached
        Unknown,      // let cache decide what to do with it
        New,          // inserting new item
        Pending,      // only partially loaded
        Cached        // regular case
    };

    CachedResource(const String& url, Type, bool forCache = true, bool sendResourceLoadCallbacks = false);
    virtual ~CachedResource();

    virtual void setEncoding(const String&) { }
    virtual String encoding() const { return String(); }
    virtual void data(PassRefPtr<SharedBuffer> data, bool allDataReceived) = 0;
    virtual void error() = 0;

    const String &url() const { return m_url; }
    Type type() const { return m_type; }

    virtual void ref(CachedResourceClient*);
    void deref(CachedResourceClient*);
    bool referenced() const { return !m_clients.isEmpty(); }
    virtual void allReferencesRemoved() {};

    unsigned count() const { return m_clients.size(); }

    Status status() const { return m_status; }

    unsigned size() const { return encodedSize() + decodedSize(); }
    unsigned encodedSize() const { return m_encodedSize; }
    unsigned decodedSize() const { return m_decodedSize; }
    
    bool isLoaded() const { return !m_loading; }
    void setLoading(bool b) { m_loading = b; }

    virtual bool isImage() const { return false; }

    unsigned accessCount() const { return m_accessCount; }
    void increaseAccessCount() { m_accessCount++; }

    // Computes the status of an object after loading.  
    // Updates the expire date on the cache entry file
    void finish();

    // Called by the cache if the object has been removed from the cache
    // while still being referenced. This means the object should delete itself
    // if the number of clients observing it ever drops to 0.
    void setInCache(bool b) { m_inCache = b; }
    bool inCache() const { return m_inCache; }
    
    void setInLiveDecodedResourcesList(bool b) { m_inLiveDecodedResourcesList = b; }
    bool inLiveDecodedResourcesList() { return m_inLiveDecodedResourcesList; }
    
    void setRequest(Request*);

    SharedBuffer* data() const { return m_data.get(); }

    void setResponse(const ResourceResponse& response) { m_response = response; }
    const ResourceResponse& response() const { return m_response; }
    
    bool canDelete() const { return !referenced() && !m_request; }

    bool isExpired() const;

    virtual bool schedule() const { return false; }

    // List of acceptable MIME types seperated by ",".
    // A MIME type may contain a wildcard, e.g. "text/*".
    String accept() const { return m_accept; }
    void setAccept(const String& accept) { m_accept = accept; }

    bool errorOccurred() const { return m_errorOccurred; }
    bool treatAsLocal() const { return m_shouldTreatAsLocal; }
    bool sendResourceLoadCallbacks() const { return m_sendResourceLoadCallbacks; }
    
    virtual void destroyDecodedData() {};

    void setDocLoader(DocLoader* docLoader) { m_docLoader = docLoader; }
    
protected:
    void setEncodedSize(unsigned);
    void setDecodedSize(unsigned);
    void didAccessDecodedData(double timeStamp);
    
    HashCountedSet<CachedResourceClient*> m_clients;

    String m_url;
    String m_accept;
    Request* m_request;

    ResourceResponse m_response;
    RefPtr<SharedBuffer> m_data;

    Type m_type;
    Status m_status;

    bool m_errorOccurred;

private:
    unsigned m_encodedSize;
    unsigned m_decodedSize;
    unsigned m_accessCount;
    unsigned m_inLiveDecodedResourcesList;
    double m_lastDecodedAccessTime; // Used as a "thrash guard" in the cache
    
    bool m_sendResourceLoadCallbacks;
protected:
    bool m_inCache;
    bool m_loading;
    bool m_expireDateChanged;
#ifndef NDEBUG
    bool m_deleted;
    unsigned m_lruIndex;
#endif

private:
    CachedResource* m_nextInAllResourcesList;
    CachedResource* m_prevInAllResourcesList;
    
    CachedResource* m_nextInLiveResourcesList;
    CachedResource* m_prevInLiveResourcesList;

    bool m_shouldTreatAsLocal;

    DocLoader* m_docLoader; // only non-0 for resources that are not in the cache
};

}

#endif
