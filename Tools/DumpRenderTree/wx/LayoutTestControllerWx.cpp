/*
 * Copyright (C) 2008 Kevin Ollivier <kevino@theolliviers.com>
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

#include "DumpRenderTree.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>

#include <stdio.h>



LayoutTestController::~LayoutTestController()
{
    // FIXME: implement
}

void LayoutTestController::addDisallowedURL(JSStringRef url)
{
    // FIXME: implement
}

void LayoutTestController::clearBackForwardList()
{
}

JSStringRef LayoutTestController::copyDecodedHostName(JSStringRef name)
{
    // FIXME: implement
    return 0;
}

JSStringRef LayoutTestController::copyEncodedHostName(JSStringRef name)
{
    // FIXME: implement
    return 0;
}

void LayoutTestController::dispatchPendingLoadRequests()
{
    // FIXME: Implement for testing fix for 6727495
}

void LayoutTestController::display()
{
}

void LayoutTestController::keepWebHistory()
{
    // FIXME: implement
}

void LayoutTestController::notifyDone()
{
    if (m_waitToDump && !WorkQueue::shared()->count())
        notifyDoneFired();
    m_waitToDump = false;
}

JSStringRef LayoutTestController::pathToLocalResource(JSContextRef context, JSStringRef url)
{
    // Function introduced in r28690. This may need special-casing on Windows.
    return JSStringRetain(url); // Do nothing on Unix.
}

void LayoutTestController::queueLoad(JSStringRef url, JSStringRef target)
{
    // FIXME: We need to resolve relative URLs here
    WorkQueue::shared()->queue(new LoadItem(url, target));
}

void LayoutTestController::setAcceptsEditing(bool acceptsEditing)
{
}

void LayoutTestController::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
    // FIXME: Implement this (and restore the default value before running each test in DumpRenderTree.cpp).
}

void LayoutTestController::setCustomPolicyDelegate(bool, bool)
{
    // FIXME: implement
}

void LayoutTestController::setMainFrameIsFirstResponder(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool cycles)
{
    // FIXME: implement
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
{
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef path)
{
}

void LayoutTestController::setValueForUser(JSContextRef context, JSValueRef element, JSStringRef value)
{
    // FIXME: implement
}

void LayoutTestController::setViewModeMediaFeature(JSStringRef mode)
{
    // FIXME: implement
}

void LayoutTestController::setWindowIsKey(bool windowIsKey)
{
    // FIXME: implement
}

void LayoutTestController::setSmartInsertDeleteEnabled(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool flag)
{
}

void LayoutTestController::setWaitToDump(bool waitUntilDone)
{
    static const int timeoutSeconds = 10;

    m_waitToDump = waitUntilDone;
}

int LayoutTestController::windowCount()
{
    // FIXME: implement
    return 1;
}

void LayoutTestController::setPrivateBrowsingEnabled(bool privateBrowsingEnabled)
{
    // FIXME: implement
}

void LayoutTestController::setJavaScriptCanAccessClipboard(bool enabled)
{
    // FIXME: implement
}

void LayoutTestController::setXSSAuditorEnabled(bool enabled)
{
    // FIXME: implement
}

void LayoutTestController::setFrameFlatteningEnabled(bool enabled)
{
    // FIXME: implement
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(bool enabled)
{
    // FIXME: implement
}

void LayoutTestController::setAllowFileAccessFromFileURLs(bool enabled)
{
    // FIXME: implement
}

void LayoutTestController::setAuthorAndUserStylesEnabled(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setAutofilled(JSContextRef, JSValueRef element, bool isAutofilled)
{
    // FIXME: implement
}

void LayoutTestController::setPopupBlockingEnabled(bool popupBlockingEnabled)
{
    // FIXME: implement
}

void LayoutTestController::setPluginsEnabled(bool flag)
{
    // FIXME: Implement
}

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(JSStringRef id) 
{
    // FIXME: implement
    return false;
}

void LayoutTestController::execCommand(JSStringRef name, JSStringRef value)
{
    // FIXME: implement
}

void LayoutTestController::setPersistentUserStyleSheetLocation(JSStringRef jsURL)
{
    // FIXME: implement
}

void LayoutTestController::clearPersistentUserStyleSheet()
{
    // FIXME: implement
}

void LayoutTestController::clearAllApplicationCaches()
{
    // FIXME: Implement to support application cache quotas.
}

void LayoutTestController::clearApplicationCacheForOrigin(JSStringRef url)
{
    // FIXME: Implement to support deleting all application cache for an origin.
}

long long LayoutTestController::localStorageDiskUsageForOrigin(JSStringRef originIdentifier)
{
    // FIXME: Implement to support getting disk usage in bytes for an origin.
    return 0;
}

void LayoutTestController::setApplicationCacheOriginQuota(unsigned long long quota)
{
    // FIXME: Implement to support application cache quotas.
}

long long LayoutTestController::applicationCacheDiskUsageForOrigin(JSStringRef origin)
{
    // FIXME: Implement to support getting disk usage by all application caches for an origin.
    return 0;
}


JSValueRef LayoutTestController::originsWithApplicationCache(JSContextRef context)
{
    // FIXME: Implement to get origins that have application caches.
    return 0;
}

void LayoutTestController::clearAllDatabases()
{
    // FIXME: implement
}
 
void LayoutTestController::setDatabaseQuota(unsigned long long quota)
{    
    // FIXME: implement
}

void LayoutTestController::goBack()
{
    // FIXME: implement to enable loader/navigation-while-deferring-loads.html
}

void LayoutTestController::setDefersLoading(bool)
{
    // FIXME: implement to enable loader/navigation-while-deferring-loads.html
}

void LayoutTestController::setAppCacheMaximumSize(unsigned long long size)
{
    // FIXME: implement
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    // FIXME: implement
    return 0;
}

unsigned LayoutTestController::workerThreadCount() const
{
    // FIXME: implement
    return 0;
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(bool flag)
{
    // FIXME: implement
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId)
{
    // FIXME: implement
    return false;
}

void LayoutTestController::setMockDeviceOrientation(bool canProvideAlpha, double alpha, bool canProvideBeta, double beta, bool canProvideGamma, double gamma)
{
    // FIXME: Implement for DeviceOrientation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=30335.
}

void LayoutTestController::setMockGeolocationPosition(double latitude, double longitude, double accuracy)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
}

void LayoutTestController::setMockGeolocationError(int code, JSStringRef message)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
}

void LayoutTestController::setGeolocationPermission(bool allow)
{
    // FIXME: Implement for Geolocation layout tests.
    setGeolocationPermissionCommon(allow);
}

int LayoutTestController::numberOfPendingGeolocationPermissionRequests()
{
    // FIXME: Implement for Geolocation layout tests.
    return -1;
}

void LayoutTestController::addMockSpeechInputResult(JSStringRef result, double confidence, JSStringRef language)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void LayoutTestController::setMockSpeechInputDumpRect(bool flag)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void LayoutTestController::startSpeechInput(JSContextRef inputElement)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void LayoutTestController::setIconDatabaseEnabled(bool iconDatabaseEnabled)
{
    // FIXME: implement
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{
    // FIXME: implement
    return false;
}

void LayoutTestController::setCacheModel(int)
{
    // FIXME: implement
}

bool LayoutTestController::isCommandEnabled(JSStringRef /*name*/)
{
    // FIXME: implement
    return false;
}

