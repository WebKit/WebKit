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
#include "BrowsingContextGroup.h"
#include "Logging.h"
#include "WebInspectorUtilities.h"
#include "WebPageGroup.h"
#include "WebPageProxy.h"
#include "WebPreferences.h"
#include "WebProcessPool.h"
#include "WebURLSchemeHandler.h"
#include "WebUserContentControllerProxy.h"

#if ENABLE(APPLICATION_MANIFEST)
#include "APIApplicationManifest.h"
#endif

#if ENABLE(WK_WEB_EXTENSIONS)
#include "WebExtensionController.h"
#include "WebExtensionMatchPattern.h"
#endif

namespace API {
using namespace WebKit;

PageConfiguration::Data::Data()
    : openerInfo(Box<std::optional<OpenerInfo>>::create(std::nullopt))
    , openedSite(aboutBlankURL())
{
}

Ref<WebKit::WebProcessPool> PageConfiguration::Data::createWebProcessPool()
{
    return WebProcessPool::create(ProcessPoolConfiguration::create());
}

Ref<WebKit::WebUserContentControllerProxy> PageConfiguration::Data::createWebUserContentControllerProxy()
{
    return WebUserContentControllerProxy::create();
}

Ref<WebKit::WebPreferences> PageConfiguration::Data::createWebPreferences()
{
    return WebPreferences::create(WTF::String(), "WebKit"_s, "WebKitDebug"_s);
}

Ref<WebKit::VisitedLinkStore> PageConfiguration::Data::createVisitedLinkStore()
{
    return WebKit::VisitedLinkStore::create();
}

Ref<WebsitePolicies> PageConfiguration::Data::createWebsitePolicies()
{
    return WebsitePolicies::create();
}

Ref<PageConfiguration> PageConfiguration::create()
{
    return adoptRef(*new PageConfiguration);
}

PageConfiguration::PageConfiguration() = default;

PageConfiguration::~PageConfiguration() = default;

Ref<PageConfiguration> PageConfiguration::copy() const
{
    auto copy = create();
    copy->m_data = m_data;
    return copy;
}

void PageConfiguration::copyDataFrom(const PageConfiguration& other)
{
    m_data = other.m_data;
}

const std::optional<WebCore::WindowFeatures>& PageConfiguration::windowFeatures() const
{
    return m_data.windowFeatures;
}

void PageConfiguration::setWindowFeatures(WebCore::WindowFeatures&& windowFeatures)
{
    m_data.windowFeatures = WTF::move(windowFeatures);
}

const WebCore::Site& PageConfiguration::openedSite() const
{
    return m_data.openedSite;
}

void PageConfiguration::setOpenedSite(const WebCore::Site& site)
{
    m_data.openedSite = site;
}

const WTF::String& PageConfiguration::openedMainFrameName() const
{
    return m_data.openedMainFrameName;
}

void PageConfiguration::setOpenedMainFrameName(const WTF::String& name)
{
    m_data.openedMainFrameName = name;
}

auto PageConfiguration::openerInfo() const -> const std::optional<OpenerInfo>&
{
    return *m_data.openerInfo;
}

void PageConfiguration::setOpenerInfo(std::optional<OpenerInfo>&& info)
{
    m_data.openerInfo = Box<std::optional<OpenerInfo>>::create(WTF::move(info));
}

void PageConfiguration::consumeOpenerInfo()
{
    *m_data.openerInfo = std::nullopt;
}

bool PageConfiguration::OpenerInfo::operator==(const OpenerInfo&) const = default;

void PageConfiguration::setInitialSandboxFlags(WebCore::SandboxFlags sandboxFlags)
{
    m_data.initialSandboxFlags = sandboxFlags;
}

void PageConfiguration::setInitialReferrerPolicy(WebCore::ReferrerPolicy referrerPolicy)
{
    m_data.initialReferrerPolicy = referrerPolicy;
}

WebProcessPool& PageConfiguration::processPool() const
{
    return m_data.processPool.get();
}

void PageConfiguration::setProcessPool(RefPtr<WebProcessPool>&& processPool)
{
    m_data.processPool = WTF::move(processPool);
}

WebUserContentControllerProxy& PageConfiguration::userContentController() const
{
    return m_data.userContentController.get();
}

void PageConfiguration::setUserContentController(RefPtr<WebUserContentControllerProxy>&& userContentController)
{
    m_data.userContentController = WTF::move(userContentController);
}

#if ENABLE(WK_WEB_EXTENSIONS)
const WTF::URL& PageConfiguration::requiredWebExtensionBaseURL() const
{
    return m_data.requiredWebExtensionBaseURL;
}

void PageConfiguration::setRequiredWebExtensionBaseURL(WTF::URL&& baseURL)
{
    m_data.requiredWebExtensionBaseURL = WTF::move(baseURL);
}

WebExtensionController* PageConfiguration::webExtensionController() const
{
    return m_data.webExtensionController.get();
}

void PageConfiguration::setWebExtensionController(RefPtr<WebExtensionController>&& webExtensionController)
{
    m_data.webExtensionController = WTF::move(webExtensionController);
}

WebExtensionController* PageConfiguration::weakWebExtensionController() const
{
    return m_data.weakWebExtensionController.get();
}

void PageConfiguration::setWeakWebExtensionController(WebExtensionController* webExtensionController)
{
    m_data.weakWebExtensionController = webExtensionController;
}
#endif // ENABLE(WK_WEB_EXTENSIONS)


HashSet<WTF::String> PageConfiguration::maskedURLSchemes() const
{
    if (m_data.maskedURLSchemesWasSet)
        return m_data.maskedURLSchemes;
#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    if (webExtensionController() || weakWebExtensionController())
        return WebKit::WebExtensionMatchPattern::extensionSchemes();
#endif
    return { };
}

WebPageGroup* PageConfiguration::pageGroup()
{
    return m_data.pageGroup.get();
}

void PageConfiguration::setPageGroup(RefPtr<WebPageGroup>&& pageGroup)
{
    m_data.pageGroup = WTF::move(pageGroup);
}

WebPreferences& PageConfiguration::preferences() const
{
    return m_data.preferences.get();
}

void PageConfiguration::setPreferences(RefPtr<WebPreferences>&& preferences)
{
    m_data.preferences = WTF::move(preferences);
}

WebPageProxy* PageConfiguration::relatedPage() const
{
    return m_data.relatedPage.get();
}

BrowsingContextGroup* PageConfiguration::preferredBrowsingContextGroup() const
{
    if (auto opener = openerInfo())
        return opener->browsingContextGroup.ptr();

    if (auto relatedPage = this->relatedPage()) {
        if (!relatedPage->isClosed())
            return &relatedPage->browsingContextGroup();
    }

    return nullptr;
}

WebPageProxy* PageConfiguration::pageToCloneSessionStorageFrom() const
{
    return m_data.pageToCloneSessionStorageFrom.get();
}

void PageConfiguration::setPageToCloneSessionStorageFrom(WeakPtr<WebPageProxy>&& pageToCloneSessionStorageFrom)
{
    m_data.pageToCloneSessionStorageFrom = WTF::move(pageToCloneSessionStorageFrom);
}

WebPageProxy* PageConfiguration::alternateWebViewForNavigationGestures() const
{
    return m_data.alternateWebViewForNavigationGestures.get();
}

void PageConfiguration::setAlternateWebViewForNavigationGestures(WeakPtr<WebPageProxy>&& alternateWebViewForNavigationGestures)
{
    m_data.alternateWebViewForNavigationGestures = WTF::move(alternateWebViewForNavigationGestures);
}

WebKit::VisitedLinkStore& PageConfiguration::visitedLinkStore() const
{
    return m_data.visitedLinkStore.get();
}

void PageConfiguration::setVisitedLinkStore(RefPtr<WebKit::VisitedLinkStore>&& visitedLinkStore)
{
    m_data.visitedLinkStore = WTF::move(visitedLinkStore);
}

WebsiteDataStore& PageConfiguration::websiteDataStore() const
{
    if (!m_data.websiteDataStore)
        m_data.websiteDataStore = WebsiteDataStore::defaultDataStore();
    return *m_data.websiteDataStore;
}

WebKit::WebsiteDataStore* PageConfiguration::websiteDataStoreIfExists() const
{
    return m_data.websiteDataStore.get();
}

void PageConfiguration::setWebsiteDataStore(RefPtr<WebsiteDataStore>&& websiteDataStore)
{
    m_data.websiteDataStore = WTF::move(websiteDataStore);
}

WebsitePolicies& PageConfiguration::defaultWebsitePolicies() const
{
    return m_data.defaultWebsitePolicies.get();
}

void PageConfiguration::setDefaultWebsitePolicies(RefPtr<WebsitePolicies>&& policies)
{
    m_data.defaultWebsitePolicies = WTF::move(policies);
}

RefPtr<WebURLSchemeHandler> PageConfiguration::urlSchemeHandlerForURLScheme(const WTF::String& scheme)
{
    return m_data.urlSchemeHandlers.get(scheme);
}

void PageConfiguration::setURLSchemeHandlerForURLScheme(Ref<WebURLSchemeHandler>&& handler, const WTF::String& scheme)
{
    m_data.urlSchemeHandlers.set(scheme, WTF::move(handler));
}

bool PageConfiguration::lockdownModeEnabled() const
{
    if (RefPtr policies = m_data.defaultWebsitePolicies.getIfExists())
        return policies->lockdownModeEnabled();
    return lockdownModeEnabledBySystem();
}

bool PageConfiguration::isEnhancedSecurityEnabled() const
{
    if (RefPtr policies = m_data.defaultWebsitePolicies.getIfExists())
        return policies->isEnhancedSecurityEnabled();
    return false;
}

void PageConfiguration::setAllowPostingLegacySynchronousMessages(bool allow)
{
    m_data.allowPostingLegacySynchronousMessages = allow;
}

bool PageConfiguration::allowPostingLegacySynchronousMessages() const
{
    return m_data.allowPostingLegacySynchronousMessages;
}

void PageConfiguration::setDelaysWebProcessLaunchUntilFirstLoad(bool delaysWebProcessLaunchUntilFirstLoad)
{
    RELEASE_LOG(Process, "%p - PageConfiguration::setDelaysWebProcessLaunchUntilFirstLoad(%d)", this, delaysWebProcessLaunchUntilFirstLoad);
    m_data.delaysWebProcessLaunchUntilFirstLoad = delaysWebProcessLaunchUntilFirstLoad;
}

bool PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() const
{
    if (protect(preferences())->siteIsolationEnabled())
        return true;
    if (RefPtr processPool = m_data.processPool.getIfExists(); processPool && isInspectorProcessPool(*processPool)) {
        // Never delay process launch for inspector pages as inspector pages do not know how to transition from a terminated process.
        RELEASE_LOG(Process, "%p - PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() -> false because of WebInspector pool", this);
        return false;
    }
    if (m_data.delaysWebProcessLaunchUntilFirstLoad) {
        RELEASE_LOG(Process, "%p - PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() -> %" PUBLIC_LOG_STRING " because of explicit client value", this, *m_data.delaysWebProcessLaunchUntilFirstLoad ? "true" : "false");
        // If the client explicitly enabled / disabled the feature, then obey their directives.
        return *m_data.delaysWebProcessLaunchUntilFirstLoad;
    }
    if (RefPtr processPool = m_data.processPool.getIfExists()) {
        RELEASE_LOG(Process, "%p - PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() -> %" PUBLIC_LOG_STRING " because of associated processPool value", this, processPool->delaysWebProcessLaunchDefaultValue() ? "true" : "false");
        return processPool->delaysWebProcessLaunchDefaultValue();
    }
    RELEASE_LOG(Process, "%p - PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() -> %" PUBLIC_LOG_STRING " because of global default value", this, WebProcessPool::globalDelaysWebProcessLaunchDefaultValue() ? "true" : "false");
    return WebProcessPool::globalDelaysWebProcessLaunchDefaultValue();
}

bool PageConfiguration::isLockdownModeExplicitlySet() const
{
    if (RefPtr policies = m_data.defaultWebsitePolicies.getIfExists())
        return policies->isLockdownModeExplicitlySet();
    return false;
}

#if ENABLE(APPLICATION_MANIFEST)
ApplicationManifest* PageConfiguration::applicationManifest() const
{
    return m_data.applicationManifest.get();
}

void PageConfiguration::setApplicationManifest(RefPtr<ApplicationManifest>&& applicationManifest)
{
    m_data.applicationManifest = WTF::move(applicationManifest);
}
#endif

#if ENABLE(APPLE_PAY)

bool PageConfiguration::applePayEnabled() const
{
    if (auto applePayEnabledOverride = m_data.applePayEnabledOverride)
        return *applePayEnabledOverride;

    return protect(preferences())->applePayEnabled();
}

void PageConfiguration::setApplePayEnabled(bool enabled)
{
    m_data.applePayEnabledOverride = enabled;
}

#endif

} // namespace API
