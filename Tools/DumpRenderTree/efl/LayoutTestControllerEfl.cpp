/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Michael Alonzo <jmalonzo@gmail.com>
 * Copyright (C) 2009,2011 Collabora Ltd.
 * Copyright (C) 2010 Joone Hur <joone@kldp.org>
 * Copyright (C) 2011 ProFUSION Embedded Systems
 * Copyright (C) 2011 Samsung Electronics
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
#include "DumpRenderTreeChrome.h"
#include "JSStringUtils.h"
#include "NotImplemented.h"
#include "WebCoreSupport/DumpRenderTreeSupportEfl.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include "ewk_private.h"
#include <EWebKit.h>
#include <Ecore_File.h>
#include <JavaScriptCore/JSRetainPtr.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/OpaqueJSString.h>
#include <JavaScriptCore/wtf/text/WTFString.h>
#include <KURL.h>
#include <editing/FindOptions.h>
#include <stdio.h>

LayoutTestController::~LayoutTestController()
{
}

void LayoutTestController::addDisallowedURL(JSStringRef)
{
    notImplemented();
}

void LayoutTestController::clearBackForwardList()
{
    notImplemented();
}

JSStringRef LayoutTestController::copyDecodedHostName(JSStringRef)
{
    notImplemented();
    return 0;
}

JSStringRef LayoutTestController::copyEncodedHostName(JSStringRef)
{
    notImplemented();
    return 0;
}

void LayoutTestController::dispatchPendingLoadRequests()
{
    // FIXME: Implement for testing fix for 6727495
    notImplemented();
}

void LayoutTestController::display()
{
    displayWebView();
}

JSRetainPtr<JSStringRef> LayoutTestController::counterValueForElementById(JSStringRef id)
{
    const Evas_Object* mainFrame = browser->mainFrame();
    const String counterValue(DumpRenderTreeSupportEfl::counterValueByElementId(mainFrame, id->ustring().utf8().data()));
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithUTF8CString(counterValue.utf8().data()));
}

void LayoutTestController::keepWebHistory()
{
    notImplemented();
}

JSValueRef LayoutTestController::computedStyleIncludingVisitedInfo(JSContextRef context, JSValueRef)
{
    notImplemented();
    return JSValueMakeUndefined(context);
}

JSValueRef LayoutTestController::nodesFromRect(JSContextRef context, JSValueRef, int, int, unsigned, unsigned, unsigned, unsigned, bool)
{
    notImplemented();
    return JSValueMakeUndefined(context);
}

JSRetainPtr<JSStringRef> LayoutTestController::layerTreeAsText() const
{
    notImplemented();
    return JSRetainPtr<JSStringRef>(Adopt, JSStringCreateWithUTF8CString(""));
}

int LayoutTestController::pageNumberForElementById(JSStringRef id, float pageWidth, float pageHeight)
{
    return DumpRenderTreeSupportEfl::numberOfPagesForElementId(browser->mainFrame(), id->ustring().utf8().data(), pageWidth, pageHeight);
}

int LayoutTestController::numberOfPages(float pageWidth, float pageHeight)
{
    return DumpRenderTreeSupportEfl::numberOfPages(browser->mainFrame(), pageWidth, pageHeight);
}

JSRetainPtr<JSStringRef> LayoutTestController::pageProperty(const char*, int) const
{
    notImplemented();
    return 0;
}

bool LayoutTestController::isPageBoxVisible(int) const
{
    notImplemented();
    return false;
}

JSRetainPtr<JSStringRef> LayoutTestController::pageSizeAndMarginsInPixels(int, int, int, int, int, int, int) const
{
    notImplemented();
    return 0;
}

size_t LayoutTestController::webHistoryItemCount()
{
    const Ewk_History* history = ewk_view_history_get(browser->mainView());
    if (!history)
        return -1;

    return ewk_history_back_list_length(history) + ewk_history_forward_list_length(history);
}

unsigned LayoutTestController::workerThreadCount() const
{
    return DumpRenderTreeSupportEfl::workerThreadCount();
}

void LayoutTestController::notifyDone()
{
    if (m_waitToDump && !topLoadingFrame && !WorkQueue::shared()->count())
        dump();
    m_waitToDump = false;
    waitForPolicy = false;
}

JSStringRef LayoutTestController::pathToLocalResource(JSContextRef context, JSStringRef url)
{
    // Function introduced in r28690. This may need special-casing on Windows.
    return JSStringRetain(url); // Do nothing on Unix.
}

void LayoutTestController::queueLoad(JSStringRef url, JSStringRef target)
{
    WebCore::KURL baseURL(WebCore::KURL(), String::fromUTF8(ewk_frame_uri_get(browser->mainFrame())));
    WebCore::KURL absoluteURL(baseURL, WTF::String(url->characters(), url->length()));

    JSRetainPtr<JSStringRef> jsAbsoluteURL(
        Adopt, JSStringCreateWithUTF8CString(absoluteURL.string().utf8().data()));

    WorkQueue::shared()->queue(new LoadItem(jsAbsoluteURL.get(), target));
}

