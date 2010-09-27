/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Nuanti Ltd.
 * Copyright (C) 2009 Jan Michael Alonzo <jmalonzo@gmail.com>
 * Copyright (C) 2009 Collabora Ltd.
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
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <webkit/webkit.h>
#include <wtf/gobject/GOwnPtr.h>

extern "C" {
bool webkit_web_frame_pause_animation(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);
bool webkit_web_frame_pause_transition(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);
bool webkit_web_frame_pause_svg_animation(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);
unsigned int webkit_web_frame_number_of_active_animations(WebKitWebFrame* frame);
void webkit_web_frame_suspend_animations(WebKitWebFrame* frame);
void webkit_web_frame_resume_animations(WebKitWebFrame* frame);
void webkit_application_cache_set_maximum_size(unsigned long long size);
unsigned int webkit_worker_thread_count(void);
void webkit_white_list_access_from_origin(const gchar* sourceOrigin, const gchar* destinationProtocol, const gchar* destinationHost, bool allowDestinationSubdomains);
gchar* webkit_web_frame_counter_value_for_element_by_id(WebKitWebFrame* frame, const gchar* id);
int webkit_web_frame_page_number_for_element_by_id(WebKitWebFrame* frame, const gchar* id, float pageWidth, float pageHeight);
int webkit_web_frame_number_of_pages(WebKitWebFrame* frame, float pageWidth, float pageHeight);
void webkit_web_inspector_execute_script(WebKitWebInspector* inspector, long callId, const gchar* script);
gchar* webkit_web_frame_marker_text_for_list_item(WebKitWebFrame* frame, JSContextRef context, JSValueRef nodeObject);
void webkit_web_view_execute_core_command_by_name(WebKitWebView* webView, const gchar* name, const gchar* value);
gboolean webkit_web_view_is_command_enabled(WebKitWebView* webView, const gchar* name);
}

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
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebBackForwardList* list = webkit_web_view_get_back_forward_list(webView);
    WebKitWebHistoryItem* item = webkit_web_back_forward_list_get_current_item(list);
    g_object_ref(item);

    // We clear the history by setting the back/forward list's capacity to 0
    // then restoring it back and adding back the current item.
    gint limit = webkit_web_back_forward_list_get_limit(list);
    webkit_web_back_forward_list_set_limit(list, 0);
    webkit_web_back_forward_list_set_limit(list, limit);
    webkit_web_back_forward_list_add_item(list, item);
    webkit_web_back_forward_list_go_to_item(list, item);
    g_object_unref(item);
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
    displayWebView();
}

JSRetainPtr<JSStringRef> LayoutTestController::counterValueForElementById(JSStringRef id)
{
    gchar* idGChar = JSStringCopyUTF8CString(id);
    gchar* counterValueGChar = webkit_web_frame_counter_value_for_element_by_id(mainFrame, idGChar);
    g_free(idGChar);
    if (!counterValueGChar)
        return 0;
    JSRetainPtr<JSStringRef> counterValue(Adopt, JSStringCreateWithUTF8CString(counterValueGChar));
    return counterValue;
}

void LayoutTestController::keepWebHistory()
{
    // FIXME: implement
}

JSValueRef LayoutTestController::computedStyleIncludingVisitedInfo(JSContextRef context, JSValueRef value)
{
    // FIXME: Implement this.
    return JSValueMakeUndefined(context);
}

JSValueRef LayoutTestController::nodesFromRect(JSContextRef context, JSValueRef value, int , int, unsigned, unsigned, bool)
{
    // FIXME: Implement this.
    return JSValueMakeUndefined(context);
}

JSRetainPtr<JSStringRef> LayoutTestController::layerTreeAsText() const
{
    // FIXME: implement
    JSRetainPtr<JSStringRef> string(Adopt, JSStringCreateWithUTF8CString(""));
    return string;
}

int LayoutTestController::pageNumberForElementById(JSStringRef id, float pageWidth, float pageHeight)
{
    gchar* idGChar = JSStringCopyUTF8CString(id);
    int pageNumber = webkit_web_frame_page_number_for_element_by_id(mainFrame, idGChar, pageWidth, pageHeight);
    g_free(idGChar);
    return pageNumber;
}

int LayoutTestController::numberOfPages(float pageWidth, float pageHeight)
{
    return webkit_web_frame_number_of_pages(mainFrame, pageWidth, pageHeight);
}

