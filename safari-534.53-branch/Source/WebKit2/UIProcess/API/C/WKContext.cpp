/*
 * Copyright (C) 2010, 2011 Apple Inc. All rights reserved.
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
#include "WKContext.h"
#include "WKContextPrivate.h"

#include "WKAPICast.h"
#include "WebContext.h"
#include "WebURLRequest.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;

WKTypeID WKContextGetTypeID()
{
    return toAPI(WebContext::APIType);
}

WKContextRef WKContextCreate()
{
    RefPtr<WebContext> context = WebContext::create(String());
    return toAPI(context.release().releaseRef());
}

WKContextRef WKContextCreateWithInjectedBundlePath(WKStringRef pathRef)
{
    RefPtr<WebContext> context = WebContext::create(toImpl(pathRef)->string());
    return toAPI(context.release().releaseRef());
}

WKContextRef WKContextGetSharedProcessContext()
{
    return toAPI(WebContext::sharedProcessContext());
}

WKContextRef WKContextGetSharedThreadContext()
{
    return toAPI(WebContext::sharedThreadContext());
}

void WKContextSetInjectedBundleClient(WKContextRef contextRef, const WKContextInjectedBundleClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(contextRef)->initializeInjectedBundleClient(wkClient);
}

void WKContextSetHistoryClient(WKContextRef contextRef, const WKContextHistoryClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(contextRef)->initializeHistoryClient(wkClient);
}

void WKContextSetDownloadClient(WKContextRef contextRef, const WKContextDownloadClient* wkClient)
{
    if (wkClient && wkClient->version)
        return;
    toImpl(contextRef)->initializeDownloadClient(wkClient);
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
    statistics->wkFrameCount = webContextStatistics.wkViewCount;
}

void WKContextAddVisitedLink(WKContextRef contextRef, WKStringRef visitedURL)
{
    toImpl(contextRef)->addVisitedLink(toImpl(visitedURL)->string());
}

void WKContextSetCacheModel(WKContextRef contextRef, WKCacheModel cacheModel)
{
    toImpl(contextRef)->setCacheModel(toCacheModel(cacheModel));
}

WKCacheModel WKContextGetCacheModel(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->cacheModel());
}

void _WKContextSetAlwaysUsesComplexTextCodePath(WKContextRef contextRef, bool alwaysUseComplexTextCodePath)
{
    toImpl(contextRef)->setAlwaysUsesComplexTextCodePath(alwaysUseComplexTextCodePath);
}

void _WKContextSetAdditionalPluginsDirectory(WKContextRef contextRef, WKStringRef pluginsDirectory)
{
    toImpl(contextRef)->setAdditionalPluginsDirectory(toImpl(pluginsDirectory)->string());
}

void _WKContextRegisterURLSchemeAsEmptyDocument(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->registerURLSchemeAsEmptyDocument(toImpl(urlScheme)->string());
}

void WKContextRegisterURLSchemeAsSecure(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->registerURLSchemeAsSecure(toImpl(urlScheme)->string());
}

void WKContextSetDomainRelaxationForbiddenForURLScheme(WKContextRef contextRef, WKStringRef urlScheme)
{
    toImpl(contextRef)->setDomainRelaxationForbiddenForURLScheme(toImpl(urlScheme)->string());
}

WKCookieManagerRef WKContextGetCookieManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->cookieManagerProxy());
}

WKApplicationCacheManagerRef WKContextGetApplicationCacheManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->applicationCacheManagerProxy());
}

WKDatabaseManagerRef WKContextGetDatabaseManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->databaseManagerProxy());
}

WKGeolocationManagerRef WKContextGetGeolocationManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->geolocationManagerProxy());
}

WKIconDatabaseRef WKContextGetIconDatabase(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->iconDatabase());
}

WKKeyValueStorageManagerRef WKContextGetKeyValueStorageManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->keyValueStorageManagerProxy());
}

WKMediaCacheManagerRef WKContextGetMediaCacheManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->mediaCacheManagerProxy());
}

WKPluginSiteDataManagerRef WKContextGetPluginSiteDataManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->pluginSiteDataManager());
}

WKResourceCacheManagerRef WKContextGetResourceCacheManager(WKContextRef contextRef)
{
    return toAPI(toImpl(contextRef)->resourceCacheManagerProxy());
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

void WKContextSetDatabaseDirectory(WKContextRef contextRef, WKStringRef databaseDirectory)
{
    toImpl(contextRef)->setDatabaseDirectory(toImpl(databaseDirectory)->string());
}

void WKContextSetLocalStorageDirectory(WKContextRef contextRef, WKStringRef localStorageDirectory)
{
    toImpl(contextRef)->setLocalStorageDirectory(toImpl(localStorageDirectory)->string());
}

void WKContextDisableProcessTermination(WKContextRef contextRef)
{
    toImpl(contextRef)->disableProcessTermination();
}

void WKContextEnableProcessTermination(WKContextRef contextRef)
{
    toImpl(contextRef)->enableProcessTermination();
}

void _WKContextSetHTTPPipeliningEnabled(WKContextRef contextRef, bool enabled)
{
    toImpl(contextRef)->setHTTPPipeliningEnabled(enabled);
}

void WKContextWarmInitialProcess(WKContextRef contextRef)
{
    toImpl(contextRef)->warmInitialProcess();
}

void WKContextGetStatistics(WKContextRef contextRef, void* context, WKContextGetStatisticsFunction callback)
{
    toImpl(contextRef)->getWebCoreStatistics(DictionaryCallback::create(context, callback));    
}

void WKContextGarbageCollectJavaScriptObjects(WKContextRef contextRef)
{
    toImpl(contextRef)->garbageCollectJavaScriptObjects();
}

