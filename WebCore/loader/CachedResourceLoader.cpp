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
#include "CachedResourceLoader.h"

#include "loader.h"
#include "Cache.h"
#include "CachedCSSStyleSheet.h"
#include "CachedFont.h"
#include "CachedImage.h"
#include "CachedScript.h"
#include "CachedXSLStyleSheet.h"
#include "Console.h"
#include "Document.h"
#include "DOMWindow.h"
#include "HTMLElement.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "FrameLoaderClient.h"
#include "PingLoader.h"
#include "SecurityOrigin.h"
#include "Settings.h"
#include <wtf/text/CString.h>

#define PRELOAD_DEBUG 0

namespace WebCore {

CachedResourceLoader::CachedResourceLoader(Document* doc)
    : m_cache(cache())
    , m_doc(doc)
    , m_requestCount(0)
    , m_autoLoadImages(true)
    , m_loadInProgress(false)
    , m_allowStaleResources(false)
{
    m_cache->addCachedResourceLoader(this);
}

CachedResourceLoader::~CachedResourceLoader()
{
    if (m_requestCount)
        m_cache->loader()->cancelRequests(this);

    clearPreloads();
    DocumentResourceMap::iterator end = m_documentResources.end();
    for (DocumentResourceMap::iterator it = m_documentResources.begin(); it != end; ++it)
        it->second->setCachedResourceLoader(0);
    m_cache->removeCachedResourceLoader(this);

    // Make sure no requests still point to this CachedResourceLoader
    ASSERT(m_requestCount == 0);
}

Frame* CachedResourceLoader::frame() const
{
    return m_doc->frame();
}

void CachedResourceLoader::checkForReload(const KURL& fullURL)
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
    case CachePolicyAllowStale:
        return;
    }

    m_reloadedURLs.add(fullURL.string());
}

CachedImage* CachedResourceLoader::requestImage(const String& url)
{
    if (Frame* f = frame()) {
        Settings* settings = f->settings();
        if (!f->loader()->client()->allowImages(!settings || settings->areImagesEnabled()))
            return 0;

        if (f->loader()->pageDismissalEventBeingDispatched()) {
            KURL completeURL = m_doc->completeURL(url);
            if (completeURL.isValid() && canRequest(CachedResource::ImageResource, completeURL))
                PingLoader::loadImage(f, completeURL);
            return 0;
        }
    }
    CachedImage* resource = static_cast<CachedImage*>(requestResource(CachedResource::ImageResource, url, String()));
    if (autoLoadImages() && resource && resource->stillNeedsLoad()) {
        resource->setLoading(true);
        cache()->loader()->load(this, resource, true);
    }
    return resource;
}

CachedFont* CachedResourceLoader::requestFont(const String& url)
{
    return static_cast<CachedFont*>(requestResource(CachedResource::FontResource, url, String()));
}

CachedCSSStyleSheet* CachedResourceLoader::requestCSSStyleSheet(const String& url, const String& charset)
{
    return static_cast<CachedCSSStyleSheet*>(requestResource(CachedResource::CSSStyleSheet, url, charset));
}

CachedCSSStyleSheet* CachedResourceLoader::requestUserCSSStyleSheet(const String& url, const String& charset)
{
    return cache()->requestUserCSSStyleSheet(this, url, charset);
}

CachedScript* CachedResourceLoader::requestScript(const String& url, const String& charset)
{
    return static_cast<CachedScript*>(requestResource(CachedResource::Script, url, charset));
}

#if ENABLE(XSLT)
CachedXSLStyleSheet* CachedResourceLoader::requestXSLStyleSheet(const String& url)
{
    return static_cast<CachedXSLStyleSheet*>(requestResource(CachedResource::XSLStyleSheet, url, String()));
}
#endif

#if ENABLE(LINK_PREFETCH)
CachedResource* CachedResourceLoader::requestLinkPrefetch(const String& url)
{
    ASSERT(frame());
    return requestResource(CachedResource::LinkPrefetch, url, String());
}
#endif