JSRetainPtr<JSStringRef> LayoutTestController::pageProperty(const char* propertyName, int pageNumber) const
{
    // FIXME: implement
    return JSRetainPtr<JSStringRef>();
}

bool LayoutTestController::isPageBoxVisible(int pageNumber) const
{
    // FIXME: implement
    return false;
}

JSRetainPtr<JSStringRef> LayoutTestController::pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft) const
{
    // FIXME: implement
    return JSRetainPtr<JSStringRef>();
}

size_t LayoutTestController::webHistoryItemCount()
{
    // FIXME: implement
    return 0;
}

unsigned LayoutTestController::workerThreadCount() const
{
    return webkit_worker_thread_count();
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
    gchar* relativeURL = JSStringCopyUTF8CString(url);
    SoupURI* baseURI = soup_uri_new(webkit_web_frame_get_uri(mainFrame));

    SoupURI* absoluteURI = soup_uri_new_with_base(baseURI, relativeURL);
    soup_uri_free(baseURI);
    g_free(relativeURL);

    gchar* absoluteCString;
    if (absoluteURI) {
        absoluteCString = soup_uri_to_string(absoluteURI, FALSE);
        soup_uri_free(absoluteURI);
    } else
        absoluteCString = JSStringCopyUTF8CString(url);

    JSRetainPtr<JSStringRef> absoluteURL(Adopt, JSStringCreateWithUTF8CString(absoluteCString));
    g_free(absoluteCString);

    WorkQueue::shared()->queue(new LoadItem(absoluteURL.get(), target));
}

void LayoutTestController::setAcceptsEditing(bool acceptsEditing)
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    webkit_web_view_set_editable(webView, acceptsEditing);
}

void LayoutTestController::setAlwaysAcceptCookies(bool alwaysAcceptCookies)
{
#ifdef HAVE_LIBSOUP_2_29_90
    SoupSession* session = webkit_get_default_session();
    SoupCookieJar* jar = reinterpret_cast<SoupCookieJar*>(soup_session_get_feature(session, SOUP_TYPE_COOKIE_JAR));

    /* If the jar was not created - we create it on demand, i.e, just
       in case we have HTTP requests - then we must create it here in
       order to set the proper accept policy */
    if (!jar) {
        jar = soup_cookie_jar_new();
        soup_session_add_feature(session, SOUP_SESSION_FEATURE(jar));
        g_object_unref(jar);
    }

    SoupCookieJarAcceptPolicy policy;

    if (alwaysAcceptCookies)
        policy = SOUP_COOKIE_JAR_ACCEPT_ALWAYS;
    else
        policy = SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY;

    g_object_set(G_OBJECT(jar), SOUP_COOKIE_JAR_ACCEPT_POLICY, policy, NULL);
#endif
}

void LayoutTestController::setCustomPolicyDelegate(bool setDelegate, bool permissive)
{
    // FIXME: implement
}

void LayoutTestController::waitForPolicyDelegate()
{
    waitForPolicy = true;
    setWaitToDump(true);
}

void LayoutTestController::setScrollbarPolicy(JSStringRef orientation, JSStringRef policy)
{
    // FIXME: implement
}

void LayoutTestController::addOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef protocol, JSStringRef host, bool includeSubdomains)
{
    gchar* sourceOriginGChar = JSStringCopyUTF8CString(sourceOrigin);
    gchar* protocolGChar = JSStringCopyUTF8CString(protocol);
    gchar* hostGChar = JSStringCopyUTF8CString(host);
    webkit_white_list_access_from_origin(sourceOriginGChar, protocolGChar, hostGChar, includeSubdomains);
    g_free(sourceOriginGChar);
    g_free(protocolGChar);
    g_free(hostGChar);
}

void LayoutTestController::removeOriginAccessWhitelistEntry(JSStringRef sourceOrigin, JSStringRef protocol, JSStringRef host, bool includeSubdomains)
{
    // FIXME: implement
}

void LayoutTestController::setMainFrameIsFirstResponder(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setTabKeyCyclesThroughElements(bool cycles)
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebSettings* settings = webkit_web_view_get_settings(webView);
    g_object_set(G_OBJECT(settings), "tab-key-cycles-through-elements", cycles, NULL);
}

void LayoutTestController::setTimelineProfilingEnabled(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebInspector* inspector = webkit_web_view_get_inspector(view);
    g_object_set(G_OBJECT(inspector), "timeline-profiling-enabled", flag, NULL);
}

