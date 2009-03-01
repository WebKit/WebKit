/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Alp Toker <alp@nuanti.com>
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
extern GSList* webkit_web_frame_get_children(WebKitWebFrame* frame);
extern gchar* webkit_web_frame_get_inner_text(WebKitWebFrame* frame);
extern gchar* webkit_web_frame_dump_render_tree(WebKitWebFrame* frame);
extern void webkit_web_settings_add_extra_plugin_directory(WebKitWebView* view, const gchar* directory);
}

volatile bool done;
static bool printSeparators;
static int dumpPixels;
static int dumpTree = 1;

LayoutTestController* gLayoutTestController = 0;
static WebKitWebView* webView;
WebKitWebFrame* mainFrame = 0;
WebKitWebFrame* topLoadingFrame = 0;
guint waitToDumpWatchdog = 0;

const unsigned maxViewHeight = 600;
const unsigned maxViewWidth = 800;

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

static void invalidateAnyPreviousWaitToDumpWatchdog()
{
    if (waitToDumpWatchdog) {
        g_source_remove(waitToDumpWatchdog);
        waitToDumpWatchdog = 0;
    }
}

void dump()
{
    invalidateAnyPreviousWaitToDumpWatchdog();
    if (dumpTree) {
        char* result = 0;

        bool dumpAsText = gLayoutTestController->dumpAsText();
        // FIXME: Also dump text resuls as text.
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
        }

        if (gLayoutTestController->dumpBackForwardList()) {
            // FIXME: not implemented
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

    webkit_web_settings_add_extra_plugin_directory(webView, TEST_PLUGIN_DIR);
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
    gtk_widget_size_allocate(GTK_WIDGET(webView), &size);

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
    // quit doing work once a load is in progress
    while (WorkQueue::shared()->count() > 0 && !topLoadingFrame) {
        WorkQueueItem* item = WorkQueue::shared()->dequeue();
        ASSERT(item);
        item->invoke();
        delete item;
    }

    // if we didn't start a new load, then we finished all the commands, so we're ready to dump state
    if (!topLoadingFrame && !gLayoutTestController->waitToDump())
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
    GtkContainer* container = GTK_CONTAINER(gtk_fixed_new());
    gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(container));
    gtk_widget_realize(window);

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    gtk_container_add(container, GTK_WIDGET(webView));
    gtk_widget_realize(GTK_WIDGET(webView));
    mainFrame = webkit_web_view_get_main_frame(webView);

    g_signal_connect(G_OBJECT(webView), "load-started", G_CALLBACK(webViewLoadStarted), 0);
    g_signal_connect(G_OBJECT(webView), "load-finished", G_CALLBACK(webViewLoadFinished), 0);
    g_signal_connect(G_OBJECT(webView), "window-object-cleared", G_CALLBACK(webViewWindowObjectCleared), 0);
    g_signal_connect(G_OBJECT(webView), "console-message", G_CALLBACK(webViewConsoleMessage), 0);
    g_signal_connect(G_OBJECT(webView), "script-alert", G_CALLBACK(webViewScriptAlert), 0);
    g_signal_connect(G_OBJECT(webView), "script-prompt", G_CALLBACK(webViewScriptPrompt), 0);
    g_signal_connect(G_OBJECT(webView), "script-confirm", G_CALLBACK(webViewScriptConfirm), 0);
    g_signal_connect(G_OBJECT(webView), "title-changed", G_CALLBACK(webViewTitleChanged), 0);

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
