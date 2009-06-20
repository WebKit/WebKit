/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Alp Toker <alp@nuanti.com>
 * Copyright (C) 2009 Jan Alonzo <jmalonzo@gmail.com>
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
#include "DumpRenderTree.h"

#include "LayoutTestController.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"

#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <JavaScriptCore/JavaScript.h>

#include <wtf/Assertions.h>

#include <cassert>
#include <getopt.h>
#include <stdlib.h>
#include <string.h>

using namespace std;

extern "C" {
// This API is not yet public.
extern G_CONST_RETURN gchar* webkit_web_history_item_get_target(WebKitWebHistoryItem*);
extern gboolean webkit_web_history_item_is_target_item(WebKitWebHistoryItem*);
extern GList* webkit_web_history_item_get_children(WebKitWebHistoryItem*);
extern GSList* webkit_web_frame_get_children(WebKitWebFrame* frame);
extern gchar* webkit_web_frame_get_inner_text(WebKitWebFrame* frame);
extern gchar* webkit_web_frame_dump_render_tree(WebKitWebFrame* frame);
extern void webkit_web_settings_add_extra_plugin_directory(WebKitWebView* view, const gchar* directory);
extern gchar* webkit_web_frame_get_response_mime_type(WebKitWebFrame* frame);
extern void webkit_web_frame_clear_main_frame_name(WebKitWebFrame* frame);
}

volatile bool done;
static bool printSeparators;
static int dumpPixels;
static int dumpTree = 1;

LayoutTestController* gLayoutTestController = 0;
static WebKitWebView* webView;
static GtkWidget* container;
WebKitWebFrame* mainFrame = 0;
WebKitWebFrame* topLoadingFrame = 0;
guint waitToDumpWatchdog = 0;
bool waitForPolicy = false;

// current b/f item at the end of the previous test
static WebKitWebHistoryItem* prevTestBFItem = NULL;

const unsigned maxViewHeight = 600;
const unsigned maxViewWidth = 800;
const unsigned historyItemIndent = 8;

static gchar* autocorrectURL(const gchar* url)
{
    if (strncmp("http://", url, 7) != 0 && strncmp("https://", url, 8) != 0) {
        GString* string = g_string_new("file://");
        g_string_append(string, url);
        return g_string_free(string, FALSE);
    }

    return g_strdup(url);
}

static bool shouldLogFrameLoadDelegates(const char* pathOrURL)
{
    return strstr(pathOrURL, "loading/");
}

void dumpFrameScrollPosition(WebKitWebFrame* frame)
{

}

void displayWebView()
{

}

static void appendString(gchar*& target, gchar* string)
{
    gchar* oldString = target;
    target = g_strconcat(target, string, NULL);
    g_free(oldString);
}

static gchar* dumpFramesAsText(WebKitWebFrame* frame)
{
    gchar* result = 0;

    // Add header for all but the main frame.
    bool isMainFrame = (webkit_web_view_get_main_frame(webView) == frame);

    gchar* innerText = webkit_web_frame_get_inner_text(frame);
    if (isMainFrame)
        result = g_strdup_printf("%s\n", innerText);
    else {
        const gchar* frameName = webkit_web_frame_get_name(frame);
        result = g_strdup_printf("\n--------\nFrame: '%s'\n--------\n%s\n", frameName, innerText);
    }
    g_free(innerText);

    if (gLayoutTestController->dumpChildFramesAsText()) {
        GSList* children = webkit_web_frame_get_children(frame);
        for (GSList* child = children; child; child = g_slist_next(child))
           appendString(result, dumpFramesAsText((WebKitWebFrame*)children->data));
        g_slist_free(children);
    }

    return result;
}

static gint compareHistoryItems(gpointer* item1, gpointer* item2)
{
    return g_ascii_strcasecmp(webkit_web_history_item_get_target(WEBKIT_WEB_HISTORY_ITEM(item1)),
                              webkit_web_history_item_get_target(WEBKIT_WEB_HISTORY_ITEM(item2)));
}

