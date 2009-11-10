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

#ifndef WebDataSourceImpl_h
#define WebDataSourceImpl_h

// FIXME: This relative path is a temporary hack to support using this
// header from webkit/glue.
#include "../public/WebDataSource.h"

#include "DocumentLoader.h"
#include "KURL.h"

#include "WebPluginLoadObserver.h"
#include "WrappedResourceRequest.h"
#include "WrappedResourceResponse.h"

#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/Vector.h>


namespace WebKit {

class WebPluginLoadObserver;

class WebDataSourceImpl : public WebCore::DocumentLoader, public WebDataSource {
public:
    static PassRefPtr<WebDataSourceImpl> create(const WebCore::ResourceRequest&,
                                                const WebCore::SubstituteData&);

    static WebDataSourceImpl* fromDocumentLoader(WebCore::DocumentLoader* loader)
    {
        return static_cast<WebDataSourceImpl*>(loader);
    }

    // WebDataSource methods:
    virtual const WebURLRequest& originalRequest() const;
    virtual const WebURLRequest& request() const;
    virtual const WebURLResponse& response() const;
    virtual bool hasUnreachableURL() const;
    virtual WebURL unreachableURL() const;
    virtual void redirectChain(WebVector<WebURL>&) const;
    virtual WebString pageTitle() const;
    virtual WebNavigationType navigationType() const;
    virtual double triggeringEventTime() const;
    virtual ExtraData* extraData() const;
    virtual void setExtraData(ExtraData*);

    static WebNavigationType toWebNavigationType(WebCore::NavigationType type);

    bool hasRedirectChain() const { return !m_redirectChain.isEmpty(); }
    const WebCore::KURL& endOfRedirectChain() const;
    void clearRedirectChain();
    void appendRedirect(const WebCore::KURL& url);

    PassOwnPtr<WebPluginLoadObserver> releasePluginLoadObserver() { return m_pluginLoadObserver.release(); }
    static void setNextPluginLoadObserver(PassOwnPtr<WebPluginLoadObserver>);

private:
    WebDataSourceImpl(const WebCore::ResourceRequest&, const WebCore::SubstituteData&);
    ~WebDataSourceImpl();

    // Mutable because the const getters will magically sync these to the
    // latest version from WebKit.
    mutable WrappedResourceRequest m_originalRequestWrapper;
    mutable WrappedResourceRequest m_requestWrapper;
    mutable WrappedResourceResponse m_responseWrapper;

    // Lists all intermediate URLs that have redirected for the current provisional load.
    // See WebFrameLoaderClient::dispatchDidReceiveServerRedirectForProvisionalLoad for a
    // description of who modifies this when to keep it up to date.
    Vector<WebCore::KURL> m_redirectChain;

    OwnPtr<ExtraData> m_extraData;
    OwnPtr<WebPluginLoadObserver> m_pluginLoadObserver;

    static WebPluginLoadObserver* m_nextPluginLoadObserver;
};

} // namespace WebKit

#endif  // WebDataSourceImpl_h
