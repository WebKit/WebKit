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
#include "WKData.h"
#include "WKMutableArray.h"
#include "WKMutableDictionary.h"
#include "WKNumber.h"
#include "WKRetainPtr.h"
#include "WKString.h"
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
    WebKit::toProtectedImpl(bundleRef)->setClient(makeUnique<WebKit::InjectedBundleClient>(wkClient));
}

void WKBundleSetServiceWorkerProxyCreationCallback(WKBundleRef bundleRef, void (*callback)(uint64_t))
{
    WebKit::toProtectedImpl(bundleRef)->setServiceWorkerProxyCreationCallback(callback);
}

void WKBundlePostMessage(WKBundleRef bundleRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef)
{
    WebKit::toProtectedImpl(bundleRef)->postMessage(WebKit::toWTFString(messageNameRef), WebKit::toProtectedImpl(messageBodyRef).get());
}

void WKBundlePostSynchronousMessage(WKBundleRef bundleRef, WKStringRef messageNameRef, WKTypeRef messageBodyRef, WKTypeRef* returnRetainedDataRef)
{
    RefPtr<API::Object> returnData;
    WebKit::toProtectedImpl(bundleRef)->postSynchronousMessage(WebKit::toWTFString(messageNameRef), WebKit::toProtectedImpl(messageBodyRef).get(), returnData);
    if (returnRetainedDataRef)
        SUPPRESS_UNCOUNTED_ARG *returnRetainedDataRef = WebKit::toAPI(returnData.leakRef());
}

void WKBundleGarbageCollectJavaScriptObjects(WKBundleRef bundleRef)
{
    WebKit::toProtectedImpl(bundleRef)->garbageCollectJavaScriptObjects();
}

void WKBundleGarbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(WKBundleRef bundleRef, bool waitUntilDone)
{
    WebKit::toProtectedImpl(bundleRef)->garbageCollectJavaScriptObjectsOnAlternateThreadForDebugging(waitUntilDone);
}

size_t WKBundleGetJavaScriptObjectsCount(WKBundleRef bundleRef)
{
    return WebKit::toProtectedImpl(bundleRef)->javaScriptObjectsCount();
}

void WKBundleAddOriginAccessAllowListEntry(WKBundleRef bundleRef, WKStringRef sourceOrigin, WKStringRef destinationProtocol, WKStringRef destinationHost, bool allowDestinationSubdomains)
{
    WebKit::toProtectedImpl(bundleRef)->addOriginAccessAllowListEntry(WebKit::toWTFString(sourceOrigin), WebKit::toWTFString(destinationProtocol), WebKit::toWTFString(destinationHost), allowDestinationSubdomains);
}

void WKBundleRemoveOriginAccessAllowListEntry(WKBundleRef bundleRef, WKStringRef sourceOrigin, WKStringRef destinationProtocol, WKStringRef destinationHost, bool allowDestinationSubdomains)
{
    WebKit::toProtectedImpl(bundleRef)->removeOriginAccessAllowListEntry(WebKit::toWTFString(sourceOrigin), WebKit::toWTFString(destinationProtocol), WebKit::toWTFString(destinationHost), allowDestinationSubdomains);
}

void WKBundleResetOriginAccessAllowLists(WKBundleRef bundleRef)
{
    WebKit::toProtectedImpl(bundleRef)->resetOriginAccessAllowLists();
}

void WKBundleSetAsynchronousSpellCheckingEnabledForTesting(WKBundleRef bundleRef, bool enabled)
{
    WebKit::toProtectedImpl(bundleRef)->setAsynchronousSpellCheckingEnabled(enabled);
}