void LayoutTestController::setUseDashboardCompatibilityMode(bool flag)
{
    // FIXME: implement
}

static gchar* userStyleSheet = NULL;
static gboolean userStyleSheetEnabled = TRUE;

void LayoutTestController::setUserStyleSheetEnabled(bool flag)
{
    userStyleSheetEnabled = flag;

    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebSettings* settings = webkit_web_view_get_settings(webView);
    if (flag && userStyleSheet)
        g_object_set(G_OBJECT(settings), "user-stylesheet-uri", userStyleSheet, NULL);
    else
        g_object_set(G_OBJECT(settings), "user-stylesheet-uri", "", NULL);
}

void LayoutTestController::setUserStyleSheetLocation(JSStringRef path)
{
    g_free(userStyleSheet);
    userStyleSheet = JSStringCopyUTF8CString(path);
    if (userStyleSheetEnabled)
        setUserStyleSheetEnabled(true);
}

void LayoutTestController::setViewModeMediaFeature(JSStringRef mode)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    char* viewMode = JSStringCopyUTF8CString(mode);

    if (!g_strcmp0(viewMode, "windowed"))
        webkit_web_view_set_view_mode(view, WEBKIT_WEB_VIEW_VIEW_MODE_WINDOWED);
    else if (!g_strcmp0(viewMode, "floating"))
        webkit_web_view_set_view_mode(view, WEBKIT_WEB_VIEW_VIEW_MODE_FLOATING);
    else if (!g_strcmp0(viewMode, "fullscreen"))
        webkit_web_view_set_view_mode(view, WEBKIT_WEB_VIEW_VIEW_MODE_FULLSCREEN);
    else if (!g_strcmp0(viewMode, "maximized"))
        webkit_web_view_set_view_mode(view, WEBKIT_WEB_VIEW_VIEW_MODE_MAXIMIZED);
    else if (!g_strcmp0(viewMode, "minimized"))
        webkit_web_view_set_view_mode(view, WEBKIT_WEB_VIEW_VIEW_MODE_MINIMIZED);

    g_free(viewMode);
}

void LayoutTestController::setWindowIsKey(bool windowIsKey)
{
    // FIXME: implement
}

void LayoutTestController::setSmartInsertDeleteEnabled(bool flag)
{
    // FIXME: implement
}

static gboolean waitToDumpWatchdogFired(void*)
{
    waitToDumpWatchdog = 0;
    gLayoutTestController->waitToDumpWatchdogTimerFired();
    return FALSE;
}

void LayoutTestController::setWaitToDump(bool waitUntilDone)
{
    static const int timeoutSeconds = 30;

    m_waitToDump = waitUntilDone;
    if (m_waitToDump && !waitToDumpWatchdog)
        waitToDumpWatchdog = g_timeout_add_seconds(timeoutSeconds, waitToDumpWatchdogFired, 0);
}

int LayoutTestController::windowCount()
{
    // +1 -> including the main view
    return g_slist_length(webViewList) + 1;
}

void LayoutTestController::setPrivateBrowsingEnabled(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "enable-private-browsing", flag, NULL);
}

void LayoutTestController::setJavaScriptCanAccessClipboard(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "javascript-can-access-clipboard", flag, NULL);
}

void LayoutTestController::setXSSAuditorEnabled(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "enable-xss-auditor", flag, NULL);
}

void LayoutTestController::setFrameFlatteningEnabled(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setSpatialNavigationEnabled(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "enable-spatial-navigation", flag, NULL);
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "enable-universal-access-from-file-uris", flag, NULL);
}

void LayoutTestController::setAllowFileAccessFromFileURLs(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "enable-file-access-from-file-uris", flag, NULL);
}

