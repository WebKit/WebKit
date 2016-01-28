/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.

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

#include "CachePolicy.h"
#include "CacheValidation.h"
#include "FrameLoaderTypes.h"
#include "ResourceError.h"
#include "ResourceLoadPriority.h"
#include "ResourceLoaderOptions.h"
#include "ResourceRequest.h"
#include "ResourceResponse.h"
#include "SessionID.h"
#include "Timer.h"
#include <time.h>
#include <wtf/HashCountedSet.h>
#include <wtf/HashSet.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class MemoryCache;
class CachedResourceClient;
class CachedResourceHandleBase;
class CachedResourceLoader;
class InspectorResource;
class SecurityOrigin;
class SharedBuffer;
class SubresourceLoader;

// A resource that is held in the cache. Classes who want to use this object should derive
// from CachedResourceClient, to get the function calls in case the requested data has arrived.
// This class also does the actual communication with the loader to obtain the resource from the network.
class CachedResource {
    WTF_MAKE_NONCOPYABLE(CachedResource); WTF_MAKE_FAST_ALLOCATED;
    friend class MemoryCache;
    friend class InspectorResource;
    
public:
    enum Type {
        MainResource,
        ImageResource,
        CSSStyleSheet,
        Script,
        FontResource,
#if ENABLE(SVG_FONTS)
        SVGFontResource,
#endif
        RawResource,
        SVGDocumentResource
#if ENABLE(XSLT)
        , XSLStyleSheet
#endif
#if ENABLE(LINK_PREFETCH)
        , LinkPrefetch
        , LinkSubresource
#endif
#if ENABLE(VIDEO_TRACK)
        , TextTrackResource
#endif
    };

    enum Status {
        Unknown,      // let cache decide what to do with it
        Pending,      // only partially loaded
        Cached,       // regular case
        LoadError,
        DecodeError
    };

    CachedResource(const ResourceRequest&, Type, SessionID);
    virtual ~CachedResource();

    virtual void load(CachedResourceLoader&, const ResourceLoaderOptions&);

    virtual void setEncoding(const String&) { }
    virtual String encoding() const { return String(); }
    virtual void addDataBuffer(SharedBuffer&);
    virtual void addData(const char* data, unsigned length);
    virtual void finishLoading(SharedBuffer*);
    virtual void error(CachedResource::Status);

    void setResourceError(const ResourceError& error) { m_error = error; }
    const ResourceError& resourceError() const { return m_error; }

    virtual bool shouldIgnoreHTTPStatusCodeErrors() const { return false; }

    ResourceRequest& resourceRequest() { return m_resourceRequest; }
    const URL& url() const { return m_resourceRequest.url();}
#if ENABLE(CACHE_PARTITIONING)
    const String& cachePartition() const { return m_resourceRequest.cachePartition(); }
#endif
    SessionID sessionID() const { return m_sessionID; }
    Type type() const { return static_cast<Type>(m_type); }
    
    ResourceLoadPriority loadPriority() const { return m_loadPriority; }
    void setLoadPriority(const Optional<ResourceLoadPriority>&);

    WEBCORE_EXPORT void addClient(CachedResourceClient*);
    WEBCORE_EXPORT void removeClient(CachedResourceClient*);
    bool hasClients() const { return !m_clients.isEmpty() || !m_clientsAwaitingCallback.isEmpty(); }
    bool hasClient(CachedResourceClient* client) { return m_clients.contains(client) || m_clientsAwaitingCallback.contains(client); }
    bool deleteIfPossible();

    enum PreloadResult {
        PreloadNotReferenced,
        PreloadReferenced,
        PreloadReferencedWhileLoading,
        PreloadReferencedWhileComplete
    };
    PreloadResult preloadResult() const { return static_cast<PreloadResult>(m_preloadResult); }

    virtual void didAddClient(CachedResourceClient*);
    virtual void didRemoveClient(CachedResourceClient*) { }
    virtual void allClientsRemoved() { }
    void destroyDecodedDataIfNeeded();

    unsigned count() const { return m_clients.size(); }

    Status status() const { return static_cast<Status>(m_status); }
    void setStatus(Status status) { m_status = status; }

    unsigned size() const { return encodedSize() + decodedSize() + overheadSize(); }
    unsigned encodedSize() const { return m_encodedSize; }
    unsigned decodedSize() const { return m_decodedSize; }
    unsigned overheadSize() const;

    virtual bool decodedDataIsPurgeable() const { return false; }
    
    bool isLoaded() const { return !m_loading; } // FIXME. Method name is inaccurate. Loading might not have started yet.

