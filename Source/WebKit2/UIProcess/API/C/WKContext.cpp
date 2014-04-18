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

#include "APIClient.h"
#include "APIDownloadClient.h"
#include "APIHistoryClient.h"
#include "APINavigationData.h"
#include "APIURLRequest.h"
#include "WKAPICast.h"
#include "WKRetainPtr.h"
#include "WebContext.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

// Supplements
#include "WebApplicationCacheManagerProxy.h"
#include "WebCookieManagerProxy.h"
#include "WebGeolocationManagerProxy.h"
#include "WebKeyValueStorageManager.h"
#include "WebMediaCacheManagerProxy.h"
#include "WebNotificationManagerProxy.h"
#include "WebOriginDataManagerProxy.h"
#include "WebResourceCacheManagerProxy.h"
#if ENABLE(SQL_DATABASE)
#include "WebDatabaseManagerProxy.h"
#endif
#if ENABLE(BATTERY_STATUS)
#include "WebBatteryManagerProxy.h"
#endif

namespace API {
template<> struct ClientTraits<WKContextDownloadClientBase> {
    typedef std::tuple<WKContextDownloadClientV0> Versions;
};
template<> struct ClientTraits<WKContextHistoryClientBase> {
    typedef std::tuple<WKContextHistoryClientV0> Versions;
};
}

using namespace WebCore;
using namespace WebKit;

typedef GenericAPICallback<WKDictionaryRef> DictionaryAPICallback;

WKTypeID WKContextGetTypeID()
{
    return toAPI(WebContext::APIType);
}

WKContextRef WKContextCreate()
{
    RefPtr<WebContext> context = WebContext::create(String());
    return toAPI(context.release().leakRef());
}

WKContextRef WKContextCreateWithInjectedBundlePath(WKStringRef pathRef)
{
    RefPtr<WebContext> context = WebContext::create(toImpl(pathRef)->string());
    return toAPI(context.release().leakRef());
}

void WKContextSetClient(WKContextRef contextRef, const WKContextClientBase* wkClient)
{
    toImpl(contextRef)->initializeClient(wkClient);
}

void WKContextSetInjectedBundleClient(WKContextRef contextRef, const WKContextInjectedBundleClientBase* wkClient)
{
    toImpl(contextRef)->initializeInjectedBundleClient(wkClient);
}

