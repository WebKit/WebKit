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
#include "WKAPICast.h"
#include "WKArray.h"
#include "WKContextConfigurationRef.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WebCertificateInfo.h"
#include "WebContextInjectedBundleClient.h"
#include "WebProcessPool.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

// Supplements
#include "WebCookieManagerProxy.h"
#include "WebGeolocationManagerProxy.h"
#include "WebNotificationManagerProxy.h"

namespace API {
template<> struct ClientTraits<WKContextDownloadClientBase> {
    typedef std::tuple<WKContextDownloadClientV0, WKContextDownloadClientV1> Versions;
};
template<> struct ClientTraits<WKContextHistoryClientBase> {
    typedef std::tuple<WKContextHistoryClientV0> Versions;
};
}

using namespace WebCore;
using namespace WebKit;

WKTypeID WKContextGetTypeID()
{
    return toAPI(WebProcessPool::APIType);
}

WKContextRef WKContextCreate()
{
    auto configuration = API::ProcessPoolConfiguration::createWithLegacyOptions();
    return toAPI(&WebProcessPool::create(configuration).leakRef());
}

WKContextRef WKContextCreateWithInjectedBundlePath(WKStringRef pathRef)
{
    auto configuration = API::ProcessPoolConfiguration::createWithLegacyOptions();
    configuration->setInjectedBundlePath(toWTFString(pathRef));

    return toAPI(&WebProcessPool::create(configuration).leakRef());
}

WKContextRef WKContextCreateWithConfiguration(WKContextConfigurationRef configuration)
{
    return toAPI(&WebProcessPool::create(*toImpl(configuration)).leakRef());
}

void WKContextSetClient(WKContextRef contextRef, const WKContextClientBase* wkClient)
{
    toImpl(contextRef)->initializeClient(wkClient);
}

void WKContextSetInjectedBundleClient(WKContextRef contextRef, const WKContextInjectedBundleClientBase* wkClient)
{
    toImpl(contextRef)->setInjectedBundleClient(std::make_unique<WebContextInjectedBundleClient>(wkClient));
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
        void didNavigateWithNavigationData(WebProcessPool& processPool, WebPageProxy& page, const WebNavigationDataStore& navigationDataStore, WebFrameProxy& frame) override
        {
            if (!m_client.didNavigateWithNavigationData)
                return;

            RefPtr<API::NavigationData> navigationData = API::NavigationData::create(navigationDataStore);
            m_client.didNavigateWithNavigationData(toAPI(&processPool), toAPI(&page), toAPI(navigationData.get()), toAPI(&frame), m_client.base.clientInfo);
        }

        void didPerformClientRedirect(WebProcessPool& processPool, WebPageProxy& page, const String& sourceURL, const String& destinationURL, WebFrameProxy& frame) override
        {
            if (!m_client.didPerformClientRedirect)
                return;

            m_client.didPerformClientRedirect(toAPI(&processPool), toAPI(&page), toURLRef(sourceURL.impl()), toURLRef(destinationURL.impl()), toAPI(&frame), m_client.base.clientInfo);
        }

        void didPerformServerRedirect(WebProcessPool& processPool, WebPageProxy& page, const String& sourceURL, const String& destinationURL, WebFrameProxy& frame) override
        {
            if (!m_client.didPerformServerRedirect)
                return;

            m_client.didPerformServerRedirect(toAPI(&processPool), toAPI(&page), toURLRef(sourceURL.impl()), toURLRef(destinationURL.impl()), toAPI(&frame), m_client.base.clientInfo);
        }

        void didUpdateHistoryTitle(WebProcessPool& processPool, WebPageProxy& page, const String& title, const String& url, WebFrameProxy& frame) override
        {
            if (!m_client.didUpdateHistoryTitle)
                return;

            m_client.didUpdateHistoryTitle(toAPI(&processPool), toAPI(&page), toAPI(title.impl()), toURLRef(url.impl()), toAPI(&frame), m_client.base.clientInfo);
        }

        void populateVisitedLinks(WebProcessPool& processPool) override
        {
            if (!m_client.populateVisitedLinks)
                return;

            m_client.populateVisitedLinks(toAPI(&processPool), m_client.base.clientInfo);
        }

        bool addsVisitedLinks() const override
        {
            return m_client.populateVisitedLinks;
        }
    };

    WebProcessPool& processPool = *toImpl(contextRef);
    processPool.setHistoryClient(std::make_unique<HistoryClient>(wkClient));

    bool addsVisitedLinks = processPool.historyClient().addsVisitedLinks();

    for (auto& process : processPool.processes()) {
        for (auto& page : process->pages())
            page->setAddsVisitedLinks(addsVisitedLinks);
    }
}

