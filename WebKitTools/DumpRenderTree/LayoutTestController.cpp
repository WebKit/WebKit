/*
 * Copyright (C) 2007, 2008, 2009 Apple Inc. All rights reserved.
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
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <stdio.h>
#include <wtf/Assertions.h>
#include <wtf/MathExtras.h>
#include <wtf/RefPtr.h>

LayoutTestController::LayoutTestController(const std::string& testPathOrURL, const std::string& expectedPixelHash)
    : m_dumpAsText(false)
    , m_dumpAsPDF(false)
    , m_dumpBackForwardList(false)
    , m_dumpChildFrameScrollPositions(false)
    , m_dumpChildFramesAsText(false)
    , m_dumpDatabaseCallbacks(false)
    , m_dumpDOMAsWebArchive(false)
    , m_dumpSelectionRect(false)
    , m_dumpSourceAsWebArchive(false)
    , m_dumpStatusCallbacks(false)
    , m_dumpTitleChanges(false)
    , m_dumpEditingCallbacks(false)
    , m_dumpResourceLoadCallbacks(false)
    , m_dumpResourceResponseMIMETypes(false)
    , m_dumpWillCacheResponse(false)
    , m_dumpFrameLoadCallbacks(false)
    , m_callCloseOnWebViews(true)
    , m_canOpenWindows(false)
    , m_closeRemainingWindowsWhenComplete(true)
    , m_stopProvisionalFrameLoads(false)
    , m_testOnscreen(false)
    , m_testRepaint(false)
    , m_testRepaintSweepHorizontally(false)
    , m_waitToDump(false)
    , m_willSendRequestReturnsNullOnRedirect(false)
    , m_windowIsKey(true)
    , m_alwaysAcceptCookies(false)
    , m_globalFlag(false)
    , m_isGeolocationPermissionSet(false)
    , m_geolocationPermission(false)
    , m_testPathOrURL(testPathOrURL)
    , m_expectedPixelHash(expectedPixelHash)
{
}

// Static Functions

static JSValueRef dumpAsTextCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpAsText(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpAsPDFCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpAsPDF(true);
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

static JSValueRef dumpResourceLoadCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpResourceLoadCallbacks(true);
    return JSValueMakeUndefined(context);
}

static JSValueRef dumpFrameLoadCallbacksCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setDumpFrameLoadCallbacks(true);
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

static JSValueRef clearAllDatabasesCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->clearAllDatabases();

    return JSValueMakeUndefined(context);
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

static JSValueRef notifyDoneCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    // May be able to be made platform independant by using shared WorkQueue
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->notifyDone();
    return JSValueMakeUndefined(context);
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

static JSValueRef setXSSAuditorEnabledCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    // Has mac & windows implementation
    if (argumentCount < 1)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setXSSAuditorEnabled(JSValueToBoolean(context, arguments[0]));

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

static JSValueRef numberOfActiveAnimationsCallback(JSContextRef context, JSObjectRef function, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 0)
        return JSValueMakeUndefined(context);

    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    return JSValueMakeNumber(context, controller->numberOfActiveAnimations());
}

static JSValueRef waitForPolicyDelegateCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t, const JSValueRef[], JSValueRef*)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->waitForPolicyDelegate();
    return JSValueMakeUndefined(context);
}

static JSValueRef whiteListAccessFromOriginCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
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
    controller->whiteListAccessFromOrigin(sourceOrigin.get(), destinationProtocol.get(), destinationHost.get(), allowDestinationSubdomains);
    return JSValueMakeUndefined(context);
}

static JSValueRef addUserScriptCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 2)
        return JSValueMakeUndefined(context);
    
    JSRetainPtr<JSStringRef> source(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
    bool runAtStart = JSValueToBoolean(context, arguments[1]);
    
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->addUserScript(source.get(), runAtStart);
    return JSValueMakeUndefined(context);
}
 
static JSValueRef addUserStyleSheetCallback(JSContextRef context, JSObjectRef, JSObjectRef thisObject, size_t argumentCount, const JSValueRef arguments[], JSValueRef* exception)
{
    if (argumentCount != 1)
        return JSValueMakeUndefined(context);
    
    JSRetainPtr<JSStringRef> source(Adopt, JSValueToStringCopy(context, arguments[0], exception));
    ASSERT(!*exception);
   
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->addUserStyleSheet(source.get());
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

static bool setGlobalFlagCallback(JSContextRef context, JSObjectRef thisObject, JSStringRef propertyName, JSValueRef value, JSValueRef* exception)
{
    LayoutTestController* controller = static_cast<LayoutTestController*>(JSObjectGetPrivate(thisObject));
    controller->setGlobalFlag(JSValueToBoolean(context, value));
    return true;
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
    JSValueRef layoutTestContollerObject = JSObjectMake(context, getJSClass(), this);
    JSObjectSetProperty(context, windowObject, layoutTestContollerStr.get(), layoutTestContollerObject, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, exception);
}

JSClassRef LayoutTestController::getJSClass()
{
    static JSClassRef layoutTestControllerClass;

    if (!layoutTestControllerClass) {
        JSStaticValue* staticValues = LayoutTestController::staticValues();
        JSStaticFunction* staticFunctions = LayoutTestController::staticFunctions();
        JSClassDefinition classDefinition = {
            0, kJSClassAttributeNone, "LayoutTestController", 0, staticValues, staticFunctions,
            0, layoutTestControllerObjectFinalize, 0, 0, 0, 0, 0, 0, 0, 0, 0
        };

        layoutTestControllerClass = JSClassCreate(&classDefinition);
    }

    return layoutTestControllerClass;
}

JSStaticValue* LayoutTestController::staticValues()
{
    static JSStaticValue staticValues[] = {
        { "globalFlag", getGlobalFlagCallback, setGlobalFlagCallback, kJSPropertyAttributeNone },
        { "webHistoryItemCount", getWebHistoryItemCountCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "workerThreadCount", getWorkerThreadCountCallback, 0, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0, 0 }
    };
    return staticValues;
}

JSStaticFunction* LayoutTestController::staticFunctions()
{
    static JSStaticFunction staticFunctions[] = {
        { "addDisallowedURL", addDisallowedURLCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addUserScript", addUserScriptCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "addUserStyleSheet", addUserStyleSheetCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clearAllDatabases", clearAllDatabasesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clearBackForwardList", clearBackForwardListCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "clearPersistentUserStyleSheet", clearPersistentUserStyleSheetCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "decodeHostName", decodeHostNameCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "disableImageLoading", disableImageLoadingCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dispatchPendingLoadRequests", dispatchPendingLoadRequestsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "display", displayCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpAsText", dumpAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpBackForwardList", dumpBackForwardListCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpChildFrameScrollPositions", dumpChildFrameScrollPositionsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpChildFramesAsText", dumpChildFramesAsTextCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpDOMAsWebArchive", dumpDOMAsWebArchiveCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpDatabaseCallbacks", dumpDatabaseCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpEditingCallbacks", dumpEditingCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpResourceLoadCallbacks", dumpResourceLoadCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpFrameLoadCallbacks", dumpFrameLoadCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpResourceResponseMIMETypes", dumpResourceResponseMIMETypesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpSelectionRect", dumpSelectionRectCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpSourceAsWebArchive", dumpSourceAsWebArchiveCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpStatusCallbacks", dumpStatusCallbacksCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpTitleChanges", dumpTitleChangesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "dumpWillCacheResponse", dumpWillCacheResponseCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "elementDoesAutoCompleteForElementWithId", elementDoesAutoCompleteForElementWithIdCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "encodeHostName", encodeHostNameCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "execCommand", execCommandCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "grantDesktopNotificationPermission", grantDesktopNotificationPermissionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete }, 
        { "isCommandEnabled", isCommandEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "keepWebHistory", keepWebHistoryCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "notifyDone", notifyDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "numberOfActiveAnimations", numberOfActiveAnimationsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "overridePreference", overridePreferenceCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pathToLocalResource", pathToLocalResourceCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pauseAnimationAtTimeOnElementWithId", pauseAnimationAtTimeOnElementWithIdCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "pauseTransitionAtTimeOnElementWithId", pauseTransitionAtTimeOnElementWithIdCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "printToPDF", dumpAsPDFCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueBackNavigation", queueBackNavigationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueForwardNavigation", queueForwardNavigationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueLoad", queueLoadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueLoadingScript", queueLoadingScriptCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueNonLoadingScript", queueNonLoadingScriptCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "queueReload", queueReloadCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "repaintSweepHorizontally", repaintSweepHorizontallyCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAcceptsEditing", setAcceptsEditingCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAlwaysAcceptCookies", setAlwaysAcceptCookiesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAppCacheMaximumSize", setAppCacheMaximumSizeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete }, 
        { "setAuthenticationPassword", setAuthenticationPasswordCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAuthenticationUsername", setAuthenticationUsernameCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setAuthorAndUserStylesEnabled", setAuthorAndUserStylesEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCallCloseOnWebViews", setCallCloseOnWebViewsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCanOpenWindows", setCanOpenWindowsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCacheModel", setCacheModelCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCloseRemainingWindowsWhenComplete", setCloseRemainingWindowsWhenCompleteCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setCustomPolicyDelegate", setCustomPolicyDelegateCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setDatabaseQuota", setDatabaseQuotaCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete }, 
        { "setGeolocationPermission", setGeolocationPermissionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setHandlesAuthenticationChallenges", setHandlesAuthenticationChallengesCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setIconDatabaseEnabled", setIconDatabaseEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setJavaScriptProfilingEnabled", setJavaScriptProfilingEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setMainFrameIsFirstResponder", setMainFrameIsFirstResponderCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setMockGeolocationPosition", setMockGeolocationPositionCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setMockGeolocationError", setMockGeolocationErrorCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPersistentUserStyleSheetLocation", setPersistentUserStyleSheetLocationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPopupBlockingEnabled", setPopupBlockingEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setPrivateBrowsingEnabled", setPrivateBrowsingEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setXSSAuditorEnabled", setXSSAuditorEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSelectTrailingWhitespaceEnabled", setSelectTrailingWhitespaceEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setSmartInsertDeleteEnabled", setSmartInsertDeleteEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setStopProvisionalFrameLoads", setStopProvisionalFrameLoadsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setTabKeyCyclesThroughElements", setTabKeyCyclesThroughElementsCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setUseDashboardCompatibilityMode", setUseDashboardCompatibilityModeCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setUserStyleSheetEnabled", setUserStyleSheetEnabledCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setUserStyleSheetLocation", setUserStyleSheetLocationCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setWillSendRequestReturnsNullOnRedirect", setWillSendRequestReturnsNullOnRedirectCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "setWindowIsKey", setWindowIsKeyCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "testOnscreen", testOnscreenCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "testRepaint", testRepaintCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "waitForPolicyDelegate", waitForPolicyDelegateCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "waitUntilDone", waitUntilDoneCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "windowCount", windowCountCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { "whiteListAccessFromOrigin", whiteListAccessFromOriginCallback, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete },
        { 0, 0, 0 }
    };

    return staticFunctions;
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

void LayoutTestController::waitToDumpWatchdogTimerFired()
{
    const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
    fprintf(stderr, "%s", message);
    fprintf(stdout, "%s", message);
    notifyDone();
}

void LayoutTestController::setGeolocationPermission(bool allow)
{
    m_isGeolocationPermissionSet = true;
    m_geolocationPermission = allow;
}
