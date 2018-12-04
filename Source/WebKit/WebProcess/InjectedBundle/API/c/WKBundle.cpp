/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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
#include "WKBundle.h"

#include "APIArray.h"
#include "APIData.h"
#include "InjectedBundle.h"
#include "InjectedBundleClient.h"
#include "InjectedBundleScriptWorld.h"
#include "WKAPICast.h"
#include "WKBundleAPICast.h"
#include "WKBundlePrivate.h"
#include "WKMutableArray.h"
#include "WKMutableDictionary.h"
#include "WKNumber.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WebConnection.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebPageGroupProxy.h"
#include <WebCore/DatabaseTracker.h>
#include <WebCore/MemoryRelease.h>
#include <WebCore/ResourceLoadObserver.h>
#include <WebCore/ServiceWorkerThreadProxy.h>

WKTypeID WKBundleGetTypeID()
{
    return WebKit::toAPI(WebKit::InjectedBundle::APIType);
}

void WKBundleSetClient(WKBundleRef bundleRef, WKBundleClientBase *wkClient)
{
    WebKit::toImpl(bundleRef)->setClient(std::make_unique<WebKit::InjectedBundleClient>(wkClient));
}

void WKBundleSetServiceWorkerProxyCreationCallback(WKBundleRef bundleRef, void (*callback)(uint64_t))
{
    WebKit::toImpl(bundleRef)->setServiceWorkerProxyCreationCallback(callback);
}

void WKBundlePostMessage(WKBundleRef bundleRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    WebKit::toImpl(bundleRef)->postMessage(WebKit::toWTFString(messageNameRef), WebKit::toImpl(messageBodyRef));
}

void WKBundlePostSynchronousMessage(WKBundleRef bundleRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef, WKTypeRef* returnDataRef)
{
    RefPtr<API::Object> returnData;
    WebKit::toImpl(bundleRef)->postSynchronousMessage(WebKit::toWTFString(messageNameRef), WebKit::toImpl(messageBodyRef), returnData);
    if (returnDataRef)
        *returnDataRef = WebKit::toAPI(returnData.leakRef());
}

WKConnectionRef WKBundleGetApplicationConnection(WKBundleRef bundleRef)
{
    return toAPI(WebKit::toImpl(bundleRef)->webConnectionToUIProcess());
}

void WKBundleGarbageCollectJavaScriptObjects(WKBundleRef bundleRef)
{
    WebKit::toImpl(bundleRef)->garbageCollectJavaScriptObjects();
}

void WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(WKBundleRef bundleRef, bool waitUntilDone)
{
    WebKit::toImpl(bundleRef)->garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(waitUntilDone);
}

size_t WKBundleGetJavaScriptObjectsCount(WKBundleRef bundleRef)
{
    return WebKit::toImpl(bundleRef)->javaScriptObjectsCount();
}

void WKBundleAddUserScript(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef, WKStringRef sourceRef, WKURLRef urlRef, WKArrayRef whitelistRef, WKArrayRef blacklistRef, _WKUserScriptInjectionTime injectionTimeRef, WKUserContentInjectedFrames injectedFramesRef)
{
    WebKit::toImpl(bundleRef)->addUserScript(WebKit::toImpl(pageGroupRef), WebKit::toImpl(scriptWorldRef), WebKit::toWTFString(sourceRef), WebKit::toWTFString(urlRef), WebKit::toImpl(whitelistRef), WebKit::toImpl(blacklistRef), WebKit::toUserScriptInjectionTime(injectionTimeRef), WebKit::toUserContentInjectedFrames(injectedFramesRef));
}

void WKBundleAddUserStyleSheet(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef, WKStringRef sourceRef, WKURLRef urlRef, WKArrayRef whitelistRef, WKArrayRef blacklistRef, WKUserContentInjectedFrames injectedFramesRef)
{
    WebKit::toImpl(bundleRef)->addUserStyleSheet(WebKit::toImpl(pageGroupRef), WebKit::toImpl(scriptWorldRef), WebKit::toWTFString(sourceRef), WebKit::toWTFString(urlRef), WebKit::toImpl(whitelistRef), WebKit::toImpl(blacklistRef), WebKit::toUserContentInjectedFrames(injectedFramesRef));
}

void WKBundleRemoveUserScript(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef, WKURLRef urlRef)
{
    WebKit::toImpl(bundleRef)->removeUserScript(WebKit::toImpl(pageGroupRef), WebKit::toImpl(scriptWorldRef), WebKit::toWTFString(urlRef));
}

void WKBundleRemoveUserStyleSheet(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef, WKURLRef urlRef)
{
    WebKit::toImpl(bundleRef)->removeUserStyleSheet(WebKit::toImpl(pageGroupRef), WebKit::toImpl(scriptWorldRef), WebKit::toWTFString(urlRef));
}

