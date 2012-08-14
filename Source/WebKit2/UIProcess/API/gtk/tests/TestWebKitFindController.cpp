/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"

#include "LoadTrackingTest.h"
#include <gtk/gtk.h>
#include <webkit2/webkit2.h>
#include <wtf/gobject/GRefPtr.h>

static const char* testString = "<html><body>first testing second testing secondHalf</body></html>";

class FindControllerTest: public WebViewTest {
public:
    MAKE_GLIB_TEST_FIXTURE(FindControllerTest);

    FindControllerTest()
        : m_findController(webkit_web_view_get_find_controller(m_webView))
        , m_runFindUntilCompletion(false)
    {
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(m_findController.get()));
    }

    ~FindControllerTest()
    {
        if (m_findController)
            g_signal_handlers_disconnect_matched(m_findController.get(), G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, this);
    }

    void find(const char* searchText, guint32 findOptions, guint maxMatchCount)
    {
        g_signal_connect(m_findController.get(), "found-text", G_CALLBACK(foundTextCallback), this);
        g_signal_connect(m_findController.get(), "failed-to-find-text", G_CALLBACK(failedToFindTextCallback), this);
        webkit_find_controller_search(m_findController.get(), searchText, findOptions, maxMatchCount);
    }

    void count(const char* searchText, guint32 findOptions, guint maxMatchCount)
    {
        g_signal_connect(m_findController.get(), "counted-matches", G_CALLBACK(countedMatchesCallback), this);
        webkit_find_controller_count_matches(m_findController.get(), searchText, findOptions, maxMatchCount);
    }

    void waitUntilFindFinished()
    {
        m_runFindUntilCompletion = true;
        g_main_loop_run(m_mainLoop);
    }

    void waitUntilWebViewDrawSignal()
    {
        g_signal_connect_after(m_webView, "draw", G_CALLBACK(webViewDraw), this);
        g_main_loop_run(m_mainLoop);
    }

    GRefPtr<WebKitFindController> m_findController;
    bool m_textFound;
    unsigned m_matchCount;

private:
    bool m_runFindUntilCompletion;

    static void webViewDraw(GtkWidget *widget, cairo_t *cr, FindControllerTest* test)
    {
        g_main_loop_quit(test->m_mainLoop);
        g_signal_handlers_disconnect_matched(widget, G_SIGNAL_MATCH_DATA, 0, 0, 0, 0, test);
    }

    static void foundTextCallback(WebKitFindController*, guint matchCount, FindControllerTest* test)
    {
        test->m_textFound = true;
        test->m_matchCount = matchCount;
        if (test->m_runFindUntilCompletion)
            g_main_loop_quit(test->m_mainLoop);
    }

    static void failedToFindTextCallback(WebKitFindController*, FindControllerTest* test)
    {
        test->m_textFound = false;
        if (test->m_runFindUntilCompletion)
            g_main_loop_quit(test->m_mainLoop);
    }

    static void countedMatchesCallback(WebKitFindController*, guint matchCount, FindControllerTest* test)
    {
        test->m_matchCount = matchCount;
        if (test->m_runFindUntilCompletion)
            g_main_loop_quit(test->m_mainLoop);
    }
};

static void testFindControllerTextFound(FindControllerTest* test, gconstpointer)
{
    test->loadHtml(testString, 0);
    test->waitUntilLoadFinished();

    test->find("testing", WEBKIT_FIND_OPTIONS_NONE, 1);
    test->waitUntilFindFinished();

    g_assert(test->m_textFound);
}

static void testFindControllerTextNotFound(FindControllerTest* test, gconstpointer)
{
    test->loadHtml(testString, 0);
    test->waitUntilLoadFinished();

    test->find("notFound", WEBKIT_FIND_OPTIONS_NONE, 1);
    test->waitUntilFindFinished();

    g_assert(!test->m_textFound);
}

static void testFindControllerMatchCount(FindControllerTest* test, gconstpointer)
{
    test->loadHtml(testString, 0);
    test->waitUntilLoadFinished();

    test->find("testing", WEBKIT_FIND_OPTIONS_NONE, 2);
    test->waitUntilFindFinished();

    g_assert(test->m_matchCount == 2);
    g_assert(test->m_textFound);
}

static void testFindControllerMaxMatchCount(FindControllerTest* test, gconstpointer)
{
    test->loadHtml(testString, 0);
    test->waitUntilLoadFinished();

    test->find("testing", WEBKIT_FIND_OPTIONS_NONE, 1);
    test->waitUntilFindFinished();

    g_assert(test->m_matchCount == G_MAXUINT);
    g_assert(test->m_textFound);
}