    bool isLoading() const { return m_loading; }
    void setLoading(bool b) { m_loading = b; }
    virtual bool stillNeedsLoad() const { return false; }

    SubresourceLoader* loader() { return m_loader.get(); }

    bool areAllClientsXMLHttpRequests() const;

    bool isImage() const { return type() == ImageResource; }
    // FIXME: CachedRawResource could be either a main resource or a raw XHR resource.
    bool isMainOrRawResource() const { return type() == MainResource || type() == RawResource; }
    bool ignoreForRequestCount() const
    {
        return type() == MainResource
#if ENABLE(LINK_PREFETCH)
            || type() == LinkPrefetch
            || type() == LinkSubresource
#endif
            || type() == RawResource;
    }

    unsigned accessCount() const { return m_accessCount; }
    void increaseAccessCount() { m_accessCount++; }

    // Computes the status of an object after loading.  
    // Updates the expire date on the cache entry file
    void finish();

    bool passesAccessControlCheck(SecurityOrigin&);
    bool passesSameOriginPolicyCheck(SecurityOrigin&);

    // Called by the cache if the object has been removed from the cache
    // while still being referenced. This means the object should delete itself
    // if the number of clients observing it ever drops to 0.
    // The resource can be brought back to cache after successful revalidation.
    void setInCache(bool inCache) { m_inCache = inCache; }
    bool inCache() const { return m_inCache; }
    
    void clearLoader();

    SharedBuffer* resourceBuffer() const { return m_data.get(); }

    virtual void redirectReceived(ResourceRequest&, const ResourceResponse&);
    virtual void responseReceived(const ResourceResponse&);
    void setResponse(const ResourceResponse& response) { m_response = response; }
    const ResourceResponse& response() const { return m_response; }
    // This is the same as response() except after HTTP redirect to data: URL.
    const ResourceResponse& responseForSameOriginPolicyChecks() const;

    bool canDelete() const { return !hasClients() && !m_loader && !m_preloadCount && !m_handleCount && !m_resourceToRevalidate && !m_proxyResource; }
    bool hasOneHandle() const { return m_handleCount == 1; }

    bool isExpired() const;

    // List of acceptable MIME types separated by ",".
    // A MIME type may contain a wildcard, e.g. "text/*".
    String accept() const { return m_accept; }
    void setAccept(const String& accept) { m_accept = accept; }

    void cancelLoad();
    bool wasCanceled() const { return m_error.isCancellation(); }
    bool errorOccurred() const { return m_status == LoadError || m_status == DecodeError; }
    bool loadFailedOrCanceled() const { return !m_error.isNull(); }

    bool shouldSendResourceLoadCallbacks() const { return m_options.sendLoadCallbacks() == SendCallbacks; }
    DataBufferingPolicy dataBufferingPolicy() const { return m_options.dataBufferingPolicy(); }

    bool allowsCaching() const { return m_options.cachingPolicy() == CachingPolicy::AllowCaching; }
    
    virtual void destroyDecodedData() { }

    void setOwningCachedResourceLoader(CachedResourceLoader* cachedResourceLoader) { m_owningCachedResourceLoader = cachedResourceLoader; }
    
    bool isPreloaded() const { return m_preloadCount; }
    void increasePreloadCount() { ++m_preloadCount; }
    void decreasePreloadCount() { ASSERT(m_preloadCount); --m_preloadCount; }
    
    void registerHandle(CachedResourceHandleBase* h);
    WEBCORE_EXPORT void unregisterHandle(CachedResourceHandleBase* h);
    
    bool canUseCacheValidator() const;

    enum class RevalidationDecision { No, YesDueToCachePolicy, YesDueToNoStore, YesDueToNoCache, YesDueToExpired };
    virtual RevalidationDecision makeRevalidationDecision(CachePolicy) const;
    bool redirectChainAllowsReuse(ReuseExpiredRedirectionOrNot) const;

    bool isCacheValidator() const { return m_resourceToRevalidate; }
    CachedResource* resourceToRevalidate() const { return m_resourceToRevalidate; }
    
    // HTTP revalidation support methods for CachedResourceLoader.
    void setResourceToRevalidate(CachedResource*);
    virtual void switchClientsToRevalidatedResource();
    void clearResourceToRevalidate();
    void updateResponseAfterRevalidation(const ResourceResponse& validatingResponse);
    bool validationInProgress() const { return m_proxyResource; }
    bool validationCompleting() const { return m_proxyResource && m_proxyResource->m_switchingClientsToRevalidatedResource; }