void WKContextSetDownloadClient(WKContextRef contextRef, const WKContextDownloadClientBase* wkClient)
{
    class DownloadClient final : public API::Client<WKContextDownloadClientBase>, public API::DownloadClient {
    public:
        explicit DownloadClient(const WKContextDownloadClientBase* client)
        {
            initialize(client);
        }
    private:
        void didStart(WebProcessPool& processPool, DownloadProxy& downloadProxy) final
        {
            if (!m_client.didStart)
                return;

            m_client.didStart(toAPI(&processPool), toAPI(&downloadProxy), m_client.base.clientInfo);
        }

        void didReceiveAuthenticationChallenge(WebProcessPool& processPool, DownloadProxy& downloadProxy, AuthenticationChallengeProxy& authenticationChallengeProxy) final
        {
            if (!m_client.didReceiveAuthenticationChallenge)
                return;

            m_client.didReceiveAuthenticationChallenge(toAPI(&processPool), toAPI(&downloadProxy), toAPI(&authenticationChallengeProxy), m_client.base.clientInfo);
        }

        void didReceiveResponse(WebProcessPool& processPool, DownloadProxy& downloadProxy, const ResourceResponse& response) final
        {
            if (!m_client.didReceiveResponse)
                return;

            m_client.didReceiveResponse(toAPI(&processPool), toAPI(&downloadProxy), toAPI(API::URLResponse::create(response).ptr()), m_client.base.clientInfo);
        }

        void didReceiveData(WebProcessPool& processPool, DownloadProxy& downloadProxy, uint64_t length) final
        {
            if (!m_client.didReceiveData)
                return;

            m_client.didReceiveData(toAPI(&processPool), toAPI(&downloadProxy), length, m_client.base.clientInfo);
        }

        void decideDestinationWithSuggestedFilename(WebProcessPool& processPool, DownloadProxy& downloadProxy, const String& filename, Function<void(AllowOverwrite, WTF::String)>&& completionHandler) final
        {
            if (!m_client.decideDestinationWithSuggestedFilename)
                return completionHandler(AllowOverwrite::No, { });

            bool allowOverwrite = false;
            WKRetainPtr<WKStringRef> destination(AdoptWK, m_client.decideDestinationWithSuggestedFilename(toAPI(&processPool), toAPI(&downloadProxy), toAPI(filename.impl()), &allowOverwrite, m_client.base.clientInfo));
            completionHandler(allowOverwrite ? AllowOverwrite::Yes : AllowOverwrite::No, toWTFString(destination.get()));
        }

        void didCreateDestination(WebProcessPool& processPool, DownloadProxy& downloadProxy, const String& path) final
        {
            if (!m_client.didCreateDestination)
                return;

            m_client.didCreateDestination(toAPI(&processPool), toAPI(&downloadProxy), toAPI(path.impl()), m_client.base.clientInfo);
        }

        void didFinish(WebProcessPool& processPool, DownloadProxy& downloadProxy) final
        {
            if (!m_client.didFinish)
                return;

            m_client.didFinish(toAPI(&processPool), toAPI(&downloadProxy), m_client.base.clientInfo);
        }

        void didFail(WebProcessPool& processPool, DownloadProxy& downloadProxy, const ResourceError& error) final
        {
            if (!m_client.didFail)
                return;

            m_client.didFail(toAPI(&processPool), toAPI(&downloadProxy), toAPI(error), m_client.base.clientInfo);
        }
        
        void didCancel(WebProcessPool& processPool, DownloadProxy& downloadProxy) final
        {
            if (!m_client.didCancel)
                return;
            
            m_client.didCancel(toAPI(&processPool), toAPI(&downloadProxy), m_client.base.clientInfo);
        }
        
        void processDidCrash(WebProcessPool& processPool, DownloadProxy& downloadProxy) final
        {
            if (!m_client.processDidCrash)
                return;
            
            m_client.processDidCrash(toAPI(&processPool), toAPI(&downloadProxy), m_client.base.clientInfo);
        }

        void willSendRequest(WebProcessPool& processPool, DownloadProxy& downloadProxy, ResourceRequest&& request, const ResourceResponse&, CompletionHandler<void(ResourceRequest&&)>&& completionHandler) final
        {
            if (m_client.didReceiveServerRedirect)
                m_client.didReceiveServerRedirect(toAPI(&processPool), toAPI(&downloadProxy), toURLRef(request.url().string().impl()), m_client.base.clientInfo);

            completionHandler(WTFMove(request));
        }


    };

    toImpl(contextRef)->setDownloadClient(std::make_unique<DownloadClient>(wkClient));
}

