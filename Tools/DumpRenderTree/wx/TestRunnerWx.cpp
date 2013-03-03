/*
 * Copyright (C) 2008 Kevin Ollivier <kevino@theolliviers.com>
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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
#include "TestRunner.h"

#include "DumpRenderTree.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>

#include <stdio.h>



TestRunner::~TestRunner()
{
    // FIXME: implement
}

void TestRunner::addDisallowedURL(JSStringRef url)
{
    // FIXME: implement
}

void TestRunner::clearBackForwardList()
{
}

JSStringRef TestRunner::copyDecodedHostName(JSStringRef name)
{
    // FIXME: implement
    return 0;
}

JSStringRef TestRunner::copyEncodedHostName(JSStringRef name)
{
    // FIXME: implement
    return 0;
}

void TestRunner::dispatchPendingLoadRequests()
{
    // FIXME: Implement for testing fix for 6727495
}

void TestRunner::display()
{
}

void TestRunner::keepWebHistory()
{
    // FIXME: implement
}

void TestRunner::notifyDone()
{
    if (m_waitToDump && !WorkQueue::shared()->count())
        notifyDoneFired();
    m_waitToDump = false;
}

JSStringRef TestRunner::pathToLocalResource(JSContextRef context, JSStringRef url)
{
    // Function introduced in r28690. This may need special-casing on Windows.
    return JSStringRetain(url); // Do nothing on Unix.
}

void TestRunner::queueLoad(JSStringRef url, JSStringRef target)
{
    // FIXME: We need to resolve relative URLs here
    WorkQueue::shared()->queue(new LoadItem(url, target));
}

void TestRunner::setAcceptsEditing(bool acceptsEditing)
{
}

void TestRunner::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
    // FIXME: Implement this (and restore the default value before running each test in DumpRenderTree.cpp).
}

void TestRunner::setCustomPolicyDelegate(bool, bool)
{
    // FIXME: implement
}

void TestRunner::setMainFrameIsFirstResponder(bool flag)
{
    // FIXME: implement
}

void TestRunner::setTabKeyCyclesThroughElements(bool cycles)
{
    // FIXME: implement
}

void TestRunner::setUseDashboardCompatibilityMode(bool flag)
{
    // FIXME: implement
}

void TestRunner::setUserStyleSheetEnabled(bool flag)
{
}

void TestRunner::setUserStyleSheetLocation(JSStringRef path)
{
}

void TestRunner::setValueForUser(JSContextRef context, JSValueRef element, JSStringRef value)
{
    // FIXME: implement
}

void TestRunner::setViewModeMediaFeature(JSStringRef mode)
{
    // FIXME: implement
}

void TestRunner::setWindowIsKey(bool windowIsKey)
{
    // FIXME: implement
}

void TestRunner::setSmartInsertDeleteEnabled(bool flag)
{
    // FIXME: implement
}

void TestRunner::setWaitToDump(bool waitUntilDone)
{
    static const int timeoutSeconds = 10;

    m_waitToDump = waitUntilDone;
}

int TestRunner::windowCount()
{
    // FIXME: implement
    return 1;
}

void TestRunner::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    // FIXME: implement
}

void TestRunner::setJavaScriptCanAccessClipboard(bool enabled)
{
    // FIXME: implement
}

void TestRunner::setXSSAuditorEnabled(bool enabled)
{
    // FIXME: implement
}

void TestRunner::setAllowUniversalAccessFromFileURLs(bool enabled)
{
    // FIXME: implement
}

void TestRunner::setAllowFileAccessFromFileURLs(bool enabled)
{
    // FIXME: implement
}

void TestRunner::setAuthorAndUserStylesEnabled(bool flag)
{
    // FIXME: implement
}

void TestRunner::setPopupBlockingEnabled(bool popupBlockingEnabled)
{
    // FIXME: implement
}

void TestRunner::setPluginsEnabled(bool flag)
{
    // FIXME: Implement
}

bool TestRunner::elementDoesAutoCompleteForElementWithId(JSStringRef id) 
{
    // FIXME: implement
    return false;
}

void TestRunner::execCommand(JSStringRef name, JSStringRef value)
{
    // FIXME: implement
}

void TestRunner::setPersistentUserStyleSheetLocation(JSStringRef jsURL)
{
    // FIXME: implement
}

void TestRunner::clearPersistentUserStyleSheet()
{
    // FIXME: implement
}

void TestRunner::clearAllApplicationCaches()
{
    // FIXME: Implement to support application cache quotas.
}

void TestRunner::clearApplicationCacheForOrigin(JSStringRef url)
{
    // FIXME: Implement to support deleting all application cache for an origin.
}

long long TestRunner::localStorageDiskUsageForOrigin(JSStringRef originIdentifier)
{
    // FIXME: Implement to support getting disk usage in bytes for an origin.
    return 0;
}

void TestRunner::setApplicationCacheOriginQuota(unsigned long long quota)
{
    // FIXME: Implement to support application cache quotas.
}

long long TestRunner::applicationCacheDiskUsageForOrigin(JSStringRef origin)
{
    // FIXME: Implement to support getting disk usage by all application caches for an origin.
    return 0;
}


JSValueRef TestRunner::originsWithApplicationCache(JSContextRef context)
{
    // FIXME: Implement to get origins that have application caches.
    return 0;
}

void TestRunner::clearAllDatabases()
{
    // FIXME: implement
}
 
void TestRunner::setDatabaseQuota(unsigned long long quota)
{    
    // FIXME: implement
}

void TestRunner::goBack()
{
    // FIXME: implement to enable loader/navigation-while-deferring-loads.html
}

void TestRunner::setDefersLoading(bool)
{
    // FIXME: implement to enable loader/navigation-while-deferring-loads.html
}

void TestRunner::setDomainRelaxationForbiddenForURLScheme(bool, JSStringRef)
{
    // FIXME: implement
}

void TestRunner::setAppCacheMaximumSize(unsigned long long size)
{
    // FIXME: implement
}

void TestRunner::setSelectTrailingWhitespaceEnabled(bool flag)
{
    // FIXME: implement
}

void TestRunner::setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
    // FIXME: Implement for DeviceOrientation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=30335.
}

void TestRunner::setMockGeolocationPosition(double latitude, double longitude, double accuracy, bool providesAltitude, double altitude, bool providesAltitudeAccuracy, double altitudeAccuracy, bool providesHeading, double heading, bool providesSpeed, double speed)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
}

void TestRunner::setMockGeolocationPositionUnavailableError(JSStringRef)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
}

void TestRunner::setGeolocationPermission(bool allow)
{
    // FIXME: Implement for Geolocation layout tests.
    setGeolocationPermissionCommon(allow);
}

int TestRunner::numberOfPendingGeolocationPermissionRequests()
{
    // FIXME: Implement for Geolocation layout tests.
    return -1;
}

void TestRunner::addMockSpeechInputResult(JSStringRef result, double confidence, JSStringRef language)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void TestRunner::setMockSpeechInputDumpRect(bool flag)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void TestRunner::startSpeechInput(JSContextRef inputElement)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void TestRunner::setIconDatabaseEnabled(bool iconDatabaseEnabled)
{
    // FIXME: implement
}

void TestRunner::setCacheModel(int)
{
    // FIXME: implement
}

bool TestRunner::isCommandEnabled(JSStringRef /*name*/)
{
    // FIXME: implement
    return false;
}

