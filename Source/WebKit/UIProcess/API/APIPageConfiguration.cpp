/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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
#include "APIPageConfiguration.h"

#include "APIProcessPoolConfiguration.h"
#include "APIWebsitePolicies.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebURLSchemeHandler.h"
#include "WebUserContentControllerProxy.h"

#if ENABLE(APPLICATION_MANIFEST)
#include "APIApplicationManifest.h"
#endif

namespace API {
using namespace WebKit;

Ref<PageConfiguration> PageConfiguration::create()
{
    return adoptRef(*new PageConfiguration);
}

PageConfiguration::PageConfiguration()
{
}

PageConfiguration::~PageConfiguration() = default;

Ref<PageConfiguration> PageConfiguration::copy() const
{
    auto copy = create();

    copy->m_processPool = this->m_processPool;
    copy->m_userContentController = this->m_userContentController;
    copy->m_pageGroup = this->m_pageGroup;
    copy->m_preferences = this->m_preferences;
    copy->m_relatedPage = this->m_relatedPage;
    copy->m_visitedLinkStore = this->m_visitedLinkStore;
    copy->m_websiteDataStore = this->m_websiteDataStore;
#if PLATFORM(IOS_FAMILY)
    copy->m_clientNavigationsRunAtForegroundPriority = this->m_clientNavigationsRunAtForegroundPriority;
    copy->m_canShowWhileLocked = this->m_canShowWhileLocked;
    copy->m_clickInteractionDriverForTesting = this->m_clickInteractionDriverForTesting;
#endif
    copy->m_initialCapitalizationEnabled = this->m_initialCapitalizationEnabled;
    copy->m_waitsForPaintAfterViewDidMoveToWindow = this->m_waitsForPaintAfterViewDidMoveToWindow;
    copy->m_drawsBackground = this->m_drawsBackground;
    copy->m_controlledByAutomation = this->m_controlledByAutomation;
    copy->m_cpuLimit = this->m_cpuLimit;
    copy->m_overrideContentSecurityPolicy = this->m_overrideContentSecurityPolicy;
#if ENABLE(APPLICATION_MANIFEST)
    copy->m_applicationManifest = this->m_applicationManifest;
#endif
    copy->m_shouldRelaxThirdPartyCookieBlocking = this->m_shouldRelaxThirdPartyCookieBlocking;
    for (auto& pair : this->m_urlSchemeHandlers)
        copy->m_urlSchemeHandlers.set(pair.key, pair.value.copyRef());
    copy->m_corsDisablingPatterns = this->m_corsDisablingPatterns;
    copy->m_crossOriginAccessControlCheckEnabled = this->m_crossOriginAccessControlCheckEnabled;
    copy->m_userScriptsShouldWaitUntilNotification = this->m_userScriptsShouldWaitUntilNotification;
    copy->m_webViewCategory = this->m_webViewCategory;

    copy->m_processDisplayName = this->m_processDisplayName;
    copy->m_loadsSubresources = this->m_loadsSubresources;
    copy->m_loadsFromNetwork = this->m_loadsFromNetwork;
#if ENABLE(APP_BOUND_DOMAINS)
    copy->m_ignoresAppBoundDomains = this->m_ignoresAppBoundDomains;
    copy->m_limitsNavigationsToAppBoundDomains = this->m_limitsNavigationsToAppBoundDomains;
#endif

    return copy;
}


WebProcessPool* PageConfiguration::processPool()
{
    return m_processPool.get();
}

void PageConfiguration::setProcessPool(WebProcessPool* processPool)
{
    m_processPool = processPool;
}

WebUserContentControllerProxy* PageConfiguration::userContentController()
{
    return m_userContentController.get();
}

void PageConfiguration::setUserContentController(WebUserContentControllerProxy* userContentController)
{
    m_userContentController = userContentController;
}

WebPageGroup* PageConfiguration::pageGroup()
{
    return m_pageGroup.get();
}

void PageConfiguration::setPageGroup(WebPageGroup* pageGroup)
{
    m_pageGroup = pageGroup;
}

WebPreferences* PageConfiguration::preferences()
{
    return m_preferences.get();
}

void PageConfiguration::setPreferences(WebPreferences* preferences)
{
    m_preferences = preferences;
}

WebPageProxy* PageConfiguration::relatedPage() const
{
    return m_relatedPage.get();
}

void PageConfiguration::setRelatedPage(WebPageProxy* relatedPage)
{
    m_relatedPage = relatedPage;
}

WebKit::VisitedLinkStore* PageConfiguration::visitedLinkStore()
{
    return m_visitedLinkStore.get();
}

void PageConfiguration::setVisitedLinkStore(WebKit::VisitedLinkStore* visitedLinkStore)
{
    m_visitedLinkStore = visitedLinkStore;
}

WebKit::WebsiteDataStore* PageConfiguration::websiteDataStore()
{
    return m_websiteDataStore.get();
}

void PageConfiguration::setWebsiteDataStore(WebKit::WebsiteDataStore* websiteDataStore)
{
    m_websiteDataStore = websiteDataStore;
}

WebsitePolicies* PageConfiguration::defaultWebsitePolicies() const
{
    return m_defaultWebsitePolicies.get();
}

void PageConfiguration::setDefaultWebsitePolicies(WebsitePolicies* policies)
{
    m_defaultWebsitePolicies = policies;
}

RefPtr<WebKit::WebURLSchemeHandler> PageConfiguration::urlSchemeHandlerForURLScheme(const WTF::String& scheme)
{
    return m_urlSchemeHandlers.get(scheme);
}

void PageConfiguration::setURLSchemeHandlerForURLScheme(Ref<WebKit::WebURLSchemeHandler>&& handler, const WTF::String& scheme)
{
    m_urlSchemeHandlers.set(scheme, WTFMove(handler));
}

#if ENABLE(APPLICATION_MANIFEST)
ApplicationManifest* PageConfiguration::applicationManifest() const
{
    return m_applicationManifest.get();
}

void PageConfiguration::setApplicationManifest(ApplicationManifest* applicationManifest)
{
    m_applicationManifest = applicationManifest;
}
#endif

} // namespace API