void WKContextSetConnectionClient(WKContextRef contextRef, const WKContextConnectionClientBase* wkClient)
{
    toImpl(contextRef)->initializeConnectionClient(wkClient);
}

WKDownloadRef WKContextDownloadURLRequest(WKContextRef contextRef, WKURLRequestRef requestRef)
{
    return toAPI(toImpl(contextRef)->download(0, toImpl(requestRef)->resourceRequest()));
}

WKDownloadRef WKContextResumeDownload(WKContextRef contextRef, WKDataRef resumeData, WKStringRef path)
{
    return toAPI(toImpl(contextRef)->resumeDownload(toImpl(resumeData), toWTFString(path)));
}

void WKContextSetInitializationUserDataForInjectedBundle(WKContextRef contextRef,  WKTypeRef userDataRef)
{
    toImpl(contextRef)->setInjectedBundleInitializationUserData(toImpl(userDataRef));
}

void WKContextPostMessageToInjectedBundle(WKContextRef contextRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    toImpl(contextRef)->postMessageToInjectedBundle(toImpl(messageNameRef)->string(), toImpl(messageBodyRef));
}

void WKContextGetGlobalStatistics(WKContextStatistics* statistics)
{
    const WebProcessPool::Statistics& webContextStatistics = WebProcessPool::statistics();

    statistics->wkViewCount = webContextStatistics.wkViewCount;
    statistics->wkPageCount = webContextStatistics.wkPageCount;
    statistics->wkFrameCount = webContextStatistics.wkFrameCount;
}

void WKContextAddVisitedLink(WKContextRef contextRef, WKStringRef visitedURL)
{
    String visitedURLString = toImpl(visitedURL)->string();
    if (visitedURLString.isEmpty())
        return;

    toImpl(contextRef)->visitedLinkStore().addVisitedLinkHash(computeSharedStringHash(visitedURLString));
}

void WKContextClearVisitedLinks(WKContextRef contextRef)
{
    toImpl(contextRef)->visitedLinkStore().removeAll();
}

void WKContextSetCacheModel(WKContextRef contextRef, WKCacheModel cacheModel)
{
    toImpl(contextRef)->setCacheModel(toCacheModel(cacheModel));
}

WKCacheModel WKContextGetCacheModel(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->cacheModel());
}

