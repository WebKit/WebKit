/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "APIWebsitePolicies.h"

#include "APIWebsiteDataStore.h"
#include "WebsitePoliciesData.h"

namespace API {

WebsitePolicies::WebsitePolicies() = default;

WebsitePolicies::WebsitePolicies(bool contentBlockersEnabled, OptionSet<WebKit::WebsiteAutoplayQuirk> allowedAutoplayQuirks, WebKit::WebsiteAutoplayPolicy autoplayPolicy, Vector<WebCore::HTTPHeaderField>&& customHeaderFields, WebKit::WebsitePopUpPolicy popUpPolicy, RefPtr<WebsiteDataStore>&& websiteDataStore)
    : m_contentBlockersEnabled(contentBlockersEnabled)
    , m_allowedAutoplayQuirks(allowedAutoplayQuirks)
    , m_autoplayPolicy(autoplayPolicy)
    , m_customHeaderFields(WTFMove(customHeaderFields))
    , m_popUpPolicy(popUpPolicy)
    , m_websiteDataStore(WTFMove(websiteDataStore))
{ }

WebsitePolicies::~WebsitePolicies()
{
}

void WebsitePolicies::setWebsiteDataStore(RefPtr<WebsiteDataStore>&& websiteDataStore)
{
    m_websiteDataStore = WTFMove(websiteDataStore);
}

WebKit::WebsitePoliciesData WebsitePolicies::data()
{
    std::optional<WebKit::WebsiteDataStoreParameters> parameters;
    if (m_websiteDataStore)
        parameters = m_websiteDataStore->websiteDataStore().parameters();
    return { contentBlockersEnabled(), allowedAutoplayQuirks(), autoplayPolicy(), customHeaderFields(), popUpPolicy(), WTFMove(parameters), m_customUserAgent, m_customNavigatorPlatform };
}

}