size_t LayoutTestController::webHistoryItemCount()
{
    // FIXME: implement
    return 0;
}

void LayoutTestController::waitForPolicyDelegate()
{
    // FIXME: Implement this.
}

void LayoutTestController::overridePreference(JSStringRef /* key */, JSStringRef /* value */)
{
    // FIXME: implement
}

void LayoutTestController::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    printf("LayoutTestController::addUserScript not implemented.\n");
}

void LayoutTestController::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    printf("LayoutTestController::addUserStyleSheet not implemented.\n");
}

void LayoutTestController::showWebInspector()
{
    // FIXME: Implement this.
}

void LayoutTestController::closeWebInspector()
{
    // FIXME: Implement this.
}

void LayoutTestController::evaluateInWebInspector(long callId, JSStringRef script)
{
    // FIXME: Implement this.
}

void LayoutTestController::removeAllVisitedLinks()
{
    // FIXME: Implement this.
}

void LayoutTestController::evaluateScriptInIsolatedWorldAndReturnValue(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{

}

void LayoutTestController::evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{

}

void LayoutTestController::disableImageLoading()
{

}

void LayoutTestController::addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    // FIXME: implement
}

void LayoutTestController::removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef destinationProtocol, JSStringRef destinationHost, bool allowDestinationSubdomains)
{
    // FIXME: implement
}