void WKContextSetHistoryClient(WKContextRef contextRef, const WKContextHistoryClientBase* wkClient)
{
    class HistoryClient final : public API::Client<WKContextHistoryClientBase>, public API::HistoryClient {
    public:
        explicit HistoryClient(const WKContextHistoryClientBase* client)
        {
            initialize(client);
        }

    private:
        virtual void didNavigateWithNavigationData(WebContext* context, WebPageProxy* page, const WebNavigationDataStore& navigationDataStore, WebFrameProxy* frame) override
        {
            if (!m_client.didNavigateWithNavigationData)
                return;

            RefPtr<API::NavigationData> navigationData = API::NavigationData::create(navigationDataStore);
            m_client.didNavigateWithNavigationData(toAPI(context), toAPI(page), toAPI(navigationData.get()), toAPI(frame), m_client.base.clientInfo);
        }

        virtual void didPerformClientRedirect(WebContext* context, WebPageProxy* page, const String& sourceURL, const String& destinationURL, WebFrameProxy* frame) override
        {
            if (!m_client.didPerformClientRedirect)
                return;

            m_client.didPerformClientRedirect(toAPI(context), toAPI(page), toURLRef(sourceURL.impl()), toURLRef(destinationURL.impl()), toAPI(frame), m_client.base.clientInfo);
        }

        virtual void didPerformServerRedirect(WebContext* context, WebPageProxy* page, const String& sourceURL, const String& destinationURL, WebFrameProxy* frame) override
        {
            if (!m_client.didPerformServerRedirect)
                return;

            m_client.didPerformServerRedirect(toAPI(context), toAPI(page), toURLRef(sourceURL.impl()), toURLRef(destinationURL.impl()), toAPI(frame), m_client.base.clientInfo);
        }

        virtual void didUpdateHistoryTitle(WebContext* context, WebPageProxy* page, const String& title, const String& url, WebFrameProxy* frame) override
        {
            if (!m_client.didUpdateHistoryTitle)
                return;

            m_client.didUpdateHistoryTitle(toAPI(context), toAPI(page), toAPI(title.impl()), toURLRef(url.impl()), toAPI(frame), m_client.base.clientInfo);
        }

        virtual void populateVisitedLinks(WebContext* context) override
        {
            if (!m_client.populateVisitedLinks)
                return;

            m_client.populateVisitedLinks(toAPI(context), m_client.base.clientInfo);
        }

        virtual bool shouldTrackVisitedLinks() const
        {
            return m_client.populateVisitedLinks;
        }
    };
    
    toImpl(contextRef)->setHistoryClient(std::make_unique<HistoryClient>(wkClient));
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
        virtual void didStart(WebContext* webContext, DownloadProxy* downloadProxy) override
        {
            if (!m_client.didStart)
                return;

            m_client.didStart(toAPI(webContext), toAPI(downloadProxy), m_client.base.clientInfo);
        }

        virtual void didReceiveAuthenticationChallenge(WebContext* webContext, DownloadProxy* downloadProxy, AuthenticationChallengeProxy* authenticationChallengeProxy) override
        {
            if (!m_client.didReceiveAuthenticationChallenge)
                return;

            m_client.didReceiveAuthenticationChallenge(toAPI(webContext), toAPI(downloadProxy), toAPI(authenticationChallengeProxy), m_client.base.clientInfo);
        }

        virtual void didReceiveResponse(WebContext* webContext, DownloadProxy* downloadProxy, const ResourceResponse& response) override
        {
            if (!m_client.didReceiveResponse)
                return;

            m_client.didReceiveResponse(toAPI(webContext), toAPI(downloadProxy), toAPI(API::URLResponse::create(response).get()), m_client.base.clientInfo);
        }

        virtual void didReceiveData(WebContext* webContext, DownloadProxy* downloadProxy, uint64_t length) override
        {
            if (!m_client.didReceiveData)
                return;

            m_client.didReceiveData(toAPI(webContext), toAPI(downloadProxy), length, m_client.base.clientInfo);
        }

        virtual bool shouldDecodeSourceDataOfMIMEType(WebContext* webContext, DownloadProxy* downloadProxy, const String& mimeType) override
        {
            if (!m_client.shouldDecodeSourceDataOfMIMEType)
                return true;

            return m_client.shouldDecodeSourceDataOfMIMEType(toAPI(webContext), toAPI(downloadProxy), toAPI(mimeType.impl()), m_client.base.clientInfo);
        }

        virtual String decideDestinationWithSuggestedFilename(WebContext* webContext, DownloadProxy* downloadProxy, const String& filename, bool& allowOverwrite) override
        {
            if (!m_client.decideDestinationWithSuggestedFilename)
                return String();

            WKRetainPtr<WKStringRef> destination(AdoptWK, m_client.decideDestinationWithSuggestedFilename(toAPI(webContext), toAPI(downloadProxy), toAPI(filename.impl()), &allowOverwrite, m_client.base.clientInfo));
            return toWTFString(destination.get());
        }

        virtual void didCreateDestination(WebContext* webContext, DownloadProxy* downloadProxy, const String& path) override
        {
            if (!m_client.didCreateDestination)
                return;

            m_client.didCreateDestination(toAPI(webContext), toAPI(downloadProxy), toAPI(path.impl()), m_client.base.clientInfo);
        }

        virtual void didFinish(WebContext* webContext, DownloadProxy* downloadProxy) override
        {
            if (!m_client.didFinish)
                return;

            m_client.didFinish(toAPI(webContext), toAPI(downloadProxy), m_client.base.clientInfo);
        }

        virtual void didFail(WebContext* webContext, DownloadProxy* downloadProxy, const ResourceError& error) override
        {
            if (!m_client.didFail)
                return;

            m_client.didFail(toAPI(webContext), toAPI(downloadProxy), toAPI(error), m_client.base.clientInfo);
        }
        
        virtual void didCancel(WebContext* webContext, DownloadProxy* downloadProxy) override
        {
            if (!m_client.didCancel)
                return;
            
            m_client.didCancel(toAPI(webContext), toAPI(downloadProxy), m_client.base.clientInfo);
        }
        
        virtual void processDidCrash(WebContext* webContext, DownloadProxy* downloadProxy) override
        {
            if (!m_client.processDidCrash)
                return;
            
            m_client.processDidCrash(toAPI(webContext), toAPI(downloadProxy), m_client.base.clientInfo);
        }

    };

    toImpl(contextRef)->setDownloadClient(std::make_unique<DownloadClient>(wkClient));
}

