/*
 * Copyright (C) 2007 Eric Seidel <eric@webkit.org>
 * Copyright (C) 2008 Alp Toker <alp@nuanti.com>
 * Copyright (C) 2009 Jan Alonzo <jmalonzo@gmail.com>
 * Copyright (C) 2010 Igalia S.L.
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

#include "AccessibilityController.h"
#include "EventSender.h"
#include "GCController.h"
#include "GOwnPtr.h"
#include "LayoutTestController.h"
#include "PixelDumpSupport.h"
#include "WorkQueue.h"
#include "WorkQueueItem.h"
#include <JavaScriptCore/JavaScript.h>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <getopt.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>
#include <wtf/Assertions.h>

#if PLATFORM(X11)
#include <fontconfig/fontconfig.h>
#endif


using namespace std;

extern "C" {
// This API is not yet public.
extern G_CONST_RETURN gchar* webkit_web_history_item_get_target(WebKitWebHistoryItem*);
extern gboolean webkit_web_history_item_is_target_item(WebKitWebHistoryItem*);
extern GList* webkit_web_history_item_get_children(WebKitWebHistoryItem*);
extern GSList* webkit_web_frame_get_children(WebKitWebFrame* frame);
extern gchar* webkit_web_frame_get_inner_text(WebKitWebFrame* frame);
extern gchar* webkit_web_frame_dump_render_tree(WebKitWebFrame* frame);
extern guint webkit_web_frame_get_pending_unload_event_count(WebKitWebFrame* frame);
extern void webkit_web_settings_add_extra_plugin_directory(WebKitWebView* view, const gchar* directory);
extern gchar* webkit_web_frame_get_response_mime_type(WebKitWebFrame* frame);
extern void webkit_web_frame_clear_main_frame_name(WebKitWebFrame* frame);
extern void webkit_web_view_set_group_name(WebKitWebView* view, const gchar* groupName);
extern void webkit_reset_origin_access_white_lists();
}

volatile bool done;
static bool printSeparators;
static int dumpPixels;
static int dumpTree = 1;

AccessibilityController* axController = 0;
RefPtr<LayoutTestController> gLayoutTestController;
static GCController* gcController = 0;
static WebKitWebView* webView;
static GtkWidget* window;
static GtkWidget* container;
static GtkWidget* webInspectorWindow;
WebKitWebFrame* mainFrame = 0;
WebKitWebFrame* topLoadingFrame = 0;
guint waitToDumpWatchdog = 0;
bool waitForPolicy = false;

// This is a list of opened webviews
GSList* webViewList = 0;

// current b/f item at the end of the previous test
static WebKitWebHistoryItem* prevTestBFItem = NULL;

const unsigned historyItemIndent = 8;

static void runTest(const string& testPathOrURL);

static bool shouldLogFrameLoadDelegates(const string& pathOrURL)
{
    return pathOrURL.find("loading/") != string::npos;
}

static bool shouldOpenWebInspector(const string& pathOrURL)
{
    return pathOrURL.find("inspector/") != string::npos;
}

static bool shouldEnableDeveloperExtras(const string& pathOrURL)
{
    return shouldOpenWebInspector(pathOrURL) || pathOrURL.find("inspector-enabled/") != string::npos;
}

void dumpFrameScrollPosition(WebKitWebFrame* frame)
{

}

void displayWebView()
{
    gtk_widget_queue_draw(GTK_WIDGET(webView));
}

static void appendString(gchar*& target, gchar* string)
{
    gchar* oldString = target;
    target = g_strconcat(target, string, NULL);
    g_free(oldString);
}

static void initializeFonts()
{
#if PLATFORM(X11)
    FcInit();

    // If a test resulted a font being added or removed via the @font-face rule, then
    // we want to reset the FontConfig configuration to prevent it from affecting other tests.
    static int numFonts = 0;
    FcFontSet* appFontSet = FcConfigGetFonts(0, FcSetApplication);
    if (appFontSet && numFonts && appFontSet->nfont == numFonts)
        return;

    // Load our configuration file, which sets up proper aliases for family
    // names like sans, serif and monospace.
    FcConfig* config = FcConfigCreate();
    GOwnPtr<gchar> fontConfigFilename(g_build_filename(FONTS_CONF_DIR, "fonts.conf", NULL));
    if (!FcConfigParseAndLoad(config, reinterpret_cast<FcChar8*>(fontConfigFilename.get()), true))
        g_error("Couldn't load font configuration file from: %s", fontConfigFilename.get());

    static const char *const fontPaths[] = {
        "/usr/share/fonts/truetype/ttf-liberation/LiberationMono-BoldItalic.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationMono-Bold.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationMono-Italic.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationMono-Regular.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationSans-BoldItalic.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationSans-Bold.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationSans-Italic.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationSerif-BoldItalic.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationSerif-Bold.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationSerif-Italic.ttf",
        "/usr/share/fonts/truetype/ttf-liberation/LiberationSerif-Regular.ttf",
        "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
    };

    // TODO: Some tests use Lucida. We should load these as well, once it becomes
    // clear how to install these fonts easily on Fedora.
    for (size_t i = 0; i < G_N_ELEMENTS(fontPaths); i++) {
        if (!g_file_test(fontPaths[i], G_FILE_TEST_EXISTS))
            g_error("Could not find font at %s. Either install this font or file "
                    "a bug at http://bugs.webkit.org if it is installed in another location.",
                    fontPaths[i]);
        if (!FcConfigAppFontAddFile(config, reinterpret_cast<const FcChar8*>(fontPaths[i])))
            g_error("Could not load font at %s!", fontPaths[i]);
    }

    // Ahem is used by many layout tests.
    GOwnPtr<gchar> ahemFontFilename(g_build_filename(FONTS_CONF_DIR, "AHEM____.TTF", NULL));
    if (!FcConfigAppFontAddFile(config, reinterpret_cast<FcChar8*>(ahemFontFilename.get())))
        g_error("Could not load font at %s!", ahemFontFilename.get()); 

    if (!FcConfigSetCurrent(config))
        g_error("Could not set the current font configuration!");

    numFonts = FcConfigGetFonts(config, FcSetApplication)->nfont;
#endif
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
            appendString(result, dumpFramesAsText(static_cast<WebKitWebFrame* >(child->data)));
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

    // normalize file URLs.
    const gchar* uri = webkit_web_history_item_get_uri(item);
    gchar* uriScheme = g_uri_parse_scheme(uri);
    if (g_strcmp0(uriScheme, "file") == 0) {
        gchar* pos = g_strstr_len(uri, -1, "/LayoutTests/");
        if (!pos)
            return;

        GString* result = g_string_sized_new(strlen(uri));
        result = g_string_append(result, "(file test):");
        result = g_string_append(result, pos + strlen("/LayoutTests/"));
        printf("%s", result->str);
        g_string_free(result, TRUE);
    } else
        printf("%s", uri);

    g_free(uriScheme);

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

static void dumpBackForwardListForAllWebViews()
{
    // Dump the back forward list of the main WebView first
    dumpBackForwardListForWebView(webView);

    // The view list is prepended. Reverse the list so we get the order right.
    GSList* viewList = g_slist_reverse(webViewList);
    for (unsigned i = 0; i < g_slist_length(viewList); ++i)
        dumpBackForwardListForWebView(WEBKIT_WEB_VIEW(g_slist_nth_data(viewList, i)));
}

static void invalidateAnyPreviousWaitToDumpWatchdog()
{
    if (waitToDumpWatchdog) {
        g_source_remove(waitToDumpWatchdog);
        waitToDumpWatchdog = 0;
    }

    waitForPolicy = false;
}

static void resetDefaultsToConsistentValues()
{
    WebKitWebSettings* settings = webkit_web_view_get_settings(webView);
    g_object_set(G_OBJECT(settings),
                 "enable-private-browsing", FALSE,
                 "enable-developer-extras", FALSE,
                 "enable-spell-checking", TRUE,
                 "enable-html5-database", TRUE,
                 "enable-html5-local-storage", TRUE,
                 "enable-xss-auditor", FALSE,
                 "enable-spatial-navigation", FALSE,
                 "javascript-can-access-clipboard", TRUE,
                 "javascript-can-open-windows-automatically", TRUE,
                 "enable-offline-web-application-cache", TRUE,
                 "enable-universal-access-from-file-uris", TRUE,
                 "enable-scripts", TRUE,
                 "enable-dom-paste", TRUE,
                 "default-font-family", "Times",
                 "monospace-font-family", "Courier",
                 "serif-font-family", "Times",
                 "sans-serif-font-family", "Helvetica",
                 "default-font-size", 16,
                 "default-monospace-font-size", 13,
                 "minimum-font-size", 1,
                 "enable-caret-browsing", FALSE,
                 "enable-page-cache", FALSE,
                 "auto-resize-window", TRUE,
                 "enable-java-applet", FALSE,
                 "enable-plugins", TRUE,
                 "enable-hyperlink-auditing", FALSE,
                 "editing-behavior", WEBKIT_EDITING_BEHAVIOR_MAC,
                 NULL);

    webkit_web_frame_clear_main_frame_name(mainFrame);

    WebKitWebInspector* inspector = webkit_web_view_get_inspector(webView);
    g_object_set(G_OBJECT(inspector), "javascript-profiling-enabled", FALSE, NULL);

    webkit_web_view_set_zoom_level(webView, 1.0);

    webkit_reset_origin_access_white_lists();

#ifdef HAVE_LIBSOUP_2_29_90
    SoupSession* session = webkit_get_default_session();
    SoupCookieJar* jar = reinterpret_cast<SoupCookieJar*>(soup_session_get_feature(session, SOUP_TYPE_COOKIE_JAR));

    // We only create the jar when the soup backend needs to do
    // HTTP. Should we initialize it earlier, perhaps?
    if (jar)
        g_object_set(G_OBJECT(jar), SOUP_COOKIE_JAR_ACCEPT_POLICY, SOUP_COOKIE_JAR_ACCEPT_NO_THIRD_PARTY, NULL);
#endif

    setlocale(LC_ALL, "");
}

static bool useLongRunningServerMode(int argc, char *argv[])
{
    // This assumes you've already called getopt_long
    return (argc == optind+1 && !strcmp(argv[optind], "-"));
}

static void runTestingServerLoop()
{
    // When DumpRenderTree runs in server mode, we just wait around for file names
    // to be passed to us and read each in turn, passing the results back to the client
    char filenameBuffer[2048];
    while (fgets(filenameBuffer, sizeof(filenameBuffer), stdin)) {
        char* newLineCharacter = strchr(filenameBuffer, '\n');
        if (newLineCharacter)
            *newLineCharacter = '\0';

        if (!strlen(filenameBuffer))
            continue;

        runTest(filenameBuffer);
    }
}

static void initializeGlobalsFromCommandLineOptions(int argc, char *argv[])
{
    struct option options[] = {
        {"notree", no_argument, &dumpTree, false},
        {"pixel-tests", no_argument, &dumpPixels, true},
        {"tree", no_argument, &dumpTree, true},
        {NULL, 0, NULL, 0}
    };
    
    int option;
    while ((option = getopt_long(argc, (char * const *)argv, "", options, NULL)) != -1) {
        switch (option) {
        case '?': // unknown or ambiguous option
        case ':': // missing argument
            exit(1);
            break;
        }
    }
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
        else {
            // Widget resizing is done asynchronously in GTK+. We pump the main
            // loop here, to flush any pending resize requests. This prevents
            // timing issues which affect the size of elements in the output.
            // We only enable this workaround for tests that print the render tree
            // because this seems to break some dumpAsText tests: see bug 39988
            // After fixing that test, we should apply this approach to all dumps.
            while (gtk_events_pending())
                gtk_main_iteration();

            result = webkit_web_frame_dump_render_tree(mainFrame);
        }

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

            if (gLayoutTestController->dumpBackForwardList())
                dumpBackForwardListForAllWebViews();
        }

        if (printSeparators) {
            puts("#EOF"); // terminate the content block
            fputs("#EOF\n", stderr);
            fflush(stdout);
            fflush(stderr);
        }
    }

    if (dumpPixels
     && gLayoutTestController->generatePixelResults()
     && !gLayoutTestController->dumpDOMAsWebArchive()
     && !gLayoutTestController->dumpSourceAsWebArchive())
        dumpWebViewAsPixelsAndCompareWithExpected(gLayoutTestController->expectedPixelHash());

    // FIXME: call displayWebView here when we support --paint

    done = true;
    gtk_main_quit();
}

static void setDefaultsToConsistentStateValuesForTesting()
{
    gdk_screen_set_resolution(gdk_screen_get_default(), 72.0);

    resetDefaultsToConsistentValues();

    /* Disable the default auth dialog for testing */
    SoupSession* session = webkit_get_default_session();
    soup_session_remove_feature_by_type(session, WEBKIT_TYPE_SOUP_AUTH_DIALOG);