void LayoutTestController::setAuthorAndUserStylesEnabled(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::disableImageLoading()
{
    // FIXME: Implement for testing fix for https://bugs.webkit.org/show_bug.cgi?id=27896
    // Also need to make sure image loading is re-enabled for each new test.
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

void LayoutTestController::setMockSpeechInputResult(JSStringRef result)
{
    // FIXME: Implement for speech input layout tests.
    // See https://bugs.webkit.org/show_bug.cgi?id=39485.
}

void LayoutTestController::setIconDatabaseEnabled(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    setDeveloperExtrasEnabled(flag);

    WebKitWebInspector* inspector = webkit_web_view_get_inspector(view);
    g_object_set(G_OBJECT(inspector), "javascript-profiling-enabled", flag, NULL);
}

void LayoutTestController::setSelectTrailingWhitespaceEnabled(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setPopupBlockingEnabled(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "javascript-can-open-windows-automatically", !flag, NULL);

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
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    gchar* cName = JSStringCopyUTF8CString(name);
    gchar* cValue = JSStringCopyUTF8CString(value);
    webkit_web_view_execute_core_command_by_name(view, cName, cValue);
    g_free(cName);
    g_free(cValue);
}

bool LayoutTestController::isCommandEnabled(JSStringRef name)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    gchar* cName = JSStringCopyUTF8CString(name);
    gboolean result = webkit_web_view_is_command_enabled(view, cName);
    g_free(cName);
    return result;
}

void LayoutTestController::setCacheModel(int)
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
    // FIXME: implement to support Application Cache quotas.
}

void LayoutTestController::setApplicationCacheOriginQuota(unsigned long long quota)
{
    // FIXME: implement to support Application Cache quotas.
}

void LayoutTestController::clearAllDatabases()
{
    webkit_remove_all_web_databases();
}
 
void LayoutTestController::setDatabaseQuota(unsigned long long quota)
{
    WebKitSecurityOrigin* origin = webkit_web_frame_get_security_origin(mainFrame);
    webkit_security_origin_set_web_database_quota(origin, quota);
}

void LayoutTestController::setDomainRelaxationForbiddenForURLScheme(bool, JSStringRef)
{
    // FIXME: implement
}

void LayoutTestController::setAppCacheMaximumSize(unsigned long long size)
{
    webkit_application_cache_set_maximum_size(size);
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{    
    gchar* name = JSStringCopyUTF8CString(animationName);
    gchar* element = JSStringCopyUTF8CString(elementId);
    bool returnValue = webkit_web_frame_pause_animation(mainFrame, name, time, element);
    g_free(name);
    g_free(element);
    return returnValue;
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId)
{    
    gchar* name = JSStringCopyUTF8CString(propertyName);
    gchar* element = JSStringCopyUTF8CString(elementId);
    bool returnValue = webkit_web_frame_pause_transition(mainFrame, name, time, element);
    g_free(name);
    g_free(element);
    return returnValue;
}

bool LayoutTestController::sampleSVGAnimationForElementAtTime(JSStringRef animationId, double time, JSStringRef elementId)
{    
    gchar* name = JSStringCopyUTF8CString(animationId);
    gchar* element = JSStringCopyUTF8CString(elementId);
    bool returnValue = webkit_web_frame_pause_svg_animation(mainFrame, name, time, element);
    g_free(name);
    g_free(element);
    return returnValue;
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    return webkit_web_frame_number_of_active_animations(mainFrame);
}

void LayoutTestController::suspendAnimations() const
{
    webkit_web_frame_suspend_animations(mainFrame);
}

void LayoutTestController::resumeAnimations() const
{
    webkit_web_frame_resume_animations(mainFrame);
}

void LayoutTestController::overridePreference(JSStringRef key, JSStringRef value)
{
    GOwnPtr<gchar> originalName(JSStringCopyUTF8CString(key));
    GOwnPtr<gchar> valueAsString(JSStringCopyUTF8CString(value));

    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    // This transformation could be handled by a hash table (and it once was), but
    // having it prominent, makes it easier for people from other ports to keep the
    // list up to date.
    const gchar* propertyName = 0;
    if (g_str_equal(originalName.get(), "WebKitJavaScriptEnabled"))
        propertyName = "enable-scripts";
    else if (g_str_equal(originalName.get(), "WebKitDefaultFontSize"))
        propertyName = "default-font-size";
    else if (g_str_equal(originalName.get(), "WebKitEnableCaretBrowsing"))
        propertyName = "enable-caret-browsing";
    else if (g_str_equal(originalName.get(), "WebKitUsesPageCachePreferenceKey"))
        propertyName = "enable-page-cache";
    else if (g_str_equal(originalName.get(), "WebKitPluginsEnabled"))
         propertyName = "enable-plugins";
    else if (g_str_equal(originalName.get(), "WebKitHyperlinkAuditingEnabled"))
         propertyName = "enable-hyperlink-auditing";
    else {
        fprintf(stderr, "LayoutTestController::overridePreference tried to override "
                "unknown preference '%s'.\n", originalName.get());
        return;
    }

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    GParamSpec* pspec = g_object_class_find_property(G_OBJECT_CLASS(
        WEBKIT_WEB_SETTINGS_GET_CLASS(settings)), propertyName);
    GValue currentPropertyValue = { 0, { { 0 } } };
    g_value_init(&currentPropertyValue, pspec->value_type);

    if (G_VALUE_HOLDS_STRING(&currentPropertyValue))
        g_object_set(settings, propertyName, valueAsString.get(), NULL);
    else if (G_VALUE_HOLDS_BOOLEAN(&currentPropertyValue))
        g_object_set(G_OBJECT(settings), propertyName, !g_ascii_strcasecmp(valueAsString.get(), "true")
                        || !g_ascii_strcasecmp(valueAsString.get(), "1"), NULL);
    else if (G_VALUE_HOLDS_INT(&currentPropertyValue))
        g_object_set(G_OBJECT(settings), propertyName, atoi(valueAsString.get()), NULL);
    else if (G_VALUE_HOLDS_FLOAT(&currentPropertyValue)) {
        gfloat newValue = g_ascii_strtod(valueAsString.get(), 0);
        g_object_set(G_OBJECT(settings), propertyName, newValue, NULL);
    } else
        fprintf(stderr, "LayoutTestController::overridePreference failed to override "
                "preference '%s'.\n", originalName.get());
}

void LayoutTestController::addUserScript(JSStringRef source, bool runAtStart, bool allFrames)
{
    printf("LayoutTestController::addUserScript not implemented.\n");
}

void LayoutTestController::addUserStyleSheet(JSStringRef source, bool allFrames)
{
    printf("LayoutTestController::addUserStyleSheet not implemented.\n");
}

void LayoutTestController::setDeveloperExtrasEnabled(bool enabled)
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebSettings* webSettings = webkit_web_view_get_settings(webView);

    g_object_set(webSettings, "enable-developer-extras", enabled, NULL);
}