size_t TestRunner::webHistoryItemCount()
{
    // FIXME: implement
    return 0;
}

void TestRunner::waitForPolicyDelegate()
{
    // FIXME: Implement this.
}

void TestRunner::overridePreference(JSStringRef /* key */, JSStringRef /* value */)
{
    // FIXME: implement
}

void TestRunner::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    printf("TestRunner::addUserScript not implemented.\n");
}

void TestRunner::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    printf("TestRunner::addUserStyleSheet not implemented.\n");
}

void TestRunner::showWebInspector()
{
    // FIXME: Implement this.
}

void TestRunner::closeWebInspector()
{
    // FIXME: Implement this.
}

void TestRunner::evaluateInWebInspector(long callId, JSStringRef script)
{
    // FIXME: Implement this.
}

void TestRunner::removeAllVisitedLinks()
{
    // FIXME: Implement this.
}

void TestRunner::evaluateScriptInIsolatedWorldAndReturnValue(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{

}

void TestRunner::evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{

}

void TestRunner::addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    // FIXME: implement
}

void TestRunner::removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    // FIXME: implement
}

void TestRunner::setScrollbarPolicy(JSStringRef orientation, JSStringRef policy)
{
    // FIXME: implement
}

void TestRunner::apiTestNewWindowDataLoadBaseURL(JSStringRef utf8Data, JSStringRef baseURL)
{

}

void TestRunner::apiTestGoToCurrentBackForwardItem()
{

}

void TestRunner::setSpatialNavigationEnabled(bool)
{

}

void TestRunner::setWebViewEditable(bool)
{
}

bool TestRunner::callShouldCloseOnWebView()
{
    return false;
}

void TestRunner::authenticateSession(JSStringRef, JSStringRef, JSStringRef)
{
}

void TestRunner::abortModal()
{
}

void TestRunner::setAsynchronousSpellCheckingEnabled(bool)
{
    // FIXME: Implement this.
}

bool TestRunner::findString(JSContextRef context, JSStringRef target, JSObjectRef optionsArray)
{
    // FIXME: Implement
    return false;
}

void TestRunner::setSerializeHTTPLoads(bool)
{
    // FIXME: Implement.
}

void TestRunner::syncLocalStorage()
{
    // FIXME: Implement.
}

void TestRunner::observeStorageTrackerNotifications(unsigned number)
{
    // FIXME: Implement.
}

void TestRunner::deleteAllLocalStorage()
{
    // FIXME: Implement.
}

JSValueRef TestRunner::originsWithLocalStorage(JSContextRef context)
{
    // FIXME: Implement.
    return 0;
}

void TestRunner::deleteLocalStorageForOrigin(JSStringRef URL)
{
    // FIXME: Implement.
}

void TestRunner::setTextDirection(JSStringRef direction)
{
    // FIXME: Implement.
}

void TestRunner::addChromeInputField()
{
}

void TestRunner::removeChromeInputField()
{
}

void TestRunner::focusWebView()
{
}

void TestRunner::setBackingScaleFactor(double)
{
}

void TestRunner::simulateDesktopNotificationClick(JSStringRef title)
{
    // FIXME: Implement.
}

void TestRunner::resetPageVisibility()
{
    // FIXME: Implement this.
}

void TestRunner::setPageVisibility(const char*)
{
    // FIXME: Implement this.
}

void TestRunner::setAutomaticLinkDetectionEnabled(bool)
{
    // FIXME: Implement this.
}

void TestRunner::setStorageDatabaseIdleInterval(double)
{
    // FIXME: Implement this.
}

void TestRunner::closeIdleLocalStorageDatabases()
{
    // FIXME: Implement this.
}