void WKContextSetConnectionClient(WKContextRef contextRef, const WKContextConnectionClientBase* wkClient)
{
    toImpl(contextRef)->initializeConnectionClient(wkClient);
}

WKDownloadRef WKContextDownloadURLRequest(WKContextRef contextRef, const WKURLRequestRef requestRef)
{
    return toAPI(toImpl(contextRef)->download(0, toImpl(requestRef)->resourceRequest()));
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
    const WebContext::Statistics& webContextStatistics = WebContext::statistics();

    statistics->wkViewCount = webContextStatistics.wkViewCount;
    statistics->wkPageCount = webContextStatistics.wkPageCount;
    statistics->wkFrameCount = webContextStatistics.wkFrameCount;
}

void WKContextAddVisitedLink(WKContextRef contextRef, WKStringRef visitedURL)
{
    String visitedURLString = toImpl(visitedURL)->string();
    if (visitedURLString.isEmpty())
        return;

    toImpl(contextRef)->visitedLinkProvider().addVisitedLinkHash(visitedLinkHash(visitedURLString));
}

void WKContextSetCacheModel(WKContextRef contextRef, WKCacheModel cacheModel)
{
    toImpl(contextRef)->setCacheModel(toCacheModel(cacheModel));
}

WKCacheModel WKContextGetCacheModel(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->cacheModel());
}

void WKContextSetProcessModel(WKContextRef contextRef, WKProcessModel processModel)
{
    toImpl(contextRef)->setProcessModel(toProcessModel(processModel));
}

WKProcessModel WKContextGetProcessModel(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->processModel());
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

void WKContextRegisterURLSchemeAsEmptyDocument(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->registerURLSchemeAsEmptyDocument(toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsSecure(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->registerURLSchemeAsSecure(toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsCachePartitioned(WKContextRef contextRef, WKStringRef urlScheme)
{
#if ENABLE(CACHE_PARTITIONING)
    toImpl(contextRef)->registerURLSchemeAsCachePartitioned(toImpl(urlScheme)->string());
#else
    UNUSED_PARAM(contextRef);
    UNUSED_PARAM(urlScheme);
#endif
}

void WKContextSetDomainRelaxationForbiddenForURLScheme(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->setDomainRelaxationForbiddenForURLScheme(toImpl(urlScheme)->string());
}

WKCookieManagerRef WKContextGetCookieManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebCookieManagerProxy>());
}

WKApplicationCacheManagerRef WKContextGetApplicationCacheManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebApplicationCacheManagerProxy>());
}

WKBatteryManagerRef WKContextGetBatteryManager(WKContextRef contextRef)
{
#if ENABLE(BATTERY_STATUS)
    return toAPI(toImpl(contextRef)->supplement<WebBatteryManagerProxy>());
#else
    UNUSED_PARAM(contextRef);
    return 0;
#endif
}

WKDatabaseManagerRef WKContextGetDatabaseManager(WKContextRef contextRef)
{
#if ENABLE(SQL_DATABASE)
    return toAPI(toImpl(contextRef)->supplement<WebDatabaseManagerProxy>());
#else
    UNUSED_PARAM(contextRef);
    return 0;
#endif
}

WKGeolocationManagerRef WKContextGetGeolocationManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebGeolocationManagerProxy>());
}

WKIconDatabaseRef WKContextGetIconDatabase(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->iconDatabase());
}

WKKeyValueStorageManagerRef WKContextGetKeyValueStorageManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebKeyValueStorageManager>());
}

WKMediaCacheManagerRef WKContextGetMediaCacheManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebMediaCacheManagerProxy>());
}

WKNotificationManagerRef WKContextGetNotificationManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebNotificationManagerProxy>());
}

WKPluginSiteDataManagerRef WKContextGetPluginSiteDataManager(WKContextRef contextRef)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    return toAPI(toImpl(contextRef)->pluginSiteDataManager());
#else
    UNUSED_PARAM(contextRef);
    return 0;
#endif
}

WKResourceCacheManagerRef WKContextGetResourceCacheManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebResourceCacheManagerProxy>());
}

WKOriginDataManagerRef WKContextGetOriginDataManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->supplement<WebOriginDataManagerProxy>());
}

