/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Nuanti Ltd.
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
#include <glib.h>
#include <libsoup/soup.h>
#include <webkit/webkit.h>

extern "C" {
bool webkit_web_frame_pause_animation(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);
bool webkit_web_frame_pause_transition(WebKitWebFrame* frame, const gchar* name, double time, const gchar* element);
unsigned int webkit_web_frame_number_of_active_animations(WebKitWebFrame* frame);
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

void LayoutTestController::keepWebHistory()
{
    // FIXME: implement
}

size_t LayoutTestController::webHistoryItemCount()
{
    // FIXME: implement
    return 0;
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

void LayoutTestController::setCustomPolicyDelegate(bool setDelegate, bool permissive)
{
    // FIXME: implement
}

void LayoutTestController::waitForPolicyDelegate()
{
    waitForPolicy = true;
    setWaitToDump(true);
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
    const char* message = "FAIL: Timed out waiting for notifyDone to be called\n";
    fprintf(stderr, "%s", message);
    fprintf(stdout, "%s", message);
    waitToDumpWatchdog = 0;
    dump();
    return FALSE;
}

void LayoutTestController::setWaitToDump(bool waitUntilDone)
{
    static const int timeoutSeconds = 10;

    m_waitToDump = waitUntilDone;
    if (m_waitToDump && !waitToDumpWatchdog)
        waitToDumpWatchdog = g_timeout_add_seconds(timeoutSeconds, waitToDumpWatchdogFired, 0);
}

int LayoutTestController::windowCount()
{
    // FIXME: implement
    return 1;
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

void LayoutTestController::setAuthorAndUserStylesEnabled(bool flag)
{
    // FIXME: implement
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

void LayoutTestController::setPopupBlockingEnabled(bool popupBlockingEnabled)
{
    // FIXME: implement
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
    // FIXME: implement
}
 
void LayoutTestController::setDatabaseQuota(unsigned long long quota)
{    
    // FIXME: implement
}

bool LayoutTestController::pauseAnimationAtTimeOnElementWithId(JSStringRef animationName, double time, JSStringRef elementId)
{    
    gchar* name = JSStringCopyUTF8CString(animationName);
    gchar* element = JSStringCopyUTF8CString(elementId);
    return webkit_web_frame_pause_animation(mainFrame, name, time, element);
}

bool LayoutTestController::pauseTransitionAtTimeOnElementWithId(JSStringRef propertyName, double time, JSStringRef elementId)
{    
    gchar* name = JSStringCopyUTF8CString(propertyName);
    gchar* element = JSStringCopyUTF8CString(elementId);
    return webkit_web_frame_pause_transition(mainFrame, name, time, element);
}

unsigned LayoutTestController::numberOfActiveAnimations() const
{
    return webkit_web_frame_number_of_active_animations(mainFrame);
}
