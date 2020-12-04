/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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
#include "WKContextPrivate.h"

#include "APIArray.h"
#include "APIClient.h"
#include "APIDownloadClient.h"
#include "APILegacyContextHistoryClient.h"
#include "APINavigationData.h"
#include "APIProcessPoolConfiguration.h"
#include "APIURLRequest.h"
#include "AuthenticationChallengeProxy.h"
#include "DownloadProxy.h"
#include "LegacyGlobalSettings.h"
#include "WKAPICast.h"
#include "WKArray.h"
#include "WKContextConfigurationRef.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WKWebsiteDataStoreRef.h"
#include "WebCertificateInfo.h"
#include "WebContextInjectedBundleClient.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

// Supplements
#include "WebCookieManagerProxy.h"
#include "WebGeolocationManagerProxy.h"
#include "WebNotificationManagerProxy.h"

namespace API {
template<> struct ClientTraits<WKContextHistoryClientBase> {
    typedef std::tuple<WKContextHistoryClientV0> Versions;
};
}

WKTypeID WKContextGetTypeID()
{
    return WebKit::toAPI(WebKit::WebProcessPool::APIType);
}

WKContextRef WKContextCreate()
{
    auto configuration = API::ProcessPoolConfiguration::create();
    return WebKit::toAPI(&WebKit::WebProcessPool::create(configuration).leakRef());
}

WKContextRef WKContextCreateWithInjectedBundlePath(WKStringRef pathRef)
{
    auto configuration = API::ProcessPoolConfiguration::create();
    configuration->setInjectedBundlePath(WebKit::toWTFString(pathRef));

    return WebKit::toAPI(&WebKit::WebProcessPool::create(configuration).leakRef());
}

WKContextRef WKContextCreateWithConfiguration(WKContextConfigurationRef configuration)
{
    RefPtr<API::ProcessPoolConfiguration> apiConfiguration = WebKit::toImpl(configuration);
    if (!apiConfiguration)
        apiConfiguration = API::ProcessPoolConfiguration::create();
    return WebKit::toAPI(&WebKit::WebProcessPool::create(*apiConfiguration).leakRef());
}

void WKContextSetClient(WKContextRef contextRef, const WKContextClientBase* wkClient)
{
    WebKit::toImpl(contextRef)->initializeClient(wkClient);
}

void WKContextSetInjectedBundleClient(WKContextRef contextRef, const WKContextInjectedBundleClientBase* wkClient)
{
    WebKit::toImpl(contextRef)->setInjectedBundleClient(makeUnique<WebKit::WebContextInjectedBundleClient>(wkClient));
}

void WKContextSetHistoryClient(WKContextRef contextRef, const WKContextHistoryClientBase* wkClient)
{
    class HistoryClient final : public API::Client<WKContextHistoryClientBase>, public API::LegacyContextHistoryClient {
    public:
        explicit HistoryClient(const WKContextHistoryClientBase* client)
        {
            initialize(client);
        }

    private:
        void didNavigateWithNavigationData(WebKit::WebProcessPool& processPool, WebKit::WebPageProxy& page, const WebKit::WebNavigationDataStore& navigationDataStore, WebKit::WebFrameProxy& frame) override
        {
            if (!m_client.didNavigateWithNavigationData)
                return;

            RefPtr<API::NavigationData> navigationData = API::NavigationData::create(navigationDataStore);
            m_client.didNavigateWithNavigationData(WebKit::toAPI(&processPool), WebKit::toAPI(&page), WebKit::toAPI(navigationData.get()), WebKit::toAPI(&frame), m_client.base.clientInfo);
        }

        void didPerformClientRedirect(WebKit::WebProcessPool& processPool, WebKit::WebPageProxy& page, const String& sourceURL, const String& destinationURL, WebKit::WebFrameProxy& frame) override
        {
            if (!m_client.didPerformClientRedirect)
                return;

            m_client.didPerformClientRedirect(WebKit::toAPI(&processPool), WebKit::toAPI(&page), WebKit::toURLRef(sourceURL.impl()), WebKit::toURLRef(destinationURL.impl()), WebKit::toAPI(&frame), m_client.base.clientInfo);
        }

        void didPerformServerRedirect(WebKit::WebProcessPool& processPool, WebKit::WebPageProxy& page, const String& sourceURL, const String& destinationURL, WebKit::WebFrameProxy& frame) override
        {
            if (!m_client.didPerformServerRedirect)
                return;

            m_client.didPerformServerRedirect(WebKit::toAPI(&processPool), WebKit::toAPI(&page), WebKit::toURLRef(sourceURL.impl()), WebKit::toURLRef(destinationURL.impl()), WebKit::toAPI(&frame), m_client.base.clientInfo);
        }

        void didUpdateHistoryTitle(WebKit::WebProcessPool& processPool, WebKit::WebPageProxy& page, const String& title, const String& url, WebKit::WebFrameProxy& frame) override
        {
            if (!m_client.didUpdateHistoryTitle)
                return;

            m_client.didUpdateHistoryTitle(WebKit::toAPI(&processPool), WebKit::toAPI(&page), WebKit::toAPI(title.impl()), WebKit::toURLRef(url.impl()), WebKit::toAPI(&frame), m_client.base.clientInfo);
        }

        void populateVisitedLinks(WebKit::WebProcessPool& processPool) override
        {
            if (!m_client.populateVisitedLinks)
                return;

            m_client.populateVisitedLinks(WebKit::toAPI(&processPool), m_client.base.clientInfo);
        }

        bool addsVisitedLinks() const override
        {
            return m_client.populateVisitedLinks;
        }
    };

    WebKit::WebProcessPool& processPool = *WebKit::toImpl(contextRef);
    processPool.setHistoryClient(makeUnique<HistoryClient>(wkClient));

    bool addsVisitedLinks = processPool.historyClient().addsVisitedLinks();

    for (auto& process : processPool.processes()) {
        for (auto& page : process->pages())
            page->setAddsVisitedLinks(addsVisitedLinks);
    }
}

