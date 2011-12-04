/*
 * Copyright (C) 2007, 2008, 2009, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Joone Hur <joone@kldp.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "LayoutTestController.h"

#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include <cstring>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <locale.h>
#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/OwnArrayPtr.h>
#include <wtf/RefPtr.h>

LayoutTestController::LayoutTestController(const std::string& testPathOrURL, const std::string& expectedPixelHash)
    : m_disallowIncreaseForApplicationCacheQuota(false)
    , m_dumpApplicationCacheDelegateCallbacks(false)
    , m_dumpAsAudio(false)
    , m_dumpAsPDF(false)
    , m_dumpAsText(false)
    , m_dumpBackForwardList(false)
    , m_dumpChildFrameScrollPositions(false)
    , m_dumpChildFramesAsText(false)
    , m_dumpDOMAsWebArchive(false)
    , m_dumpDatabaseCallbacks(false)
    , m_dumpEditingCallbacks(false)
    , m_dumpFrameLoadCallbacks(false)
    , m_dumpProgressFinishedCallback(false)
    , m_dumpUserGestureInFrameLoadCallbacks(false)
    , m_dumpHistoryDelegateCallbacks(false)
    , m_dumpResourceLoadCallbacks(false)
    , m_dumpResourceResponseMIMETypes(false)
    , m_dumpSelectionRect(false)
    , m_dumpSourceAsWebArchive(false)
    , m_dumpStatusCallbacks(false)
    , m_dumpTitleChanges(false)
    , m_dumpIconChanges(false)
    , m_dumpVisitedLinksCallback(false)
    , m_dumpWillCacheResponse(false)
    , m_generatePixelResults(true)
    , m_callCloseOnWebViews(true)
    , m_canOpenWindows(false)
    , m_closeRemainingWindowsWhenComplete(true)
    , m_newWindowsCopyBackForwardList(false)
    , m_stopProvisionalFrameLoads(false)
    , m_testOnscreen(false)
    , m_testRepaint(false)
    , m_testRepaintSweepHorizontally(false)
    , m_waitToDump(false)
    , m_willSendRequestReturnsNull(false)
    , m_willSendRequestReturnsNullOnRedirect(false)
    , m_windowIsKey(true)
    , m_alwaysAcceptCookies(false)
    , m_globalFlag(false)
    , m_isGeolocationPermissionSet(false)
    , m_geolocationPermission(false)
    , m_handlesAuthenticationChallenges(false)
    , m_isPrinting(false)
    , m_deferMainResourceDataLoad(true)
    , m_shouldPaintBrokenImage(true)
    , m_shouldStayOnPageAfterHandlingBeforeUnload(false)
    , m_testPathOrURL(testPathOrURL)
    , m_expectedPixelHash(expectedPixelHash)
    , m_areDesktopNotificationPermissionRequestsIgnored(false)
{
}

PassRefPtr<LayoutTestController> LayoutTestController::create(const std::string& testPathOrURL, const std::string& expectedPixelHash)
{
    return adoptRef(new LayoutTestController(testPathOrURL, expectedPixelHash));
}

// Static Functions

static JSValueRef disallowIncreaseForApplicationCacheQuotaCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDisallowIncreaseForApplicationCacheQuota(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpApplicationCacheDelegateCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpApplicationCacheDelegateCallbacks(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpAsPDFCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpAsPDF(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpAsTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpAsText(true);

    // Optional paramater, describing whether it's allowed to dump pixel results in dumpAsText mode.
    controller->setGeneratePixelResults(argumentCount > 0 ? JSValueToBoolean(context, arguments[0]) : false);

    return JSValueMakeUndefined(context);
}

static JSValueRef dumpBackForwardListCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpBackForwardList(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpChildFramesAsTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpChildFramesAsText(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpChildFrameScrollPositionsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpChildFrameScrollPositions(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpConfigurationForViewportCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);


    double deviceDPI = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);
    double deviceWidth = JSValueToNumber(context, arguments[1], exception);
    ASSERT(!*exception);
    double deviceHeight = JSValueToNumber(context, arguments[2], exception);
    ASSERT(!*exception);
    double availableWidth = JSValueToNumber(context, arguments[3], exception);
    ASSERT(!*exception);
    double availableHeight = JSValueToNumber(context, arguments[4], exception);
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->dumpConfigurationForViewport(static_cast<int>(deviceDPI), static_cast<int>(deviceWidth), static_cast<int>(deviceHeight), static_cast<int>(availableWidth), static_cast<int>(availableHeight));

    return JSValueMakeUndefined(context);
}

static JSValueRef dumpDatabaseCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpDatabaseCallbacks(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpDOMAsWebArchiveCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpDOMAsWebArchive(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpEditingCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpEditingCallbacks(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpFrameLoadCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpFrameLoadCallbacks(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpProgressFinishedCallbackCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpProgressFinishedCallback(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpUserGestureInFrameLoadCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpUserGestureInFrameLoadCallbacks(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpResourceLoadCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpResourceLoadCallbacks(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpResourceResponseMIMETypesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpResourceResponseMIMETypes(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpSelectionRectCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpSelectionRect(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpSourceAsWebArchiveCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpSourceAsWebArchive(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpStatusCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpStatusCallbacks(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpTitleChangesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpTitleChanges(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpIconChangesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpIconChanges(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpWillCacheResponseCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpWillCacheResponse(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef pathToLocalResourceCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> localPath(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    JSRetainPtr<JSStringRef> convertedPath(Adopt, controller->pathToLocalResource(context, localPath.get()));
    if (!convertedPath)
        return JSValueMakeUndefined(context);

    return JSValueMakeString(context, convertedPath.get());
}

static JSValueRef removeAllVisitedLinksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpVisitedLinksCallback(true);
    controller->removeAllVisitedLinks();
    return JSValueMakeUndefined(context);
}

static JSValueRef repaintSweepHorizontallyCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setTestRepaintSweepHorizontally(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef setCallCloseOnWebViewsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setCallCloseOnWebViews(JSValueToBoolean(context, arguments[0]));
    return JSValueMakeUndefined(context);
}

static JSValueRef setCanOpenWindowsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setCanOpenWindows(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef setCloseRemainingWindowsWhenCompleteCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setCloseRemainingWindowsWhenComplete(JSValueToBoolean(context, arguments[0]));
    return JSValueMakeUndefined(context);
}

static JSValueRef setEncodedAudioDataCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> encodedAudioData(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    
    size_t maxLength = JSStringGetMaximumUTF8CStringSize(encodedAudioData.get());
    OwnArrayPtr<char> encodedAudioDataBuffer = adoptArrayPtr(new char[maxLength + 1]);
    JSStringGetUTF8CString(encodedAudioData.get(), encodedAudioDataBuffer.get(), maxLength + 1);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setEncodedAudioData(encodedAudioDataBuffer.get());
    controller->setDumpAsAudio(true);
    
    return JSValueMakeUndefined(context);
}

static JSValueRef testOnscreenCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setTestOnscreen(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef testRepaintCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setTestRepaint(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef addDisallowedURLCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> url(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->addDisallowedURL(url.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef addURLToRedirectCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> origin(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    JSRetainPtr<JSStringRef> destination(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);

    size_t maxLength = JSStringGetMaximumUTF8CStringSize(origin.get());
    OwnArrayPtr<char> originBuffer = adoptArrayPtr(new char[maxLength + 1]);
    JSStringGetUTF8CString(origin.get(), originBuffer.get(), maxLength + 1);

    maxLength = JSStringGetMaximumUTF8CStringSize(destination.get());
    OwnArrayPtr<char> destinationBuffer = adoptArrayPtr(new char[maxLength + 1]);
    JSStringGetUTF8CString(destination.get(), destinationBuffer.get(), maxLength + 1);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->addURLToRedirect(originBuffer.get(), destinationBuffer.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef callShouldCloseOnWebViewCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    return JSValueMakeBoolean(context, controller->callShouldCloseOnWebView());
}

static JSValueRef clearAllApplicationCachesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->clearAllApplicationCaches();

    return JSValueMakeUndefined(context);
}

static JSValueRef clearApplicationCacheForOriginCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> originURL(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->clearApplicationCacheForOrigin(originURL.get());
    
    return JSValueMakeUndefined(context);
}

static JSValueRef applicationCacheDiskUsageForOriginCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> originURL(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    return JSValueMakeNumber(context, controller->applicationCacheDiskUsageForOrigin(originURL.get()));
}

static JSValueRef originsWithApplicationCacheCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return controller->originsWithApplicationCache(context);
}

static JSValueRef clearAllDatabasesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->clearAllDatabases();

    return JSValueMakeUndefined(context);
}

static JSValueRef syncLocalStorageCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    controller->syncLocalStorage();

    return JSValueMakeUndefined(context);
}

static JSValueRef observeStorageTrackerNotificationsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    unsigned numNotifications = JSValueToNumber(context, arguments[0], exception);

    ASSERT(!*exception);

    controller->observeStorageTrackerNotifications(numNotifications);

    return JSValueMakeUndefined(context);
}

static JSValueRef deleteAllLocalStorageCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->deleteAllLocalStorage();

    return JSValueMakeUndefined(context);
}

static JSValueRef deleteLocalStorageForOriginCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> url(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    controller->deleteLocalStorageForOrigin(url.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef localStorageDiskUsageForOriginCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> originURL(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    return JSValueMakeNumber(context, controller->localStorageDiskUsageForOrigin(originURL.get()));
}

static JSValueRef originsWithLocalStorageCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return controller->originsWithLocalStorage(context);
}

static JSValueRef clearBackForwardListCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->clearBackForwardList();

    return JSValueMakeUndefined(context);
}

static JSValueRef clearPersistentUserStyleSheetCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->clearPersistentUserStyleSheet();

    return JSValueMakeUndefined(context);
}

static JSValueRef decodeHostNameCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> name(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> decodedHostName(Adopt, controller->copyDecodedHostName(name.get()));
    return JSValueMakeString(context, decodedHostName.get());
}

static JSValueRef disableImageLoadingCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation, needs windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->disableImageLoading();
    
    return JSValueMakeUndefined(context);
}

static JSValueRef dispatchPendingLoadRequestsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation, needs windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->dispatchPendingLoadRequests();
    
    return JSValueMakeUndefined(context);
}

static JSValueRef displayCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->display();

    return JSValueMakeUndefined(context);
}

static JSValueRef displayInvalidatedRegionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    // LayoutTestController::display() only renders the invalidated region so
    // we can just use that.
    controller->display();

    return JSValueMakeUndefined(context);
}

static JSValueRef encodeHostNameCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> name(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> encodedHostName(Adopt, controller->copyEncodedHostName(name.get()));
    return JSValueMakeString(context, encodedHostName.get());
}

static JSValueRef execCommandCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Mac & Windows implementations.
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> name(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    // Ignoring the second parameter (userInterface), as this command emulates a manual action.

    JSRetainPtr<JSStringRef> value;
    if (argumentCount >= 3) {
        value.adopt(JSValueToStringCopy(context, arguments[2], exception));
        ASSERT(!*exception);
    } else
        value.adopt(JSStringCreateWithUTF8CString(""));


    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->execCommand(name.get(), value.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef findStringCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Mac implementation.
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> target(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    JSObjectRef options = JSValueToObject(context, arguments[1], exception);
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeBoolean(context, controller->findString(context, target.get(), options));
}

static JSValueRef counterValueForElementByIdCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> elementId(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    if (*exception)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> counterValue(controller->counterValueForElementById(elementId.get()));
    if (!counterValue.get())
        return JSValueMakeUndefined(context);
    return JSValueMakeString(context, counterValue.get());
}

static JSValueRef goBackCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->goBack();
    
    return JSValueMakeUndefined(context);
}

static JSValueRef grantDesktopNotificationPermissionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    controller->grantDesktopNotificationPermission(JSValueToStringCopy(context, arguments[0], NULL));
        
    return JSValueMakeUndefined(context);
}

static JSValueRef isCommandEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Mac implementation.

    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> name(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    return JSValueMakeBoolean(context, controller->isCommandEnabled(name.get()));
}

static JSValueRef overridePreferenceCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> key(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> value(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->overridePreference(key.get(), value.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef keepWebHistoryCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->keepWebHistory();

    return JSValueMakeUndefined(context);
}

static JSValueRef computedStyleIncludingVisitedInfoCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 1)
        return JSValueMakeUndefined(context);
    
    // Has mac implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return controller->computedStyleIncludingVisitedInfo(context, arguments[0]);
}

static JSValueRef nodesFromRectCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 8)
        return JSValueMakeUndefined(context);

    int x = JSValueToNumber(context, arguments[1], NULL);
    int y = JSValueToNumber(context, arguments[2], NULL);
    int top = static_cast<unsigned>(JSValueToNumber(context, arguments[3], NULL));
    int right = static_cast<unsigned>(JSValueToNumber(context, arguments[4], NULL));
    int bottom = static_cast<unsigned>(JSValueToNumber(context, arguments[5], NULL));
    int left = static_cast<unsigned>(JSValueToNumber(context, arguments[6], NULL));
    bool ignoreClipping = JSValueToBoolean(context, arguments[7]);

    // Has mac implementation.
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return controller->nodesFromRect(context, arguments[0], x, y, top, right, bottom, left, ignoreClipping);
}

static JSValueRef layerTreeAsTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeString(context, controller->layerTreeAsText().get());
}

static JSValueRef notifyDoneCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    // May be able to be made platform independant by using shared WorkQueue
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->notifyDone();
    return JSValueMakeUndefined(context);
}

static bool parsePageParameters(JSContextRef context, int argumentCount, const JSValueRef* arguments, JSValueRef* exception, float& pageWidthInPixels, float& pageHeightInPixels)
{
    pageWidthInPixels = LayoutTestController::maxViewWidth;
    pageHeightInPixels = LayoutTestController::maxViewHeight;
    switch (argumentCount) {
    case 2:
        pageWidthInPixels = static_cast<float>(JSValueToNumber(context, arguments[0], exception));
        if (*exception)
            return false;
        pageHeightInPixels = static_cast<float>(JSValueToNumber(context, arguments[1], exception));
        if (*exception)
            return false;
    case 0: // Fall through.
        break;
    default:
        return false;
    }
    return true;
}

// Caller needs to delete[] propertyName.
static bool parsePagePropertyParameters(JSContextRef context, int argumentCount, const JSValueRef* arguments, JSValueRef* exception, char*& propertyName, int& pageNumber)
{
    pageNumber = 0;
    switch (argumentCount) {
    case 2:
        pageNumber = static_cast<float>(JSValueToNumber(context, arguments[1], exception));
        if (*exception)
            return false;
        // Fall through.
    case 1: {
        JSRetainPtr<JSStringRef> propertyNameString(Adopt, JSValueToStringCopy(context, arguments[0], exception));
        if (*exception)
            return false;

        size_t maxLength = JSStringGetMaximumUTF8CStringSize(propertyNameString.get());
        propertyName = new char[maxLength + 1];
        JSStringGetUTF8CString(propertyNameString.get(), propertyName, maxLength + 1);
        return true;
    }
    case 0:
    default:
        return false;
    }
}

static bool parsePageNumber(JSContextRef context, int argumentCount, const JSValueRef* arguments, JSValueRef* exception, int& pageNumber)
{
    pageNumber = 0;
    switch (argumentCount) {
    case 1:
        pageNumber = static_cast<int>(JSValueToNumber(context, arguments[0], exception));
        if (*exception)
            return false;
        // Fall through.
    case 0:
        return true;
    default:
        return false;
    }
}

static bool parsePageNumberSizeMarings(JSContextRef context, int argumentCount, const JSValueRef* arguments, JSValueRef* exception, int& pageNumber, int& width, int& height, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft)
{
    pageNumber = 0;
    width = height = 0;
    marginTop = marginRight = marginBottom = marginLeft = 0;

    switch (argumentCount) {
    case 7:
        marginLeft = static_cast<int>(JSValueToNumber(context, arguments[6], exception));
        if (*exception)
            return false;
        // Fall through.
    case 6:
        marginBottom = static_cast<int>(JSValueToNumber(context, arguments[5], exception));
        if (*exception)
            return false;
        // Fall through.
    case 5:
        marginRight = static_cast<int>(JSValueToNumber(context, arguments[4], exception));
        if (*exception)
            return false;
        // Fall through.
    case 4:
        marginTop = static_cast<int>(JSValueToNumber(context, arguments[3], exception));
        if (*exception)
            return false;
        // Fall through.
    case 3:
        height = static_cast<int>(JSValueToNumber(context, arguments[2], exception));
        if (*exception)
            return false;
        // Fall through.
    case 2:
        width = static_cast<int>(JSValueToNumber(context, arguments[1], exception));
        if (*exception)
            return false;
        // Fall through.
    case 1:
        pageNumber = static_cast<int>(JSValueToNumber(context, arguments[0], exception));
        if (*exception)
            return false;
        // Fall through.
        return true;
    default:
        return false;
    }
}

static JSValueRef pageNumberForElementByIdCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    float pageWidthInPixels = 0;
    float pageHeightInPixels = 0;
    if (!parsePageParameters(context, argumentCount - 1, arguments + 1, exception, pageWidthInPixels, pageHeightInPixels))
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> elementId(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    if (*exception)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    int pageNumber = controller->pageNumberForElementById(elementId.get(), pageWidthInPixels, pageHeightInPixels);
    return JSValueMakeNumber(context, pageNumber);
}

static JSValueRef numberOfPagesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    float pageWidthInPixels = 0;
    float pageHeightInPixels = 0;
    if (!parsePageParameters(context, argumentCount, arguments, exception, pageWidthInPixels, pageHeightInPixels))
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, controller->numberOfPages(pageWidthInPixels, pageHeightInPixels));
}

static JSValueRef numberOfPendingGeolocationPermissionRequestsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, controller->numberOfPendingGeolocationPermissionRequests());
}

static JSValueRef pagePropertyCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    char* propertyName = 0;
    int pageNumber = 0;
    if (!parsePagePropertyParameters(context, argumentCount, arguments, exception, propertyName, pageNumber))
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    JSValueRef value = JSValueMakeString(context, controller->pageProperty(propertyName, pageNumber).get());

    delete[] propertyName;
    return value;
}

static JSValueRef isPageBoxVisibleCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int pageNumber = 0;
    if (!parsePageNumber(context, argumentCount, arguments, exception, pageNumber))
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeBoolean(context, controller->isPageBoxVisible(pageNumber));
}

static JSValueRef pageSizeAndMarginsInPixelsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    int pageNumber = 0;
    int width = 0, height = 0;
    int marginTop = 0, marginRight = 0, marginBottom = 0, marginLeft = 0;
    if (!parsePageNumberSizeMarings(context, argumentCount, arguments, exception, pageNumber, width, height, marginTop, marginRight, marginBottom, marginLeft))
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeString(context, controller->pageSizeAndMarginsInPixels(pageNumber, width, height, marginTop, marginRight, marginBottom, marginLeft).get());
}

static JSValueRef queueBackNavigationCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    // May be able to be made platform independant by using shared WorkQueue
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    double howFarBackDouble = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->queueBackNavigation(static_cast<int>(howFarBackDouble));

    return JSValueMakeUndefined(context);
}

static JSValueRef queueForwardNavigationCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    // May be able to be made platform independant by using shared WorkQueue
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    double howFarForwardDouble = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->queueForwardNavigation(static_cast<int>(howFarForwardDouble));

    return JSValueMakeUndefined(context);
}

static JSValueRef queueLoadCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    // May be able to be made platform independant by using shared WorkQueue
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> url(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    JSRetainPtr<JSStringRef> target;
    if (argumentCount >= 2) {
        target.adopt(JSValueToStringCopy(context, arguments[1], exception));
        ASSERT(!*exception);
    } else
        target.adopt(JSStringCreateWithUTF8CString(""));

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->queueLoad(url.get(), target.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef queueLoadHTMLStringCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Mac & Windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> content(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    JSRetainPtr<JSStringRef> baseURL;
    if (argumentCount >= 2) {
        baseURL.adopt(JSValueToStringCopy(context, arguments[1], exception));
        ASSERT(!*exception);
    } else
        baseURL.adopt(JSStringCreateWithUTF8CString(""));

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    if (argumentCount >= 3) {
        JSRetainPtr<JSStringRef> unreachableURL;
        unreachableURL.adopt(JSValueToStringCopy(context, arguments[2], exception));
        ASSERT(!*exception);
        controller->queueLoadAlternateHTMLString(content.get(), baseURL.get(), unreachableURL.get());
        return JSValueMakeUndefined(context);
    }

    controller->queueLoadHTMLString(content.get(), baseURL.get());
    return JSValueMakeUndefined(context);
}

static JSValueRef queueReloadCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    // May be able to be made platform independant by using shared WorkQueue

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->queueReload();

    return JSValueMakeUndefined(context);
}

static JSValueRef queueLoadingScriptCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    // May be able to be made platform independant by using shared WorkQueue
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> script(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->queueLoadingScript(script.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef queueNonLoadingScriptCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    // May be able to be made platform independant by using shared WorkQueue
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> script(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->queueNonLoadingScript(script.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef setAcceptsEditingCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setAcceptsEditing(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setAlwaysAcceptCookiesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setAlwaysAcceptCookies(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setAppCacheMaximumSizeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    double size = JSValueToNumber(context, arguments[0], NULL);
    if (!isnan(size))
        controller->setAppCacheMaximumSize(static_cast<unsigned long long>(size));
        
    return JSValueMakeUndefined(context);
}

static JSValueRef setApplicationCacheOriginQuotaCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    double size = JSValueToNumber(context, arguments[0], NULL);
    if (!isnan(size))
        controller->setApplicationCacheOriginQuota(static_cast<unsigned long long>(size));

    return JSValueMakeUndefined(context);
}

static JSValueRef setAuthenticationPasswordCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> password(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    size_t maxLength = JSStringGetMaximumUTF8CStringSize(password.get());
    char* passwordBuffer = new char[maxLength + 1];
    JSStringGetUTF8CString(password.get(), passwordBuffer, maxLength + 1);
    
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setAuthenticationPassword(passwordBuffer);
    delete[] passwordBuffer;

    return JSValueMakeUndefined(context);
}

static JSValueRef setAuthenticationUsernameCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> username(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    size_t maxLength = JSStringGetMaximumUTF8CStringSize(username.get());
    char* usernameBuffer = new char[maxLength + 1];
    JSStringGetUTF8CString(username.get(), usernameBuffer, maxLength + 1);
    
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setAuthenticationUsername(usernameBuffer);
    delete[] usernameBuffer;

    return JSValueMakeUndefined(context);
}

static JSValueRef setAuthorAndUserStylesEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setAuthorAndUserStylesEnabled(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setAutofilledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 2 || !arguments[0])
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setAutofilled(context, arguments[0], JSValueToBoolean(context, arguments[1]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setCacheModelCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Mac implementation.
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    int cacheModel = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setCacheModel(cacheModel);

    return JSValueMakeUndefined(context);
}

static JSValueRef setCustomPolicyDelegateCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);
    
    bool permissive = false;
    if (argumentCount >= 2)
        permissive = JSValueToBoolean(context, arguments[1]);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setCustomPolicyDelegate(JSValueToBoolean(context, arguments[0]), permissive);

    return JSValueMakeUndefined(context);
}

static JSValueRef setDatabaseQuotaCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    double quota = JSValueToNumber(context, arguments[0], NULL);
    if (!isnan(quota))
        controller->setDatabaseQuota(static_cast<unsigned long long>(quota));
        
    return JSValueMakeUndefined(context);
}

static JSValueRef setDeferMainResourceDataLoadCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Mac and Windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDeferMainResourceDataLoad(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setDefersLoadingCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDefersLoading(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setDomainRelaxationForbiddenForURLSchemeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Mac and Windows implementation
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    bool forbidden = JSValueToBoolean(context, arguments[0]);
    JSRetainPtr<JSStringRef> scheme(Adopt, JSValueToStringCopy(context, arguments[1], 0));
    controller->setDomainRelaxationForbiddenForURLScheme(forbidden, scheme.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef setMockDeviceOrientationCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 6)
        return JSValueMakeUndefined(context);

    bool canProvideAlpha = JSValueToBoolean(context, arguments[0]);
    double alpha = JSValueToNumber(context, arguments[1], exception);
    ASSERT(!*exception);
    bool canProvideBeta = JSValueToBoolean(context, arguments[2]);
    double beta = JSValueToNumber(context, arguments[3], exception);
    ASSERT(!*exception);
    bool canProvideGamma = JSValueToBoolean(context, arguments[4]);
    double gamma = JSValueToNumber(context, arguments[5], exception);
    ASSERT(!*exception);

    LayoutTestController* controller = reinterpret_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setMockDeviceOrientation(canProvideAlpha, alpha, canProvideBeta, beta, canProvideGamma, gamma);

    return JSValueMakeUndefined(context);
}

static JSValueRef setMockGeolocationPositionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 3)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = reinterpret_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setMockGeolocationPosition(JSValueToNumber(context, arguments[0], NULL),  // latitude
                                           JSValueToNumber(context, arguments[1], NULL),  // longitude
                                           JSValueToNumber(context, arguments[2], NULL));  // accuracy

    return JSValueMakeUndefined(context);
}

static JSValueRef setMockGeolocationErrorCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 2)
        return JSValueMakeUndefined(context);

    int code = JSValueToNumber(context, arguments[0], NULL);
    JSRetainPtr<JSStringRef> message(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = reinterpret_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setMockGeolocationError(code, message.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef addMockSpeechInputResultCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 3)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> result(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    double confidence = JSValueToNumber(context, arguments[1], exception);

    JSRetainPtr<JSStringRef> language(Adopt, JSValueToStringCopy(context, arguments[2], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->addMockSpeechInputResult(result.get(), confidence, language.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef setNewWindowsCopyBackForwardListCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);
    
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setNewWindowsCopyBackForwardList(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setGeolocationPermissionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setGeolocationPermission(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setHandlesAuthenticationChallengesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setHandlesAuthenticationChallenges(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setPOSIXLocaleCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> locale(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    controller->setPOSIXLocale(locale.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef setIconDatabaseEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setIconDatabaseEnabled(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setJavaScriptProfilingEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setJavaScriptProfilingEnabled(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setMainFrameIsFirstResponderCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setMainFrameIsFirstResponder(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setPersistentUserStyleSheetLocationCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> path(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setPersistentUserStyleSheetLocation(path.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef setPrivateBrowsingEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setPrivateBrowsingEnabled(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setJavaScriptCanAccessClipboardCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setJavaScriptCanAccessClipboard(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setXSSAuditorEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setXSSAuditorEnabled(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setSpatialNavigationEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation.
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setSpatialNavigationEnabled(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setPrintingCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setIsPrinting(true);
    return JSValueMakeUndefined(context);
}


static JSValueRef setFrameFlatteningEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setFrameFlatteningEnabled(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setAllowUniversalAccessFromFileURLsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setAllowUniversalAccessFromFileURLs(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setAllowFileAccessFromFileURLsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setAllowFileAccessFromFileURLs(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setTabKeyCyclesThroughElementsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setTabKeyCyclesThroughElements(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setUseDashboardCompatibilityModeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setUseDashboardCompatibilityMode(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setUserStyleSheetEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setUserStyleSheetEnabled(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setUserStyleSheetLocationCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> path(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setUserStyleSheetLocation(path.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef setValueForUserCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount != 2)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> value(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setValueForUser(context, arguments[0], value.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef setViewModeMediaFeatureCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> mode(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setViewModeMediaFeature(mode.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef setWillSendRequestClearHeaderCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> header(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    size_t maxLength = JSStringGetMaximumUTF8CStringSize(header.get());
    OwnArrayPtr<char> headerBuffer = adoptArrayPtr(new char[maxLength + 1]);
    JSStringGetUTF8CString(header.get(), headerBuffer.get(), maxLength + 1);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setWillSendRequestClearHeader(headerBuffer.get());

    return JSValueMakeUndefined(context);
}

static JSValueRef setWillSendRequestReturnsNullCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has cross-platform implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setWillSendRequestReturnsNull(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setWillSendRequestReturnsNullOnRedirectCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has cross-platform implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setWillSendRequestReturnsNullOnRedirect(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setWindowIsKeyCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setWindowIsKey(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef waitUntilDoneCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setWaitToDump(true);

    return JSValueMakeUndefined(context);
}

static JSValueRef windowCountCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    int windows = controller->windowCount();
    return JSValueMakeNumber(context, windows);
}

static JSValueRef setPopupBlockingEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setPopupBlockingEnabled(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef setPluginsEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);
    
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setPluginsEnabled(JSValueToBoolean(context, arguments[0]));
    
    return JSValueMakeUndefined(context);
}    

static JSValueRef setPageVisibilityCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> visibility(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    size_t maxLength = JSStringGetMaximumUTF8CStringSize(visibility.get());
    char* visibilityBuffer = new char[maxLength + 1];
    JSStringGetUTF8CString(visibility.get(), visibilityBuffer, maxLength + 1);
    
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setPageVisibility(visibilityBuffer);
    delete[] visibilityBuffer;
    
    return JSValueMakeUndefined(context);
}    

static JSValueRef resetPageVisibilityCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->resetPageVisibility();
    return JSValueMakeUndefined(context);
}    

static JSValueRef setSmartInsertDeleteEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setSmartInsertDeleteEnabled(JSValueToBoolean(context, arguments[0]));
    return JSValueMakeUndefined(context);
}

static JSValueRef setSelectTrailingWhitespaceEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setSelectTrailingWhitespaceEnabled(JSValueToBoolean(context, arguments[0]));
    return JSValueMakeUndefined(context);
}

static JSValueRef setStopProvisionalFrameLoadsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setStopProvisionalFrameLoads(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef setAsynchronousSpellCheckingEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setAsynchronousSpellCheckingEnabled(JSValueToBoolean(context, arguments[0]));
    return JSValueMakeUndefined(context);
}

static JSValueRef showWebInspectorCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->showWebInspector();
    return JSValueMakeUndefined(context);
}

static JSValueRef closeWebInspectorCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->closeWebInspector();
    return JSValueMakeUndefined(context);
}

static JSValueRef evaluateInWebInspectorCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    double callId = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> script(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);

    controller->evaluateInWebInspector(static_cast<long>(callId), script.get());
    return JSValueMakeUndefined(context);
}

static JSValueRef evaluateScriptInIsolatedWorldCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    double worldID = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> script(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);

    controller->evaluateScriptInIsolatedWorld(static_cast<unsigned>(worldID), JSContextGetGlobalObject(context), script.get());
    return JSValueMakeUndefined(context);
}

static JSValueRef elementDoesAutoCompleteForElementWithIdCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> elementId(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    bool autoCompletes = controller->elementDoesAutoCompleteForElementWithId(elementId.get());

    return JSValueMakeBoolean(context, autoCompletes);
}

static JSValueRef pauseAnimationAtTimeOnElementWithIdCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 3)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> animationName(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    double time = JSValueToNumber(context, arguments[1], exception);
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> elementId(Adopt, JSValueToStringCopy(context, arguments[2], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeBoolean(context, controller->pauseAnimationAtTimeOnElementWithId(animationName.get(), time, elementId.get()));
}

static JSValueRef pauseTransitionAtTimeOnElementWithIdCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 3)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> propertyName(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    double time = JSValueToNumber(context, arguments[1], exception);
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> elementId(Adopt, JSValueToStringCopy(context, arguments[2], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeBoolean(context, controller->pauseTransitionAtTimeOnElementWithId(propertyName.get(), time, elementId.get()));
}

static JSValueRef sampleSVGAnimationForElementAtTimeCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 3)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> animationId(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    double time = JSValueToNumber(context, arguments[1], exception);
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> elementId(Adopt, JSValueToStringCopy(context, arguments[2], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeBoolean(context, controller->sampleSVGAnimationForElementAtTime(animationId.get(), time, elementId.get()));
}

static JSValueRef numberOfActiveAnimationsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 0)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, controller->numberOfActiveAnimations());
}

static JSValueRef suspendAnimationsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->suspendAnimations();
    return JSValueMakeUndefined(context);
}

static JSValueRef resumeAnimationsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->resumeAnimations();
    return JSValueMakeUndefined(context);
}

static JSValueRef waitForPolicyDelegateCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->waitForPolicyDelegate();
    return JSValueMakeUndefined(context);
}

static JSValueRef addOriginAccessWhitelistEntryCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 4)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> sourceOrigin(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> destinationProtocol(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> destinationHost(Adopt, JSValueToStringCopy(context, arguments[2], exception));
    ASSERT(!*exception);
    bool allowDestinationSubdomains = JSValueToBoolean(context, arguments[3]);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->addOriginAccessWhitelistEntry(sourceOrigin.get(), destinationProtocol.get(), destinationHost.get(), allowDestinationSubdomains);
    return JSValueMakeUndefined(context);
}

static JSValueRef removeOriginAccessWhitelistEntryCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 4)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> sourceOrigin(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> destinationProtocol(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> destinationHost(Adopt, JSValueToStringCopy(context, arguments[2], exception));
    ASSERT(!*exception);
    bool allowDestinationSubdomains = JSValueToBoolean(context, arguments[3]);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->removeOriginAccessWhitelistEntry(sourceOrigin.get(), destinationProtocol.get(), destinationHost.get(), allowDestinationSubdomains);
    return JSValueMakeUndefined(context);
}

static JSValueRef setScrollbarPolicyCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 2)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> orientation(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> policy(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setScrollbarPolicy(orientation.get(), policy.get());
    return JSValueMakeUndefined(context);
}

static JSValueRef addUserScriptCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 3)
        return JSValueMakeUndefined(context);
    
    JSRetainPtr<JSStringRef> source(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    bool runAtStart = JSValueToBoolean(context, arguments[1]);
    bool allFrames = JSValueToBoolean(context, arguments[2]);
    
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->addUserScript(source.get(), runAtStart, allFrames);
    return JSValueMakeUndefined(context);
}
 
static JSValueRef addUserStyleSheetCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 2)
        return JSValueMakeUndefined(context);
    
    JSRetainPtr<JSStringRef> source(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    bool allFrames = JSValueToBoolean(context, arguments[1]);
   
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->addUserStyleSheet(source.get(), allFrames);
    return JSValueMakeUndefined(context);
}

static JSValueRef setShouldPaintBrokenImageCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setShouldPaintBrokenImage(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef apiTestNewWindowDataLoadBaseURLCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 2)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> utf8Data(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    JSRetainPtr<JSStringRef> baseURL(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);
        
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->apiTestNewWindowDataLoadBaseURL(utf8Data.get(), baseURL.get());
    return JSValueMakeUndefined(context);
}

static JSValueRef apiTestGoToCurrentBackForwardItemCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->apiTestGoToCurrentBackForwardItem();
    return JSValueMakeUndefined(context);
}

static JSValueRef setWebViewEditableCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has Mac implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setWebViewEditable(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}


static JSValueRef abortModalCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->abortModal();
    return JSValueMakeUndefined(context);
}

static JSValueRef hasSpellingMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 2)
        return JSValueMakeUndefined(context);

    int from = JSValueToNumber(context, arguments[0], 0);
    int length = JSValueToNumber(context, arguments[1], 0);
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    bool ok = controller->hasSpellingMarker(from, length);

    return JSValueMakeBoolean(context, ok);
}

static JSValueRef hasGrammarMarkerCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 2)
        return JSValueMakeUndefined(context);
    
    int from = JSValueToNumber(context, arguments[0], 0);
    int length = JSValueToNumber(context, arguments[1], 0);
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    bool ok = controller->hasGrammarMarker(from, length);
    
    return JSValueMakeBoolean(context, ok);
}

static JSValueRef markerTextForListItemCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);
    return JSValueMakeString(context, controller->markerTextForListItem(context, arguments[0]).get());
}

static JSValueRef authenticateSessionCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // authenticateSession(url, username, password)
    if (argumentCount != 3)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> url(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> username(Adopt, JSValueToStringCopy(context, arguments[1], exception));
    ASSERT(!*exception);
    JSRetainPtr<JSStringRef> password(Adopt, JSValueToStringCopy(context, arguments[2], exception));
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->authenticateSession(url.get(), username.get(), password.get());
    return JSValueMakeUndefined(context);
}

static JSValueRef setEditingBehaviorCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // The editing behavior string.
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    JSRetainPtr<JSStringRef> editingBehavior(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);

    size_t maxLength = JSStringGetMaximumUTF8CStringSize(editingBehavior.get());
    char* behaviorBuffer = new char[maxLength + 1];
    JSStringGetUTF8CString(editingBehavior.get(), behaviorBuffer, maxLength);

    if (strcmp(behaviorBuffer, "mac") && strcmp(behaviorBuffer, "win") && strcmp(behaviorBuffer, "unix")) {
        JSRetainPtr<JSStringRef> invalidArgument(JSStringCreateWithUTF8CString("Passed invalid editing behavior. Must be 'mac', 'win', or 'unix'."));
        *exception = JSValueMakeString(context, invalidArgument.get());
        return JSValueMakeUndefined(context);
    }

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setEditingBehavior(behaviorBuffer);

    delete [] behaviorBuffer;

    return JSValueMakeUndefined(context);
}

static JSValueRef setSerializeHTTPLoadsCallback(JSContextRef context, JSObjectRef, JSObjectRef, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    bool serialize = true;
    if (argumentCount == 1)
        serialize = JSValueToBoolean(context, arguments[0]);

    LayoutTestController::setSerializeHTTPLoads(serialize);
    return JSValueMakeUndefined(context);
}

static JSValueRef allowRoundingHacksCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));

    controller->allowRoundingHacks();
    return JSValueMakeUndefined(context);
}

static JSValueRef setShouldStayOnPageAfterHandlingBeforeUnloadCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    
    if (argumentCount == 1)
        controller->setShouldStayOnPageAfterHandlingBeforeUnload(JSValueToBoolean(context, arguments[0]));

    return JSValueMakeUndefined(context);
}

static JSValueRef addChromeInputFieldCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->addChromeInputField();
    // the first argument is a callback that is called once the input field has been added
    if (argumentCount == 1)
        JSObjectCallAsFunction(context, JSValueToObject(context, arguments[0], 0), thisObject, 0, 0, 0);
    return JSValueMakeUndefined(context);
}

static JSValueRef removeChromeInputFieldCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->removeChromeInputField();
    // the first argument is a callback that is called once the input field has been added
    if (argumentCount == 1)
        JSObjectCallAsFunction(context, JSValueToObject(context, arguments[0], 0), thisObject, 0, 0, 0);
    return JSValueMakeUndefined(context);
}

static JSValueRef focusWebViewCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->focusWebView();
    // the first argument is a callback that is called once the input field has been added
    if (argumentCount == 1)
        JSObjectCallAsFunction(context, JSValueToObject(context, arguments[0], 0), thisObject, 0, 0, 0);
    return JSValueMakeUndefined(context);
}

static JSValueRef setBackingScaleFactorCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 2)
        return JSValueMakeUndefined(context);

    double backingScaleFactor = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setBackingScaleFactor(backingScaleFactor);

    // The second argument is a callback that is called once the backing scale factor has been set.
    JSObjectCallAsFunction(context, JSValueToObject(context, arguments[1], 0), thisObject, 0, 0, 0);
    return JSValueMakeUndefined(context);
}

// Static Values

static JSValueRef getGlobalFlagCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeBoolean(context, controller->globalFlag());
}

static JSValueRef getWebHistoryItemCountCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, controller->webHistoryItemCount());
}

static JSValueRef getWorkerThreadCountCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, controller->workerThreadCount());
}

#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WIN)
static JSValueRef getPlatformNameCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    JSRetainPtr<JSStringRef> platformName(controller->platformName());
    if (!platformName.get())
        return JSValueMakeUndefined(context);
    return JSValueMakeString(context, platformName.get());
}
#endif

static bool setGlobalFlagCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setGlobalFlag(JSValueToBoolean(context, value));
    return true;
}

static JSValueRef ignoreDesktopNotificationPermissionRequestsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->ignoreDesktopNotificationPermissionRequests();
    return JSValueMakeUndefined(context);
}

static JSValueRef simulateDesktopNotificationClickCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject)); 
    JSRetainPtr<JSStringRef> title(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    controller->simulateDesktopNotificationClick(title.get());
    return JSValueMakeUndefined(context);
}

static JSValueRef setMinimumTimerIntervalCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    double minimum = JSValueToNumber(context, arguments[0], exception);
    ASSERT(!*exception);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setMinimumTimerInterval(minimum);

    return JSValueMakeUndefined(context);
}

static JSValueRef setTextDirectionCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount == 1) {
        JSRetainPtr<JSStringRef> direction(Adopt, JSValueToStringCopy(context, arguments[0], exception));
        LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
        controller->setTextDirection(direction.get());
    }

    return JSValueMakeUndefined(context);
}

static void layoutTestControllerObjectFinalize(JSObjectRef object)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(object));
    controller->deref();
}

// Object Creation

void LayoutTestController::makeWindowObject(JSContextRef context, JSObjectRef windowObject, JSValueRef* exception)
{
    JSRetainPtr<JSStringRef> layoutTestContollerStr(Adopt, JSStringCreateWithUTF8CString("layoutTestController"));
    ref();

    JSClassRef classRef = getJSClass();
    JSValueRef layoutTestContollerObject = JSObjectMake(context, classRef, this);
    JSClassRelease(classRef);

    JSObjectSetProperty(context, windowObject, layoutTestContollerStr.get(), layoutTestContollerObject, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

JSClassRef LayoutTestController::getJSClass()
{
    static JSStaticValue* staticValues = LayoutTestController::staticValues();
    static JSStaticFunction* staticFunctions = LayoutTestController::staticFunctions();
    static JSClassDefinition classDefinition = {
        0, kJSClassAttributeNone, "LayoutTestController", 0, staticValues, staticFunctions,
        0, layoutTestControllerObjectFinalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    return JSClassCreate(&classDefinition);
}

JSStaticValue* LayoutTestController::staticValues()
{
    static JSStaticValue staticValues[] = {
        { "globalFlag", getGlobalFlagCallback, setGlobalFlagCallback, kJSPropertyAttributeNone },
        { "webHistoryItemCount", getWebHistoryItemCountCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "workerThreadCount", getWorkerThreadCountCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#if PLATFORM(MAC) || PLATFORM(GTK) || PLATFORM(WIN)
        { "platformName", getPlatformNameCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
#endif
        { 0, 0, 0, 0 }
    };
    return staticValues;
}

JSStaticFunction* LayoutTestController::staticFunctions()
{
    static JSStaticFunction staticFunctions[] = {
        { "abortModal", abortModalCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addDisallowedURL", addDisallowedURLCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addURLToRedirect", addURLToRedirectCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addUserScript", addUserScriptCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addUserStyleSheet", addUserStyleSheetCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "apiTestNewWindowDataLoadBaseURL", apiTestNewWindowDataLoadBaseURLCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "apiTestGoToCurrentBackForwardItem", apiTestGoToCurrentBackForwardItemCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "applicationCacheDiskUsageForOrigin", applicationCacheDiskUsageForOriginCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "callShouldCloseOnWebView", callShouldCloseOnWebViewCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clearAllApplicationCaches", clearAllApplicationCachesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clearAllDatabases", clearAllDatabasesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clearApplicationCacheForOrigin", clearApplicationCacheForOriginCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clearBackForwardList", clearBackForwardListCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clearPersistentUserStyleSheet", clearPersistentUserStyleSheetCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "closeWebInspector", closeWebInspectorCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "computedStyleIncludingVisitedInfo", computedStyleIncludingVisitedInfoCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "nodesFromRect", nodesFromRectCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "decodeHostName", decodeHostNameCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "disableImageLoading", disableImageLoadingCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "disallowIncreaseForApplicationCacheQuota", disallowIncreaseForApplicationCacheQuotaCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dispatchPendingLoadRequests", dispatchPendingLoadRequestsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "display", displayCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "displayInvalidatedRegion", displayInvalidatedRegionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpApplicationCacheDelegateCallbacks", dumpApplicationCacheDelegateCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpAsText", dumpAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpBackForwardList", dumpBackForwardListCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpChildFrameScrollPositions", dumpChildFrameScrollPositionsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpChildFramesAsText", dumpChildFramesAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpConfigurationForViewport", dumpConfigurationForViewportCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpDOMAsWebArchive", dumpDOMAsWebArchiveCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpDatabaseCallbacks", dumpDatabaseCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpEditingCallbacks", dumpEditingCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpFrameLoadCallbacks", dumpFrameLoadCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpProgressFinishedCallback", dumpProgressFinishedCallbackCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpUserGestureInFrameLoadCallbacks", dumpUserGestureInFrameLoadCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },        
        { "dumpResourceLoadCallbacks", dumpResourceLoadCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpResourceResponseMIMETypes", dumpResourceResponseMIMETypesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpSelectionRect", dumpSelectionRectCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpSourceAsWebArchive", dumpSourceAsWebArchiveCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpStatusCallbacks", dumpStatusCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpTitleChanges", dumpTitleChangesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpIconChanges", dumpIconChangesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpWillCacheResponse", dumpWillCacheResponseCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "elementDoesAutoCompleteForElementWithId", elementDoesAutoCompleteForElementWithIdCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "encodeHostName", encodeHostNameCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "evaluateInWebInspector", evaluateInWebInspectorCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "evaluateScriptInIsolatedWorld", evaluateScriptInIsolatedWorldCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "execCommand", execCommandCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "findString", findStringCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "counterValueForElementById", counterValueForElementByIdCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "originsWithApplicationCache", originsWithApplicationCacheCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "goBack", goBackCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete }, 
        { "grantDesktopNotificationPermission", grantDesktopNotificationPermissionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete }, 
        { "hasSpellingMarker", hasSpellingMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "hasGrammarMarker", hasGrammarMarkerCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "ignoreDesktopNotificationPermissionRequests", ignoreDesktopNotificationPermissionRequestsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isCommandEnabled", isCommandEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "isPageBoxVisible", isPageBoxVisibleCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "keepWebHistory", keepWebHistoryCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "layerTreeAsText", layerTreeAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "numberOfPages", numberOfPagesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "numberOfPendingGeolocationPermissionRequests", numberOfPendingGeolocationPermissionRequestsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "markerTextForListItem", markerTextForListItemCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "notifyDone", notifyDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "numberOfActiveAnimations", numberOfActiveAnimationsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "suspendAnimations", suspendAnimationsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "resumeAnimations", resumeAnimationsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "overridePreference", overridePreferenceCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pageNumberForElementById", pageNumberForElementByIdCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pageSizeAndMarginsInPixels", pageSizeAndMarginsInPixelsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pageProperty", pagePropertyCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pathToLocalResource", pathToLocalResourceCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pauseAnimationAtTimeOnElementWithId", pauseAnimationAtTimeOnElementWithIdCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pauseTransitionAtTimeOnElementWithId", pauseTransitionAtTimeOnElementWithIdCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "sampleSVGAnimationForElementAtTime", sampleSVGAnimationForElementAtTimeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "printToPDF", dumpAsPDFCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueBackNavigation", queueBackNavigationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueForwardNavigation", queueForwardNavigationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueLoad", queueLoadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueLoadHTMLString", queueLoadHTMLStringCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueLoadingScript", queueLoadingScriptCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueNonLoadingScript", queueNonLoadingScriptCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueReload", queueReloadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "removeAllVisitedLinks", removeAllVisitedLinksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "removeOriginAccessWhitelistEntry", removeOriginAccessWhitelistEntryCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "repaintSweepHorizontally", repaintSweepHorizontallyCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "resetPageVisibility", resetPageVisibilityCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAcceptsEditing", setAcceptsEditingCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAllowUniversalAccessFromFileURLs", setAllowUniversalAccessFromFileURLsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAllowFileAccessFromFileURLs", setAllowFileAccessFromFileURLsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAlwaysAcceptCookies", setAlwaysAcceptCookiesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAppCacheMaximumSize", setAppCacheMaximumSizeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setApplicationCacheOriginQuota", setApplicationCacheOriginQuotaCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setEncodedAudioData", setEncodedAudioDataCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAuthenticationPassword", setAuthenticationPasswordCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAuthenticationUsername", setAuthenticationUsernameCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAuthorAndUserStylesEnabled", setAuthorAndUserStylesEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAutofilled", setAutofilledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCacheModel", setCacheModelCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCallCloseOnWebViews", setCallCloseOnWebViewsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCanOpenWindows", setCanOpenWindowsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCloseRemainingWindowsWhenComplete", setCloseRemainingWindowsWhenCompleteCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCustomPolicyDelegate", setCustomPolicyDelegateCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setDatabaseQuota", setDatabaseQuotaCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete }, 
        { "setDeferMainResourceDataLoad", setDeferMainResourceDataLoadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setDefersLoading", setDefersLoadingCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setDomainRelaxationForbiddenForURLScheme", setDomainRelaxationForbiddenForURLSchemeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setEditingBehavior", setEditingBehaviorCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setFrameFlatteningEnabled", setFrameFlatteningEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setGeolocationPermission", setGeolocationPermissionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setHandlesAuthenticationChallenges", setHandlesAuthenticationChallengesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setIconDatabaseEnabled", setIconDatabaseEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setJavaScriptProfilingEnabled", setJavaScriptProfilingEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setMainFrameIsFirstResponder", setMainFrameIsFirstResponderCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setMinimumTimerInterval", setMinimumTimerIntervalCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setMockDeviceOrientation", setMockDeviceOrientationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setMockGeolocationError", setMockGeolocationErrorCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setMockGeolocationPosition", setMockGeolocationPositionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addMockSpeechInputResult", addMockSpeechInputResultCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setNewWindowsCopyBackForwardList", setNewWindowsCopyBackForwardListCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPageVisibility", setPageVisibilityCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPOSIXLocale", setPOSIXLocaleCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPersistentUserStyleSheetLocation", setPersistentUserStyleSheetLocationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPopupBlockingEnabled", setPopupBlockingEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPluginsEnabled", setPluginsEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPrinting", setPrintingCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPrivateBrowsingEnabled", setPrivateBrowsingEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSelectTrailingWhitespaceEnabled", setSelectTrailingWhitespaceEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSerializeHTTPLoads", setSerializeHTTPLoadsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSmartInsertDeleteEnabled", setSmartInsertDeleteEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSpatialNavigationEnabled", setSpatialNavigationEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setStopProvisionalFrameLoads", setStopProvisionalFrameLoadsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setTabKeyCyclesThroughElements", setTabKeyCyclesThroughElementsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setUseDashboardCompatibilityMode", setUseDashboardCompatibilityModeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setUserStyleSheetEnabled", setUserStyleSheetEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setUserStyleSheetLocation", setUserStyleSheetLocationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setValueForUser", setValueForUserCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setViewModeMediaFeature", setViewModeMediaFeatureCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setWebViewEditable", setWebViewEditableCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setWillSendRequestClearHeader", setWillSendRequestClearHeaderCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setWillSendRequestReturnsNull", setWillSendRequestReturnsNullCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setWillSendRequestReturnsNullOnRedirect", setWillSendRequestReturnsNullOnRedirectCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setWindowIsKey", setWindowIsKeyCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setJavaScriptCanAccessClipboard", setJavaScriptCanAccessClipboardCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setXSSAuditorEnabled", setXSSAuditorEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAsynchronousSpellCheckingEnabled", setAsynchronousSpellCheckingEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "showWebInspector", showWebInspectorCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "simulateDesktopNotificationClick", simulateDesktopNotificationClickCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "testOnscreen", testOnscreenCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "testRepaint", testRepaintCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "waitForPolicyDelegate", waitForPolicyDelegateCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "waitUntilDone", waitUntilDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "windowCount", windowCountCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addOriginAccessWhitelistEntry", addOriginAccessWhitelistEntryCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setScrollbarPolicy", setScrollbarPolicyCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "authenticateSession", authenticateSessionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "deleteAllLocalStorage", deleteAllLocalStorageCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "syncLocalStorage", syncLocalStorageCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },                
        { "observeStorageTrackerNotifications", observeStorageTrackerNotificationsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },        
        { "deleteLocalStorageForOrigin", deleteLocalStorageForOriginCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "localStorageDiskUsageForOrigin", localStorageDiskUsageForOriginCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "originsWithLocalStorage", originsWithLocalStorageCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setShouldPaintBrokenImage", setShouldPaintBrokenImageCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setTextDirection", setTextDirectionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "allowRoundingHacks", allowRoundingHacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setShouldStayOnPageAfterHandlingBeforeUnload", setShouldStayOnPageAfterHandlingBeforeUnloadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addChromeInputField", addChromeInputFieldCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "removeChromeInputField", removeChromeInputFieldCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "focusWebView", focusWebViewCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setBackingScaleFactor", setBackingScaleFactorCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    return staticFunctions;
}

void LayoutTestController::queueLoadHTMLString(JSStringRef content, JSStringRef baseURL)
{
    WorkQueue::shared()->queue(new LoadHTMLStringItem(content, baseURL));
}

void LayoutTestController::queueLoadAlternateHTMLString(JSStringRef content, JSStringRef baseURL, JSStringRef unreachableURL)
{
    WorkQueue::shared()->queue(new LoadHTMLStringItem(content, baseURL, unreachableURL));
}

void LayoutTestController::queueBackNavigation(int howFarBack)
{
    WorkQueue::shared()->queue(new BackItem(howFarBack));
}

void LayoutTestController::queueForwardNavigation(int howFarForward)
{
    WorkQueue::shared()->queue(new ForwardItem(howFarForward));
}

void LayoutTestController::queueLoadingScript(JSStringRef script)
{
    WorkQueue::shared()->queue(new LoadingScriptItem(script));
}

void LayoutTestController::queueNonLoadingScript(JSStringRef script)
{
    WorkQueue::shared()->queue(new NonLoadingScriptItem(script));
}

void LayoutTestController::queueReload()
{
    WorkQueue::shared()->queue(new ReloadItem);
}

void LayoutTestController::grantDesktopNotificationPermission(JSStringRef origin)
{
    m_desktopNotificationAllowedOrigins.push_back(JSStringRetain(origin));
}

bool LayoutTestController::checkDesktopNotificationPermission(JSStringRef origin)
{
    std::vector<JSStringRef>::iterator i;
    for (i = m_desktopNotificationAllowedOrigins.begin();
         i != m_desktopNotificationAllowedOrigins.end();
         ++i) {
        if (JSStringIsEqual(*i, origin))
            return true;
    }
    return false;
}

void LayoutTestController::ignoreDesktopNotificationPermissionRequests()
{
    m_areDesktopNotificationPermissionRequestsIgnored = false;
}

void LayoutTestController::waitToDumpWatchdogTimerFired()
{
    const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
    fprintf(stderr, "%s", message);
    fprintf(stdout, "%s", message);
    notifyDone();
}

void LayoutTestController::setGeolocationPermissionCommon(bool allow)
{
    m_isGeolocationPermissionSet = true;
    m_geolocationPermission = allow;
}

void LayoutTestController::setPOSIXLocale(JSStringRef locale)
{
    char localeBuf[32];
    JSStringGetUTF8CString(locale, localeBuf, sizeof(localeBuf));
    setlocale(LC_ALL, localeBuf);
}

void LayoutTestController::addURLToRedirect(std::string origin, std::string destination)
{
    m_URLsToRedirect[origin] = destination;
}

const std::string& LayoutTestController::redirectionDestinationForURL(std::string origin)
{
    return m_URLsToRedirect[origin];
}

void LayoutTestController::setShouldPaintBrokenImage(bool shouldPaintBrokenImage)
{
    m_shouldPaintBrokenImage = shouldPaintBrokenImage;
}

const unsigned LayoutTestController::maxViewWidth = 800;
const unsigned LayoutTestController::maxViewHeight = 600;
