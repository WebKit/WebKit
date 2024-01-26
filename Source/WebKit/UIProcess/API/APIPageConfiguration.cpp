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
#endif

namespace API {
using namespace WebKit;

Ref<PageConfiguration> PageConfiguration::create()
{
    return adoptRef(*new PageConfiguration);
}

PageConfiguration::PageConfiguration()
    : m_data { BrowsingContextGroup::create() }
{
}

PageConfiguration::~PageConfiguration() = default;

Ref<PageConfiguration> PageConfiguration::copy() const
{
    auto copy = create();
    copy->m_data = m_data;
    return copy;
}

BrowsingContextGroup& PageConfiguration::browsingContextGroup()
{
    return m_data.browsingContextGroup.get();
}

WebProcessPool* PageConfiguration::processPool()
{
    return m_data.processPool.get();
}

void PageConfiguration::setProcessPool(RefPtr<WebProcessPool>&& processPool)
{
    m_data.processPool = WTFMove(processPool);
}

WebUserContentControllerProxy* PageConfiguration::userContentController()
{
    return m_data.userContentController.get();
}

void PageConfiguration::setUserContentController(RefPtr<WebUserContentControllerProxy>&& userContentController)
{
    m_data.userContentController = WTFMove(userContentController);
}

#if ENABLE(WK_WEB_EXTENSIONS)
const WTF::URL& PageConfiguration::requiredWebExtensionBaseURL() const
{
    return m_data.requiredWebExtensionBaseURL;
}

void PageConfiguration::setRequiredWebExtensionBaseURL(WTF::URL&& baseURL)
{
    m_data.requiredWebExtensionBaseURL = WTFMove(baseURL);
}

WebExtensionController* PageConfiguration::webExtensionController() const
{
    return m_data.webExtensionController.get();
}

void PageConfiguration::setWebExtensionController(RefPtr<WebExtensionController>&& webExtensionController)
{
    m_data.webExtensionController = WTFMove(webExtensionController);
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

WebPageGroup* PageConfiguration::pageGroup()
{
    return m_data.pageGroup.get();
}

void PageConfiguration::setPageGroup(RefPtr<WebPageGroup>&& pageGroup)
{
    m_data.pageGroup = WTFMove(pageGroup);
}

WebPreferences* PageConfiguration::preferences()
{
    return m_data.preferences.get();
}

void PageConfiguration::setPreferences(RefPtr<WebPreferences>&& preferences)
{
    m_data.preferences = WTFMove(preferences);
}

WebPageProxy* PageConfiguration::relatedPage() const
{
    return m_data.relatedPage.get();
}

void PageConfiguration::setRelatedPage(RefPtr<WebPageProxy>&& relatedPage)
{
    m_data.relatedPage = WTFMove(relatedPage);
}

WebKit::WebPageProxy* PageConfiguration::pageToCloneSessionStorageFrom() const
{
    return m_data.pageToCloneSessionStorageFrom.get();
}

void PageConfiguration::setPageToCloneSessionStorageFrom(WebKit::WebPageProxy* pageToCloneSessionStorageFrom)
{
    m_data.pageToCloneSessionStorageFrom = pageToCloneSessionStorageFrom;
}

WebKit::VisitedLinkStore* PageConfiguration::visitedLinkStore()
{
    return m_data.visitedLinkStore.get();
}

void PageConfiguration::setVisitedLinkStore(RefPtr<WebKit::VisitedLinkStore>&& visitedLinkStore)
{
    m_data.visitedLinkStore = WTFMove(visitedLinkStore);
}

WebKit::WebsiteDataStore* PageConfiguration::websiteDataStore()
{
    return m_data.websiteDataStore.get();
}

RefPtr<WebKit::WebsiteDataStore> PageConfiguration::protectedWebsiteDataStore()
{
    return m_data.websiteDataStore;
}

void PageConfiguration::setWebsiteDataStore(RefPtr<WebKit::WebsiteDataStore>&& websiteDataStore)
{
    m_data.websiteDataStore = WTFMove(websiteDataStore);
}

WebsitePolicies* PageConfiguration::defaultWebsitePolicies() const
{
    return m_data.defaultWebsitePolicies.get();
}

void PageConfiguration::setDefaultWebsitePolicies(RefPtr<WebsitePolicies>&& policies)
{
    m_data.defaultWebsitePolicies = WTFMove(policies);
}

RefPtr<WebKit::WebURLSchemeHandler> PageConfiguration::urlSchemeHandlerForURLScheme(const WTF::String& scheme)
{
    return m_data.urlSchemeHandlers.get(scheme);
}

void PageConfiguration::setURLSchemeHandlerForURLScheme(Ref<WebKit::WebURLSchemeHandler>&& handler, const WTF::String& scheme)
{
    m_data.urlSchemeHandlers.set(scheme, WTFMove(handler));
}

bool PageConfiguration::lockdownModeEnabled() const
{
    if (m_data.defaultWebsitePolicies)
        return m_data.defaultWebsitePolicies->lockdownModeEnabled();
    return lockdownModeEnabledBySystem();
}

void PageConfiguration::setDelaysWebProcessLaunchUntilFirstLoad(bool delaysWebProcessLaunchUntilFirstLoad)
{
    RELEASE_LOG(Process, "%p - PageConfiguration::setDelaysWebProcessLaunchUntilFirstLoad(%d)", this, delaysWebProcessLaunchUntilFirstLoad);
    m_data.delaysWebProcessLaunchUntilFirstLoad = delaysWebProcessLaunchUntilFirstLoad;
}

bool PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() const
{
    if (RefPtr processPool = m_data.processPool; processPool && isInspectorProcessPool(*processPool)) {
        // Never delay process launch for inspector pages as inspector pages do not know how to transition from a terminated process.
        RELEASE_LOG(Process, "%p - PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() -> false because of WebInspector pool", this);
        return false;
    }
    if (m_data.delaysWebProcessLaunchUntilFirstLoad) {
        RELEASE_LOG(Process, "%p - PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() -> %{public}s because of explicit client value", this, *m_data.delaysWebProcessLaunchUntilFirstLoad ? "true" : "false");
        // If the client explicitly enabled / disabled the feature, then obey their directives.
        return *m_data.delaysWebProcessLaunchUntilFirstLoad;
    }
    if (m_data.processPool) {
        RELEASE_LOG(Process, "%p - PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() -> %{public}s because of associated processPool value", this, m_data.processPool->delaysWebProcessLaunchDefaultValue() ? "true" : "false");
        return m_data.processPool->delaysWebProcessLaunchDefaultValue();
    }
    RELEASE_LOG(Process, "%p - PageConfiguration::delaysWebProcessLaunchUntilFirstLoad() -> %{public}s because of global default value", this, WebProcessPool::globalDelaysWebProcessLaunchDefaultValue() ? "true" : "false");
    return WebProcessPool::globalDelaysWebProcessLaunchDefaultValue();
}

bool PageConfiguration::isLockdownModeExplicitlySet() const
{
    return m_data.defaultWebsitePolicies && m_data.defaultWebsitePolicies->isLockdownModeExplicitlySet();
}

#if ENABLE(APPLICATION_MANIFEST)
ApplicationManifest* PageConfiguration::applicationManifest() const
{
    return m_data.applicationManifest.get();
}

void PageConfiguration::setApplicationManifest(RefPtr<ApplicationManifest>&& applicationManifest)
{
    m_data.applicationManifest = WTFMove(applicationManifest);
}
#endif

#if ENABLE(GPU_PROCESS)
WebKit::GPUProcessPreferencesForWebProcess PageConfiguration::preferencesForGPUProcess() const
{
    RefPtr preferences = m_data.preferences;
    RELEASE_ASSERT(preferences);

    return {
        preferences->webGLEnabled(),
        preferences->webGPUEnabled(),
        preferences->useGPUProcessForDOMRenderingEnabled(),
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
        preferences->useCGDisplayListsForDOMRendering(),
#endif
        allowTestOnlyIPC()
    };
}
#endif

} // namespace API
