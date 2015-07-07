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

#include "WebKitNavigationPolicyDecisionPrivate.h"
#include "WebKitResponsePolicyDecisionPrivate.h"
#include "WebKitWebViewBasePrivate.h"
#include "WebKitWebViewPrivate.h"
#include <wtf/gobject/GRefPtr.h>
#include <wtf/text/CString.h>

using namespace WebKit;

static inline WebKitNavigationType toWebKitNavigationType(WKFrameNavigationType type)
{
    switch (type) {
    case kWKFrameNavigationTypeLinkClicked:
        return WEBKIT_NAVIGATION_TYPE_LINK_CLICKED;
    case kWKFrameNavigationTypeFormSubmitted:
        return WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED;
    case kWKFrameNavigationTypeBackForward:
        return WEBKIT_NAVIGATION_TYPE_BACK_FORWARD;
    case kWKFrameNavigationTypeReload:
        return WEBKIT_NAVIGATION_TYPE_RELOAD;
    case kWKFrameNavigationTypeFormResubmitted:
        return WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED;
    case kWKFrameNavigationTypeOther:
        return WEBKIT_NAVIGATION_TYPE_OTHER;
    default:
        ASSERT_NOT_REACHED();
        return WEBKIT_NAVIGATION_TYPE_LINK_CLICKED;
    }
}

static void decidePolicyForNavigationAction(WKPageRef, WKFrameRef, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKFrameRef, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef /* userData */, const void* clientInfo)
{
    GRefPtr<WebKitNavigationPolicyDecision> decision =
        adoptGRef(webkitNavigationPolicyDecisionCreate(toWebKitNavigationType(navigationType),
                                                       wkEventMouseButtonToWebKitMouseButton(mouseButton),
                                                       wkEventModifiersToGdkModifiers(modifiers),
                                                       toImpl(request),
                                                       0, /* frame name */
                                                       toImpl(listener)));
    webkitWebViewMakePolicyDecision(WEBKIT_WEB_VIEW(clientInfo),
                                    WEBKIT_POLICY_DECISION_TYPE_NAVIGATION_ACTION,
                                    WEBKIT_POLICY_DECISION(decision.get()));
}

static void decidePolicyForNewWindowAction(WKPageRef, WKFrameRef, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKStringRef frameName, WKFramePolicyListenerRef listener, WKTypeRef /* userData */, const void* clientInfo)
{
    GRefPtr<WebKitNavigationPolicyDecision> decision =
        adoptGRef(webkitNavigationPolicyDecisionCreate(toWebKitNavigationType(navigationType),
                                                       wkEventMouseButtonToWebKitMouseButton(mouseButton),
                                                       wkEventModifiersToGdkModifiers(modifiers),
                                                       toImpl(request),
                                                       toImpl(frameName)->string().utf8().data(),
                                                       toImpl(listener)));
    webkitWebViewMakePolicyDecision(WEBKIT_WEB_VIEW(clientInfo),
                                    WEBKIT_POLICY_DECISION_TYPE_NEW_WINDOW_ACTION,
                                    WEBKIT_POLICY_DECISION(decision.get()));
}

static void decidePolicyForResponse(WKPageRef, WKFrameRef, WKURLResponseRef response, WKURLRequestRef request, bool canShowMIMEType, WKFramePolicyListenerRef listener, WKTypeRef /* userData */, const void* clientInfo)
{
    GRefPtr<WebKitResponsePolicyDecision> decision =
        adoptGRef(webkitResponsePolicyDecisionCreate(toImpl(request), toImpl(response), canShowMIMEType, toImpl(listener)));
    webkitWebViewMakePolicyDecision(WEBKIT_WEB_VIEW(clientInfo),
                                    WEBKIT_POLICY_DECISION_TYPE_RESPONSE,
                                    WEBKIT_POLICY_DECISION(decision.get()));
}

void attachPolicyClientToView(WebKitWebView* webView)
{
    WKPagePolicyClientV1 policyClient = {
        {
            1, // version
            webView, // clientInfo
        },
        0, // decidePolicyForNavigationAction_deprecatedForUseWithV0
        decidePolicyForNewWindowAction,
        0, // decidePolicyForResponse_deprecatedForUseWithV0
        0, // unableToImplementPolicy
        decidePolicyForNavigationAction,
        decidePolicyForResponse
    };
    WKPageSetPagePolicyClient(toAPI(webkitWebViewBaseGetPage(WEBKIT_WEB_VIEW_BASE(webView))), &policyClient.base);
}