void WKContextSetMaximumNumberOfProcesses(WKContextRef contextRef, unsigned numberOfProcesses)
{
    toImpl(contextRef)->setMaximumNumberOfProcesses(numberOfProcesses);
}

unsigned WKContextGetMaximumNumberOfProcesses(WKContextRef contextRef)
{
    return toImpl(contextRef)->maximumNumberOfProcesses();
}

void WKContextSetAlwaysUsesComplexTextCodePath(WKContextRef contextRef, bool alwaysUseComplexTextCodePath)
{
    toImpl(contextRef)->setAlwaysUsesComplexTextCodePath(alwaysUseComplexTextCodePath);
}

void WKContextSetShouldUseFontSmoothing(WKContextRef contextRef, bool useFontSmoothing)
{
    toImpl(contextRef)->setShouldUseFontSmoothing(useFontSmoothing);
}

void WKContextSetAdditionalPluginsDirectory(WKContextRef contextRef, WKStringRef pluginsDirectory)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    toImpl(contextRef)->setAdditionalPluginsDirectory(toImpl(pluginsDirectory)->string());
#else
    UNUSED_PARAM(contextRef);
    UNUSED_PARAM(pluginsDirectory);
#endif
}

void WKContextRefreshPlugIns(WKContextRef context)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    toImpl(context)->refreshPlugins();
#else
    UNUSED_PARAM(context);
#endif
}

