/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
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

#include "webkitwebframe.h"
#include "webkitwebview.h"

#include <gtk/gtk.h>

#include <string.h>

#include <JavaScriptCore/JSBase.h>
#include <JavaScriptCore/JSContextRef.h>
#include <JavaScriptCore/JSStringRef.h>
#include <JavaScriptCore/JSObjectRef.h>
#include <JavaScriptCore/JSValueRef.h>

#include "LayoutTestController.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"

#include <getopt.h>
#include <stdlib.h>

extern "C" {
// This API is not yet public.
extern GSList* webkit_web_frame_get_children(WebKitWebFrame* frame);
extern gchar* webkit_web_frame_get_inner_text(WebKitWebFrame* frame);
}

volatile bool done;
static bool printSeparators;
static int testRepaintDefault;
static int repaintSweepHorizontallyDefault;
static int dumpPixels;
static int dumpTree;
static gchar* currentTest;

LayoutTestController* layoutTestController = 0;
static WebKitWebView* view;
WebKitWebFrame* mainFrame = 0;

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
    bool isMainFrame = (webkit_web_view_get_main_frame(view) == frame);

    if (isMainFrame) {
        const gchar* frameName = webkit_web_frame_get_name(frame);
        gchar* innerText = webkit_web_frame_get_inner_text(frame);

        result = g_strdup_printf("\n--------\nFrame: '%s'\n--------\n%s", frameName, innerText);

        g_free(innerText);
    }

    if (layoutTestController->dumpChildFramesAsText()) {
        GSList* children = webkit_web_frame_get_children(frame);
        for (;children; children = g_slist_next(children))
           appendString(result, dumpFramesAsText((WebKitWebFrame*)children->data));
    }

    return result;
}

static gchar* dumpRenderTreeAsText(WebKitWebFrame* frame)
{
    // FIXME: this will require new WebKitGtk SPI
    return strdup("foo");
}

void dump()
{
    if (dumpTree) {
        char* result = 0;

        bool dumpAsText = layoutTestController->dumpAsText();
        // FIXME: Also dump text resuls as text.
        layoutTestController->setDumpAsText(dumpAsText);
        if (layoutTestController->dumpAsText())
            result = dumpFramesAsText(mainFrame);
        else {
            bool isSVGW3CTest = (g_strrstr(currentTest, "svg/W3C-SVG-1.1"));
            if (isSVGW3CTest)
                gtk_widget_set_size_request(GTK_WIDGET(view), 480, 360);
            else
                gtk_widget_set_size_request(GTK_WIDGET(view), maxViewWidth, maxViewHeight);
            result = dumpRenderTreeAsText(mainFrame);
        }

        if (!result) {
            const char* errorMessage;
            if (layoutTestController->dumpAsText())
                errorMessage = "[documentElement innerText]";
            else if (layoutTestController->dumpDOMAsWebArchive())
                errorMessage = "[[mainFrame DOMDocument] webArchive]";
            else if (layoutTestController->dumpSourceAsWebArchive())
                errorMessage = "[[mainFrame dataSource] webArchive]";
            else
                errorMessage = "[mainFrame renderTreeAsExternalRepresentation]";
            printf("ERROR: nil result from %s", errorMessage);
        } else {
            printf("%s", result);
        g_free(result);
            if (!layoutTestController->dumpAsText() && !layoutTestController->dumpDOMAsWebArchive() && !layoutTestController->dumpSourceAsWebArchive())
                dumpFrameScrollPosition(mainFrame);
        }

        if (layoutTestController->dumpBackForwardList()) {
            // FIXME: not implemented
        }

        if (printSeparators)
            puts("#EOF");
    }

    if (dumpPixels) {
        if (!layoutTestController->dumpAsText() && !layoutTestController->dumpDOMAsWebArchive() && !layoutTestController->dumpSourceAsWebArchive()) {
            // FIXME: Add support for dumping pixels
        }
    }

    fflush(stdout);

    // FIXME: call displayWebView here when we support --paint

    done = true;
}

static void runTest(const char* pathOrURL)
{
    gchar* url = autocorrectURL(pathOrURL);

    layoutTestController = new LayoutTestController(testRepaintDefault, repaintSweepHorizontallyDefault);

    done = false;

    if (shouldLogFrameLoadDelegates(pathOrURL))
        layoutTestController->setDumpFrameLoadCallbacks(true);

    if (currentTest)
        g_free(currentTest);
    currentTest = url;

    WorkQueue::shared()->clear();
    WorkQueue::shared()->setFrozen(false);

    webkit_web_view_open(view, url);

    while (!done)
        g_main_context_iteration(NULL, true);

    WorkQueue::shared()->clear();

    delete layoutTestController;
    layoutTestController = 0;
}

int main(int argc, char* argv[])
{

    struct option options[] = {
        {"horizontal-sweep", no_argument, &repaintSweepHorizontallyDefault, true},
        {"notree", no_argument, &dumpTree, false},
        {"pixel-tests", no_argument, &dumpPixels, true},
        {"repaint", no_argument, &testRepaintDefault, true},
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

    view = WEBKIT_WEB_VIEW(webkit_web_view_new());
    mainFrame = webkit_web_view_get_main_frame(view);

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
