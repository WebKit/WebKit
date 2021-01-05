/*
 * Copyright (C) 2013 Haiku, Inc.
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
#include "TestRunner.h"

#include "DumpRenderTree.h"
#include "JSStringUtils.h"
#include "NotImplemented.h"
#include "wtf/URL.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"

#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include "FindOptions.h"
#include <stdio.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

#include <Application.h>
#include <DumpRenderTreeClient.h>
#include <WebPage.h>
#include <WebView.h>

// Same as Mac cache model enum in Source/WebKit/mac/WebView/WebPreferences.h.
enum {
    WebCacheModelDocumentViewer = 0,
    WebCacheModelDocumentBrowser = 1,
    WebCacheModelPrimaryWebBrowser = 2
};

extern BWebView* webView;

TestRunner::~TestRunner()
{
}

JSContextRef TestRunner::mainFrameJSContext()
{
    notImplemented();
	return nullptr;
}

void TestRunner::addDisallowedURL(JSStringRef)
{
    notImplemented();
}

void TestRunner::clearBackForwardList()
{
    notImplemented();
}

JSRetainPtr<JSStringRef> TestRunner::copyDecodedHostName(JSStringRef)
{
    notImplemented();
    return 0;
}

JSRetainPtr<JSStringRef> TestRunner::copyEncodedHostName(JSStringRef)
{
    notImplemented();
    return 0;
}

void TestRunner::dispatchPendingLoadRequests()
{
    // FIXME: Implement for testing fix for 6727495
    notImplemented();
}

void TestRunner::display()
{
    notImplemented();
    //displayWebView();
}

void TestRunner::displayAndTrackRepaints()
{
    notImplemented();
    //displayWebView();
}

void TestRunner::keepWebHistory()
{
    WebCore::DumpRenderTreeClient::setShouldTrackVisitedLinks(true);
}

size_t TestRunner::webHistoryItemCount()
{
    notImplemented();
    return -1;
}

void TestRunner::notifyDone()
{
    if (m_waitToDump && !topLoadingFrame && !DRT::WorkQueue::singleton().count())
        dump();
    m_waitToDump = false;
    waitForPolicy = false;
}

void TestRunner::forceImmediateCompletion()
{
    // Same as on mac. This can be shared.
    if (m_waitToDump && !DRT::WorkQueue::singleton().count())
        dump();
    m_waitToDump = false;
}

JSRetainPtr<JSStringRef> TestRunner::pathToLocalResource(JSContextRef context, JSStringRef url)
{
    String requestedUrl(url->characters(), url->length());
    String resourceRoot;
    String requestedRoot;

    if (requestedUrl.find("LayoutTests") != notFound) {
        // If the URL contains LayoutTests we need to remap that to
        // LOCAL_RESOURCE_ROOT which is the path of the LayoutTests directory
        // within the WebKit source tree.
        requestedRoot = "/tmp/LayoutTests";
        resourceRoot = getenv("LOCAL_RESOURCE_ROOT");
    } else if (requestedUrl.find("tmp") != notFound) {
        // If the URL is a child of /tmp we need to convert it to be a child
        // DUMPRENDERTREE_TEMP replace tmp with DUMPRENDERTREE_TEMP
        requestedRoot = "/tmp";
        resourceRoot = getenv("DUMPRENDERTREE_TEMP");
    }

    size_t indexOfRootStart = requestedUrl.reverseFind(requestedRoot);
    size_t indexOfSeparatorAfterRoot = indexOfRootStart + requestedRoot.length();
    String fullPathToUrl = "file://" + resourceRoot + requestedUrl.substring(indexOfSeparatorAfterRoot);

    return JSStringCreateWithUTF8CString(fullPathToUrl.utf8().data());
}

void TestRunner::queueLoad(JSStringRef url, JSStringRef target)
{
    notImplemented();
}

void TestRunner::setAcceptsEditing(bool acceptsEditing)
{
    notImplemented();
}

void TestRunner::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
    notImplemented();
}

void TestRunner::setOnlyAcceptFirstPartyCookies(bool onlyAcceptFirstPartyCookies)
{
    // FIXME: Implement.
    fprintf(testResult, "ERROR: TestRunner::setOnlyAcceptFirstPartyCookies() not implemented\n");
}

void TestRunner::setCustomPolicyDelegate(bool enabled, bool permissive)
{
    notImplemented();
}

void TestRunner::waitForPolicyDelegate()
{
    setCustomPolicyDelegate(true, false);
    waitForPolicy = true;
    setWaitToDump(true);
}

void TestRunner::setScrollbarPolicy(JSStringRef, JSStringRef)
{
    notImplemented();
}

void TestRunner::addOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef protocol, JSStringRef host, bool includeSubdomains)
{
    notImplemented();
}

void TestRunner::removeOriginAccessAllowListEntry(JSStringRef sourceOrigin, JSStringRef protocol, JSStringRef host, bool includeSubdomains)
{
    notImplemented();
}

void TestRunner::setMainFrameIsFirstResponder(bool)
{
    notImplemented();
}

void TestRunner::setTabKeyCyclesThroughElements(bool)
{
    notImplemented();
}

static CString gUserStyleSheet;
static bool gUserStyleSheetEnabled = true;

void TestRunner::setUserStyleSheetEnabled(bool flag)
{
    notImplemented();
}

void TestRunner::setUserStyleSheetLocation(JSStringRef path)
{
    gUserStyleSheet = path->string().utf8();

    if (gUserStyleSheetEnabled)
        setUserStyleSheetEnabled(true);
}

void TestRunner::setValueForUser(JSContextRef context, JSValueRef nodeObject, JSStringRef value)
{
    WebCore::DumpRenderTreeClient::setValueForUser(context, nodeObject, value->string());
}

void TestRunner::setWindowIsKey(bool)
{
    notImplemented();
}

void TestRunner::setViewSize(double width, double height)
{
	notImplemented();
}

void watchdogFired()
{
    delete waitToDumpWatchdog;
    waitToDumpWatchdog = NULL;
    gTestRunner->waitToDumpWatchdogTimerFired();
}

void TestRunner::setWaitToDump(bool waitUntilDone)
{
    m_waitToDump = waitUntilDone;

    if (m_waitToDump && shouldSetWaitToDumpWatchdog()) {
        BMessage message('dwdg');
        waitToDumpWatchdog = new BMessageRunner(be_app, &message, 30000000, 1);
    }
}

int TestRunner::windowCount()
{
    return be_app->CountWindows();
}

void TestRunner::setPrivateBrowsingEnabled(bool flag)
{
    notImplemented();
}

void TestRunner::setMockDeviceOrientation(bool, double, bool, double, bool, double)
{
    // FIXME: Implement for DeviceOrientation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=30335.
    notImplemented();
}

void TestRunner::setMockGeolocationPositionUnavailableError(JSStringRef message)
{
    notImplemented();
}

void TestRunner::setGeolocationPermission(bool allow)
{
    notImplemented();
}

void TestRunner::setMockGeolocationPosition(double, double, double, bool, double, bool, double, bool, double, bool, double, bool, double)
{
    notImplemented();
}

int TestRunner::numberOfPendingGeolocationPermissionRequests()
{
    // FIXME: Implement for Geolocation layout tests.
    printf("ERROR: TestRunner::numberOfPendingGeolocationPermissionRequests() not implemented\n");
    return -1;
}

bool TestRunner::isGeolocationProviderActive()
{
    // FIXME: Implement for Geolocation layout tests.
    printf("ERROR: TestRunner::isGeolocationProviderActive() not implemented\n");
    return false;
}

void TestRunner::setIconDatabaseEnabled(bool enabled)
{
    notImplemented();
}

void TestRunner::execCommand(JSStringRef name, JSStringRef value)
{
    WebCore::DumpRenderTreeClient::executeCoreCommandByName(webView, name->string(), value->string());
}

bool TestRunner::findString(JSContextRef context, JSStringRef target, JSObjectRef optionsArray)
{
    notImplemented();
    return false;
}

bool TestRunner::isCommandEnabled(JSStringRef name)
{
    notImplemented();
    return false;
}

void TestRunner::setCacheModel(int cacheModel)
{
    notImplemented();
}

void TestRunner::setPersistentUserStyleSheetLocation(JSStringRef)
{
    notImplemented();
}

void TestRunner::clearPersistentUserStyleSheet()
{
    notImplemented();
}

void TestRunner::clearAllApplicationCaches()
{
    notImplemented();
}

void TestRunner::clearApplicationCacheForOrigin(OpaqueJSString* url)
{
    notImplemented();
}

JSValueRef TestRunner::originsWithApplicationCache(JSContextRef context)
{
    // FIXME: Implement to get origins that contain application caches.
    notImplemented();
    return JSValueMakeUndefined(context);
}

long long TestRunner::applicationCacheDiskUsageForOrigin(JSStringRef)
{
    notImplemented();
    return 0;
}

void TestRunner::clearAllDatabases()
{
    notImplemented();
}

void TestRunner::setDatabaseQuota(unsigned long long quota)
{
    notImplemented();
}

void TestRunner::setDomainRelaxationForbiddenForURLScheme(bool forbidden, JSStringRef scheme)
{
    WebCore::DumpRenderTreeClient::setDomainRelaxationForbiddenForURLScheme(forbidden, scheme->string());
}

void TestRunner::goBack()
{
    notImplemented();
}

void TestRunner::setDefersLoading(bool defers)
{
    notImplemented();
}

void TestRunner::setAppCacheMaximumSize(unsigned long long size)
{
    notImplemented();
}

static inline bool toBool(JSStringRef value)
{
    return equals(value, "true") || equals(value, "1");
}

static inline int toInt(JSStringRef value)
{
    return atoi(value->string().utf8().data());
}

void TestRunner::overridePreference(JSStringRef key, JSStringRef value)
{
    if (equals(key, "WebKitAcceleratedCompositingEnabled"))
        notImplemented();
    else if (equals(key, "WebKitCSSCustomFilterEnabled"))
        notImplemented();
    else if (equals(key, "WebKitCSSGridLayoutEnabled"))
        notImplemented();
    else if (equals(key, "WebKitCSSRegionsEnabled"))
        notImplemented();
    else if (equals(key, "WebKitDefaultFontSize"))
        notImplemented();
    else if (equals(key, "WebKitDisplayImagesKey"))
        notImplemented();
    else if (equals(key, "WebKitEnableCaretBrowsing"))
        notImplemented();
    else if (equals(key, "WebKitJavaEnabled"))
        notImplemented();
    else if (equals(key, "WebKitJavaScriptEnabled"))
        notImplemented();
    else if (equals(key, "WebKitPageCacheSupportsPluginsPreferenceKey"))
        notImplemented();
    else if (equals(key, "WebKitShouldRespectImageOrientation"))
        notImplemented();
    else if (equals(key, "WebKitSupportsMultipleWindows"))
        notImplemented();
    else if (equals(key, "WebKitTabToLinksPreferenceKey"))
        notImplemented();
    else if (equals(key, "WebKitUsePreHTML5ParserQuirks"))
        notImplemented();
    else if (equals(key, "WebKitUsesPageCachePreferenceKey"))
        notImplemented();
    else if (equals(key, "WebKitWebAudioEnabled"))
        notImplemented();
    else if (equals(key, "WebKitWebGLEnabled"))
        notImplemented();
    else
        fprintf(stderr, "TestRunner::overridePreference tried to override unknown preference '%s'.\n", key->string().utf8().data());
}

void TestRunner::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    WebCore::DumpRenderTreeClient::addUserScript(webView, source->string(),
        runAtStart, allFrames);
}

void TestRunner::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    notImplemented();
}

void TestRunner::showWebInspector()
{
    notImplemented();
}

void TestRunner::closeWebInspector()
{
    notImplemented();
}

void TestRunner::evaluateInWebInspector(JSStringRef script)
{
    notImplemented();
}

JSRetainPtr<JSStringRef> TestRunner::inspectorTestStubURL()
{
    notImplemented();
    return nullptr;
}

void TestRunner::evaluateScriptInIsolatedWorldAndReturnValue(unsigned, JSObjectRef, JSStringRef)
{
    notImplemented();
}

void TestRunner::evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    notImplemented();
}

void TestRunner::removeAllVisitedLinks()
{
    notImplemented();
}

bool TestRunner::callShouldCloseOnWebView()
{
    notImplemented();
}

void TestRunner::apiTestNewWindowDataLoadBaseURL(JSStringRef, JSStringRef)
{
    notImplemented();
}

void TestRunner::apiTestGoToCurrentBackForwardItem()
{
    notImplemented();
}

void TestRunner::setWebViewEditable(bool)
{
    notImplemented();
}

void TestRunner::authenticateSession(JSStringRef, JSStringRef, JSStringRef)
{
    notImplemented();
}

void TestRunner::abortModal()
{
    notImplemented();
}

void TestRunner::setSerializeHTTPLoads(bool serialize)
{
    WebCore::DumpRenderTreeClient::setSerializeHTTPLoads(serialize);
}

void TestRunner::setTextDirection(JSStringRef direction)
{
    notImplemented();
}

void TestRunner::addChromeInputField()
{
    notImplemented();
}

void TestRunner::removeChromeInputField()
{
    notImplemented();
}

void TestRunner::focusWebView()
{
    notImplemented();
}

void TestRunner::setBackingScaleFactor(double)
{
    notImplemented();
}

void TestRunner::grantWebNotificationPermission(JSStringRef origin)
{
}

void TestRunner::denyWebNotificationPermission(JSStringRef jsOrigin)
{
}

void TestRunner::removeAllWebNotificationPermissions()
{
}

void TestRunner::simulateWebNotificationClick(JSValueRef jsNotification)
{
}

void TestRunner::simulateLegacyWebNotificationClick(JSStringRef title)
{
}

unsigned TestRunner::imageCountInGeneralPasteboard() const
{
    printf("ERROR: TestRunner::imageCountInGeneralPasteboard() not implemented\n");
    return 0;
}

void TestRunner::setSpellCheckerLoggingEnabled(bool enabled)
{
    printf("ERROR: TestRunner::setSpellCheckerLoggingEnabled() not implemented\n");
}

void TestRunner::resetPageVisibility()
{
    notImplemented();
}

void TestRunner::setPageVisibility(const char* visibility)
{
    notImplemented();
}

void TestRunner::setAutomaticLinkDetectionEnabled(bool)
{
    notImplemented();
}

void TestRunner::setStorageDatabaseIdleInterval(double)
{
    notImplemented();
}

void TestRunner::closeIdleLocalStorageDatabases()
{
    notImplemented();
}

