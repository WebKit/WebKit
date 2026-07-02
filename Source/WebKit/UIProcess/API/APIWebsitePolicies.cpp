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

#include "WebProcessPool.h"
#include "WebUserContentControllerProxy.h"
#include "WebsiteDataStore.h"
#include "WebsitePoliciesData.h"

#if PLATFORM(COCOA)
#include "WebPagePreferencesLockdownModeObserver.h"
#endif

namespace API {

WebsitePolicies::WebsitePolicies()
#if PLATFORM(COCOA)
    : m_lockdownModeObserver(makeUniqueWithoutRefCountedCheck<WebKit::WebPagePreferencesLockdownModeObserver, WebKit::LockdownModeObserver>(*this))
#endif
{
}

Ref<WebsitePolicies> WebsitePolicies::copy() const
{
    auto policies = WebsitePolicies::create();
    policies->m_data = m_data;
    policies->setWebsiteDataStore(m_websiteDataStore.get());
    policies->setUserContentController(m_userContentController.get());
    policies->setLockdownModeEnabled(m_lockdownModeEnabled);
    policies->setIsEnhancedSecurityEnabled(m_isEnhancedSecurityEnabled);
    return policies;
}

WebsitePolicies::~WebsitePolicies() = default;

void WebsitePolicies::setWebsiteDataStore(RefPtr<WebKit::WebsiteDataStore>&& websiteDataStore)
{
    m_websiteDataStore = WTF::move(websiteDataStore);
}

void WebsitePolicies::setUserContentController(RefPtr<WebKit::WebUserContentControllerProxy>&& controller)
{
    m_userContentController = WTF::move(controller);
}

WebKit::WebsitePoliciesData WebsitePolicies::dataForProcess(WebKit::WebProcessProxy& process) const
{
    auto data = m_data;
    if (RefPtr controller = m_userContentController)
        data.userContentControllerParameters = controller->parametersForProcess(process);
    return data;
}

bool WebsitePolicies::lockdownModeEnabled() const
{
    return m_lockdownModeEnabled ? *m_lockdownModeEnabled : WebKit::lockdownModeEnabledBySystem();
}

const WebCore::ResourceRequest& WebsitePolicies::alternateRequest() const
{
    return m_data.alternateRequest;
}

void WebsitePolicies::setAlternateRequest(WebCore::ResourceRequest&& request)
{
    m_data.alternateRequest = WTF::move(request);
}

}
