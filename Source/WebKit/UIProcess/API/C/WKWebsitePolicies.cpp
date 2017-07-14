/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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
#include "WKWebsitePolicies.h"

#include "APIWebsitePolicies.h"
#include "WKAPICast.h"
#include "WebsitePolicies.h"

using namespace WebKit;

WKTypeID WKWebsitePoliciesGetTypeID()
{
    return toAPI(API::WebsitePolicies::APIType);
}

WKWebsitePoliciesRef WKWebsitePoliciesCreate()
{
    return toAPI(&API::WebsitePolicies::create().leakRef());
}

void WKWebsitePoliciesSetContentBlockersEnabled(WKWebsitePoliciesRef websitePolicies, bool enabled)
{
    toImpl(websitePolicies)->setContentBlockersEnabled(enabled);
}

bool WKWebsitePoliciesGetContentBlockersEnabled(WKWebsitePoliciesRef websitePolicies)
{
    return toImpl(websitePolicies)->contentBlockersEnabled();
}

void WKWebsitePoliciesSetAllowedAutoplayQuirks(WKWebsitePoliciesRef websitePolicies, WKWebsiteAutoplayQuirk allowedQuirks)
{
    OptionSet<WebsiteAutoplayQuirk> quirks;
    if (allowedQuirks & kWKWebsiteAutoplayQuirkInheritedUserGestures)
        quirks |= WebsiteAutoplayQuirk::InheritedUserGestures;

    if (allowedQuirks & kWKWebsiteAutoplayQuirkSynthesizedPauseEvents)
        quirks |= WebsiteAutoplayQuirk::SynthesizedPauseEvents;

    toImpl(websitePolicies)->setAllowedAutoplayQuirks(quirks);
}

WKWebsiteAutoplayQuirk WKWebsitePoliciesGetAllowedAutoplayQuirks(WKWebsitePoliciesRef websitePolicies)
{
    WKWebsiteAutoplayQuirk quirks = 0;
    auto allowedQuirks = toImpl(websitePolicies)->allowedAutoplayQuirks();

    if (allowedQuirks.contains(WebsiteAutoplayQuirk::SynthesizedPauseEvents))
        quirks |= kWKWebsiteAutoplayQuirkSynthesizedPauseEvents;

    if (allowedQuirks.contains(WebsiteAutoplayQuirk::InheritedUserGestures))
        quirks |= kWKWebsiteAutoplayQuirkInheritedUserGestures;

    return quirks;
}

WKWebsiteAutoplayPolicy WKWebsitePoliciesGetAutoplayPolicy(WKWebsitePoliciesRef websitePolicies)
{
    switch (toImpl(websitePolicies)->autoplayPolicy()) {
    case WebKit::WebsiteAutoplayPolicy::Default:
        return kWKWebsiteAutoplayPolicyDefault;
    case WebsiteAutoplayPolicy::Allow:
        return kWKWebsiteAutoplayPolicyAllow;
    case WebsiteAutoplayPolicy::AllowWithoutSound:
        return kWKWebsiteAutoplayPolicyAllowWithoutSound;
    case WebsiteAutoplayPolicy::Deny:
        return kWKWebsiteAutoplayPolicyDeny;
    }
    ASSERT_NOT_REACHED();
    return kWKWebsiteAutoplayPolicyDefault;
}

void WKWebsitePoliciesSetAutoplayPolicy(WKWebsitePoliciesRef websitePolicies, WKWebsiteAutoplayPolicy policy)
{
    switch (policy) {
    case kWKWebsiteAutoplayPolicyDefault:
        toImpl(websitePolicies)->setAutoplayPolicy(WebsiteAutoplayPolicy::Default);
        return;
    case kWKWebsiteAutoplayPolicyAllow:
        toImpl(websitePolicies)->setAutoplayPolicy(WebsiteAutoplayPolicy::Allow);
        return;
    case kWKWebsiteAutoplayPolicyAllowWithoutSound:
        toImpl(websitePolicies)->setAutoplayPolicy(WebsiteAutoplayPolicy::AllowWithoutSound);
        return;
    case kWKWebsiteAutoplayPolicyDeny:
        toImpl(websitePolicies)->setAutoplayPolicy(WebsiteAutoplayPolicy::Deny);
        return;
    }
    ASSERT_NOT_REACHED();
}