static void dumpHistoryItem(WebKitWebHistoryItem* item, int indent, bool current)
{
    ASSERT(item != NULL);
    int start = 0;
    g_object_ref(item);
    if (current) {
        printf("curr->");
        start = 6;
    }
    for (int i = start; i < indent; i++)
        putchar(' ');
    printf("%s", webkit_web_history_item_get_uri(item));
    const gchar* target = webkit_web_history_item_get_target(item);
    if (target && strlen(target) > 0)
        printf(" (in frame \"%s\")", target);
    if (webkit_web_history_item_is_target_item(item))
        printf("  **nav target**");
    putchar('\n');
    GList* kids = webkit_web_history_item_get_children(item);
    if (kids) {
        // must sort to eliminate arbitrary result ordering which defeats reproducible testing
        kids = g_list_sort(kids, (GCompareFunc) compareHistoryItems);
        for (unsigned i = 0; i < g_list_length(kids); i++)
            dumpHistoryItem(WEBKIT_WEB_HISTORY_ITEM(g_list_nth_data(kids, i)), indent+4, FALSE);
    }
    g_object_unref(item);
}

static void dumpBackForwardListForWebView(WebKitWebView* view)
{
    printf("\n============== Back Forward List ==============\n");
    WebKitWebBackForwardList* bfList = webkit_web_view_get_back_forward_list(view);

    // Print out all items in the list after prevTestBFItem, which was from the previous test
    // Gather items from the end of the list, the print them out from oldest to newest
    GList* itemsToPrint = NULL;
    gint forwardListCount = webkit_web_back_forward_list_get_forward_length(bfList);
    for (int i = forwardListCount; i > 0; i--) {
        WebKitWebHistoryItem* item = webkit_web_back_forward_list_get_nth_item(bfList, i);
        // something is wrong if the item from the last test is in the forward part of the b/f list
        ASSERT(item != prevTestBFItem);
        g_object_ref(item);
        itemsToPrint = g_list_append(itemsToPrint, item);
    }

    WebKitWebHistoryItem* currentItem = webkit_web_back_forward_list_get_current_item(bfList);

    g_object_ref(currentItem);
    itemsToPrint = g_list_append(itemsToPrint, currentItem);

    gint currentItemIndex = g_list_length(itemsToPrint) - 1;
    gint backListCount = webkit_web_back_forward_list_get_back_length(bfList);
    for (int i = -1; i >= -(backListCount); i--) {
        WebKitWebHistoryItem* item = webkit_web_back_forward_list_get_nth_item(bfList, i);
        if (item == prevTestBFItem)
            break;
        g_object_ref(item);
        itemsToPrint = g_list_append(itemsToPrint, item);
    }

    for (int i = g_list_length(itemsToPrint) - 1; i >= 0; i--) {
        WebKitWebHistoryItem* item = WEBKIT_WEB_HISTORY_ITEM(g_list_nth_data(itemsToPrint, i));
        dumpHistoryItem(item, historyItemIndent, i == currentItemIndex);
        g_object_unref(item);
    }
    g_list_free(itemsToPrint);
    printf("===============================================\n");
}

static void invalidateAnyPreviousWaitToDumpWatchdog()
{
    if (waitToDumpWatchdog) {
        g_source_remove(waitToDumpWatchdog);
        waitToDumpWatchdog = 0;
    }

    waitForPolicy = false;
}

static void resetWebViewToConsistentStateBeforeTesting()
{
    WebKitWebSettings* settings = webkit_web_view_get_settings(webView);
    g_object_set(G_OBJECT(settings),
                 "enable-private-browsing", FALSE,
                 "enable-developer-extras", FALSE,
                 "enable-spell-checking", TRUE,
                 "enable-html5-database", TRUE,
                 "enable-html5-local-storage", TRUE,
                 "enable-xss-auditor", TRUE,
                 NULL);

    webkit_web_frame_clear_main_frame_name(mainFrame);

    WebKitWebInspector* inspector = webkit_web_view_get_inspector(webView);
    g_object_set(G_OBJECT(inspector), "javascript-profiling-enabled", FALSE, NULL);
}