void WKContextRegisterURLSchemeAsEmptyDocument(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->registerURLSchemeAsEmptyDocument(toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsSecure(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->registerURLSchemeAsSecure(toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsBypassingContentSecurityPolicy(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->registerURLSchemeAsBypassingContentSecurityPolicy(toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsCachePartitioned(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->registerURLSchemeAsCachePartitioned(toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsCanDisplayOnlyIfCanRequest(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->registerURLSchemeAsCanDisplayOnlyIfCanRequest(toImpl(urlScheme)->string());
}

void WKContextSetDomainRelaxationForbiddenForURLScheme(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->setDomainRelaxationForbiddenForURLScheme(toImpl(urlScheme)->string());
}

void WKContextSetCanHandleHTTPSServerTrustEvaluation(WKContextRef contextRef, bool value)
{
    toImpl(contextRef)->setCanHandleHTTPSServerTrustEvaluation(value);
}

void WKContextSetPrewarmsProcessesAutomatically(WKContextRef contextRef, bool value)
{
    toImpl(contextRef)->configuration().setIsAutomaticProcessWarmingEnabled(value);
}

void WKContextSetCustomWebContentServiceBundleIdentifier(WKContextRef contextRef, WKStringRef name)
{
    toImpl(contextRef)->setCustomWebContentServiceBundleIdentifier(toImpl(name)->string());
}

void WKContextSetDiskCacheSpeculativeValidationEnabled(WKContextRef contextRef, bool value)
{
    toImpl(contextRef)->configuration().setDiskCacheSpeculativeValidationEnabled(value);
}

void WKContextPreconnectToServer(WKContextRef contextRef, WKURLRef serverURLRef)
{
    toImpl(contextRef)->preconnectToServer(URL(URL(), toWTFString(serverURLRef)));
}

WKCookieManagerRef WKContextGetCookieManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebCookieManagerProxy>());
}

WKWebsiteDataStoreRef WKContextGetWebsiteDataStore(WKContextRef context)
{
    auto* dataStore = toImpl(context)->websiteDataStore();
    if (!dataStore) {
        auto defaultDataStore = API::WebsiteDataStore::defaultDataStore();
        toImpl(context)->setPrimaryDataStore(defaultDataStore.get());
        dataStore = defaultDataStore.ptr();
    }

    return toAPI(dataStore);
}

WKApplicationCacheManagerRef WKContextGetApplicationCacheManager(WKContextRef context)
{
    return reinterpret_cast<WKApplicationCacheManagerRef>(WKContextGetWebsiteDataStore(context));
}

WKGeolocationManagerRef WKContextGetGeolocationManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebGeolocationManagerProxy>());
}

WKIconDatabaseRef WKContextGetIconDatabase(WKContextRef)
{
    return nullptr;
}

WKKeyValueStorageManagerRef WKContextGetKeyValueStorageManager(WKContextRef context)
{
    return reinterpret_cast<WKKeyValueStorageManagerRef>(WKContextGetWebsiteDataStore(context));
}

WKMediaSessionFocusManagerRef WKContextGetMediaSessionFocusManager(WKContextRef context)
{
#if ENABLE(MEDIA_SESSION)
    return toAPI(toImpl(context)->supplement<WebMediaSessionFocusManager>());
#else
    UNUSED_PARAM(context);
    return nullptr;
#endif
}

WKNotificationManagerRef WKContextGetNotificationManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebNotificationManagerProxy>());
}

WKResourceCacheManagerRef WKContextGetResourceCacheManager(WKContextRef context)
{
    return reinterpret_cast<WKResourceCacheManagerRef>(WKContextGetWebsiteDataStore(context));
}

void WKContextStartMemorySampler(WKContextRef contextRef, WKDoubleRef interval)
{
    toImpl(contextRef)->startMemorySampler(toImpl(interval)->value());
}

void WKContextStopMemorySampler(WKContextRef contextRef)
{
    toImpl(contextRef)->stopMemorySampler();
}

void WKContextSetIconDatabasePath(WKContextRef, WKStringRef)
{
}

void WKContextAllowSpecificHTTPSCertificateForHost(WKContextRef contextRef, WKCertificateInfoRef certificateRef, WKStringRef hostRef)
{
    toImpl(contextRef)->allowSpecificHTTPSCertificateForHost(toImpl(certificateRef), toImpl(hostRef)->string());
}

void WKContextDisableProcessTermination(WKContextRef contextRef)
{
    toImpl(contextRef)->disableProcessTermination();
}

void WKContextEnableProcessTermination(WKContextRef contextRef)
{
    toImpl(contextRef)->enableProcessTermination();
}

void WKContextSetHTTPPipeliningEnabled(WKContextRef contextRef, bool enabled)
{
    toImpl(contextRef)->setHTTPPipeliningEnabled(enabled);
}

void WKContextWarmInitialProcess(WKContextRef contextRef)
{
    toImpl(contextRef)->prewarmProcess();
}

void WKContextGetStatistics(WKContextRef contextRef, void* context, WKContextGetStatisticsFunction callback)
{
    toImpl(contextRef)->getStatistics(0xFFFFFFFF, toGenericCallbackFunction(context, callback));
}

void WKContextGetStatisticsWithOptions(WKContextRef contextRef, WKStatisticsOptions optionsMask, void* context, WKContextGetStatisticsFunction callback)
{
    toImpl(contextRef)->getStatistics(optionsMask, toGenericCallbackFunction(context, callback));
}

bool WKContextJavaScriptConfigurationFileEnabled(WKContextRef contextRef)
{
    return toImpl(contextRef)->javaScriptConfigurationFileEnabled();
}

void WKContextSetJavaScriptConfigurationFileEnabled(WKContextRef contextRef, bool enable)
{
    toImpl(contextRef)->setJavaScriptConfigurationFileEnabled(enable);
}

void WKContextGarbageCollectJavaScriptObjects(WKContextRef contextRef)
{
    toImpl(contextRef)->garbageCollectJavaScriptObjects();
}

void WKContextSetJavaScriptGarbageCollectorTimerEnabled(WKContextRef contextRef, bool enable)
{
    toImpl(contextRef)->setJavaScriptGarbageCollectorTimerEnabled(enable);
}

void WKContextUseTestingNetworkSession(WKContextRef context)
{
    toImpl(context)->useTestingNetworkSession();
}

void WKContextSetAllowsAnySSLCertificateForWebSocketTesting(WKContextRef context, bool allows)
{
    toImpl(context)->setAllowsAnySSLCertificateForWebSocket(allows);
}

void WKContextSetAllowsAnySSLCertificateForServiceWorkerTesting(WKContextRef context, bool allows)
{
#if ENABLE(SERVICE_WORKER)
    toImpl(context)->setAllowsAnySSLCertificateForServiceWorker(allows);
#endif
}

void WKContextClearCachedCredentials(WKContextRef context)
{
    toImpl(context)->clearCachedCredentials();
}

WKDictionaryRef WKContextCopyPlugInAutoStartOriginHashes(WKContextRef contextRef)
{
    return toAPI(&toImpl(contextRef)->plugInAutoStartOriginHashes().leakRef());
}

void WKContextSetPlugInAutoStartOriginHashes(WKContextRef contextRef, WKDictionaryRef dictionaryRef)
{
    if (!dictionaryRef)
        return;
    toImpl(contextRef)->setPlugInAutoStartOriginHashes(*toImpl(dictionaryRef));
}

void WKContextSetPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(WKContextRef contextRef, WKDictionaryRef dictionaryRef, double time)
{
    if (!dictionaryRef)
        return;
    toImpl(contextRef)->setPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(*toImpl(dictionaryRef), WallTime::fromRawSeconds(time));
}

void WKContextSetPlugInAutoStartOrigins(WKContextRef contextRef, WKArrayRef arrayRef)
{
    if (!arrayRef)
        return;
    toImpl(contextRef)->setPlugInAutoStartOrigins(*toImpl(arrayRef));
}

void WKContextSetInvalidMessageFunction(WKContextInvalidMessageFunction invalidMessageFunction)
{
    WebProcessPool::setInvalidMessageCallback(invalidMessageFunction);
}

void WKContextSetMemoryCacheDisabled(WKContextRef contextRef, bool disabled)
{
    toImpl(contextRef)->setMemoryCacheDisabled(disabled);
}

void WKContextSetFontWhitelist(WKContextRef contextRef, WKArrayRef arrayRef)
{
    toImpl(contextRef)->setFontWhitelist(toImpl(arrayRef));
}

void WKContextTerminateNetworkProcess(WKContextRef context)
{
    toImpl(context)->terminateNetworkProcess();
}

void WKContextTerminateServiceWorkerProcess(WKContextRef context)
{
    toImpl(context)->terminateServiceWorkerProcesses();
}

ProcessID WKContextGetNetworkProcessIdentifier(WKContextRef contextRef)
{
    return toImpl(contextRef)->networkProcessIdentifier();
}

void WKContextAddSupportedPlugin(WKContextRef contextRef, WKStringRef domainRef, WKStringRef nameRef, WKArrayRef mimeTypesRef, WKArrayRef extensionsRef)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    HashSet<String> mimeTypes;
    HashSet<String> extensions;

    size_t count = WKArrayGetSize(mimeTypesRef);
    for (size_t i = 0; i < count; ++i)
        mimeTypes.add(toWTFString(static_cast<WKStringRef>(WKArrayGetItemAtIndex(mimeTypesRef, i))));
    count = WKArrayGetSize(extensionsRef);
    for (size_t i = 0; i < count; ++i)
        extensions.add(toWTFString(static_cast<WKStringRef>(WKArrayGetItemAtIndex(extensionsRef, i))));

    toImpl(contextRef)->addSupportedPlugin(toWTFString(domainRef), toWTFString(nameRef), WTFMove(mimeTypes), WTFMove(extensions));
#endif
}

void WKContextClearSupportedPlugins(WKContextRef contextRef)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    toImpl(contextRef)->clearSupportedPlugins();
#endif
}

void WKContextSetIDBPerOriginQuota(WKContextRef contextRef, uint64_t quota)
{
    toImpl(contextRef)->setIDBPerOriginQuota(quota);
}