void LayoutTestController::showWebInspector()
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebInspector* inspector = webkit_web_view_get_inspector(webView);

    webkit_web_inspector_show(inspector);
}

void LayoutTestController::closeWebInspector()
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebInspector* inspector = webkit_web_view_get_inspector(webView);

    webkit_web_inspector_close(inspector);
}

void LayoutTestController::evaluateInWebInspector(long callId, JSStringRef script)
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebInspector* inspector = webkit_web_view_get_inspector(webView);
    char* scriptString = JSStringCopyUTF8CString(script);

    webkit_web_inspector_execute_script(inspector, callId, scriptString);
    g_free(scriptString);
}

void LayoutTestController::evaluateScriptInIsolatedWorld(unsigned worldID, JSObjectRef globalObject, JSStringRef script)
{
    // FIXME: Implement this.
}

void LayoutTestController::removeAllVisitedLinks()
{
    // FIXME: Implement this.
}

bool LayoutTestController::callShouldCloseOnWebView()
{
    // FIXME: Implement for testing fix for https://bugs.webkit.org/show_bug.cgi?id=27481
    return false;
}

void LayoutTestController::apiTestNewWindowDataLoadBaseURL(JSStringRef utf8Data, JSStringRef baseURL)
{

}

void LayoutTestController::apiTestGoToCurrentBackForwardItem()
{

}

void LayoutTestController::setWebViewEditable(bool)
{
}

JSRetainPtr<JSStringRef> LayoutTestController::markerTextForListItem(JSContextRef context, JSValueRef nodeObject) const
{
    gchar* markerTextGChar = webkit_web_frame_marker_text_for_list_item(mainFrame, context, nodeObject);
    if (!markerTextGChar)
        return 0;

    JSRetainPtr<JSStringRef> markerText(Adopt, JSStringCreateWithUTF8CString(markerTextGChar));
    g_free(markerTextGChar);
    return markerText;
}

void LayoutTestController::authenticateSession(JSStringRef, JSStringRef, JSStringRef)
{
}

void LayoutTestController::setEditingBehavior(const char* editingBehavior)
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebSettings* settings = webkit_web_view_get_settings(webView);

    if (!strcmp(editingBehavior, "win"))
        g_object_set(G_OBJECT(settings), "editing-behavior", WEBKIT_EDITING_BEHAVIOR_WINDOWS, NULL);
    if (!strcmp(editingBehavior, "mac"))
        g_object_set(G_OBJECT(settings), "editing-behavior", WEBKIT_EDITING_BEHAVIOR_MAC, NULL);
}

void LayoutTestController::abortModal()
{
}