void dump()
{
    invalidateAnyPreviousWaitToDumpWatchdog();

    bool dumpAsText = gLayoutTestController->dumpAsText();
    if (dumpTree) {
        char* result = 0;
        gchar* responseMimeType = webkit_web_frame_get_response_mime_type(mainFrame);

        dumpAsText = g_str_equal(responseMimeType, "text/plain");
        g_free(responseMimeType);

        // Test can request controller to be dumped as text even
        // while test's response mime type is not text/plain.
        // Overriding this behavior with dumpAsText being false is a bad idea.
        if (dumpAsText)
            gLayoutTestController->setDumpAsText(dumpAsText);

        if (gLayoutTestController->dumpAsText())
            result = dumpFramesAsText(mainFrame);
        else
            result = webkit_web_frame_dump_render_tree(mainFrame);

        if (!result) {
            const char* errorMessage;
            if (gLayoutTestController->dumpAsText())
                errorMessage = "[documentElement innerText]";
            else if (gLayoutTestController->dumpDOMAsWebArchive())
                errorMessage = "[[mainFrame DOMDocument] webArchive]";
            else if (gLayoutTestController->dumpSourceAsWebArchive())
                errorMessage = "[[mainFrame dataSource] webArchive]";
            else
                errorMessage = "[mainFrame renderTreeAsExternalRepresentation]";
            printf("ERROR: nil result from %s", errorMessage);
        } else {
            printf("%s", result);
            g_free(result);
            if (!gLayoutTestController->dumpAsText() && !gLayoutTestController->dumpDOMAsWebArchive() && !gLayoutTestController->dumpSourceAsWebArchive())
                dumpFrameScrollPosition(mainFrame);

            if (gLayoutTestController->dumpBackForwardList()) {
                // FIXME: multiple windows support
                dumpBackForwardListForWebView(webView);

            }
        }

        if (printSeparators) {
            puts("#EOF"); // terminate the content block
            fputs("#EOF\n", stderr);
            fflush(stdout);
            fflush(stderr);
        }
    }

    if (dumpPixels) {
        if (!gLayoutTestController->dumpAsText() && !gLayoutTestController->dumpDOMAsWebArchive() && !gLayoutTestController->dumpSourceAsWebArchive()) {
            // FIXME: Add support for dumping pixels
        }
    }

    // FIXME: call displayWebView here when we support --paint

    puts("#EOF"); // terminate the (possibly empty) pixels block

    fflush(stdout);
    fflush(stderr);

    done = true;
}

static void setDefaultsToConsistentStateValuesForTesting()
{
    gdk_screen_set_resolution(gdk_screen_get_default(), 72.0);

    WebKitWebSettings* settings = webkit_web_view_get_settings(webView);
    g_object_set(G_OBJECT(settings),
                 "default-font-family", "Times",
                 "monospace-font-family", "Courier",
                 "serif-font-family", "Times",
                 "sans-serif-font-family", "Helvetica",
                 "default-font-size", 16,
                 "default-monospace-font-size", 13,
                 "minimum-font-size", 1,
                 NULL);

    /* Disable the default auth dialog for testing */
    SoupSession* session = webkit_get_default_session();
    soup_session_remove_feature_by_type(session, WEBKIT_TYPE_SOUP_AUTH_DIALOG);

#if PLATFORM(X11)
    webkit_web_settings_add_extra_plugin_directory(webView, TEST_PLUGIN_DIR);
#endif
}

static void runTest(const string& testPathOrURL)
{
    ASSERT(!testPathOrURL.empty());

    // Look for "'" as a separator between the path or URL, and the pixel dump hash that follows.
    string pathOrURL(testPathOrURL);
    string expectedPixelHash;

    size_t separatorPos = pathOrURL.find("'");
    if (separatorPos != string::npos) {
        pathOrURL = string(testPathOrURL, 0, separatorPos);
        expectedPixelHash = string(testPathOrURL, separatorPos + 1);
    }

    gchar* url = autocorrectURL(pathOrURL.c_str());
    const string testURL(url);

    resetWebViewToConsistentStateBeforeTesting();

    gLayoutTestController = new LayoutTestController(testURL, expectedPixelHash);
    topLoadingFrame = 0;
    done = false;

    gLayoutTestController->setIconDatabaseEnabled(false);

    if (shouldLogFrameLoadDelegates(pathOrURL.c_str()))
        gLayoutTestController->setDumpFrameLoadCallbacks(true);

    WorkQueue::shared()->clear();
    WorkQueue::shared()->setFrozen(false);

    bool isSVGW3CTest = (gLayoutTestController->testPathOrURL().find("svg/W3C-SVG-1.1") != string::npos);
    GtkAllocation size;
    size.width = isSVGW3CTest ? 480 : maxViewWidth;
    size.height = isSVGW3CTest ? 360 : maxViewHeight;
    gtk_widget_size_allocate(container, &size);

    if (prevTestBFItem)
        g_object_unref(prevTestBFItem);
    WebKitWebBackForwardList* bfList = webkit_web_view_get_back_forward_list(webView);
    prevTestBFItem = webkit_web_back_forward_list_get_current_item(bfList);
    if (prevTestBFItem)
        g_object_ref(prevTestBFItem);


    webkit_web_view_open(webView, url);

    g_free(url);
    url = NULL;

    while (!done)
        g_main_context_iteration(NULL, TRUE);

    // A blank load seems to be necessary to reset state after certain tests.
    webkit_web_view_open(webView, "about:blank");

    gLayoutTestController->deref();
    gLayoutTestController = 0;
}