void WKContextSetConnectionClient(WKContextRef contextRef, const WKContextConnectionClientBase* wkClient)
{
    WebKit::toImpl(contextRef)->initializeConnectionClient(wkClient);
}

WKDownloadRef WKContextDownloadURLRequest(WKContextRef, WKURLRequestRef)
{
    return nullptr;
}

WKDownloadRef WKContextResumeDownload(WKContextRef, WKDataRef, WKStringRef)
{
    return nullptr;
}

void WKContextSetInitializationUserDataForInjectedBundle(WKContextRef contextRef,  WKTypeRef userDataRef)
{
    WebKit::toImpl(contextRef)->setInjectedBundleInitializationUserData(WebKit::toImpl(userDataRef));
}

void WKContextPostMessageToInjectedBundle(WKContextRef contextRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    WebKit::toImpl(contextRef)->postMessageToInjectedBundle(WebKit::toImpl(messageNameRef)->string(), WebKit::toImpl(messageBodyRef));
}

void WKContextGetGlobalStatistics(WKContextStatistics* statistics)
{
    const WebKit::WebProcessPool::Statistics& webContextStatistics = WebKit::WebProcessPool::statistics();

    statistics->wkViewCount = webContextStatistics.wkViewCount;
    statistics->wkPageCount = webContextStatistics.wkPageCount;
    statistics->wkFrameCount = webContextStatistics.wkFrameCount;
}

void WKContextAddVisitedLink(WKContextRef contextRef, WKStringRef visitedURL)
{
    String visitedURLString = WebKit::toImpl(visitedURL)->string();
    if (visitedURLString.isEmpty())
        return;

    WebKit::toImpl(contextRef)->visitedLinkStore().addVisitedLinkHash(WebCore::computeSharedStringHash(visitedURLString));
}

void WKContextClearVisitedLinks(WKContextRef contextRef)
{
    WebKit::toImpl(contextRef)->visitedLinkStore().removeAll();
}

void WKContextSetCacheModel(WKContextRef contextRef, WKCacheModel cacheModel)
{
    WebKit::LegacyGlobalSettings::singleton().setCacheModel(WebKit::toCacheModel(cacheModel));
}

WKCacheModel WKContextGetCacheModel(WKContextRef contextRef)
{
    return WebKit::toAPI(WebKit::LegacyGlobalSettings::singleton().cacheModel());
}

void WKContextSetMaximumNumberOfProcesses(WKContextRef, unsigned)
{
    // Deprecated.
}

unsigned WKContextGetMaximumNumberOfProcesses(WKContextRef)
{
    // Deprecated.
    return std::numeric_limits<unsigned>::max();
}

void WKContextSetAlwaysUsesComplexTextCodePath(WKContextRef contextRef, bool alwaysUseComplexTextCodePath)
{
    WebKit::toImpl(contextRef)->setAlwaysUsesComplexTextCodePath(alwaysUseComplexTextCodePath);
}

void WKContextSetShouldUseFontSmoothing(WKContextRef contextRef, bool useFontSmoothing)
{
    WebKit::toImpl(contextRef)->setShouldUseFontSmoothing(useFontSmoothing);
}

void WKContextSetAdditionalPluginsDirectory(WKContextRef contextRef, WKStringRef pluginsDirectory)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    WebKit::toImpl(contextRef)->setAdditionalPluginsDirectory(WebKit::toImpl(pluginsDirectory)->string());
#else
    UNUSED_PARAM(contextRef);
    UNUSED_PARAM(pluginsDirectory);
