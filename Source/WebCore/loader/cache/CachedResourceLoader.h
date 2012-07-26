/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller <mueller@kde.org>
    Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
    Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/

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

    This class provides all functionality needed for loading images, style sheets and html
    pages from the web. It has a memory cache for these objects.
*/

#ifndef CachedResourceLoader_h
#define CachedResourceLoader_h

#include "CachedResource.h"
#include "CachedResourceHandle.h"
#include "CachePolicy.h"
#include "ResourceLoadPriority.h"
#include "Timer.h"
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class CachedCSSStyleSheet;
class CachedSVGDocument;
class CachedFont;
class CachedImage;
class CachedRawResource;
class CachedScript;
class CachedShader;
class CachedTextTrack;
class CachedXSLStyleSheet;
class Document;
class Frame;
class ImageLoader;
class KURL;

// The CachedResourceLoader manages the loading of scripts/images/stylesheets for a single document.
class CachedResourceLoader {
    WTF_MAKE_NONCOPYABLE(CachedResourceLoader); WTF_MAKE_FAST_ALLOCATED;
friend class ImageLoader;
friend class ResourceCacheValidationSuppressor;

public:
    CachedResourceLoader(Document*);
    ~CachedResourceLoader();

    CachedResourceHandle<CachedImage> requestImage(ResourceRequest&);
    CachedResourceHandle<CachedCSSStyleSheet> requestCSSStyleSheet(ResourceRequest&, const String& charset, ResourceLoadPriority = ResourceLoadPriorityUnresolved);
    CachedResourceHandle<CachedCSSStyleSheet> requestUserCSSStyleSheet(ResourceRequest&, const String& charset);
    CachedResourceHandle<CachedScript> requestScript(ResourceRequest&, const String& charset);
    CachedResourceHandle<CachedFont> requestFont(ResourceRequest&);
    CachedResourceHandle<CachedRawResource> requestRawResource(ResourceRequest&, const ResourceLoaderOptions&);

#if ENABLE(SVG)
    CachedResourceHandle<CachedSVGDocument> requestSVGDocument(ResourceRequest&);
#endif
#if ENABLE(XSLT)
    CachedResourceHandle<CachedXSLStyleSheet> requestXSLStyleSheet(ResourceRequest&);
#endif
#if ENABLE(LINK_PREFETCH)
    CachedResourceHandle<CachedResource> requestLinkResource(CachedResource::Type, ResourceRequest&, ResourceLoadPriority = ResourceLoadPriorityUnresolved);
#endif
#if ENABLE(VIDEO_TRACK)
    CachedResourceHandle<CachedTextTrack> requestTextTrack(ResourceRequest&);
#endif
#if ENABLE(CSS_SHADERS)
    CachedResourceHandle<CachedShader> requestShader(ResourceRequest&);
#endif

    // Logs an access denied message to the console for the specified URL.
    void printAccessDeniedMessage(const KURL& url) const;

    CachedResource* cachedResource(const String& url) const;
    CachedResource* cachedResource(const KURL& url) const;
    
    typedef HashMap<String, CachedResourceHandle<CachedResource> > DocumentResourceMap;
    const DocumentResourceMap& allCachedResources() const { return m_documentResources; }

    bool autoLoadImages() const { return m_autoLoadImages; }
    void setAutoLoadImages(bool);
    
    CachePolicy cachePolicy() const;
    
    Frame* frame() const; // Can be NULL
    Document* document() const { return m_document; }

    void removeCachedResource(CachedResource*) const;
    void loadDone();
    
    void incrementRequestCount(const CachedResource*);
    void decrementRequestCount(const CachedResource*);
    int requestCount() const { return m_requestCount; }

    bool isPreloaded(const String& urlString) const;
    void clearPreloads();
    void clearPendingPreloads();
    void preload(CachedResource::Type, ResourceRequest&, const String& charset, bool referencedFromBody);
    void checkForPendingPreloads();
    void printPreloadStats();
    bool canRequest(CachedResource::Type, const KURL&, bool forPreload = false);
    
private:
    CachedResourceHandle<CachedResource> requestResource(CachedResource::Type, ResourceRequest&, const String& charset, const ResourceLoaderOptions&, ResourceLoadPriority = ResourceLoadPriorityUnresolved, bool isPreload = false);
    CachedResourceHandle<CachedResource> revalidateResource(CachedResource*, ResourceLoadPriority, const ResourceLoaderOptions&);
    CachedResourceHandle<CachedResource> loadResource(CachedResource::Type, ResourceRequest&, const String& charset, ResourceLoadPriority, const ResourceLoaderOptions&);
    void requestPreload(CachedResource::Type, ResourceRequest&, const String& charset);

    enum RevalidationPolicy { Use, Revalidate, Reload, Load };
    RevalidationPolicy determineRevalidationPolicy(CachedResource::Type, ResourceRequest&, bool forPreload, CachedResource* existingResource) const;
    
    void notifyLoadedFromMemoryCache(CachedResource*);
    bool checkInsecureContent(CachedResource::Type, const KURL&) const;

    void garbageCollectDocumentResourcesTimerFired(Timer<CachedResourceLoader>*);
    void performPostLoadActions();
    
    HashSet<String> m_validatedURLs;
    mutable DocumentResourceMap m_documentResources;
    Document* m_document;
    
    int m_requestCount;
    
    OwnPtr<ListHashSet<CachedResource*> > m_preloads;
    struct PendingPreload {
        CachedResource::Type m_type;
        ResourceRequest m_request;
        String m_charset;
    };
    Deque<PendingPreload> m_pendingPreloads;

    Timer<CachedResourceLoader> m_garbageCollectDocumentResourcesTimer;

    // 30 bits left
    bool m_autoLoadImages : 1;
    bool m_allowStaleResources : 1;
};

class ResourceCacheValidationSuppressor {
    WTF_MAKE_NONCOPYABLE(ResourceCacheValidationSuppressor);
    WTF_MAKE_FAST_ALLOCATED;
public:
    ResourceCacheValidationSuppressor(CachedResourceLoader* loader)
        : m_loader(loader)
        , m_previousState(false)
    {
        if (m_loader) {
            m_previousState = m_loader->m_allowStaleResources;
            m_loader->m_allowStaleResources = true;
        }
    }
    ~ResourceCacheValidationSuppressor()
    {
        if (m_loader)
            m_loader->m_allowStaleResources = m_previousState;
    }
private:
    CachedResourceLoader* m_loader;
    bool m_previousState;
};

} // namespace WebCore

#endif