#if PLATFORM(X11)
    webkit_web_settings_add_extra_plugin_directory(webView, TEST_PLUGIN_DIR);
#endif

    gchar* databaseDirectory = g_build_filename(g_get_user_data_dir(), "gtkwebkitdrt", "databases", NULL);
    webkit_set_web_database_directory_path(databaseDirectory);
    g_free(databaseDirectory);
}

static void sendPixelResultsEOF()
{
    puts("#EOF");

    fflush(stdout);
    fflush(stderr);
}

static void runTest(const string& testPathOrURL)
{
    ASSERT(!testPathOrURL.empty());

    // Look for "'" as a separator between the path or URL, and the pixel dump hash that follows.
    string testURL(testPathOrURL);
    string expectedPixelHash;
    size_t separatorPos = testURL.find("'");
    if (separatorPos != string::npos) {
        testURL = string(testPathOrURL, 0, separatorPos);
        expectedPixelHash = string(testPathOrURL, separatorPos + 1);
    }

    // Convert the path into a full file URL if it does not look
    // like an HTTP/S URL (doesn't start with http:// or https://).
    if (testURL.find("http://") && testURL.find("https://")) {
        GFile* testFile = g_file_new_for_path(testURL.c_str());
        gchar* testURLCString = g_file_get_uri(testFile);
        testURL = testURLCString;
        g_free(testURLCString);
        g_object_unref(testFile);
    }

    resetDefaultsToConsistentValues();

    gLayoutTestController = LayoutTestController::create(testURL, expectedPixelHash);
    topLoadingFrame = 0;
    done = false;

    gLayoutTestController->setIconDatabaseEnabled(false);

    if (shouldLogFrameLoadDelegates(testURL))
        gLayoutTestController->setDumpFrameLoadCallbacks(true);

    if (shouldEnableDeveloperExtras(testURL)) {
        gLayoutTestController->setDeveloperExtrasEnabled(true);
        if (shouldOpenWebInspector(testURL))
            gLayoutTestController->showWebInspector();
    }

    WorkQueue::shared()->clear();
    WorkQueue::shared()->setFrozen(false);

    bool isSVGW3CTest = (testURL.find("svg/W3C-SVG-1.1") != string::npos);
    GtkAllocation size;
    size.x = size.y = 0;
    size.width = isSVGW3CTest ? 480 : LayoutTestController::maxViewWidth;
    size.height = isSVGW3CTest ? 360 : LayoutTestController::maxViewHeight;
    gtk_window_resize(GTK_WINDOW(window), size.width, size.height);
    gtk_widget_size_allocate(container, &size);

    if (prevTestBFItem)
        g_object_unref(prevTestBFItem);
    WebKitWebBackForwardList* bfList = webkit_web_view_get_back_forward_list(webView);
    prevTestBFItem = webkit_web_back_forward_list_get_current_item(bfList);
    if (prevTestBFItem)
        g_object_ref(prevTestBFItem);

    initializeFonts();

    // Focus the web view before loading the test to avoid focusing problems
    gtk_widget_grab_focus(GTK_WIDGET(webView));
    webkit_web_view_open(webView, testURL.c_str());

    gtk_main();

    // If developer extras enabled Web Inspector may have been open by the test.
    if (shouldEnableDeveloperExtras(testURL)) {
        gLayoutTestController->closeWebInspector();
        gLayoutTestController->setDeveloperExtrasEnabled(false);
    }

    // Also check if we still have opened webViews and free them.
    if (gLayoutTestController->closeRemainingWindowsWhenComplete() || webViewList) {
        while (webViewList) {
            g_object_unref(WEBKIT_WEB_VIEW(webViewList->data));
            webViewList = g_slist_next(webViewList);
        }
        g_slist_free(webViewList);
        webViewList = 0;
    }

    // A blank load seems to be necessary to reset state after certain tests.
    webkit_web_view_open(webView, "about:blank");

    gLayoutTestController.clear();

    // terminate the (possibly empty) pixels block after all the state reset
    sendPixelResultsEOF();
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

static char* getFrameNameSuitableForTestResult(WebKitWebView* view, WebKitWebFrame* frame)
{
    char* frameName = g_strdup(webkit_web_frame_get_name(frame));

    if (frame == webkit_web_view_get_main_frame(view)) {
        // This is a bit strange. Shouldn't web_frame_get_name return NULL?
        if (frameName && (frameName[0] != '\0')) {
            char* tmp = g_strdup_printf("main frame \"%s\"", frameName);
            g_free(frameName);
            frameName = tmp;
        } else {
            g_free(frameName);
            frameName = g_strdup("main frame");
        }
    } else if (!frameName || (frameName[0] == '\0')) {
        g_free(frameName);
        frameName = g_strdup("frame (anonymous)");
    } else {
        char* tmp = g_strdup_printf("frame \"%s\"", frameName);
        g_free(frameName);
        frameName = tmp;
    }

    return frameName;
}

static void webViewLoadCommitted(WebKitWebView* view, WebKitWebFrame* frame, void*)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        char* frameName = getFrameNameSuitableForTestResult(view, frame);
        printf("%s - didCommitLoadForFrame\n", frameName);
        g_free(frameName);
    }
}