#endif
}

void WKContextRefreshPlugIns(WKContextRef context)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    WebKit::toImpl(context)->refreshPlugins();
#else
    UNUSED_PARAM(context);
#endif
}

void WKContextRegisterURLSchemeAsEmptyDocument(WKContextRef contextRef, WKStringRef urlScheme)
{
    WebKit::toImpl(contextRef)->registerURLSchemeAsEmptyDocument(WebKit::toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsSecure(WKContextRef contextRef, WKStringRef urlScheme)
{
    WebKit::toImpl(contextRef)->registerURLSchemeAsSecure(WebKit::toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(WKContextRef contextRef, WKStringRef urlScheme)
{
    WebKit::toImpl(contextRef)->registerURLSchemeAsBypassingContentSecurityPolicy(WebKit::toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsCachePartitioned(WKContextRef contextRef, WKStringRef urlScheme)
{
    WebKit::toImpl(contextRef)->registerURLSchemeAsCachePartitioned(WebKit::toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(WKContextRef contextRef, WKStringRef urlScheme)
{
    WebKit::toImpl(contextRef)->registerURLSchemeAsCanDisplayOnlyIfCanRequest(WebKit::toImpl(urlScheme)->string());
}

void WKContextSetDomainRelaxationForbiddenForURLScheme(WKContextRef contextRef, WKStringRef urlScheme)
{
    WebKit::toImpl(contextRef)->setDomainRelaxationForbiddenForURLScheme(WebKit::toImpl(urlScheme)->string());
}

void WKContextSetCanHandleHTTPSServerTrustEvaluation(WKContextRef contextRef, bool value)
{
}

void WKContextSetPrewarmsProcessesAutomatically(WKContextRef contextRef, bool value)
{
    WebKit::toImpl(contextRef)->configuration().setIsAutomaticProcessWarmingEnabled(value);
}

void WKContextSetUsesSingleWebProcess(WKContextRef contextRef, bool value)
{
    WebKit::toImpl(contextRef)->configuration().setUsesSingleWebProcess(value);
}

bool WKContextGetUsesSingleWebProcess(WKContextRef contextRef)
{
    return WebKit::toImpl(contextRef)->configuration().usesSingleWebProcess();
}

void WKContextSetCustomWebContentServiceBundleIdentifier(WKContextRef contextRef, WKStringRef name)
{
    WebKit::toImpl(contextRef)->setCustomWebContentServiceBundleIdentifier(WebKit::toImpl(name)->string());
}

void WKContextSetDiskCacheSpeculativeValidationEnabled(WKContextRef, bool)
{
}

void WKContextPreconnectToServer(WKContextRef, WKURLRef)
{
}

WKWebsiteDataStoreRef WKContextGetWebsiteDataStore(WKContextRef)
{
    return WKWebsiteDataStoreGetDefaultDataStore();
}

WKApplicationCacheManagerRef WKContextGetApplicationCacheManager(WKContextRef context)
{
    return reinterpret_cast<WKApplicationCacheManagerRef>(WKWebsiteDataStoreGetDefaultDataStore());
}

WKGeolocationManagerRef WKContextGetGeolocationManager(WKContextRef contextRef)
{
    return WebKit::toAPI(WebKit::toImpl(contextRef)->supplement<WebKit::WebGeolocationManagerProxy>());
}

WKIconDatabaseRef WKContextGetIconDatabase(WKContextRef)
{
    return nullptr;
}

WKKeyValueStorageManagerRef WKContextGetKeyValueStorageManager(WKContextRef context)
{
    return reinterpret_cast<WKKeyValueStorageManagerRef>(WKWebsiteDataStoreGetDefaultDataStore());
}

WKNotificationManagerRef WKContextGetNotificationManager(WKContextRef contextRef)
{
    return WebKit::toAPI(WebKit::toImpl(contextRef)->supplement<WebKit::WebNotificationManagerProxy>());
}

WKResourceCacheManagerRef WKContextGetResourceCacheManager(WKContextRef context)
{
    return reinterpret_cast<WKResourceCacheManagerRef>(WKWebsiteDataStoreGetDefaultDataStore());
}

void WKContextStartMemorySampler(WKContextRef contextRef, WKDoubleRef interval)
{
    WebKit::toImpl(contextRef)->startMemorySampler(WebKit::toImpl(interval)->value());
}

void WKContextStopMemorySampler(WKContextRef contextRef)
{
    WebKit::toImpl(contextRef)->stopMemorySampler();
}

void WKContextSetIconDatabasePath(WKContextRef, WKStringRef)
{
}

void WKContextAllowSpecificHTTPSCertificateForHost(WKContextRef, WKCertificateInfoRef, WKStringRef)
{
}

void WKContextDisableProcessTermination(WKContextRef contextRef)
{
    WebKit::toImpl(contextRef)->disableProcessTermination();
}

void WKContextEnableProcessTermination(WKContextRef contextRef)
{
    WebKit::toImpl(contextRef)->enableProcessTermination();
}

void WKContextSetHTTPPipeliningEnabled(WKContextRef contextRef, bool enabled)
{
    WebKit::toImpl(contextRef)->setHTTPPipeliningEnabled(enabled);
}

void WKContextWarmInitialProcess(WKContextRef contextRef)
{
    WebKit::toImpl(contextRef)->prewarmProcess();
}

void WKContextGetStatistics(WKContextRef contextRef, void* context, WKContextGetStatisticsFunction callback)
{
}

void WKContextGetStatisticsWithOptions(WKContextRef contextRef, WKStatisticsOptions optionsMask, void* context, WKContextGetStatisticsFunction callback)
{
}

bool WKContextJavaScriptConfigurationFileEnabled(WKContextRef contextRef)
{
    return WebKit::toImpl(contextRef)->javaScriptConfigurationFileEnabled();
}

void WKContextSetJavaScriptConfigurationFileEnabled(WKContextRef contextRef, bool enable)
{
    WebKit::toImpl(contextRef)->setJavaScriptConfigurationFileEnabled(enable);
}

void WKContextGarbageCollectJavaScriptObjects(WKContextRef contextRef)
{
    WebKit::toImpl(contextRef)->garbageCollectJavaScriptObjects();
}

void WKContextSetJavaScriptGarbageCollectorTimerEnabled(WKContextRef contextRef, bool enable)
{
    WebKit::toImpl(contextRef)->setJavaScriptGarbageCollectorTimerEnabled(enable);
}

WKDictionaryRef WKContextCopyPlugInAutoStartOriginHashes(WKContextRef)
{
    return nullptr;
}

void WKContextSetPlugInAutoStartOriginHashes(WKContextRef, WKDictionaryRef)
{
}

void WKContextSetPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(WKContextRef, WKDictionaryRef, double)
{
}

void WKContextSetPlugInAutoStartOrigins(WKContextRef, WKArrayRef)
{
}

void WKContextSetInvalidMessageFunction(WKContextInvalidMessageFunction invalidMessageFunction)
{
    WebKit::WebProcessPool::setInvalidMessageCallback(invalidMessageFunction);
}

void WKContextSetMemoryCacheDisabled(WKContextRef contextRef, bool disabled)
{
    WebKit::toImpl(contextRef)->setMemoryCacheDisabled(disabled);
}

void WKContextSetFontAllowList(WKContextRef contextRef, WKArrayRef arrayRef)
{
    WebKit::toImpl(contextRef)->setFontAllowList(WebKit::toImpl(arrayRef));
}

void WKContextTerminateServiceWorkers(WKContextRef context)
{
    WebKit::toImpl(context)->terminateServiceWorkers();
}

void WKContextAddSupportedPlugin(WKContextRef contextRef, WKStringRef domainRef, WKStringRef nameRef, WKArrayRef mimeTypesRef, WKArrayRef extensionsRef)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    HashSet<String> mimeTypes;
    HashSet<String> extensions;

    size_t count = WKArrayGetSize(mimeTypesRef);
    for (size_t i = 0; i < count; ++i)
        mimeTypes.add(WebKit::toWTFString(static_cast<WKStringRef>(WKArrayGetItemAtIndex(mimeTypesRef, i))));
    count = WKArrayGetSize(extensionsRef);
    for (size_t i = 0; i < count; ++i)
        extensions.add(WebKit::toWTFString(static_cast<WKStringRef>(WKArrayGetItemAtIndex(extensionsRef, i))));

    WebKit::toImpl(contextRef)->addSupportedPlugin(WebKit::toWTFString(domainRef), WebKit::toWTFString(nameRef), WTFMove(mimeTypes), WTFMove(extensions));
#endif
}

void WKContextClearSupportedPlugins(WKContextRef contextRef)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    WebKit::toImpl(contextRef)->clearSupportedPlugins();
#endif
}

void WKContextClearCurrentModifierStateForTesting(WKContextRef contextRef)
{
    WebKit::toImpl(contextRef)->clearCurrentModifierStateForTesting();
}

void WKContextSetUseSeparateServiceWorkerProcess(WKContextRef, bool useSeparateServiceWorkerProcess)
{
    WebKit::WebProcessPool::setUseSeparateServiceWorkerProcess(useSeparateServiceWorkerProcess);
}

void WKContextSetPrimaryWebsiteDataStore(WKContextRef, WKWebsiteDataStoreRef)
{
}
