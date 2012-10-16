/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "WKFrame.h"
#include "WKFramePolicyListener.h"
#include "ewk_navigation_policy_decision.h"
#include "ewk_navigation_policy_decision_private.h"
#include "ewk_view_policy_client_private.h"
#include "ewk_view_private.h"
#include <WebCore/HTTPStatusCodes.h>
#include <wtf/text/CString.h>

using namespace WebCore;
using namespace WebKit;

static inline Evas_Object* toEwkView(const void* clientInfo)
{
    return static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
}

static void decidePolicyForNavigationAction(WKPageRef, WKFrameRef, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef /*userData*/, const void* clientInfo)
{
    RefPtr<Ewk_Navigation_Policy_Decision> decision = adoptRef(ewk_navigation_policy_decision_new(navigationType, mouseButton, modifiers, request, 0, listener));
    ewk_view_navigation_policy_decision(toEwkView(clientInfo), decision.get());
}

static void decidePolicyForNewWindowAction(WKPageRef, WKFrameRef, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKStringRef frameName, WKFramePolicyListenerRef listener, WKTypeRef /*userData*/, const void* clientInfo)
{
    RefPtr<Ewk_Navigation_Policy_Decision> decision = adoptRef(ewk_navigation_policy_decision_new(navigationType, mouseButton, modifiers, request, toImpl(frameName)->string().utf8().data(), listener));
    ewk_view_new_window_policy_decision(toEwkView(clientInfo), decision.get());
}

static void decidePolicyForResponseCallback(WKPageRef, WKFrameRef frame, WKURLResponseRef response, WKURLRequestRef, WKFramePolicyListenerRef listener, WKTypeRef /*userData*/, const void* /*clientInfo*/)
{
    const ResourceResponse resourceResponse = toImpl(response)->resourceResponse();

    // Ignore responses with an HTTP status code of 204 (No Content)
    if (resourceResponse.httpStatusCode() == HTTPNoContent) {
        WKFramePolicyListenerIgnore(listener);
        return;
    }

    // If the URL Response has "Content-Disposition: attachment;" header, then
    // we should download it.
    if (resourceResponse.isAttachment()) {
        WKFramePolicyListenerDownload(listener);
        return;
    }

    String mimeType = toImpl(response)->resourceResponse().mimeType().lower();
    bool canShowMIMEType = toImpl(frame)->canShowMIMEType(mimeType);
    if (WKFrameIsMainFrame(frame)) {
        if (canShowMIMEType) {
            WKFramePolicyListenerUse(listener);
            return;
        }

        // If we can't use (show) it then we should download it.
        WKFramePolicyListenerDownload(listener);
        return;
    }

    // We should ignore downloadable top-level content for subframes, with an exception for text/xml and application/xml so we can still support Acid3 test.
    // It makes the browser intentionally behave differently when it comes to text(application)/xml content in subframes vs. mainframe.
    if (!canShowMIMEType && !(mimeType == "text/xml" || mimeType == "application/xml")) {
        WKFramePolicyListenerIgnore(listener);
        return;
    }

    WKFramePolicyListenerUse(listener);
}

void ewk_view_policy_client_attach(WKPageRef pageRef, Evas_Object* ewkView)
{
    WKPagePolicyClient policyClient;
    memset(&policyClient, 0, sizeof(WKPagePolicyClient));
    policyClient.version = kWKPagePolicyClientCurrentVersion;
    policyClient.clientInfo = ewkView;
    policyClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
    policyClient.decidePolicyForNewWindowAction = decidePolicyForNewWindowAction;
    policyClient.decidePolicyForResponse = decidePolicyForResponseCallback;

    WKPageSetPagePolicyClient(pageRef, &policyClient);
}