static void webViewLoadFinished(WebKitWebView* view, WebKitWebFrame* frame, void*)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        char* frameName = getFrameNameSuitableForTestResult(view, frame);
        printf("%s - didFinishLoadForFrame\n", frameName);
        g_free(frameName);
    }

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

static void webViewDocumentLoadFinished(WebKitWebView* view, WebKitWebFrame* frame, void*)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        char* frameName = getFrameNameSuitableForTestResult(view, frame);
        printf("%s - didFinishDocumentLoadForFrame\n", frameName);
        g_free(frameName);
    } else if (!done) {
        guint pendingFrameUnloadEvents = webkit_web_frame_get_pending_unload_event_count(frame);
        if (pendingFrameUnloadEvents) {
            char* frameName = getFrameNameSuitableForTestResult(view, frame);
            printf("%s - has %u onunload handler(s)\n", frameName, pendingFrameUnloadEvents);
            g_free(frameName);
        }
    }
}

static void webViewOnloadEvent(WebKitWebView* view, WebKitWebFrame* frame, void*)
{
    if (!done && gLayoutTestController->dumpFrameLoadCallbacks()) {
        char* frameName = getFrameNameSuitableForTestResult(view, frame);
        printf("%s - didHandleOnloadEventsForFrame\n", frameName);
        g_free(frameName);
    }
}