void LayoutTestController::setAcceptsEditing(bool acceptsEditing)
{
    ewk_view_editable_set(browser->mainView(), acceptsEditing);
}

void LayoutTestController::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
    ewk_cookies_policy_set(alwaysAcceptCookies ? EWK_COOKIE_JAR_ACCEPT_ALWAYS : EWK_COOKIE_JAR_ACCEPT_NEVER);
}

void LayoutTestController::setCustomPolicyDelegate(bool, bool)
{
    notImplemented();
}

void LayoutTestController::waitForPolicyDelegate()
{
    waitForPolicy = true;
    setWaitToDump(true);
}

void LayoutTestController::setScrollbarPolicy(JSStringRef, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::addOriginAccessWhitelistEntry(JSStringRef, JSStringRef, JSStringRef, bool)
{
    notImplemented();
}

void LayoutTestController::removeOriginAccessWhitelistEntry(JSStringRef, JSStringRef, JSStringRef, bool)
{
    notImplemented();
}

void LayoutTestController::setMainFrameIsFirstResponder(bool)
{
    notImplemented();
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool)
{
    notImplemented();
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool)
{
    notImplemented();
}

static CString gUserStyleSheet;
static bool gUserStyleSheetEnabled = true;

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
{
    gUserStyleSheetEnabled = flag;
    ewk_view_setting_user_stylesheet_set(browser->mainView(), flag ? gUserStyleSheet.data() : 0);
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef path)
{
    gUserStyleSheet = path->ustring().utf8();

    if (gUserStyleSheetEnabled)
        setUserStyleSheetEnabled(true);
}

void LayoutTestController::setValueForUser(JSContextRef, JSValueRef, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::setViewModeMediaFeature(JSStringRef)
{
    notImplemented();
}

void LayoutTestController::setWindowIsKey(bool)
{
    notImplemented();
}

void LayoutTestController::setSmartInsertDeleteEnabled(bool)
{
    notImplemented();
}

static Eina_Bool waitToDumpWatchdogFired(void*)
{
    waitToDumpWatchdog = 0;
    gLayoutTestController->waitToDumpWatchdogTimerFired();
    return ECORE_CALLBACK_CANCEL;
}

void LayoutTestController::setWaitToDump(bool waitUntilDone)
{
    static const double timeoutSeconds = 30;

    m_waitToDump = waitUntilDone;
    if (m_waitToDump && !waitToDumpWatchdog)
        waitToDumpWatchdog = ecore_timer_add(timeoutSeconds, waitToDumpWatchdogFired, 0);
}

int LayoutTestController::windowCount()
{
    return browser->extraViews().size() + 1; // + 1 for the main view.
}

void LayoutTestController::setPrivateBrowsingEnabled(bool flag)
{
    ewk_view_setting_private_browsing_set(browser->mainView(), flag);
}

void LayoutTestController::setJavaScriptCanAccessClipboard(bool)
{
    notImplemented();
}

void LayoutTestController::setXSSAuditorEnabled(bool)
{
    notImplemented();
}

void LayoutTestController::setFrameFlatteningEnabled(bool flag)
{
    ewk_view_setting_enable_frame_flattening_set(browser->mainView(), flag);
}

void LayoutTestController::setSpatialNavigationEnabled(bool flag)
{
    ewk_view_setting_spatial_navigation_set(browser->mainView(), flag);
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(bool)
{
    notImplemented();
}

void LayoutTestController::setAllowFileAccessFromFileURLs(bool)
{
    notImplemented();
}

void LayoutTestController::setAuthorAndUserStylesEnabled(bool)
{
    notImplemented();
}

void LayoutTestController::setAutofilled(JSContextRef, JSValueRef, bool)
{
    notImplemented();
}

void LayoutTestController::disableImageLoading()
{
    // FIXME: Implement for testing fix for https://bugs.webkit.org/show_bug.cgi?id=27896
    // Also need to make sure image loading is re-enabled for each new test.
    notImplemented();
}

void LayoutTestController::setMockDeviceOrientation(bool, double, bool, double, bool, double)
{
    // FIXME: Implement for DeviceOrientation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=30335.
    notImplemented();
}

void LayoutTestController::setMockGeolocationPosition(double, double, double)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
    notImplemented();
}

void LayoutTestController::setMockGeolocationError(int, JSStringRef)
{
    // FIXME: Implement for Geolocation layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=28264.
    notImplemented();
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

void LayoutTestController::addMockSpeechInputResult(JSStringRef, double, JSStringRef)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
    notImplemented();
}

void LayoutTestController::startSpeechInput(JSContextRef inputElement)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
    notImplemented();
}

void LayoutTestController::setIconDatabaseEnabled(bool enabled)
{
    ewk_settings_icon_database_path_set(0);

    if (!enabled)
        return;

    String databasePath;
    const char* tempDir = getenv("TMPDIR");

    if (tempDir)
        databasePath = String::fromUTF8(tempDir);
    else if (tempDir = getenv("TEMP"))
        databasePath = String::fromUTF8(tempDir);
    else
        databasePath = String::fromUTF8("/tmp");

    databasePath.append("/DumpRenderTree/IconDatabase");

    if (ecore_file_mkpath(databasePath.utf8().data()))
        ewk_settings_icon_database_path_set(databasePath.utf8().data());
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool)
{
    notImplemented();
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(bool)
{
    notImplemented();
}

void LayoutTestController::setPopupBlockingEnabled(bool)
{
    notImplemented();
}

void LayoutTestController::setPluginsEnabled(bool flag)
{
    ewk_view_setting_enable_plugins_set(browser->mainView(), flag);
}

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(JSStringRef)
{
    notImplemented();
    return false;
}

void LayoutTestController::execCommand(JSStringRef, JSStringRef)
{
    notImplemented();
}

bool LayoutTestController::findString(JSContextRef context, JSStringRef target, JSObjectRef optionsArray)
{
    JSRetainPtr<JSStringRef> lengthPropertyName(Adopt, JSStringCreateWithUTF8CString("length"));
    JSValueRef lengthValue = JSObjectGetProperty(context, optionsArray, lengthPropertyName.get(), 0);
    if (!JSValueIsNumber(context, lengthValue))
        return false;

    WebCore::FindOptions options = 0;

    const size_t length = static_cast<size_t>(JSValueToNumber(context, lengthValue, 0));
    for (size_t i = 0; i < length; ++i) {
        JSValueRef value = JSObjectGetPropertyAtIndex(context, optionsArray, i, 0);
        if (!JSValueIsString(context, value))
            continue;

        JSRetainPtr<JSStringRef> optionName(Adopt, JSValueToStringCopy(context, value, 0));

        if (equals(optionName, "CaseInsensitive"))
            options |= WebCore::CaseInsensitive;
        else if (equals(optionName, "AtWordStarts"))
            options |= WebCore::AtWordStarts;
        else if (equals(optionName, "TreatMedialCapitalAsWordStart"))
            options |= WebCore::TreatMedialCapitalAsWordStart;
        else if (equals(optionName, "Backwards"))
            options |= WebCore::Backwards;
        else if (equals(optionName, "WrapAround"))
            options |= WebCore::WrapAround;
        else if (equals(optionName, "StartInSelection"))
            options |= WebCore::StartInSelection;
    }

    return DumpRenderTreeSupportEfl::findString(browser->mainView(), target->ustring().utf8().data(), options);
}

bool LayoutTestController::isCommandEnabled(JSStringRef name)
{
    return false;
}

void LayoutTestController::setCacheModel(int)
{
    notImplemented();
}

void LayoutTestController::setPersistentUserStyleSheetLocation(JSStringRef)
{
    notImplemented();
}

void LayoutTestController::clearPersistentUserStyleSheet()
{
    notImplemented();
}

void LayoutTestController::clearAllApplicationCaches()
{
    ewk_settings_application_cache_clear();
}

void LayoutTestController::setApplicationCacheOriginQuota(unsigned long long)
{
    // FIXME: Implement to support application cache quotas.
    notImplemented();
}

void LayoutTestController::clearApplicationCacheForOrigin(OpaqueJSString*)
{
    // FIXME: Implement to support deleting all application caches for an origin.
    notImplemented();
}

long long LayoutTestController::localStorageDiskUsageForOrigin(JSStringRef)
{
    // FIXME: Implement to support getting disk usage in bytes for an origin.
    notImplemented();
    return 0;
}

JSValueRef LayoutTestController::originsWithApplicationCache(JSContextRef context)
{
    // FIXME: Implement to get origins that contain application caches.
    notImplemented();
    return JSValueMakeUndefined(context);
}

long long LayoutTestController::applicationCacheDiskUsageForOrigin(JSStringRef)
{
    notImplemented();
    return 0;
}

void LayoutTestController::clearAllDatabases()
{
    ewk_settings_web_database_clear();
}

void LayoutTestController::setDatabaseQuota(unsigned long long quota)
{
    ewk_settings_web_database_default_quota_set(quota);
}

JSValueRef LayoutTestController::originsWithLocalStorage(JSContextRef context)
{
    notImplemented();
    return JSValueMakeUndefined(context);
}

void LayoutTestController::deleteAllLocalStorage()
{
    notImplemented();
}

void LayoutTestController::deleteLocalStorageForOrigin(JSStringRef)
{
    notImplemented();
}

void LayoutTestController::observeStorageTrackerNotifications(unsigned)
{
    notImplemented();
}

void LayoutTestController::syncLocalStorage()
{
    notImplemented();
}

void LayoutTestController::setDomainRelaxationForbiddenForURLScheme(bool, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::goBack()
{
    // FIXME: implement to enable loader/navigation-while-deferring-loads.html
    notImplemented();
}

void LayoutTestController::setDefersLoading(bool)
{
    // FIXME: implement to enable loader/navigation-while-deferring-loads.html
    notImplemented();
}

void LayoutTestController::setAppCacheMaximumSize(unsigned long long size)
{
    ewk_settings_application_cache_max_quota_set(size);
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{
    return DumpRenderTreeSupportEfl::pauseAnimation(browser->mainFrame(), animationName->ustring().utf8().data(), elementId->ustring().utf8().data(), time);
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId)
{
    return DumpRenderTreeSupportEfl::pauseTransition(browser->mainFrame(), propertyName->ustring().utf8().data(), elementId->ustring().utf8().data(), time);
}

bool LayoutTestController::sampleSVGAnimationForElementAtTime(JSStringRef animationId, double time, JSStringRef elementId)
{
    return DumpRenderTreeSupportEfl::pauseSVGAnimation(browser->mainFrame(), animationId->ustring().utf8().data(), elementId->ustring().utf8().data(), time);
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    return DumpRenderTreeSupportEfl::activeAnimationsCount(browser->mainFrame());
}

void LayoutTestController::suspendAnimations() const
{
    DumpRenderTreeSupportEfl::suspendAnimations(browser->mainFrame());
}

void LayoutTestController::resumeAnimations() const
{
    DumpRenderTreeSupportEfl::resumeAnimations(browser->mainFrame());
}

void LayoutTestController::overridePreference(JSStringRef, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::addUserScript(JSStringRef, bool, bool)
{
    notImplemented();
}

void LayoutTestController::addUserStyleSheet(JSStringRef, bool)
{
    // FIXME: needs more investigation why userscripts/user-style-top-frame-only.html fails when allFrames is false.
    notImplemented();
}

void LayoutTestController::setDeveloperExtrasEnabled(bool enabled)
{
    ewk_view_setting_enable_developer_extras_set(browser->mainView(), enabled);
}

void LayoutTestController::setAsynchronousSpellCheckingEnabled(bool)
{
    notImplemented();
}

void LayoutTestController::showWebInspector()
{
    notImplemented();
}

void LayoutTestController::closeWebInspector()
{
    notImplemented();
}

void LayoutTestController::evaluateInWebInspector(long, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::evaluateScriptInIsolatedWorld(unsigned, JSObjectRef, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::removeAllVisitedLinks()
{
    notImplemented();
}

bool LayoutTestController::callShouldCloseOnWebView()
{
    notImplemented();
    return false;
}

void LayoutTestController::apiTestNewWindowDataLoadBaseURL(JSStringRef, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::apiTestGoToCurrentBackForwardItem()
{
    notImplemented();
}

void LayoutTestController::setWebViewEditable(bool)
{
    ewk_frame_editable_set(browser->mainFrame(), EINA_TRUE);
}

JSRetainPtr<JSStringRef> LayoutTestController::markerTextForListItem(JSContextRef, JSValueRef) const
{
    notImplemented();
    return 0;
}

void LayoutTestController::authenticateSession(JSStringRef, JSStringRef, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::setEditingBehavior(const char*)
{
    notImplemented();
}

void LayoutTestController::abortModal()
{
    notImplemented();
}

bool LayoutTestController::hasSpellingMarker(int, int)
{
    notImplemented();
    return false;
}

bool LayoutTestController::hasGrammarMarker(int, int)
{
    notImplemented();
    return false;
}

void LayoutTestController::dumpConfigurationForViewport(int, int, int, int, int)
{
    notImplemented();
}

void LayoutTestController::setSerializeHTTPLoads(bool)
{
    // FIXME: Implement if needed for https://bugs.webkit.org/show_bug.cgi?id=50758.
    notImplemented();
}

void LayoutTestController::setMinimumTimerInterval(double)
{
    notImplemented();
}

void LayoutTestController::setTextDirection(JSStringRef)
{
    notImplemented();
}

void LayoutTestController::allowRoundingHacks()
{
    notImplemented();
}

void LayoutTestController::addChromeInputField()
{
    notImplemented();
}

void LayoutTestController::removeChromeInputField()
{
    notImplemented();
}

void LayoutTestController::focusWebView()
{
    notImplemented();
}

void LayoutTestController::setBackingScaleFactor(double)
{
    notImplemented();
}

void LayoutTestController::simulateDesktopNotificationClick(JSStringRef title)
{
}