static void testFindControllerNext(FindControllerTest* test, gconstpointer)
{
    test->loadHtml(testString, 0);
    test->waitUntilLoadFinished();

    test->find("testing", WEBKIT_FIND_OPTIONS_NONE, 2);
    test->waitUntilFindFinished();

    g_assert(test->m_textFound);
    g_assert(test->m_matchCount == 2);

    webkit_find_controller_search_next(test->m_findController.get());
    test->waitUntilFindFinished();

    g_assert(test->m_textFound);
    g_assert(test->m_matchCount == 1);
    g_assert(!(webkit_find_controller_get_options(test->m_findController.get()) & WEBKIT_FIND_OPTIONS_BACKWARDS));

    webkit_find_controller_search_next(test->m_findController.get());
    test->waitUntilFindFinished();

    g_assert(!test->m_textFound);
    g_assert(test->m_matchCount == 1);
    g_assert(!(webkit_find_controller_get_options(test->m_findController.get()) & WEBKIT_FIND_OPTIONS_BACKWARDS));
}

static void testFindControllerPrevious(FindControllerTest* test, gconstpointer)
{
    test->loadHtml(testString, 0);
    test->waitUntilLoadFinished();

    test->find("testing", WEBKIT_FIND_OPTIONS_NONE, 2);
    test->waitUntilFindFinished();

    g_assert(test->m_matchCount == 2);
    g_assert(test->m_textFound);

    webkit_find_controller_search_next(test->m_findController.get());
    test->waitUntilFindFinished();

    g_assert(test->m_textFound);
    g_assert(test->m_matchCount == 1);
    g_assert(!(webkit_find_controller_get_options(test->m_findController.get()) & WEBKIT_FIND_OPTIONS_BACKWARDS));

    webkit_find_controller_search_previous(test->m_findController.get());
    test->waitUntilFindFinished();

    g_assert(test->m_textFound);
    g_assert(test->m_matchCount == 1);
    g_assert(webkit_find_controller_get_options(test->m_findController.get()) & WEBKIT_FIND_OPTIONS_BACKWARDS);
}

static void testFindControllerCountedMatches(FindControllerTest* test, gconstpointer)
{
    test->loadHtml(testString, 0);
    test->waitUntilLoadFinished();

    test->count("testing", WEBKIT_FIND_OPTIONS_NONE, 2);
    test->waitUntilFindFinished();

    g_assert(test->m_matchCount == 2);

    test->count("first", WEBKIT_FIND_OPTIONS_NONE, 2);
    test->waitUntilFindFinished();

    g_assert(test->m_matchCount == 1);

    test->count("notFound", WEBKIT_FIND_OPTIONS_NONE, 2);
    test->waitUntilFindFinished();

    g_assert(!test->m_matchCount);
}

static void testFindControllerOptions(FindControllerTest* test, gconstpointer)
{
    test->loadHtml(testString, 0);
    test->waitUntilLoadFinished();

    test->find("Testing", WEBKIT_FIND_OPTIONS_NONE, 2);
    test->waitUntilFindFinished();

    g_assert(!test->m_textFound);

    test->find("Testing", WEBKIT_FIND_OPTIONS_CASE_INSENSITIVE, 2);
    test->waitUntilFindFinished();

    g_assert(test->m_textFound);

    test->find("esting", WEBKIT_FIND_OPTIONS_NONE, 2);
    test->waitUntilFindFinished();

    g_assert(test->m_textFound);

    test->find("esting", WEBKIT_FIND_OPTIONS_AT_WORD_STARTS, 2);
    test->waitUntilFindFinished();

    g_assert(!test->m_textFound);

    test->find("Half", WEBKIT_FIND_OPTIONS_AT_WORD_STARTS, 2);
    test->waitUntilFindFinished();

    g_assert(!test->m_textFound);

    test->find("Half", WEBKIT_FIND_OPTIONS_AT_WORD_STARTS | WEBKIT_FIND_OPTIONS_TREAT_MEDIAL_CAPITAL_AS_WORD_START, 2);
    test->waitUntilFindFinished();

    g_assert(test->m_textFound);

    test->find("testing", WEBKIT_FIND_OPTIONS_WRAP_AROUND, 3);
    test->waitUntilFindFinished();
    g_assert(test->m_textFound);

    webkit_find_controller_search_next(test->m_findController.get());
    test->waitUntilFindFinished();
    g_assert(test->m_textFound);

    webkit_find_controller_search_next(test->m_findController.get());
    test->waitUntilFindFinished();
    g_assert(test->m_textFound);
}

static gboolean gdkPixbufEqual(GdkPixbuf* firstPixbuf, GdkPixbuf* secondPixbuf)
{
    if (gdk_pixbuf_get_bits_per_sample(firstPixbuf) != gdk_pixbuf_get_bits_per_sample(secondPixbuf)
        || gdk_pixbuf_get_has_alpha(firstPixbuf) != gdk_pixbuf_get_has_alpha(secondPixbuf)
        || gdk_pixbuf_get_height(firstPixbuf) != gdk_pixbuf_get_height(secondPixbuf)
        || gdk_pixbuf_get_n_channels(firstPixbuf) != gdk_pixbuf_get_n_channels(secondPixbuf)
        || gdk_pixbuf_get_rowstride(firstPixbuf) != gdk_pixbuf_get_rowstride(secondPixbuf)
        || gdk_pixbuf_get_width(firstPixbuf) != gdk_pixbuf_get_width(secondPixbuf))
        return FALSE;

    int pixbufRowstride = gdk_pixbuf_get_rowstride(firstPixbuf);
    int pixbufHeight = gdk_pixbuf_get_height(firstPixbuf);
    int pixbufWidth = gdk_pixbuf_get_width(firstPixbuf);
    int numberOfChannels = gdk_pixbuf_get_n_channels(firstPixbuf);
    int bitsPerSample = gdk_pixbuf_get_bits_per_sample(firstPixbuf);

    // Last row can be of different length. Taken from gdk-pixbuf documentation.
    int totalLength = (pixbufHeight - 1) * pixbufRowstride \
        + pixbufWidth * ((numberOfChannels * bitsPerSample + 7) / 8);

    guchar* firstPixels = gdk_pixbuf_get_pixels(firstPixbuf);
    guchar* secondPixels = gdk_pixbuf_get_pixels(secondPixbuf);
    for (int i = 0; i < totalLength; i++)
        if (firstPixels[i] != secondPixels[i])
            return FALSE;

    return TRUE;
}