static void webViewWindowObjectCleared(WebKitWebView* view, WebKitWebFrame* frame, JSGlobalContextRef context, JSObjectRef windowObject, gpointer data)
{
    JSValueRef exception = 0;
    ASSERT(gLayoutTestController);

    gLayoutTestController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    gcController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    axController->makeWindowObject(context, windowObject, &exception);
    ASSERT(!exception);

    JSStringRef eventSenderStr = JSStringCreateWithUTF8CString("eventSender");
    JSValueRef eventSender = makeEventSender(context, !webkit_web_frame_get_parent(frame));
    JSObjectSetProperty(context, windowObject, eventSenderStr, eventSender, kJSPropertyAttributeReadOnly | kJSPropertyAttributeDontDelete, 0);
    JSStringRelease(eventSenderStr);
}

static gboolean webViewConsoleMessage(WebKitWebView* view, const gchar* message, unsigned int line, const gchar* sourceId, gpointer data)
{
    gchar* testMessage = 0;
    const gchar* uriScheme;

    // Tests expect only the filename part of local URIs
    uriScheme = g_strstr_len(message, -1, "file://");
    if (uriScheme) {
        GString* tempString = g_string_sized_new(strlen(message));
        gchar* filename = g_strrstr(uriScheme, G_DIR_SEPARATOR_S);

        if (filename) {
            filename += strlen(G_DIR_SEPARATOR_S);
            tempString = g_string_append_len(tempString, message, (uriScheme - message));
            tempString = g_string_append_len(tempString, filename, strlen(filename));
            testMessage = g_string_free(tempString, FALSE);
        }
    }

    fprintf(stdout, "CONSOLE MESSAGE: line %d: %s\n", line, testMessage ? testMessage : message);
    g_free(testMessage);

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

static gboolean webViewClose(WebKitWebView* view)
{
    ASSERT(view);

    webViewList = g_slist_remove(webViewList, view);
    g_object_unref(view);

    return TRUE;
}

static void databaseQuotaExceeded(WebKitWebView* view, WebKitWebFrame* frame, WebKitWebDatabase *database)
{
    ASSERT(view);
    ASSERT(frame);
    ASSERT(database);

    WebKitSecurityOrigin* origin = webkit_web_database_get_security_origin(database);
    if (gLayoutTestController->dumpDatabaseCallbacks()) {
        printf("UI DELEGATE DATABASE CALLBACK: exceededDatabaseQuotaForSecurityOrigin:{%s, %s, %i} database:%s\n",
            webkit_security_origin_get_protocol(origin),
            webkit_security_origin_get_host(origin),
            webkit_security_origin_get_port(origin),
            webkit_web_database_get_name(database));
    }
    webkit_security_origin_set_web_database_quota(origin, 5 * 1024 * 1024);
}

static bool
geolocationPolicyDecisionRequested(WebKitWebView*, WebKitWebFrame*, WebKitGeolocationPolicyDecision* decision)
{
    if (!gLayoutTestController->isGeolocationPermissionSet())
        return FALSE;
    if (gLayoutTestController->geolocationPermission())
        webkit_geolocation_policy_allow(decision);
    else
        webkit_geolocation_policy_deny(decision);

    return TRUE;
}


static WebKitWebView* webViewCreate(WebKitWebView*, WebKitWebFrame*);

static gboolean webInspectorShowWindow(WebKitWebInspector*, gpointer data)
{
    gtk_window_set_default_size(GTK_WINDOW(webInspectorWindow), 800, 600);
    gtk_widget_show_all(webInspectorWindow);
    return TRUE;
}

static gboolean webInspectorCloseWindow(WebKitWebInspector*, gpointer data)
{
    gtk_widget_destroy(webInspectorWindow);
    webInspectorWindow = 0;
    return TRUE;
}

static WebKitWebView* webInspectorInspectWebView(WebKitWebInspector*, gpointer data)
{
    webInspectorWindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);

    GtkWidget* webView = webkit_web_view_new();
    gtk_container_add(GTK_CONTAINER(webInspectorWindow),
                      webView);

    return WEBKIT_WEB_VIEW(webView);
}