void webViewLoadStarted(WebKitWebView* view, WebKitWebFrame* frame, void*)
{
    // Make sure we only set this once per test.  If it gets cleared, and then set again, we might
    // end up doing two dumps for one test.
    if (!topLoadingFrame && !done)
        topLoadingFrame = frame;
}

static gboolean processWork(void* data)
{
    // if we finish all the commands, we're ready to dump state
    if (WorkQueue::shared()->processWork() && !gLayoutTestController->waitToDump())
        dump();

    return FALSE;
}

static void webViewLoadFinished(WebKitWebView* view, WebKitWebFrame* frame, void*)
{
    if (frame != topLoadingFrame)
        return;

    topLoadingFrame = 0;
    WorkQueue::shared()->setFrozen(true); // first complete load freezes the queue for the rest of this test
    if (gLayoutTestController->waitToDump())
        return;

    if (WorkQueue::shared()->count())
        g_timeout_add(0, processWork, 0);
     else
        dump();
}

static void webViewWindowObjectCleared(WebKitWebView* view, WebKitWebFrame* frame, JSGlobalContextRef context, JSObjectRef windowObject, gpointer data)
{
    JSValueRef exception = 0;
    assert(gLayoutTestController);

    gLayoutTestController->makeWindowObject(context, windowObject, &exception);
    assert(!exception);
}

static gboolean webViewConsoleMessage(WebKitWebView* view, const gchar* message, unsigned int line, const gchar* sourceId, gpointer data)
{
    fprintf(stdout, "CONSOLE MESSAGE: line %d: %s\n", line, message);
    return TRUE;
}


static gboolean webViewScriptAlert(WebKitWebView* view, WebKitWebFrame* frame, const gchar* message, gpointer data)
{
    fprintf(stdout, "ALERT: %s\n", message);
    return TRUE;
}

static gboolean webViewScriptPrompt(WebKitWebView* webView, WebKitWebFrame* frame, const gchar* message, const gchar* defaultValue, gchar** value, gpointer data)
{
    fprintf(stdout, "PROMPT: %s, default text: %s\n", message, defaultValue);
    *value = g_strdup(defaultValue);
    return TRUE;
}

static gboolean webViewScriptConfirm(WebKitWebView* view, WebKitWebFrame* frame, const gchar* message, gboolean* didConfirm, gpointer data)
{
    fprintf(stdout, "CONFIRM: %s\n", message);
    *didConfirm = TRUE;
    return TRUE;
}

static void webViewTitleChanged(WebKitWebView* view, WebKitWebFrame* frame, const gchar* title, gpointer data)
{
    if (gLayoutTestController->dumpTitleChanges() && !done)
        printf("TITLE CHANGED: %s\n", title ? title : "");
}

