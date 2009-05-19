/*
    Copyright (C) 1998 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2001 Dirk Mueller (mueller@kde.org)
    Copyright (C) 2002 Waldo Bastian (bastian@kde.org)
    Copyright (C) 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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

#include "config.h"
#include "DocLoader.h"

#include "Cache.h"
#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "Console.h"
#include "CString.h"
#include "Document.h"
#include "DOMWindow.h"
#include "HTMLElement.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "loader.h"
#include "SecurityOrigin.h"
#include "Settings.h"

#define PRELOAD_DEBUG 0

namespace WebCore {

DocLoader::DocLoader(Document* doc)
    : m_cache(cache())
    , m_doc(doc)
    , m_requestCount(0)
    , m_autoLoadImages(true)
    , m_loadInProgress(false)
    , m_allowStaleResources(false)
{
    m_cache->addDocLoader(this);
}

DocLoader::~DocLoader()
{
    clearPreloads();
    DocumentResourceMap::iterator end = m_documentResources.end();
    for (DocumentResourceMap::iterator it = m_documentResources.begin(); it != end; ++it)
        it->second->setDocLoader(0);
    m_cache->removeDocLoader(this);

    // Make sure no requests still point to this DocLoader
    ASSERT(m_requestCount == 0);
}

Frame* DocLoader::frame() const
{
    return m_doc->frame();
}

void DocLoader::checkForReload(const KURL& fullURL)
{
    if (m_allowStaleResources)
        return; // Don't reload resources while pasting

    if (fullURL.isEmpty())
        return;
    
    if (m_reloadedURLs.contains(fullURL.string()))
        return;
    
    CachedResource* existing = cache()->resourceForURL(fullURL.string());
    if (!existing || existing->isPreloaded())
        return;

    switch (cachePolicy()) {
    case CachePolicyVerify:
        if (!existing->mustRevalidate(CachePolicyVerify))
            return;
        cache()->revalidateResource(existing, this);
        break;
    case CachePolicyCache:
        if (!existing->mustRevalidate(CachePolicyCache))
            return;
        cache()->revalidateResource(existing, this);
        break;
    case CachePolicyReload:
        cache()->remove(existing);        
        break;
    case CachePolicyRevalidate:
        cache()->revalidateResource(existing, this);
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_reloadedURLs.add(fullURL.string());
}

CachedImage* DocLoader::requestImage(const String& url)
{
    CachedImage* resource = static_cast<CachedImage*>(requestResource(CachedResource::ImageResource, url, String()));
    if (autoLoadImages() && resource && resource->stillNeedsLoad()) {
        resource->setLoading(true);
        cache()->loader()->load(this, resource, true);
    }
    return resource;
}

CachedFont* DocLoader::requestFont(const String& url)
{
    return static_cast<CachedFont*>(requestResource(CachedResource::FontResource, url, String()));
}

CachedCSSStyleSheet* DocLoader::requestCSSStyleSheet(const String& url, const String& charset)
{
    return static_cast<CachedCSSStyleSheet*>(requestResource(CachedResource::CSSStyleSheet, url, charset));
}

CachedCSSStyleSheet* DocLoader::requestUserCSSStyleSheet(const String& url, const String& charset)
{
    return cache()->requestUserCSSStyleSheet(this, url, charset);
}

CachedScript* DocLoader::requestScript(const String& url, const String& charset)
{
    return static_cast<CachedScript*>(requestResource(CachedResource::Script, url, charset));
}

#if ENABLE(XSLT)
CachedXSLStyleSheet* DocLoader::requestXSLStyleSheet(const String& url)
{
    return static_cast<CachedXSLStyleSheet*>(requestResource(CachedResource::XSLStyleSheet, url, String()));
}
#endif

#if ENABLE(XBL)
CachedXBLDocument* DocLoader::requestXBLDocument(const String& url)
{
    return static_cast<CachedXSLStyleSheet*>(requestResource(CachedResource::XBL, url, String()));
}
#endif

bool DocLoader::canRequest(CachedResource::Type type, const KURL& url)
{
    // Some types of resources can be loaded only from the same origin.  Other
    // types of resources, like Images, Scripts, and CSS, can be loaded from
    // any URL.
    switch (type) {
    case CachedResource::ImageResource:
    case CachedResource::CSSStyleSheet:
    case CachedResource::Script:
    case CachedResource::FontResource:
        // These types of resources can be loaded from any origin.
        // FIXME: Are we sure about CachedResource::FontResource?
        break;
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
#if ENABLE(XBL)
    case CachedResource::XBL:
#endif
#if ENABLE(XSLT) || ENABLE(XBL)
        if (!m_doc->securityOrigin()->canRequest(url)) {
            printAccessDeniedMessage(url);
            return false;
        }
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    return true;
}

CachedResource* DocLoader::requestResource(CachedResource::Type type, const String& url, const String& charset, bool isPreload)
{
    KURL fullURL = m_doc->completeURL(url);

    if (!fullURL.isValid() || !canRequest(type, fullURL))
        return 0;

    if (cache()->disabled()) {
        DocumentResourceMap::iterator it = m_documentResources.find(fullURL.string());
        
        if (it != m_documentResources.end()) {
            it->second->setDocLoader(0);
            m_documentResources.remove(it);
        }
    }

    checkForReload(fullURL);

    CachedResource* resource = cache()->requestResource(this, type, fullURL, charset, isPreload);
    if (resource) {
        // Check final URL of resource to catch redirects.
        // See <https://bugs.webkit.org/show_bug.cgi?id=21963>.
        if (!canRequest(type, KURL(resource->url())))
            return 0;

        m_documentResources.set(resource->url(), resource);
        checkCacheObjectStatus(resource);
    }
    return resource;
}

void DocLoader::printAccessDeniedMessage(const KURL& url) const
{
    if (url.isNull())
        return;

    if (!frame())
        return;

    Settings* settings = frame()->settings();
    if (!settings || settings->privateBrowsingEnabled())
        return;

    String message = m_doc->url().isNull() ?
        String::format("Unsafe attempt to load URL %s.",
                       url.string().utf8().data()) :
        String::format("Unsafe attempt to load URL %s from frame with URL %s. "
                       "Domains, protocols and ports must match.\n",
                       url.string().utf8().data(),
                       m_doc->url().string().utf8().data());

    // FIXME: provide a real line number and source URL.
    frame()->domWindow()->console()->addMessage(OtherMessageSource, ErrorMessageLevel, message, 1, String());
}

void DocLoader::setAutoLoadImages(bool enable)
{
    if (enable == m_autoLoadImages)
        return;

    m_autoLoadImages = enable;

    if (!m_autoLoadImages)
        return;

    DocumentResourceMap::iterator end = m_documentResources.end();
    for (DocumentResourceMap::iterator it = m_documentResources.begin(); it != end; ++it) {
        CachedResource* resource = it->second.get();
        if (resource->type() == CachedResource::ImageResource) {
            CachedImage* image = const_cast<CachedImage*>(static_cast<const CachedImage*>(resource));

            if (image->stillNeedsLoad())
                cache()->loader()->load(this, image, true);
        }
    }
}

CachePolicy DocLoader::cachePolicy() const
{
    return frame() ? frame()->loader()->subresourceCachePolicy() : CachePolicyVerify;
}

void DocLoader::removeCachedResource(CachedResource* resource) const
{
#ifndef NDEBUG
    DocumentResourceMap::iterator it = m_documentResources.find(resource->url());
    if (it != m_documentResources.end())
        ASSERT(it->second.get() == resource);
#endif
    m_documentResources.remove(resource->url());
}

void DocLoader::setLoadInProgress(bool load)
{
    m_loadInProgress = load;
    if (!load && frame())
        frame()->loader()->loadDone();
}

void DocLoader::checkCacheObjectStatus(CachedResource* resource)
{
    // Return from the function for objects that we didn't load from the cache or if we don't have a frame.
    if (!resource || !frame())
        return;

    switch (resource->status()) {
        case CachedResource::Cached:
            break;
        case CachedResource::NotCached:
        case CachedResource::Unknown:
        case CachedResource::New:
        case CachedResource::Pending:
            return;
    }

    // FIXME: If the WebKit client changes or cancels the request, WebCore does not respect this and continues the load.
    frame()->loader()->loadedResourceFromMemoryCache(resource);
}

void DocLoader::incrementRequestCount()
{
    ++m_requestCount;
}

void DocLoader::decrementRequestCount()
{
    --m_requestCount;
    ASSERT(m_requestCount > -1);
}

int DocLoader::requestCount()
{
    if (loadInProgress())
         return m_requestCount + 1;
    return m_requestCount;
}
    
void DocLoader::preload(CachedResource::Type type, const String& url, const String& charset, bool referencedFromBody)
{
    bool hasRendering = m_doc->body() && m_doc->body()->renderer();
    if (!hasRendering && (referencedFromBody || type == CachedResource::ImageResource)) {
        // Don't preload images or body resources before we have something to draw. This prevents
        // preloads from body delaying first display when bandwidth is limited.
        PendingPreload pendingPreload = { type, url, charset };
        m_pendingPreloads.append(pendingPreload);
        return;
    }
    requestPreload(type, url, charset);
}

void DocLoader::checkForPendingPreloads() 
{
    unsigned count = m_pendingPreloads.size();
    if (!count || !m_doc->body() || !m_doc->body()->renderer())
        return;
    for (unsigned i = 0; i < count; ++i) {
        PendingPreload& preload = m_pendingPreloads[i];
        // Don't request preload if the resource already loaded normally (this will result in double load if the page is being reloaded with cached results ignored).
        if (!cachedResource(m_doc->completeURL(preload.m_url)))
            requestPreload(preload.m_type, preload.m_url, preload.m_charset);
    }
    m_pendingPreloads.clear();
}

void DocLoader::requestPreload(CachedResource::Type type, const String& url, const String& charset)
{
    String encoding;
    if (type == CachedResource::Script || type == CachedResource::CSSStyleSheet)
        encoding = charset.isEmpty() ? m_doc->frame()->loader()->encoding() : charset;

    CachedResource* resource = requestResource(type, url, encoding, true);
    if (!resource || m_preloads.contains(resource))
        return;
    resource->increasePreloadCount();
    m_preloads.add(resource);
#if PRELOAD_DEBUG
    printf("PRELOADING %s\n",  resource->url().latin1().data());
#endif
}

void DocLoader::clearPreloads()
{
#if PRELOAD_DEBUG
    printPreloadStats();
#endif
    ListHashSet<CachedResource*>::iterator end = m_preloads.end();
    for (ListHashSet<CachedResource*>::iterator it = m_preloads.begin(); it != end; ++it) {
        CachedResource* res = *it;
        res->decreasePreloadCount();
        if (res->canDelete() && !res->inCache())
            delete res;
        else if (res->preloadResult() == CachedResource::PreloadNotReferenced)
            cache()->remove(res);
    }
    m_preloads.clear();
}

void DocLoader::clearPendingPreloads()
{
    m_pendingPreloads.clear();
}

#if PRELOAD_DEBUG
void DocLoader::printPreloadStats()
{
    unsigned scripts = 0;
    unsigned scriptMisses = 0;
    unsigned stylesheets = 0;
    unsigned stylesheetMisses = 0;
    unsigned images = 0;
    unsigned imageMisses = 0;
    ListHashSet<CachedResource*>::iterator end = m_preloads.end();
    for (ListHashSet<CachedResource*>::iterator it = m_preloads.begin(); it != end; ++it) {
        CachedResource* res = *it;
        if (res->preloadResult() == CachedResource::PreloadNotReferenced)
            printf("!! UNREFERENCED PRELOAD %s\n", res->url().latin1().data());
        else if (res->preloadResult() == CachedResource::PreloadReferencedWhileComplete)
            printf("HIT COMPLETE PRELOAD %s\n", res->url().latin1().data());
        else if (res->preloadResult() == CachedResource::PreloadReferencedWhileLoading)
            printf("HIT LOADING PRELOAD %s\n", res->url().latin1().data());
        
        if (res->type() == CachedResource::Script) {
            scripts++;
            if (res->preloadResult() < CachedResource::PreloadReferencedWhileLoading)
                scriptMisses++;
        } else if (res->type() == CachedResource::CSSStyleSheet) {
            stylesheets++;
            if (res->preloadResult() < CachedResource::PreloadReferencedWhileLoading)
                stylesheetMisses++;
        } else {
            images++;
            if (res->preloadResult() < CachedResource::PreloadReferencedWhileLoading)
                imageMisses++;
        }
        
        if (res->errorOccurred())
            cache()->remove(res);
        
        res->decreasePreloadCount();
    }
    m_preloads.clear();
    
    if (scripts)
        printf("SCRIPTS: %d (%d hits, hit rate %d%%)\n", scripts, scripts - scriptMisses, (scripts - scriptMisses) * 100 / scripts);
    if (stylesheets)
        printf("STYLESHEETS: %d (%d hits, hit rate %d%%)\n", stylesheets, stylesheets - stylesheetMisses, (stylesheets - stylesheetMisses) * 100 / stylesheets);
    if (images)
        printf("IMAGES:  %d (%d hits, hit rate %d%%)\n", images, images - imageMisses, (images - imageMisses) * 100 / images);
}
#endif
    
}
