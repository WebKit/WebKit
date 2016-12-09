/*
 * Copyright (C) 2012 Igalia S.L.
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
#include "WebKitPolicyClient.h"

#include "APIPolicyClient.h"
#include "WebKitNavigationPolicyDecisionPrivate.h"
#include "WebKitResponsePolicyDecisionPrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include "WebsitePolicies.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;

class PolicyClient: public API::PolicyClient {
public:
    explicit PolicyClient(WebKitWebView* webView)
        : m_webView(webView)
    {
    }

private:
    void decidePolicyForNavigationAction(WebPageProxy&, WebFrameProxy*, const NavigationActionData& navigationActionData, WebFrameProxy* /*originatingFrame*/, const WebCore::ResourceRequest& /*originalRequest*/, const WebCore::ResourceRequest& request, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* /*userData*/) override
    {
        GRefPtr<WebKitPolicyDecision> decision = adoptGRef(webkitNavigationPolicyDecisionCreate(navigationActionData, request, listener.ptr()));
        webkitWebViewMakePolicyDecision(m_webView, WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION, decision.get());
    }

    void decidePolicyForNewWindowAction(WebPageProxy&, WebFrameProxy&, const NavigationActionData& navigationActionData, const WebCore::ResourceRequest& request, const String& frameName, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* /*userData*/) override
    {
        GRefPtr<WebKitPolicyDecision> decision = adoptGRef(webkitNewWindowPolicyDecisionCreate(navigationActionData, request, frameName, listener.ptr()));
        webkitWebViewMakePolicyDecision(m_webView, WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION, decision.get());
    }

    void decidePolicyForResponse(WebPageProxy&, WebFrameProxy&, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request, bool canShowMIMEType, Ref<WebFramePolicyListenerProxy>&& listener, API::Object* /*userData*/) override
    {
        GRefPtr<WebKitPolicyDecision> decision = adoptGRef(webkitResponsePolicyDecisionCreate(request, response, canShowMIMEType, listener.ptr()));
        webkitWebViewMakePolicyDecision(m_webView, WEBKIT_POLICY_DECISION_TYPE_RESPONSE, decision.get());
    }

    WebKitWebView* m_webView;
};

void attachPolicyClientToView(WebKitWebView* webView)
{
    WebPageProxy* page = webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView));
    page->setPolicyClient(std::make_unique<PolicyClient>(webView));
}
