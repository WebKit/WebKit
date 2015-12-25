/*
 * Copyright (C) 2011 Igalia S.L.
 * Portions Copyright (c) 2011 Motorola Mobility, Inc.  All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitLoaderClient.h"

#include "APILoaderClient.h"
#include "WebKitBackForwardListPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

class LoaderClient : public API::LoaderClient {
public:
    explicit LoaderClient(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    void didStartProvisionalLoadForFrame(WebPageProxy&, WebFrameProxy& frame, API::Navigation*, API::Object* /* userData */) override
    {
        if (!frame.isMainFrame())
            return;
        webkitWebViewLoadChanged(m_webView, WEBKIT_LOAD_STARTED);
    }

    void didReceiveServerRedirectForProvisionalLoadForFrame(WebPageProxy&, WebFrameProxy& frame, API::Navigation*, API::Object* /* userData */) override
    {
        if (!frame.isMainFrame())
            return;
        webkitWebViewLoadChanged(m_webView, WEBKIT_LOAD_REDIRECTED);
    }

    void didFailProvisionalLoadWithErrorForFrame(WebPageProxy&, WebFrameProxy& frame, API::Navigation*, const ResourceError& resourceError, API::Object* /* userData */) override
    {
        if (!frame.isMainFrame())
            return;
        GUniquePtr<GError> error(g_error_new_literal(g_quark_from_string(resourceError.domain().utf8().data()),
            toWebKitError(resourceError.errorCode()), resourceError.localizedDescription().utf8().data()));
        if (resourceError.tlsErrors()) {
            webkitWebViewLoadFailedWithTLSErrors(m_webView, resourceError.failingURL().string().utf8().data(), error.get(),
                static_cast<GTlsCertificateFlags>(resourceError.tlsErrors()), resourceError.certificate());
        } else
            webkitWebViewLoadFailed(m_webView, WEBKIT_LOAD_STARTED, resourceError.failingURL().string().utf8().data(), error.get());
    }

    void didCommitLoadForFrame(WebPageProxy&, WebFrameProxy& frame, API::Navigation*, API::Object* /* userData */) override
    {
        if (!frame.isMainFrame())
            return;
        webkitWebViewLoadChanged(m_webView, WEBKIT_LOAD_COMMITTED);
    }

    void didFinishLoadForFrame(WebPageProxy&, WebFrameProxy& frame, API::Navigation*, API::Object* /* userData */) override
    {
        if (!frame.isMainFrame())
            return;
        webkitWebViewLoadChanged(m_webView, WEBKIT_LOAD_FINISHED);
    }

    void didFailLoadWithErrorForFrame(WebPageProxy&, WebFrameProxy& frame, API::Navigation*, const ResourceError& resourceError, API::Object* /* userData */) override
    {
        if (!frame.isMainFrame())
            return;
        GUniquePtr<GError> error(g_error_new_literal(g_quark_from_string(resourceError.domain().utf8().data()),
            toWebKitError(resourceError.errorCode()), resourceError.localizedDescription().utf8().data()));
        webkitWebViewLoadFailed(m_webView, WEBKIT_LOAD_COMMITTED, resourceError.failingURL().string().utf8().data(), error.get());
    }

    void didDisplayInsecureContentForFrame(WebPageProxy&, WebFrameProxy&, API::Object* /* userData */) override
    {
        webkitWebViewInsecureContentDetected(m_webView, WEBKIT_INSECURE_CONTENT_DISPLAYED);
    }

    void didRunInsecureContentForFrame(WebPageProxy&, WebFrameProxy&, API::Object* /* userData */) override
    {
        webkitWebViewInsecureContentDetected(m_webView, WEBKIT_INSECURE_CONTENT_RUN);
    }

    void didChangeBackForwardList(WebPageProxy&, WebBackForwardListItem* addedItem, Vector<RefPtr<WebBackForwardListItem>> removedItems) override
    {
        webkitBackForwardListChanged(webkit_web_view_get_back_forward_list(m_webView), addedItem, removedItems);
    }

    void didReceiveAuthenticationChallengeInFrame(WebPageProxy&, WebFrameProxy&, AuthenticationChallengeProxy* authenticationChallenge) override
    {
        webkitWebViewHandleAuthenticationChallenge(m_webView, authenticationChallenge);
    }

    void processDidCrash(WebPageProxy&) override
    {
        webkitWebViewWebProcessCrashed(m_webView);
    }

    WebKitWebView* m_webView;
};

void attachLoaderClientToView(WebKitWebView* webView)
{
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    page->setLoaderClient(std::make_unique<LoaderClient>(webView));
}