static void webViewLoadStatusNotified(WebKitWebView* view, gpointer user_data)
{
    WebKitLoadStatus loadStatus = webkit_web_view_get_load_status(view);

    if (gLayoutTestController->dumpFrameLoadCallbacks()) {
        if (loadStatus == WEBKIT_LOAD_PROVISIONAL) {
            char* frameName = getFrameNameSuitableForTestResult(view, mainFrame);
            printf("%s - didStartProvisionalLoadForFrame\n", frameName);
            g_free(frameName);
        }
    }
}

static WebKitWebView* createWebView()
{
    WebKitWebView* view = WEBKIT_WEB_VIEW(webkit_web_view_new());

    // From bug 11756: Use a frame group name for all WebViews created by
    // DumpRenderTree to allow testing of cross-page frame lookup.
    webkit_web_view_set_group_name(view, "org.webkit.gtk.DumpRenderTree");

    g_object_connect(G_OBJECT(view),
                     "signal::load-started", webViewLoadStarted, 0,
                     "signal::load-finished", webViewLoadFinished, 0,
                     "signal::load-committed", webViewLoadCommitted, 0,
                     "signal::window-object-cleared", webViewWindowObjectCleared, 0,
                     "signal::console-message", webViewConsoleMessage, 0,
                     "signal::script-alert", webViewScriptAlert, 0,
                     "signal::script-prompt", webViewScriptPrompt, 0,
                     "signal::script-confirm", webViewScriptConfirm, 0,
                     "signal::title-changed", webViewTitleChanged, 0,
                     "signal::navigation-policy-decision-requested", webViewNavigationPolicyDecisionRequested, 0,
                     "signal::status-bar-text-changed", webViewStatusBarTextChanged, 0,
                     "signal::create-web-view", webViewCreate, 0,
                     "signal::close-web-view", webViewClose, 0,
                     "signal::database-quota-exceeded", databaseQuotaExceeded, 0,
                     "signal::document-load-finished", webViewDocumentLoadFinished, 0,
                     "signal::geolocation-policy-decision-requested", geolocationPolicyDecisionRequested, 0,
                     "signal::onload-event", webViewOnloadEvent, 0,
                     "signal::drag-begin", dragBeginCallback, 0,
                     "signal::drag-end", dragEndCallback, 0,
                     "signal::drag-failed", dragFailedCallback, 0,

                     NULL);

    g_signal_connect(view,
                     "notify::load-status", G_CALLBACK(webViewLoadStatusNotified),
                     NULL);

    WebKitWebInspector* inspector = webkit_web_view_get_inspector(view);
    g_object_connect(G_OBJECT(inspector),
                     "signal::inspect-web-view", webInspectorInspectWebView, 0,
                     "signal::show-window", webInspectorShowWindow, 0,
                     "signal::close-window", webInspectorCloseWindow, 0,
                     NULL);

    if (webView) {
        WebKitWebSettings* settings = webkit_web_view_get_settings(webView);
        webkit_web_view_set_settings(view, settings);
    }

    return view;
}