static bool webViewNavigationPolicyDecisionRequested(WebKitWebView* view, WebKitWebFrame* frame,
                                                     WebKitNetworkRequest* request,
                                                     WebKitWebNavigationAction* navAction,
                                                     WebKitWebPolicyDecision* policyDecision)
{
    // Use the default handler if we're not waiting for policy,
    // i.e., LayoutTestController::waitForPolicyDelegate
    if (!waitForPolicy)
        return FALSE;

    gchar* typeDescription;
    WebKitWebNavigationReason reason;
    g_object_get(G_OBJECT(navAction), "reason", &reason, NULL);

    switch(reason) {
        case WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED:
            typeDescription = g_strdup("link clicked");
            break;
        case WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED:
            typeDescription = g_strdup("form submitted");
            break;
        case WEBKIT_WEB_NAVIGATION_REASON_BACK_FORWARD:
            typeDescription = g_strdup("back/forward");
            break;
        case WEBKIT_WEB_NAVIGATION_REASON_RELOAD:
            typeDescription = g_strdup("reload");
            break;
        case WEBKIT_WEB_NAVIGATION_REASON_FORM_RESUBMITTED:
            typeDescription = g_strdup("form resubmitted");
            break;
        case WEBKIT_WEB_NAVIGATION_REASON_OTHER:
            typeDescription = g_strdup("other");
            break;
        default:
            typeDescription = g_strdup("illegal value");
    }

    printf("Policy delegate: attempt to load %s with navigation type '%s'\n", webkit_network_request_get_uri(request), typeDescription);
    g_free(typeDescription);

    webkit_web_policy_decision_ignore(policyDecision);
    gLayoutTestController->notifyDone();

    return TRUE;
}

static void webViewStatusBarTextChanged(WebKitWebView* view, const gchar* message, gpointer data)
{
    // Are we doing anything wrong? One test that does not call
    // dumpStatusCallbacks gets true here
    if (gLayoutTestController->dumpStatusCallbacks()) {
        if (message && strcmp(message, ""))
            printf("UI DELEGATE STATUS CALLBACK: setStatusText:%s\n", message);
    }
}

int main(int argc, char* argv[])
{
    g_thread_init(NULL);
    gtk_init(&argc, &argv);

    struct option options[] = {
        {"notree", no_argument, &dumpTree, false},
        {"pixel-tests", no_argument, &dumpPixels, true},
        {"tree", no_argument, &dumpTree, true},
        {NULL, 0, NULL, 0}
    };

    int option;
    while ((option = getopt_long(argc, (char* const*)argv, "", options, NULL)) != -1)
        switch (option) {
            case '?':   // unknown or ambiguous option
            case ':':   // missing argument
                exit(1);
                break;
        }

    GtkWidget* window = gtk_window_new(GTK_WINDOW_POPUP);
    container = GTK_WIDGET(gtk_scrolled_window_new(NULL, NULL));
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(container), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(window), container);
    gtk_widget_realize(window);

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(webView));
    gtk_widget_realize(GTK_WIDGET(webView));
    gtk_widget_show_all(container);
    mainFrame = webkit_web_view_get_main_frame(webView);

    g_signal_connect(G_OBJECT(webView), "load-started", G_CALLBACK(webViewLoadStarted), 0);
    g_signal_connect(G_OBJECT(webView), "load-finished", G_CALLBACK(webViewLoadFinished), 0);
    g_signal_connect(G_OBJECT(webView), "window-object-cleared", G_CALLBACK(webViewWindowObjectCleared), 0);
    g_signal_connect(G_OBJECT(webView), "console-message", G_CALLBACK(webViewConsoleMessage), 0);
    g_signal_connect(G_OBJECT(webView), "script-alert", G_CALLBACK(webViewScriptAlert), 0);
    g_signal_connect(G_OBJECT(webView), "script-prompt", G_CALLBACK(webViewScriptPrompt), 0);
    g_signal_connect(G_OBJECT(webView), "script-confirm", G_CALLBACK(webViewScriptConfirm), 0);
    g_signal_connect(G_OBJECT(webView), "title-changed", G_CALLBACK(webViewTitleChanged), 0);
    g_signal_connect(G_OBJECT(webView), "navigation-policy-decision-requested", G_CALLBACK(webViewNavigationPolicyDecisionRequested), 0);
    g_signal_connect(G_OBJECT(webView), "status-bar-text-changed", G_CALLBACK(webViewStatusBarTextChanged), 0);

    setDefaultsToConsistentStateValuesForTesting();

    if (argc == optind+1 && strcmp(argv[optind], "-") == 0) {
        char filenameBuffer[2048];
        printSeparators = true;
        while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
            char* newLineCharacter = strchr(filenameBuffer, '\n');
            if (newLineCharacter)
                *newLineCharacter = '\0';

            if (strlen(filenameBuffer) == 0)
                continue;

            runTest(filenameBuffer);
        }
    } else {
        printSeparators = (optind < argc-1 || (dumpPixels && dumpTree));
        for (int i = optind; i != argc; ++i)
            runTest(argv[i]);
    }

    return 0;
}
