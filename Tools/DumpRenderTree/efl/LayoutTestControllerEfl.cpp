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
#include <KURL.h>
#include <editing/FindOptions.h>
#include <stdio.h>
#include <wtf/text/WTFString.h>

LayoutTestController::~LayoutTestController()
{
}

void LayoutTestController::addDisallowedURL(JSStringRef)
{
    notImplemented();
}

void LayoutTestController::clearBackForwardList()
{
    Ewk_History* history = ewk_view_history_get(browser->mainView());
    if (!history)
        return;

    Ewk_History_Item* item = ewk_history_history_item_current_get(history);
    ewk_history_clear(history);
    ewk_history_history_item_add(history, item);
    ewk_history_history_item_set(history, item);
    ewk_history_item_free(item);
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

JSValueRef LayoutTestController::computedStyleIncludingVisitedInfo(JSContextRef context, JSValueRef value)
{
    return DumpRenderTreeSupportEfl::computedStyleIncludingVisitedInfo(context, value);
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

JSRetainPtr<JSStringRef> LayoutTestController::pageProperty(const char* propertyName, int pageNumber) const
{
    const String property = DumpRenderTreeSupportEfl::pageProperty(browser->mainFrame(), propertyName, pageNumber);
    if (property.isEmpty())
        return 0;

    JSRetainPtr<JSStringRef> propertyValue(Adopt, JSStringCreateWithUTF8CString(property.utf8().data()));
    return propertyValue;
}

JSRetainPtr<JSStringRef> LayoutTestController::pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft) const
{
    String pageSizeAndMargins = DumpRenderTreeSupportEfl::pageSizeAndMarginsInPixels(browser->mainFrame(), pageNumber, width, height, marginTop, marginRight, marginBottom, marginLeft);

    if (pageSizeAndMargins.isEmpty())
        return 0;

    JSRetainPtr<JSStringRef> returnValue(Adopt, JSStringCreateWithUTF8CString(pageSizeAndMargins.utf8().data()));
    return returnValue;
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
    WebCore::KURL absoluteURL(baseURL, WTF::String(url->ustring().impl()));

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

void LayoutTestController::addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef protocol, JSStringRef host, bool includeSubdomains)
{
    WebCore::KURL kurl;
    kurl.setProtocol(String(protocol->characters(), protocol->length()));
    kurl.setHost(String(host->characters(), host->length()));

    ewk_security_policy_whitelist_origin_add(sourceOrigin->ustring().utf8().data(), kurl.string().utf8().data(), includeSubdomains);
}

void LayoutTestController::removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef protocol, JSStringRef host, bool includeSubdomains)
{
    WebCore::KURL kurl;
    kurl.setProtocol(String(protocol->characters(), protocol->length()));
    kurl.setHost(String(host->characters(), host->length()));

    ewk_security_policy_whitelist_origin_del(sourceOrigin->ustring().utf8().data(), kurl.string().utf8().data(), includeSubdomains);
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

void LayoutTestController::setValueForUser(JSContextRef context, JSValueRef nodeObject, JSStringRef value)
{
    DumpRenderTreeSupportEfl::setValueForUser(context, nodeObject, WTF::String(value->ustring().impl()));
}

void LayoutTestController::setViewModeMediaFeature(JSStringRef mode)
{
    Evas_Object* view = browser->mainView();
    if (!view)
        return;

    if (equals(mode, "windowed"))
        ewk_view_mode_set(view, EWK_VIEW_MODE_WINDOWED);
    else if (equals(mode, "floating"))
        ewk_view_mode_set(view, EWK_VIEW_MODE_FLOATING);
    else if (equals(mode, "fullscreen"))
        ewk_view_mode_set(view, EWK_VIEW_MODE_FULLSCREEN);
    else if (equals(mode, "maximized"))
        ewk_view_mode_set(view, EWK_VIEW_MODE_MAXIMIZED);
    else if (equals(mode, "minimized"))
        ewk_view_mode_set(view, EWK_VIEW_MODE_MINIMIZED);
}

void LayoutTestController::setWindowIsKey(bool)
{
    notImplemented();
}

void LayoutTestController::setSmartInsertDeleteEnabled(bool flag)
{
    DumpRenderTreeSupportEfl::setSmartInsertDeleteEnabled(browser->mainView(), flag);
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

void LayoutTestController::setJavaScriptCanAccessClipboard(bool flag)
{
    ewk_view_setting_scripts_can_access_clipboard_set(browser->mainView(), flag);
}

void LayoutTestController::setXSSAuditorEnabled(bool flag)
{
    ewk_view_setting_enable_xss_auditor_set(browser->mainView(), flag);
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

void LayoutTestController::setAuthorAndUserStylesEnabled(bool flag)
{
    DumpRenderTreeSupportEfl::setAuthorAndUserStylesEnabled(browser->mainView(), flag);
}

void LayoutTestController::setAutofilled(JSContextRef context, JSValueRef nodeObject, bool autofilled)
{
    DumpRenderTreeSupportEfl::setAutofilled(context, nodeObject, autofilled);
}

void LayoutTestController::disableImageLoading()
{
    ewk_view_setting_auto_load_images_set(browser->mainView(), EINA_FALSE);
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

void LayoutTestController::setMockSpeechInputDumpRect(bool)
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

void LayoutTestController::setJavaScriptProfilingEnabled(bool enabled)
{
    if (enabled)
        setDeveloperExtrasEnabled(enabled);

    DumpRenderTreeSupportEfl::setJavaScriptProfilingEnabled(browser->mainView(), enabled);
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(bool flag)
{
    DumpRenderTreeSupportEfl::setSelectTrailingWhitespaceEnabled(browser->mainView(), flag);
}

void LayoutTestController::setPopupBlockingEnabled(bool flag)
{
    ewk_view_setting_scripts_can_open_windows_set(browser->mainView(), !flag);
}

void LayoutTestController::setPluginsEnabled(bool flag)
{
    ewk_view_setting_enable_plugins_set(browser->mainView(), flag);
}

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(JSStringRef id)
{
    const String elementId(id->ustring().impl());
    const Evas_Object* mainFrame = browser->mainFrame();
    return DumpRenderTreeSupportEfl::elementDoesAutoCompleteForElementWithId(mainFrame, elementId);
}

void LayoutTestController::execCommand(JSStringRef name, JSStringRef value)
{
    DumpRenderTreeSupportEfl::executeCoreCommandByName(browser->mainView(), name->ustring().utf8().data(), value->ustring().utf8().data());
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

    return DumpRenderTreeSupportEfl::findString(browser->mainView(), WTF::String(target->ustring().impl()), options);
}

bool LayoutTestController::isCommandEnabled(JSStringRef name)
{
    return DumpRenderTreeSupportEfl::isCommandEnabled(browser->mainView(), name->ustring().utf8().data());
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

void LayoutTestController::setApplicationCacheOriginQuota(unsigned long long quota)
{
    Ewk_Security_Origin* origin = ewk_frame_security_origin_get(browser->mainFrame());
    ewk_security_origin_application_cache_quota_set(origin, quota);
    ewk_security_origin_free(origin);
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
    ewk_web_database_remove_all();
}

void LayoutTestController::setDatabaseQuota(unsigned long long quota)
{
    Ewk_Security_Origin* origin = ewk_frame_security_origin_get(browser->mainFrame());
    ewk_security_origin_web_database_quota_set(origin, quota);
    ewk_security_origin_free(origin);
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
    ewk_frame_back(browser->mainFrame());
}

void LayoutTestController::setDefersLoading(bool defers)
{
    DumpRenderTreeSupportEfl::setDefersLoading(browser->mainView(), defers);
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

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    return DumpRenderTreeSupportEfl::activeAnimationsCount(browser->mainFrame());
}

static inline bool toBool(JSStringRef value)
{
    return equals(value, "true") || equals(value, "1");
}

static inline int toInt(JSStringRef value)
{
    return atoi(value->ustring().utf8().data());
}

void LayoutTestController::overridePreference(JSStringRef key, JSStringRef value)
{
    if (equals(key, "WebKitJavaScriptEnabled"))
        ewk_view_setting_enable_scripts_set(browser->mainView(), toBool(value));
    else if (equals(key, "WebKitDefaultFontSize"))
        ewk_view_setting_font_default_size_set(browser->mainView(), toInt(value));
    else if (equals(key, "WebKitMinimumFontSize"))
        ewk_view_setting_font_minimum_size_set(browser->mainView(), toInt(value));
    else if (equals(key, "WebKitPluginsEnabled"))
        ewk_view_setting_enable_plugins_set(browser->mainView(), toBool(value));
    else if (equals(key, "WebKitWebGLEnabled"))
        ewk_view_setting_enable_webgl_set(browser->mainView(), toBool(value));
    else if (equals(key, "WebKitEnableCaretBrowsing"))
        ewk_view_setting_caret_browsing_set(browser->mainView(), toBool(value));
    else if (equals(key, "WebKitUsesPageCachePreferenceKey"))
        ewk_view_setting_page_cache_set(browser->mainView(), toBool(value));
    else if (equals(key, "WebKitHyperlinkAuditingEnabled"))
        ewk_view_setting_enable_hyperlink_auditing_set(browser->mainView(), toBool(value));
    else if (equals(key, "WebKitTabToLinksPreferenceKey"))
        ewk_view_setting_include_links_in_focus_chain_set(browser->mainView(), toBool(value));
    else if (equals(key, "WebKitLoadSiteIconsKey"))
        DumpRenderTreeSupportEfl::setLoadsSiteIconsIgnoringImageLoadingSetting(browser->mainView(), toBool(value));
    else
        fprintf(stderr, "LayoutTestController::overridePreference tried to override unknown preference '%s'.\n", value->ustring().utf8().data());
}

void LayoutTestController::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    DumpRenderTreeSupportEfl::addUserScript(browser->mainView(), String(source->ustring().impl()), runAtStart, allFrames);
}

void LayoutTestController::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    DumpRenderTreeSupportEfl::addUserStyleSheet(browser->mainView(), WTF::String(source->ustring().impl()), allFrames);
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

void LayoutTestController::evaluateScriptInIsolatedWorldAndReturnValue(unsigned, JSObjectRef, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    DumpRenderTreeSupportEfl::evaluateScriptInIsolatedWorld(browser->mainFrame(), worldID, globalObject, String(script->ustring().impl()));
}

void LayoutTestController::removeAllVisitedLinks()
{
    notImplemented();
}

bool LayoutTestController::callShouldCloseOnWebView()
{
    return DumpRenderTreeSupportEfl::callShouldCloseOnWebView(browser->mainFrame());
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

JSRetainPtr<JSStringRef> LayoutTestController::markerTextForListItem(JSContextRef context, JSValueRef nodeObject) const
{
    String markerTextChar = DumpRenderTreeSupportEfl::markerTextForListItem(context, nodeObject);
    if (markerTextChar.isEmpty())
        return 0;

    JSRetainPtr<JSStringRef> markerText(Adopt, JSStringCreateWithUTF8CString(markerTextChar.utf8().data()));
    return markerText;
}

void LayoutTestController::authenticateSession(JSStringRef, JSStringRef, JSStringRef)
{
    notImplemented();
}

void LayoutTestController::setEditingBehavior(const char* editingBehavior)
{
    DumpRenderTreeSupportEfl::setEditingBehavior(browser->mainView(), editingBehavior);
}

void LayoutTestController::abortModal()
{
    notImplemented();
}

void LayoutTestController::dumpConfigurationForViewport(int deviceDPI, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight)
{
    DumpRenderTreeSupportEfl::dumpConfigurationForViewport(browser->mainView(),
            deviceDPI,
            WebCore::IntSize(deviceWidth, deviceHeight),
            WebCore::IntSize(availableWidth, availableHeight));
}

void LayoutTestController::setSerializeHTTPLoads(bool serialize)
{
    DumpRenderTreeSupportEfl::setSerializeHTTPLoads(serialize);
}

void LayoutTestController::setMinimumTimerInterval(double minimumTimerInterval)
{
    ewk_view_setting_minimum_timer_interval_set(browser->mainView(), minimumTimerInterval);
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

void LayoutTestController::resetPageVisibility()
{
    ewk_view_visibility_state_set(browser->mainView(), EWK_PAGE_VISIBILITY_STATE_VISIBLE, true);
}

void LayoutTestController::setPageVisibility(const char* visibility)
{
    String newVisibility(visibility);
    if (newVisibility == "visible")
        ewk_view_visibility_state_set(browser->mainView(), EWK_PAGE_VISIBILITY_STATE_VISIBLE, false);
    else if (newVisibility == "hidden")
        ewk_view_visibility_state_set(browser->mainView(), EWK_PAGE_VISIBILITY_STATE_HIDDEN, false);
    else if (newVisibility == "prerender")
        ewk_view_visibility_state_set(browser->mainView(), EWK_PAGE_VISIBILITY_STATE_PRERENDER, false);
    else if (newVisibility == "preview")
        ewk_view_visibility_state_set(browser->mainView(), EWK_PAGE_VISIBILITY_STATE_PREVIEW, false);
}

void LayoutTestController::setAutomaticLinkDetectionEnabled(bool)
{
    notImplemented();
}