void WKContextStartMemorySampler(WKContextRef contextRef, WKDoubleRef interval)
{
    toImpl(contextRef)->startMemorySampler(toImpl(interval)->value());
}

void WKContextStopMemorySampler(WKContextRef contextRef)
{
    toImpl(contextRef)->stopMemorySampler();
}

void WKContextSetIconDatabasePath(WKContextRef contextRef, WKStringRef iconDatabasePath)
{
    toImpl(contextRef)->setIconDatabasePath(toImpl(iconDatabasePath)->string());
}

void WKContextAllowSpecificHTTPSCertificateForHost(WKContextRef contextRef, WKCertificateInfoRef certificateRef, WKStringRef hostRef)
{
    toImpl(contextRef)->allowSpecificHTTPSCertificateForHost(toImpl(certificateRef), toImpl(hostRef)->string());
}

WK_EXPORT void WKContextSetApplicationCacheDirectory(WKContextRef contextRef, WKStringRef applicationCacheDirectory)
{
    toImpl(contextRef)->setApplicationCacheDirectory(toImpl(applicationCacheDirectory)->string());
}

void WKContextSetDatabaseDirectory(WKContextRef contextRef, WKStringRef databaseDirectory)
{
    toImpl(contextRef)->setDatabaseDirectory(toImpl(databaseDirectory)->string());
}

void WKContextSetLocalStorageDirectory(WKContextRef contextRef, WKStringRef localStorageDirectory)
{
    toImpl(contextRef)->setLocalStorageDirectory(toImpl(localStorageDirectory)->string());
}

WK_EXPORT void WKContextSetDiskCacheDirectory(WKContextRef contextRef, WKStringRef diskCacheDirectory)
{
    toImpl(contextRef)->setDiskCacheDirectory(toImpl(diskCacheDirectory)->string());
}

WK_EXPORT void WKContextSetCookieStorageDirectory(WKContextRef contextRef, WKStringRef cookieStorageDirectory)
{
    toImpl(contextRef)->setCookieStorageDirectory(toImpl(cookieStorageDirectory)->string());
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
    toImpl(contextRef)->warmInitialProcess();
}

void WKContextGetStatistics(WKContextRef contextRef, void* context, WKContextGetStatisticsFunction callback)
{
    toImpl(contextRef)->getStatistics(0xFFFFFFFF, DictionaryAPICallback::create(context, callback));
}

void WKContextGetStatisticsWithOptions(WKContextRef contextRef, WKStatisticsOptions optionsMask, void* context, WKContextGetStatisticsFunction callback)
{
    toImpl(contextRef)->getStatistics(optionsMask, DictionaryAPICallback::create(context, callback));
}

void WKContextGarbageCollectJavaScriptObjects(WKContextRef contextRef)
{
    toImpl(contextRef)->garbageCollectJavaScriptObjects();
}

void WKContextSetJavaScriptGarbageCollectorTimerEnabled(WKContextRef contextRef, bool enable)
{
    toImpl(contextRef)->setJavaScriptGarbageCollectorTimerEnabled(enable);
}

void WKContextSetUsesNetworkProcess(WKContextRef contextRef, bool usesNetworkProcess)
{
    toImpl(contextRef)->setUsesNetworkProcess(usesNetworkProcess);
}

void WKContextUseTestingNetworkSession(WKContextRef context)
{
    toImpl(context)->useTestingNetworkSession();
}

WKDictionaryRef WKContextCopyPlugInAutoStartOriginHashes(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->plugInAutoStartOriginHashes().leakRef());
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
    toImpl(contextRef)->setPlugInAutoStartOriginsFilteringOutEntriesAddedAfterTime(*toImpl(dictionaryRef), time);
}

void WKContextSetPlugInAutoStartOrigins(WKContextRef contextRef, WKArrayRef arrayRef)
{
    if (!arrayRef)
        return;
    toImpl(contextRef)->setPlugInAutoStartOrigins(*toImpl(arrayRef));
}

void WKContextSetInvalidMessageFunction(WKContextInvalidMessageFunction invalidMessageFunction)
{
    WebContext::setInvalidMessageCallback(invalidMessageFunction);
}

void WKContextSetMemoryCacheDisabled(WKContextRef contextRef, bool disabled)
{
    toImpl(contextRef)->setMemoryCacheDisabled(disabled);
}