static void testFindControllerHide(FindControllerTest* test, gconstpointer)
{
    test->loadHtml(testString, 0);
    test->waitUntilLoadFinished();

    test->showInWindowAndWaitUntilMapped();
    int allocatedHeight = gtk_widget_get_allocated_height(GTK_WIDGET(test->m_webView));
    int allocatedWidth = gtk_widget_get_allocated_width(GTK_WIDGET(test->m_webView));
    GdkWindow* webViewGdkWindow = gtk_widget_get_window(GTK_WIDGET(test->m_webView));
    g_assert(webViewGdkWindow);

    test->waitUntilWebViewDrawSignal();
    GRefPtr<GdkPixbuf> originalPixbuf = gdk_pixbuf_get_from_window(webViewGdkWindow, 0, 0, allocatedHeight, allocatedWidth);
    g_assert(originalPixbuf);

    test->find("testing", WEBKIT_FIND_OPTIONS_NONE, 1);
    test->waitUntilFindFinished();
    g_assert(test->m_textFound);

    test->waitUntilWebViewDrawSignal();
    GRefPtr<GdkPixbuf> highlightPixbuf = gdk_pixbuf_get_from_window(webViewGdkWindow, 0, 0, allocatedHeight, allocatedWidth);
    g_assert(highlightPixbuf);
    g_assert(!gdkPixbufEqual(originalPixbuf.get(), highlightPixbuf.get()));

    WebKitFindController* findController = webkit_web_view_get_find_controller(test->m_webView);
    webkit_find_controller_search_finish(findController);
    webkit_web_view_execute_editing_command(test->m_webView, "Unselect");

    test->waitUntilWebViewDrawSignal();
    GRefPtr<GdkPixbuf> unhighlightPixbuf = gdk_pixbuf_get_from_window(webViewGdkWindow, 0, 0, allocatedHeight, allocatedWidth);
    g_assert(unhighlightPixbuf);
    g_assert(gdkPixbufEqual(originalPixbuf.get(), unhighlightPixbuf.get()));
}

static void testFindControllerInstance(FindControllerTest* test, gconstpointer)
{
    WebKitFindController* findController1 = webkit_web_view_get_find_controller(test->m_webView);
    WebKitFindController* findController2 = webkit_web_view_get_find_controller(test->m_webView);

    g_assert(findController1 == findController2);
}

static void testFindControllerGetters(FindControllerTest* test, gconstpointer)
{
    const char* searchText = "testing";
    guint maxMatchCount = 1;
    guint32 findOptions = WEBKIT_FIND_OPTIONS_WRAP_AROUND | WEBKIT_FIND_OPTIONS_AT_WORD_STARTS;
    WebKitFindController* findController = webkit_web_view_get_find_controller(test->m_webView);

    webkit_find_controller_search(findController, searchText, findOptions, maxMatchCount);
    g_assert(webkit_find_controller_get_web_view(findController) == test->m_webView);
    g_assert(!g_strcmp0(webkit_find_controller_get_search_text(findController), searchText));
    g_assert(webkit_find_controller_get_max_match_count(findController) == maxMatchCount);
    g_assert(webkit_find_controller_get_options(findController) == findOptions);
}

void beforeAll()
{
    FindControllerTest::add("WebKitFindController", "getters", testFindControllerGetters);
    FindControllerTest::add("WebKitFindController", "instance", testFindControllerInstance);
    FindControllerTest::add("WebKitFindController", "text-found", testFindControllerTextFound);
    FindControllerTest::add("WebKitFindController", "text-not-found", testFindControllerTextNotFound);
    FindControllerTest::add("WebKitFindController", "match-count", testFindControllerMatchCount);
    FindControllerTest::add("WebKitFindController", "max-match-count", testFindControllerMaxMatchCount);
    FindControllerTest::add("WebKitFindController", "next", testFindControllerNext);
    FindControllerTest::add("WebKitFindController", "previous", testFindControllerPrevious);
    FindControllerTest::add("WebKitFindController", "counted-matches", testFindControllerCountedMatches);
    FindControllerTest::add("WebKitFindController", "options", testFindControllerOptions);
    FindControllerTest::add("WebKitFindController", "hide", testFindControllerHide);
}

void afterAll()
{
}