static WebKitWebView* webViewCreate(WebKitWebView* view, WebKitWebFrame* frame)
{
    if (!gLayoutTestController->canOpenWindows())
        return 0;

    // Make sure that waitUntilDone has been called.
    ASSERT(gLayoutTestController->waitToDump());

    WebKitWebView* newWebView = createWebView();
    g_object_ref_sink(G_OBJECT(newWebView));
    webViewList = g_slist_prepend(webViewList, newWebView);
    return newWebView;
}

static void logHandler(const gchar* domain, GLogLevelFlags level, const gchar* message, gpointer data)
{
    if (level < G_LOG_LEVEL_DEBUG)
        fprintf(stderr, "%s\n", message);
}

int main(int argc, char* argv[])
{
    g_thread_init(NULL);
    gtk_init(&argc, &argv);

    // Some plugins might try to use the GLib logger for printing debug
    // messages. This will cause tests to fail because of unexpected output.
    // We squelch all debug messages sent to the logger.
    g_log_set_default_handler(logHandler, 0);

    initializeGlobalsFromCommandLineOptions(argc, argv);
    initializeFonts();

    window = gtk_window_new(GTK_WINDOW_POPUP);
    container = GTK_WIDGET(gtk_scrolled_window_new(NULL, NULL));
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(container), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(window), container);
    gtk_widget_show_all(window);

    webView = createWebView();
    gtk_container_add(GTK_CONTAINER(container), GTK_WIDGET(webView));
    gtk_widget_realize(GTK_WIDGET(webView));
    gtk_widget_show_all(container);
    gtk_widget_grab_focus(GTK_WIDGET(webView));
    mainFrame = webkit_web_view_get_main_frame(webView);

    setDefaultsToConsistentStateValuesForTesting();

    gcController = new GCController();
    axController = new AccessibilityController();

    if (useLongRunningServerMode(argc, argv)) {
        printSeparators = true;
        runTestingServerLoop();
    } else {
        printSeparators = (optind < argc-1 || (dumpPixels && dumpTree));
        for (int i = optind; i != argc; ++i)
            runTest(argv[i]);
    }

    delete gcController;
    gcController = 0;

    delete axController;
    axController = 0;

    gtk_widget_destroy(window);

    return 0;
}
