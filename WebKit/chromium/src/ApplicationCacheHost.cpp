/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ApplicationCacheHost.h"

#if ENABLE(OFFLINE_WEB_APPLICATIONS)

#include "ApplicationCacheHostInternal.h"
#include "DocumentLoader.h"
#include "DOMApplicationCache.h"
#include "Frame.h"
#include "Settings.h"
#include "WebURL.h"
#include "WebURLError.h"
#include "WebURLResponse.h"
#include "WrappedResourceRequest.h"
#include "WrappedResourceResponse.h"

using namespace WebKit;

namespace WebCore {

// We provide a custom implementation of this class that calls out to the
// embedding application instead of using WebCore's built in appcache system.
// This file replaces webcore/appcache/ApplicationCacheHost.cpp in our build.

ApplicationCacheHost::ApplicationCacheHost(DocumentLoader* documentLoader)
    : m_domApplicationCache(0)
    , m_documentLoader(documentLoader)
    , m_defersEvents(true)
{
    ASSERT(m_documentLoader);
}

ApplicationCacheHost::~ApplicationCacheHost()
{
}

void ApplicationCacheHost::maybeLoadMainResource(ResourceRequest& request, SubstituteData&)
{
    // We defer creating the outer host object to avoid spurious creation/destruction
    // around creating empty documents. At this point, we're initiating a main resource
    // load for the document, so its for real.

    if (!isApplicationCacheEnabled())
        return;

    m_internal.set(new ApplicationCacheHostInternal(this));
    if (m_internal->m_outerHost) {
        WrappedResourceRequest wrapped(request);
        m_internal->m_outerHost->willStartMainResourceRequest(wrapped);
    } else
        m_internal.clear();

    // NOTE: The semantics of this method, and others in this interface, are subtly different
    // than the method names would suggest. For example, in this method never returns an appcached
    // response in the SubstituteData out argument, instead we return the appcached response thru
    // the usual resource loading pipeline.
}

void ApplicationCacheHost::selectCacheWithoutManifest()
{
    if (m_internal)
        m_internal->m_outerHost->selectCacheWithoutManifest();
}

void ApplicationCacheHost::selectCacheWithManifest(const KURL& manifestURL)
{
    if (m_internal) {
        if (!m_internal->m_outerHost->selectCacheWithManifest(manifestURL)) {
            // It's a foreign entry, restart the current navigation from the top
            // of the navigation algorithm. The navigation will not result in the
            // same resource being loaded, because "foreign" entries are never picked
            // during navigation.
            // see WebCore::ApplicationCacheGroup::selectCache()
            const KURL& docURL = m_documentLoader->frame()->document()->url();
            String referrer = m_documentLoader->frameLoader()->referrer();
            m_documentLoader->frame()->redirectScheduler()->scheduleLocationChange(docURL, referrer);
        }
    }
}

bool ApplicationCacheHost::maybeLoadFallbackForMainResponse(const ResourceRequest&, const ResourceResponse& response)
{
    if (m_internal) {
        WrappedResourceResponse wrapped(response);
        m_internal->m_outerHost->didReceiveResponseForMainResource(wrapped);
    }
    return false;
}

bool ApplicationCacheHost::maybeLoadFallbackForMainError(const ResourceRequest&, const ResourceError& error)
{
    // N/A to the chromium port
    return false;
}

void ApplicationCacheHost::mainResourceDataReceived(const char* data, int length, long long, bool)
{
    if (m_internal)
        m_internal->m_outerHost->didReceiveDataForMainResource(data, length);
}

void ApplicationCacheHost::failedLoadingMainResource()
{
    if (m_internal)
        m_internal->m_outerHost->didFinishLoadingMainResource(false);
}

void ApplicationCacheHost::finishedLoadingMainResource()
{
    if (m_internal)
        m_internal->m_outerHost->didFinishLoadingMainResource(true);
}

bool ApplicationCacheHost::maybeLoadResource(ResourceLoader*, ResourceRequest& request, const KURL&)
{
    // FIXME: look into the purpose of the unused KURL& originalURL parameter
    if (m_internal) {
        WrappedResourceRequest wrapped(request);
        m_internal->m_outerHost->willStartSubResourceRequest(wrapped);
    }
    return false;
}

bool ApplicationCacheHost::maybeLoadFallbackForRedirect(ResourceLoader*, ResourceRequest&, const ResourceResponse&)
{
    // N/A to the chromium port
    return false;
}

bool ApplicationCacheHost::maybeLoadFallbackForResponse(ResourceLoader*, const ResourceResponse&)
{
    // N/A to the chromium port
    return false;
}

bool ApplicationCacheHost::maybeLoadFallbackForError(ResourceLoader*, const ResourceError&)
{
    // N/A to the chromium port
    return false;
}

bool ApplicationCacheHost::maybeLoadSynchronously(ResourceRequest& request, ResourceError&, ResourceResponse&, Vector<char>&)
{
    if (m_internal) {
        WrappedResourceRequest wrapped(request);
        m_internal->m_outerHost->willStartSubResourceRequest(wrapped);
    }
    return false;
}

void ApplicationCacheHost::maybeLoadFallbackSynchronously(const ResourceRequest&, ResourceError&, ResourceResponse&, Vector<char>&)
{
    // N/A to the chromium port
}

bool ApplicationCacheHost::canCacheInPageCache() const
{
    // N/A to the chromium port which doesn't use the page cache.
    return false;
}

void ApplicationCacheHost::setDOMApplicationCache(DOMApplicationCache* domApplicationCache)
{
    ASSERT(!m_domApplicationCache || !domApplicationCache);
    m_domApplicationCache = domApplicationCache;
}

void ApplicationCacheHost::notifyDOMApplicationCache(EventID id)
{
    if (m_defersEvents) {
        m_deferredEvents.append(id);
        return;
    }
    if (m_domApplicationCache) {
        ExceptionCode ec = 0;
        m_domApplicationCache->dispatchEvent(Event::create(DOMApplicationCache::toEventType(id), false, false), ec);
        ASSERT(!ec);
    }
}

void ApplicationCacheHost::stopDeferringEvents()
{
    RefPtr<DocumentLoader> protect(documentLoader());
    for (unsigned i = 0; i < m_deferredEvents.size(); ++i) {
        EventID id = m_deferredEvents[i];
        if (m_domApplicationCache) {
            ExceptionCode ec = 0;
            m_domApplicationCache->dispatchEvent(Event::create(DOMApplicationCache::toEventType(id), false, false), ec);
            ASSERT(!ec);
        }
    }
    m_deferredEvents.clear();
    m_defersEvents = false;
}

ApplicationCacheHost::Status ApplicationCacheHost::status() const
{
    return m_internal ? static_cast<Status>(m_internal->m_outerHost->status()) : UNCACHED;
}

bool ApplicationCacheHost::update()
{
    return m_internal ? m_internal->m_outerHost->startUpdate() : false;
}

bool ApplicationCacheHost::swapCache()
{
    return m_internal ? m_internal->m_outerHost->swapCache() : false;
}

bool ApplicationCacheHost::isApplicationCacheEnabled()
{
    ASSERT(m_documentLoader->frame());
    return m_documentLoader->frame()->settings()
           && m_documentLoader->frame()->settings()->offlineWebApplicationCacheEnabled();
}

}  // namespace WebCore

#endif  // ENABLE(OFFLINE_WEB_APPLICATIONS)
