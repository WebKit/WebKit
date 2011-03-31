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
#include "WebDataSourceImpl.h"

#include "ApplicationCacheHostInternal.h"
#include "WebURL.h"
#include "WebURLError.h"
#include "WebVector.h"

using namespace WebCore;

namespace WebKit {

WebPluginLoadObserver* WebDataSourceImpl::m_nextPluginLoadObserver = 0;

PassRefPtr<WebDataSourceImpl> WebDataSourceImpl::create(const ResourceRequest& request, const SubstituteData& data)
{
    return adoptRef(new WebDataSourceImpl(request, data));
}

const WebURLRequest& WebDataSourceImpl::originalRequest() const
{
    m_originalRequestWrapper.bind(DocumentLoader::originalRequest());
    return m_originalRequestWrapper;
}

const WebURLRequest& WebDataSourceImpl::request() const
{
    m_requestWrapper.bind(DocumentLoader::request());
    return m_requestWrapper;
}

const WebURLResponse& WebDataSourceImpl::response() const
{
    m_responseWrapper.bind(DocumentLoader::response());
    return m_responseWrapper;
}

bool WebDataSourceImpl::hasUnreachableURL() const
{
    return !DocumentLoader::unreachableURL().isEmpty();
}

WebURL WebDataSourceImpl::unreachableURL() const
{
    return DocumentLoader::unreachableURL();
}

void WebDataSourceImpl::redirectChain(WebVector<WebURL>& result) const
{
    result.assign(m_redirectChain);
}

WebString WebDataSourceImpl::pageTitle() const
{
    // FIXME: use direction of title as well.
    return title().string();
}

WebNavigationType WebDataSourceImpl::navigationType() const
{
    return toWebNavigationType(triggeringAction().type());
}

double WebDataSourceImpl::triggeringEventTime() const
{
    if (!triggeringAction().event())
        return 0.0;

    // DOMTimeStamp uses units of milliseconds.
    return convertDOMTimeStampToSeconds(triggeringAction().event()->timeStamp());
}

WebDataSource::ExtraData* WebDataSourceImpl::extraData() const
{
    return m_extraData.get();
}

void WebDataSourceImpl::setExtraData(ExtraData* extraData)
{
    m_extraData.set(extraData);
}

WebApplicationCacheHost* WebDataSourceImpl::applicationCacheHost()
{
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    return ApplicationCacheHostInternal::toWebApplicationCacheHost(DocumentLoader::applicationCacheHost());
#else
    return 0;
#endif
}

void WebDataSourceImpl::setDeferMainResourceDataLoad(bool defer)
{
    DocumentLoader::setDeferMainResourceDataLoad(defer);
}

WebNavigationType WebDataSourceImpl::toWebNavigationType(NavigationType type)
{
    switch (type) {
    case NavigationTypeLinkClicked:
        return WebNavigationTypeLinkClicked;
    case NavigationTypeFormSubmitted:
        return WebNavigationTypeFormSubmitted;
    case NavigationTypeBackForward:
        return WebNavigationTypeBackForward;
    case NavigationTypeReload:
        return WebNavigationTypeReload;
    case NavigationTypeFormResubmitted:
        return WebNavigationTypeFormResubmitted;
    case NavigationTypeOther:
    default:
        return WebNavigationTypeOther;
    }
}

const KURL& WebDataSourceImpl::endOfRedirectChain() const
{
    ASSERT(!m_redirectChain.isEmpty());
    return m_redirectChain.last();
}

void WebDataSourceImpl::clearRedirectChain()
{
    m_redirectChain.clear();
}

void WebDataSourceImpl::appendRedirect(const KURL& url)
{
    m_redirectChain.append(url);
}

void WebDataSourceImpl::setNextPluginLoadObserver(PassOwnPtr<WebPluginLoadObserver> observer)
{
    // This call should always be followed up with the creation of a
    // WebDataSourceImpl, so we should never leak this object.
    m_nextPluginLoadObserver = observer.leakPtr();
}

WebDataSourceImpl::WebDataSourceImpl(const ResourceRequest& request, const SubstituteData& data)
    : DocumentLoader(request, data)
{
    if (m_nextPluginLoadObserver) {
        // When a new frame is created, it initially gets a data source for an
        // empty document.  Then it is navigated to the source URL of the
        // frame, which results in a second data source being created.  We want
        // to wait to attach the WebPluginLoadObserver to that data source.
        if (!request.url().isEmpty()) {
            ASSERT(m_nextPluginLoadObserver->url() == WebURL(request.url()));
            m_pluginLoadObserver.set(m_nextPluginLoadObserver);
            m_nextPluginLoadObserver = 0;
        }
    }
}

WebDataSourceImpl::~WebDataSourceImpl()
{
}

} // namespace WebKit
