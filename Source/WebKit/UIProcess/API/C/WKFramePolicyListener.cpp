/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
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
#include "WKFramePolicyListener.h"

#include "APIWebsitePolicies.h"
#include "WKAPICast.h"
#include "WebFramePolicyListenerProxy.h"
#include "WebFrameProxy.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"

using namespace WebKit;

WKTypeID WKFramePolicyListenerGetTypeID()
{
    return toAPI(WebFramePolicyListenerProxy::APIType);
}

void WKFramePolicyListenerUse(WKFramePolicyListenerRef policyListenerRef)
{
    toImpl(policyListenerRef)->use();
}

void WKFramePolicyListenerUseInNewProcess(WKFramePolicyListenerRef policyListenerRef)
{
    toImpl(policyListenerRef)->use(nullptr, ProcessSwapRequestedByClient::Yes);
}

static void useWithPolicies(WKFramePolicyListenerRef policyListenerRef, WKWebsitePoliciesRef websitePolicies, ProcessSwapRequestedByClient processSwapRequestedByClient)
{
    toImpl(policyListenerRef)->use(toImpl(websitePolicies), processSwapRequestedByClient);
}

void WKFramePolicyListenerUseWithPolicies(WKFramePolicyListenerRef policyListenerRef, WKWebsitePoliciesRef websitePolicies)
{
    useWithPolicies(policyListenerRef, websitePolicies, ProcessSwapRequestedByClient::No);
}

void WKFramePolicyListenerUseInNewProcessWithPolicies(WKFramePolicyListenerRef policyListenerRef, WKWebsitePoliciesRef websitePolicies)
{
    useWithPolicies(policyListenerRef, websitePolicies, ProcessSwapRequestedByClient::Yes);
}

void WKFramePolicyListenerDownload(WKFramePolicyListenerRef policyListenerRef)
{
    toImpl(policyListenerRef)->download();
}

void WKFramePolicyListenerIgnore(WKFramePolicyListenerRef policyListenerRef)
{
    toImpl(policyListenerRef)->ignore();
}