void LayoutTestController::setScrollbarPolicy(JSStringRef orientation, JSStringRef policy)
{
    // FIXME: implement
}

JSRetainPtr<JSStringRef> LayoutTestController::counterValueForElementById(JSStringRef id)
{
    return 0;
}

int LayoutTestController::pageNumberForElementById(JSStringRef, float, float)
{
    // FIXME: implement
    return -1;
}

int LayoutTestController::numberOfPages(float, float)
{
    // FIXME: implement
    return -1;
}

void LayoutTestController::apiTestNewWindowDataLoadBaseURL(JSStringRef utf8Data, JSStringRef baseURL)
{

}

void LayoutTestController::apiTestGoToCurrentBackForwardItem()
{

}

void LayoutTestController::setSpatialNavigationEnabled(bool)
{

}

void LayoutTestController::setWebViewEditable(bool)
{
}

bool LayoutTestController::callShouldCloseOnWebView()
{
    return false;
}

JSRetainPtr<JSStringRef> LayoutTestController::layerTreeAsText() const
{
    return 0;
}

JSRetainPtr<JSStringRef> LayoutTestController::markerTextForListItem(JSContextRef context, JSValueRef nodeObject) const
{
    return 0;
}

JSValueRef LayoutTestController::computedStyleIncludingVisitedInfo(JSContextRef, JSValueRef)
{
    return 0;
}

void LayoutTestController::authenticateSession(JSStringRef, JSStringRef, JSStringRef)
{
}

void LayoutTestController::setEditingBehavior(const char* editingBehavior)
{
    // FIXME: Implement
}

void LayoutTestController::abortModal()
{
}

void LayoutTestController::dumpConfigurationForViewport(int /*deviceDPI*/, int /*deviceWidth*/, int /*deviceHeight*/, int /*availableWidth*/, int /*availableHeight*/)
{
    // FIXME: Implement
}

JSRetainPtr<JSStringRef> LayoutTestController::pageProperty(const char* propertyName, int pageNumber) const
{
    // FIXME: Implement
    return 0;
}

JSRetainPtr<JSStringRef> LayoutTestController::pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft) const
{
    // FIXME: Implement
    return 0;
}

void LayoutTestController::setAsynchronousSpellCheckingEnabled(bool)
{
    // FIXME: Implement this.
}

bool LayoutTestController::findString(JSContextRef context, JSStringRef target, JSObjectRef optionsArray)
{
    // FIXME: Implement
    return false;
}

void LayoutTestController::setSerializeHTTPLoads(bool)
{
    // FIXME: Implement.
}

void LayoutTestController::setMinimumTimerInterval(double interval) {

}

void LayoutTestController::syncLocalStorage()
{
    // FIXME: Implement.
}

void LayoutTestController::observeStorageTrackerNotifications(unsigned number)
{
    // FIXME: Implement.
}

void LayoutTestController::deleteAllLocalStorage()
{
    // FIXME: Implement.
}

JSValueRef LayoutTestController::originsWithLocalStorage(JSContextRef context)
{
    // FIXME: Implement.
    return 0;
}

void LayoutTestController::deleteLocalStorageForOrigin(JSStringRef URL)
{
    // FIXME: Implement.
}

void LayoutTestController::setTextDirection(JSStringRef direction)
{
    // FIXME: Implement.
}

void LayoutTestController::allowRoundingHacks()
{
}

void LayoutTestController::addChromeInputField()
{
}

void LayoutTestController::removeChromeInputField()
{
}

void LayoutTestController::focusWebView()
{
}

void LayoutTestController::setBackingScaleFactor(double)
{
}

void LayoutTestController::simulateDesktopNotificationClick(JSStringRef title)
{
    // FIXME: Implement.
}

void LayoutTestController::resetPageVisibility()
{
    // FIXME: Implement this.
}

void LayoutTestController::setPageVisibility(const char*)
{
    // FIXME: Implement this.
}
