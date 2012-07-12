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
#include "ewk_navigation_policy_decision.h"
#include "ewk_navigation_policy_decision_private.h"
#include "ewk_view_policy_client_private.h"
#include "ewk_view_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

static inline Evas_Object* toEwkView(const void* clientInfo)
{
    return static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
}

static void decidePolicyForNavigationAction(WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
{
    Ewk_Navigation_Policy_Decision* decision = ewk_navigation_policy_decision_new(navigationType, mouseButton, modifiers, request, 0, listener);
    ewk_view_navigation_policy_decision(toEwkView(clientInfo), decision);
    ewk_navigation_policy_decision_free(decision);
}

static void decidePolicyForNewWindowAction(WKPageRef page, WKFrameRef frame, WKFrameNavigationType navigationType, WKEventModifiers modifiers, WKEventMouseButton mouseButton, WKURLRequestRef request, WKStringRef frameName, WKFramePolicyListenerRef listener, WKTypeRef userData, const void* clientInfo)
{
    Ewk_Navigation_Policy_Decision* decision = ewk_navigation_policy_decision_new(navigationType, mouseButton, modifiers, request, toImpl(frameName)->string().utf8().data(), listener);
    ewk_view_new_window_policy_decision(toEwkView(clientInfo), decision);
    ewk_navigation_policy_decision_free(decision);
}

void ewk_view_policy_client_attach(WKPageRef pageRef, Evas_Object* ewkView)
{
    WKPagePolicyClient policyClient;
    memset(&policyClient, 0, sizeof(WKPagePolicyClient));
    policyClient.version = kWKPagePolicyClientCurrentVersion;
    policyClient.clientInfo = ewkView;
    policyClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
    policyClient.decidePolicyForNewWindowAction = decidePolicyForNewWindowAction;

    WKPageSetPagePolicyClient(pageRef, &policyClient);
}
