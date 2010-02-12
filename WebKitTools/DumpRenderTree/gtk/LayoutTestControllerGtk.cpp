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

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <glib.h>
#include <libsoup/soup.h>
#include <webkit/webkit.h>

extern "C" {
bool webkit_web_frame_pause_animation(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);
bool webkit_web_frame_pause_transition(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);
bool webkit_web_frame_pause_svg_animation(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);
unsigned int webkit_web_frame_number_of_active_animations(WebKitWebFrame* frame);
void webkit_application_cache_set_maximum_size(unsigned long long size);
unsigned int webkit_worker_thread_count(void);
void webkit_white_list_access_from_origin(const gchar* sourceOrigin, const gchar* destinationProtocol, const gchar* destinationHost, bool allowDestinationSubdomains);
gchar* webkit_web_frame_counter_value_for_element_by_id(WebKitWebFrame* frame, const gchar* id);
int webkit_web_frame_page_number_for_element_by_id(WebKitWebFrame* frame, const gchar* id, float pageWidth, float pageHeight);
void webkit_web_inspector_execute_script(WebKitWebInspector* inspector, long callId, const gchar* script);
}

static gchar* copyWebSettingKey(gchar* preferenceKey)
{
    static GHashTable* keyTable;

    if (!keyTable) {
        // If you add a pref here, make sure you reset the value in
        // DumpRenderTree::resetWebViewToConsistentStateBeforeTesting.
        keyTable = g_hash_table_new(g_str_hash, g_str_equal);
        g_hash_table_insert(keyTable, g_strdup("WebKitJavaScriptEnabled"), g_strdup("enable-scripts"));
        g_hash_table_insert(keyTable, g_strdup("WebKitDefaultFontSize"), g_strdup("default-font-size"));
        g_hash_table_insert(keyTable, g_strdup("WebKitEnableCaretBrowsing"), g_strdup("enable-caret-browsing"));
        g_hash_table_insert(keyTable, g_strdup("WebKitUsesPageCachePreferenceKey"), g_strdup("enable-page-cache"));
    }

    return g_strdup(static_cast<gchar*>(g_hash_table_lookup(keyTable, preferenceKey)));
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

int LayoutTestController::pageNumberForElementById(JSStringRef id, float pageWidth, float pageHeight)
{
    gchar* idGChar = JSStringCopyUTF8CString(id);
    int pageNumber = webkit_web_frame_page_number_for_element_by_id(mainFrame, idGChar, pageWidth, pageHeight);
    g_free(idGChar);
    return pageNumber;
}

int LayoutTestController::numberOfPages(float, float)
{
    // FIXME: implement
    return -1;
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
    // FIXME: Implement this (and restore the default value before running each test in DumpRenderTree.cpp).
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

void LayoutTestController::whiteListAccessFromOrigin(JSStringRef sourceOrigin, JSStringRef protocol, JSStringRef host, bool includeSubdomains)
{
    gchar* sourceOriginGChar = JSStringCopyUTF8CString(sourceOrigin);
    gchar* protocolGChar = JSStringCopyUTF8CString(protocol);
    gchar* hostGChar = JSStringCopyUTF8CString(host);
    webkit_white_list_access_from_origin(sourceOriginGChar, protocolGChar, hostGChar, includeSubdomains);
    g_free(sourceOriginGChar);
    g_free(protocolGChar);
    g_free(hostGChar);
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
    static const int timeoutSeconds = 15;

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

void LayoutTestController::setXSSAuditorEnabled(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "enable-xss-auditor", flag, NULL);
}

void LayoutTestController::setFrameSetFlatteningEnabled(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setAllowUniversalAccessFromFileURLs(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "enable-universal-access-from-file-uris", flag, NULL);
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

void LayoutTestController::setIconDatabaseEnabled(bool flag)
{
    // FIXME: implement
}

void LayoutTestController::setJavaScriptProfilingEnabled(bool flag)
{
    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    g_object_set(G_OBJECT(settings), "enable-developer-extras", flag, NULL);

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

bool LayoutTestController::elementDoesAutoCompleteForElementWithId(JSStringRef id) 
{
    // FIXME: implement
    return false;
}

void LayoutTestController::execCommand(JSStringRef name, JSStringRef value)
{
    // FIXME: implement
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

void LayoutTestController::setPersistentUserStyleSheetLocation(JSStringRef jsURL)
{
    // FIXME: implement
}

void LayoutTestController::clearPersistentUserStyleSheet()
{
    // FIXME: implement
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

void LayoutTestController::overridePreference(JSStringRef key, JSStringRef value)
{
    gchar* name = JSStringCopyUTF8CString(key);
    gchar* strValue = JSStringCopyUTF8CString(value);

    WebKitWebView* view = webkit_web_frame_get_web_view(mainFrame);
    ASSERT(view);

    WebKitWebSettings* settings = webkit_web_view_get_settings(view);
    gchar* webSettingKey = copyWebSettingKey(name);

    if (webSettingKey) {
        GValue stringValue = { 0, { { 0 } } };
        g_value_init(&stringValue, G_TYPE_STRING);
        g_value_set_string(&stringValue, const_cast<gchar*>(strValue));

        WebKitWebSettingsClass* klass = WEBKIT_WEB_SETTINGS_GET_CLASS(settings);
        GParamSpec* pspec = g_object_class_find_property(G_OBJECT_CLASS(klass), webSettingKey);
        GValue propValue = { 0, { { 0 } } };
        g_value_init(&propValue, pspec->value_type);

        if (g_value_type_transformable(G_TYPE_STRING, pspec->value_type)) {
            g_value_transform(const_cast<GValue*>(&stringValue), &propValue);
            g_object_set_property(G_OBJECT(settings), webSettingKey, const_cast<GValue*>(&propValue));
        } else if (G_VALUE_HOLDS_BOOLEAN(&propValue)) {
            char* lowered = g_utf8_strdown(strValue, -1);
            g_object_set(G_OBJECT(settings), webSettingKey,
                         g_str_equal(lowered, "true")
                         || g_str_equal(strValue, "1"),
                         NULL);
            g_free(lowered);
        } else if (G_VALUE_HOLDS_INT(&propValue)) {
            std::string str(strValue);
            std::stringstream ss(str);
            int val = 0;
            if (!(ss >> val).fail())
                g_object_set(G_OBJECT(settings), webSettingKey, val, NULL);
        } else
            printf("LayoutTestController::overridePreference failed to override preference '%s'.\n", name);
    }

    g_free(webSettingKey);
    g_free(name);
    g_free(strValue);
}

void LayoutTestController::addUserScript(JSStringRef source, bool runAtStart)
{
    printf("LayoutTestController::addUserScript not implemented.\n");
}

void LayoutTestController::addUserStyleSheet(JSStringRef source)
{
    printf("LayoutTestController::addUserStyleSheet not implemented.\n");
}

void LayoutTestController::showWebInspector()
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebSettings* webSettings = webkit_web_view_get_settings(webView);
    WebKitWebInspector* inspector = webkit_web_view_get_inspector(webView);

    g_object_set(webSettings, "enable-developer-extras", TRUE, NULL);
    webkit_web_inspector_show(inspector);
}

void LayoutTestController::closeWebInspector()
{
    WebKitWebView* webView = webkit_web_frame_get_web_view(mainFrame);
    WebKitWebSettings* webSettings = webkit_web_view_get_settings(webView);
    WebKitWebInspector* inspector = webkit_web_view_get_inspector(webView);

    webkit_web_inspector_close(inspector);
    g_object_set(webSettings, "enable-developer-extras", FALSE, NULL);
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