bool CachedResourceLoader::canRequest(CachedResource::Type type, const KURL& url)
{
    // Some types of resources can be loaded only from the same origin.  Other
    // types of resources, like Images, Scripts, and CSS, can be loaded from
    // any URL.
    switch (type) {
    case CachedResource::ImageResource:
    case CachedResource::CSSStyleSheet:
    case CachedResource::Script:
    case CachedResource::FontResource:
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
#endif
        // These types of resources can be loaded from any origin.
        // FIXME: Are we sure about CachedResource::FontResource?
        break;
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
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

    // Given that the load is allowed by the same-origin policy, we should
    // check whether the load passes the mixed-content policy.
    //
    // Note: Currently, we always allow mixed content, but we generate a
    //       callback to the FrameLoaderClient in case the embedder wants to
    //       update any security indicators.
    // 
    switch (type) {
    case CachedResource::Script:
#if ENABLE(XSLT)
    case CachedResource::XSLStyleSheet:
#endif
        // These resource can inject script into the current document.
        if (Frame* f = frame())
            f->loader()->checkIfRunInsecureContent(m_doc->securityOrigin(), url);
        break;
    case CachedResource::ImageResource:
    case CachedResource::CSSStyleSheet:
    case CachedResource::FontResource: {
        // These resources can corrupt only the frame's pixels.
        if (Frame* f = frame()) {
            Frame* top = f->tree()->top();
            top->loader()->checkIfDisplayInsecureContent(top->document()->securityOrigin(), url);
        }
        break;
    }
#if ENABLE(LINK_PREFETCH)
    case CachedResource::LinkPrefetch:
        // Prefetch cannot affect the current document.
        break;
#endif
    default:
        ASSERT_NOT_REACHED();
        break;
    }
    // FIXME: Consider letting the embedder block mixed content loads.
    return true;
}

CachedResource* CachedResourceLoader::requestResource(CachedResource::Type type, const String& url, const String& charset, bool isPreload)
{
    KURL fullURL = m_doc->completeURL(url);

    if (!fullURL.isValid() || !canRequest(type, fullURL))
        return 0;

    if (cache()->disabled()) {
        DocumentResourceMap::iterator it = m_documentResources.find(fullURL.string());
        
        if (it != m_documentResources.end()) {
            it->second->setCachedResourceLoader(0);
            m_documentResources.remove(it);
        }
    }

    checkForReload(fullURL);

    CachedResource* resource = cache()->requestResource(this, type, fullURL, charset, isPreload);
    if (resource) {
        // Check final URL of resource to catch redirects.
        // See <https://bugs.webkit.org/show_bug.cgi?id=21963>.
        if (fullURL != resource->url() && !canRequest(type, KURL(ParsedURLString, resource->url())))
            return 0;

        m_documentResources.set(resource->url(), resource);
        checkCacheObjectStatus(resource);
    }
    return resource;
}

void CachedResourceLoader::printAccessDeniedMessage(const KURL& url) const
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
    frame()->domWindow()->console()->addMessage(OtherMessageSource, LogMessageType, ErrorMessageLevel, message, 1, String());
}

void CachedResourceLoader::setAutoLoadImages(bool enable)
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

CachePolicy CachedResourceLoader::cachePolicy() const
{
    return frame() ? frame()->loader()->subresourceCachePolicy() : CachePolicyVerify;
}

void CachedResourceLoader::removeCachedResource(CachedResource* resource) const
{
#ifndef NDEBUG
    DocumentResourceMap::iterator it = m_documentResources.find(resource->url());
    if (it != m_documentResources.end())
        ASSERT(it->second.get() == resource);
#endif
    m_documentResources.remove(resource->url());
}

void CachedResourceLoader::setLoadInProgress(bool load)
{
    m_loadInProgress = load;
    if (!load && frame())
        frame()->loader()->loadDone();
}

void CachedResourceLoader::checkCacheObjectStatus(CachedResource* resource)
{
    // Return from the function for objects that we didn't load from the cache or if we don't have a frame.
    if (!resource || !frame() || resource->status() != CachedResource::Cached)
        return;

    // FIXME: If the WebKit client changes or cancels the request, WebCore does not respect this and continues the load.
    frame()->loader()->loadedResourceFromMemoryCache(resource);
}

void CachedResourceLoader::incrementRequestCount(const CachedResource* res)
{
    if (res->isPrefetch())
        return;

    ++m_requestCount;
}

void CachedResourceLoader::decrementRequestCount(const CachedResource* res)
{
    if (res->isPrefetch())
        return;

    --m_requestCount;
    ASSERT(m_requestCount > -1);
}

int CachedResourceLoader::requestCount()
{
    if (loadInProgress())
         return m_requestCount + 1;
    return m_requestCount;
}
    
void CachedResourceLoader::preload(CachedResource::Type type, const String& url, const String& charset, bool referencedFromBody)
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

void CachedResourceLoader::checkForPendingPreloads() 
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

void CachedResourceLoader::requestPreload(CachedResource::Type type, const String& url, const String& charset)
{
    String encoding;
    if (type == CachedResource::Script || type == CachedResource::CSSStyleSheet)
        encoding = charset.isEmpty() ? m_doc->frame()->loader()->writer()->encoding() : charset;

    CachedResource* resource = requestResource(type, url, encoding, true);
    if (!resource || (m_preloads && m_preloads->contains(resource)))
        return;
    resource->increasePreloadCount();

    if (!m_preloads)
        m_preloads = adoptPtr(new ListHashSet<CachedResource*>);
    m_preloads->add(resource);

#if PRELOAD_DEBUG
    printf("PRELOADING %s\n",  resource->url().latin1().data());
#endif
}

void CachedResourceLoader::clearPreloads()
{
#if PRELOAD_DEBUG
    printPreloadStats();
#endif
    if (!m_preloads)
        return;

    ListHashSet<CachedResource*>::iterator end = m_preloads->end();
    for (ListHashSet<CachedResource*>::iterator it = m_preloads->begin(); it != end; ++it) {
        CachedResource* res = *it;
        res->decreasePreloadCount();
        if (res->canDelete() && !res->inCache())
            delete res;
        else if (res->preloadResult() == CachedResource::PreloadNotReferenced)
            cache()->remove(res);
    }
    m_preloads.clear();
}

void CachedResourceLoader::clearPendingPreloads()
{
    m_pendingPreloads.clear();
}

#if PRELOAD_DEBUG
void CachedResourceLoader::printPreloadStats()
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