    virtual void didSendData(unsigned long long /* bytesSent */, unsigned long long /* totalBytesToBeSent */) { }

    void setLoadFinishTime(double finishTime) { m_loadFinishTime = finishTime; }
    double loadFinishTime() const { return m_loadFinishTime; }

#if USE(FOUNDATION) || USE(SOUP)
    WEBCORE_EXPORT void tryReplaceEncodedData(SharedBuffer&);
#endif

#if USE(SOUP)
    virtual char* getOrCreateReadBuffer(size_t /* requestedSize */, size_t& /* actualSize */) { return nullptr; }
#endif

    unsigned long identifierForLoadWithoutResourceLoader() const { return m_identifierForLoadWithoutResourceLoader; }

protected:
    void setEncodedSize(unsigned);
    void setDecodedSize(unsigned);
    void didAccessDecodedData(double timeStamp);

    virtual void didReplaceSharedBufferContents() { }

    // FIXME: Make the rest of these data members private and use functions in derived classes instead.
    HashCountedSet<CachedResourceClient*> m_clients;
    ResourceRequest m_resourceRequest;
    RefPtr<SubresourceLoader> m_loader;
    ResourceLoaderOptions m_options;
    ResourceResponse m_response;
    ResourceResponse m_redirectResponseForSameOriginPolicyChecks;
    RefPtr<SharedBuffer> m_data;
    DeferrableOneShotTimer m_decodedDataDeletionTimer;

private:
    class Callback;

    bool addClientToSet(CachedResourceClient*);

    void decodedDataDeletionTimerFired();

    virtual void checkNotify();
    virtual bool mayTryReplaceEncodedData() const { return false; }

    std::chrono::microseconds freshnessLifetime(const ResourceResponse&) const;

    void addAdditionalRequestHeaders(CachedResourceLoader&);
    void failBeforeStarting();

    HashMap<CachedResourceClient*, std::unique_ptr<Callback>> m_clientsAwaitingCallback;
    SessionID m_sessionID;
    String m_accept;
    ResourceLoadPriority m_loadPriority;
    std::chrono::system_clock::time_point m_responseTimestamp;

    String m_fragmentIdentifierForRequest;

    ResourceError m_error;

    double m_lastDecodedAccessTime; // Used as a "thrash guard" in the cache
    double m_loadFinishTime;

    unsigned m_encodedSize;
    unsigned m_decodedSize;
    unsigned m_accessCount;
    unsigned m_handleCount;
    unsigned m_preloadCount;

    unsigned m_preloadResult : 2; // PreloadResult

    bool m_requestedFromNetworkingLayer : 1;

    bool m_inCache : 1;
    bool m_loading : 1;

    bool m_switchingClientsToRevalidatedResource : 1;

    unsigned m_type : 4; // Type
    unsigned m_status : 3; // Status

#ifndef NDEBUG
    bool m_deleted;
    unsigned m_lruIndex;
#endif

    CachedResourceLoader* m_owningCachedResourceLoader; // only non-null for resources that are not in the cache
    
    // If this field is non-null we are using the resource as a proxy for checking whether an existing resource is still up to date
    // using HTTP If-Modified-Since/If-None-Match headers. If the response is 304 all clients of this resource are moved
    // to to be clients of m_resourceToRevalidate and the resource is deleted. If not, the field is zeroed and this
    // resources becomes normal resource load.
    CachedResource* m_resourceToRevalidate;

    // If this field is non-null, the resource has a proxy for checking whether it is still up to date (see m_resourceToRevalidate).
    CachedResource* m_proxyResource;

    // These handles will need to be updated to point to the m_resourceToRevalidate in case we get 304 response.
    HashSet<CachedResourceHandleBase*> m_handlesToRevalidate;

    RedirectChainCacheStatus m_redirectChainCacheStatus;

    unsigned long m_identifierForLoadWithoutResourceLoader { 0 };
};

class CachedResource::Callback {
#if !COMPILER(MSVC)
    WTF_MAKE_FAST_ALLOCATED;
#endif
public:
    Callback(CachedResource&, CachedResourceClient&);

    void cancel();

private:
    void timerFired();

    CachedResource& m_resource;
    CachedResourceClient& m_client;
    Timer m_timer;
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CACHED_RESOURCE(ToClassName, CachedResourceType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToClassName) \
    static bool isType(const WebCore::CachedResource& resource) { return resource.type() == WebCore::CachedResourceType; } \
SPECIALIZE_TYPE_TRAITS_END()

#endif // CachedResource_h