WKArrayRef WKBundleGetLiveDocumentURLsForTesting(WKBundleRef bundleRef, bool excludeDocumentsInPageGroupPages)
{
    auto liveDocuments = WebKit::toProtectedImpl(bundleRef)->liveDocumentURLs(excludeDocumentsInPageGroupPages);

    auto liveURLs = adoptWK(WKMutableArrayCreate());

    for (const auto& it : liveDocuments) {
        auto urlInfo = adoptWK(WKMutableDictionaryCreate());

        auto documentIDKey = adoptWK(WKStringCreateWithUTF8CString("id"));
        auto documentURLKey = adoptWK(WKStringCreateWithUTF8CString("url"));

        auto documentIDValue = adoptWK(WebKit::toCopiedAPI(it.key.toString()));
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

void WKBundleSetDatabaseQuota(WKBundleRef bundleRef, uint64_t quota)
{
    // Historically, we've used the following (somewhat nonsensical) string for the databaseIdentifier of local files.
    WebCore::DatabaseTracker::singleton().setQuota(*WebCore::SecurityOriginData::fromDatabaseIdentifier("file__0"_s), quota);
}

void WKBundleReleaseMemory(WKBundleRef)
{
    WebCore::releaseMemory(WTF::Critical::Yes, WTF::Synchronous::Yes);
}

WKDataRef WKBundleCreateWKDataFromUInt8Array(WKBundleRef bundle, JSContextRef context, JSValueRef data)
{
    SUPPRESS_UNCOUNTED_ARG return WebKit::toAPI(&WebKit::toProtectedImpl(bundle)->createWebDataFromUint8Array(context, data).leakRef());
}

int WKBundleNumberOfPages(WKBundleRef bundleRef, WKBundleFrameRef frameRef, double pageWidthInPixels, double pageHeightInPixels)
{
    return WebKit::toProtectedImpl(bundleRef)->numberOfPages(WebKit::toProtectedImpl(frameRef).get(), pageWidthInPixels, pageHeightInPixels);
}

int WKBundlePageNumberForElementById(WKBundleRef bundleRef, WKBundleFrameRef frameRef, WKStringRef idRef, double pageWidthInPixels, double pageHeightInPixels)
{
    return WebKit::toProtectedImpl(bundleRef)->pageNumberForElementById(WebKit::toProtectedImpl(frameRef).get(), WebKit::toWTFString(idRef), pageWidthInPixels, pageHeightInPixels);
}

WKStringRef WKBundlePageSizeAndMarginsInPixels(WKBundleRef bundleRef, WKBundleFrameRef frameRef, int pageIndex, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    return WebKit::toCopiedAPI(WebKit::toProtectedImpl(bundleRef)->pageSizeAndMarginsInPixels(WebKit::toProtectedImpl(frameRef).get(), pageIndex, width, height, marginTop, marginRight, marginBottom, marginLeft));
}

bool WKBundleIsPageBoxVisible(WKBundleRef bundleRef, WKBundleFrameRef frameRef, int pageIndex)
{
    return WebKit::toProtectedImpl(bundleRef)->isPageBoxVisible(WebKit::toProtectedImpl(frameRef).get(), pageIndex);
}

bool WKBundleIsProcessingUserGesture(WKBundleRef)
{
    return WebKit::InjectedBundle::isProcessingUserGesture();
}

void WKBundleSetUserStyleSheetLocationForTesting(WKBundleRef bundleRef, WKStringRef location)
{
    WebKit::toProtectedImpl(bundleRef)->setUserStyleSheetLocation(WebKit::toWTFString(location));
}

void WKBundleRemoveAllWebNotificationPermissions(WKBundleRef bundleRef, WKBundlePageRef pageRef)
{
    WebKit::toProtectedImpl(bundleRef)->removeAllWebNotificationPermissions(WebKit::toProtectedImpl(pageRef).get());
}

WKDataRef WKBundleCopyWebNotificationID(WKBundleRef bundleRef, JSContextRef context, JSValueRef notification)
{
    auto identifier = WebKit::toProtectedImpl(bundleRef)->webNotificationID(context, notification);
    if (!identifier)
        return nullptr;

    auto span = identifier->span();
    return WKDataCreate(span.data(), span.size());
}

void WKBundleSetTabKeyCyclesThroughElements(WKBundleRef bundleRef, WKBundlePageRef pageRef, bool enabled)
{
    WebKit::toProtectedImpl(bundleRef)->setTabKeyCyclesThroughElements(WebKit::toProtectedImpl(pageRef).get(), enabled);
}

void WKBundleClearResourceLoadStatistics(WKBundleRef)
{
    WebCore::ResourceLoadObserver::singleton().clearState();
}

void WKBundleResourceLoadStatisticsNotifyObserver(WKBundleRef, void* context, NotifyObserverCallback callback)
{
    if (!WebCore::ResourceLoadObserver::singleton().hasStatistics())
        return callback(context);

    WebCore::ResourceLoadObserver::singleton().updateCentralStatisticsStore([context, callback] {
        callback(context);
    });
}

void WKBundleExtendClassesForParameterCoder(WKBundleRef bundle, WKArrayRef classes)
{
#if PLATFORM(COCOA)
    RefPtr classList = WebKit::toImpl(classes);
    if (!classList)
        return;

    WebKit::toProtectedImpl(bundle)->extendClassesForParameterCoder(*classList);
#endif
}