void WKBundleRemoveUserScripts(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef)
{
    WebKit::toImpl(bundleRef)->removeUserScripts(WebKit::toImpl(pageGroupRef), WebKit::toImpl(scriptWorldRef));
}

void WKBundleRemoveUserStyleSheets(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKBundleScriptWorldRef scriptWorldRef)
{
    WebKit::toImpl(bundleRef)->removeUserStyleSheets(WebKit::toImpl(pageGroupRef), WebKit::toImpl(scriptWorldRef));
}

void WKBundleRemoveAllUserContent(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef)
{
    WebKit::toImpl(bundleRef)->removeAllUserContent(WebKit::toImpl(pageGroupRef));
}

void WKBundleOverrideBoolPreferenceForTestRunner(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKStringRef preference, bool enabled)
{
    WebKit::toImpl(bundleRef)->overrideBoolPreferenceForTestRunner(WebKit::toImpl(pageGroupRef), WebKit::toWTFString(preference), enabled);
}

void WKBundleSetAllowUniversalAccessFromFileURLs(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setAllowUniversalAccessFromFileURLs(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleSetAllowFileAccessFromFileURLs(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setAllowFileAccessFromFileURLs(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleSetAllowStorageAccessFromFileURLS(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool needsQuirk)
{
    WebKit::toImpl(bundleRef)->setNeedsStorageAccessFromFileURLsQuirk(WebKit::toImpl(pageGroupRef), needsQuirk);
}

void WKBundleSetMinimumLogicalFontSize(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, int size)
{
    WebKit::toImpl(bundleRef)->setMinimumLogicalFontSize(WebKit::toImpl(pageGroupRef), size);
}

void WKBundleSetFrameFlatteningEnabled(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setFrameFlatteningEnabled(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleSetAsyncFrameScrollingEnabled(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setAsyncFrameScrollingEnabled(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleSetJavaScriptCanAccessClipboard(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setJavaScriptCanAccessClipboard(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleSetPrivateBrowsingEnabled(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setPrivateBrowsingEnabled(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleSetUseDashboardCompatibilityMode(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setUseDashboardCompatibilityMode(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleSetPopupBlockingEnabled(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setPopupBlockingEnabled(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleSetAuthorAndUserStylesEnabled(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setAuthorAndUserStylesEnabled(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleSetSpatialNavigationEnabled(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setSpatialNavigationEnabled(WebKit::toImpl(pageGroupRef), enabled);
}

void WKBundleAddOriginAccessWhitelistEntry(WKBundleRef bundleRef, WKStringRef sourceOrigin, WKStringRef destinationProtocol, WKStringRef destinationHost, bool allowDestinationSubdomains)
{
    WebKit::toImpl(bundleRef)->addOriginAccessWhitelistEntry(WebKit::toWTFString(sourceOrigin), WebKit::toWTFString(destinationProtocol), WebKit::toWTFString(destinationHost), allowDestinationSubdomains);
}

void WKBundleRemoveOriginAccessWhitelistEntry(WKBundleRef bundleRef, WKStringRef sourceOrigin, WKStringRef destinationProtocol, WKStringRef destinationHost, bool allowDestinationSubdomains)
{
    WebKit::toImpl(bundleRef)->removeOriginAccessWhitelistEntry(WebKit::toWTFString(sourceOrigin), WebKit::toWTFString(destinationProtocol), WebKit::toWTFString(destinationHost), allowDestinationSubdomains);
}

void WKBundleResetOriginAccessWhitelists(WKBundleRef bundleRef)
{
    WebKit::toImpl(bundleRef)->resetOriginAccessWhitelists();
}

void WKBundleSetAsynchronousSpellCheckingEnabled(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setAsynchronousSpellCheckingEnabled(WebKit::toImpl(pageGroupRef), enabled);
}

WKArrayRef WKBundleGetLiveDocumentURLs(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, bool excludeDocumentsInPageGroupPages)
{
    auto liveDocuments = WebKit::toImpl(bundleRef)->liveDocumentURLs(WebKit::toImpl(pageGroupRef), excludeDocumentsInPageGroupPages);

    auto liveURLs = adoptWK(WKMutableArrayCreate());

    for (const auto& it : liveDocuments) {
        auto urlInfo = adoptWK(WKMutableDictionaryCreate());

        auto documentIDKey = adoptWK(WKStringCreateWithUTF8CString("id"));
        auto documentURLKey = adoptWK(WKStringCreateWithUTF8CString("url"));

        auto documentIDValue = adoptWK(WKUInt64Create(it.key));
        auto documentURLValue = adoptWK(WebKit::toCopiedAPI(it.value));

        WKDictionarySetItem(urlInfo.get(), documentIDKey.get(), documentIDValue.get());
        WKDictionarySetItem(urlInfo.get(), documentURLKey.get(), documentURLValue.get());

        WKArrayAppendItem(liveURLs.get(), urlInfo.get());
    }
    
    return liveURLs.leakRef();
}

void WKBundleReportException(JSContextRef context, JSValueRef exception)
{
    WebKit::InjectedBundle::reportException(context, exception);
}

void WKBundleClearAllDatabases(WKBundleRef)
{
    WebCore::DatabaseTracker::singleton().deleteAllDatabasesImmediately();
}

void WKBundleSetDatabaseQuota(WKBundleRef bundleRef, uint64_t quota)
{
    // Historically, we've used the following (somewhat nonsensical) string for the databaseIdentifier of local files.
    WebCore::DatabaseTracker::singleton().setQuota(*WebKit::SecurityOriginData::fromDatabaseIdentifier("file__0"), quota);
}

void WKBundleReleaseMemory(WKBundleRef)
{
    WebCore::releaseMemory(WTF::Critical::Yes, WTF::Synchronous::Yes);
}

WKDataRef WKBundleCreateWKDataFromUInt8Array(WKBundleRef bundle, JSContextRef context, JSValueRef data)
{
    return WebKit::toAPI(&WebKit::toImpl(bundle)->createWebDataFromUint8Array(context, data).leakRef());
}

int WKBundleNumberOfPages(WKBundleRef bundleRef, WKBundleFrameRef frameRef, double pageWidthInPixels, double pageHeightInPixels)
{
    return WebKit::toImpl(bundleRef)->numberOfPages(WebKit::toImpl(frameRef), pageWidthInPixels, pageHeightInPixels);
}

int WKBundlePageNumberForElementById(WKBundleRef bundleRef, WKBundleFrameRef frameRef, WKStringRef idRef, double pageWidthInPixels, double pageHeightInPixels)
{
    return WebKit::toImpl(bundleRef)->pageNumberForElementById(WebKit::toImpl(frameRef), WebKit::toWTFString(idRef), pageWidthInPixels, pageHeightInPixels);
}

WKStringRef WKBundlePageSizeAndMarginsInPixels(WKBundleRef bundleRef, WKBundleFrameRef frameRef, int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(bundleRef)->pageSizeAndMarginsInPixels(WebKit::toImpl(frameRef), pageIndex, width, height, marginTop, marginRight, marginBottom, marginLeft));
}

bool WKBundleIsPageBoxVisible(WKBundleRef bundleRef, WKBundleFrameRef frameRef, int pageIndex)
{
    return WebKit::toImpl(bundleRef)->isPageBoxVisible(WebKit::toImpl(frameRef), pageIndex);
}

bool WKBundleIsProcessingUserGesture(WKBundleRef)
{
    return WebKit::InjectedBundle::isProcessingUserGesture();
}

void WKBundleSetUserStyleSheetLocation(WKBundleRef bundleRef, WKBundlePageGroupRef pageGroupRef, WKStringRef location)
{
    WebKit::toImpl(bundleRef)->setUserStyleSheetLocation(WebKit::toImpl(pageGroupRef), WebKit::toWTFString(location));
}

void WKBundleSetWebNotificationPermission(WKBundleRef bundleRef, WKBundlePageRef pageRef, WKStringRef originStringRef, bool allowed)
{
    WebKit::toImpl(bundleRef)->setWebNotificationPermission(WebKit::toImpl(pageRef), WebKit::toWTFString(originStringRef), allowed);
}

void WKBundleRemoveAllWebNotificationPermissions(WKBundleRef bundleRef, WKBundlePageRef pageRef)
{
    WebKit::toImpl(bundleRef)->removeAllWebNotificationPermissions(WebKit::toImpl(pageRef));
}

uint64_t WKBundleGetWebNotificationID(WKBundleRef bundleRef, JSContextRef context, JSValueRef notification)
{
    return WebKit::toImpl(bundleRef)->webNotificationID(context, notification);
}

void WKBundleSetTabKeyCyclesThroughElements(WKBundleRef bundleRef, WKBundlePageRef pageRef, bool enabled)
{
    WebKit::toImpl(bundleRef)->setTabKeyCyclesThroughElements(WebKit::toImpl(pageRef), enabled);
}

void WKBundleClearResourceLoadStatistics(WKBundleRef)
{
    WebCore::ResourceLoadObserver::shared().clearState();
}

void WKBundleResourceLoadStatisticsNotifyObserver(WKBundleRef)
{
    WebCore::ResourceLoadObserver::shared().notifyObserver();
}


void WKBundleExtendClassesForParameterCoder(WKBundleRef bundle, WKArrayRef classes)
{
#if PLATFORM(COCOA) && WK_API_ENABLED
    auto classList = WebKit::toImpl(classes);
    if (!classList)
        return;

    WebKit::toImpl(bundle)->extendClassesForParameterCoder(*classList);
#endif
}
