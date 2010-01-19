/*
 * Copyright (C) 2009 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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

#include <errno.h>
#include <unistd.h>
#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#if GLIB_CHECK_VERSION(2, 16, 0) && GTK_CHECK_VERSION(2, 14, 0)

static const char* contents = "<html><body><p>This is a test. This is the second sentence. And this the third.</p></body></html>";

static const char* contentsWithNewlines = "<html><body><p>This is a test. \n\nThis\n is the second sentence. And this the third.</p></body></html>";

static const char* contentsInTextarea = "<html><body><textarea cols='80'>This is a test. This is the second sentence. And this the third.</textarea></body></html>";

static const char* contentsInTextInput = "<html><body><input type='text' size='80' value='This is a test. This is the second sentence. And this the third.'/></body></html>";

static const char* contentsInParagraphAndBodySimple = "<html><body><p>This is a test.</p>Hello world.</body></html>";

static const char* contentsInParagraphAndBodyModerate = "<html><body><p>This is a test.</p>Hello world.<br /><font color='#00cc00'>This sentence is green.</font><br />This one is not.</body></html>";

static gboolean bail_out(GMainLoop* loop)
{
    if (g_main_loop_is_running(loop))
        g_main_loop_quit(loop);

    return FALSE;
}

typedef gchar* (*AtkGetTextFunction) (AtkText*, gint, AtkTextBoundary, gint*, gint*);

static void test_get_text_function(AtkText* text_obj, AtkGetTextFunction fn, AtkTextBoundary boundary, gint offset, const char* text_result, gint start_offset_result, gint end_offset_result)
{
    gint start_offset, end_offset;
    char* text;

    text = fn(text_obj, offset, boundary, &start_offset, &end_offset);
    g_assert_cmpstr(text, ==, text_result);
    g_assert_cmpint(start_offset, ==, start_offset_result);
    g_assert_cmpint(end_offset, ==, end_offset_result);
    g_free(text);
}

static void run_get_text_tests(AtkText* text_obj)
{
    char* text = atk_text_get_text(text_obj, 0, -1);
    g_assert_cmpstr(text, ==, "This is a test. This is the second sentence. And this the third.");
    g_free(text);

    /* ATK_TEXT_BOUNDARY_CHAR */
    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_CHAR,
                           0, "T", 0, 1);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_CHAR,
                           0, "h", 1, 2);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_CHAR,
                           0, "", 0, 0);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_CHAR,
                           1, "T", 0, 1);
    
    /* ATK_TEXT_BOUNDARY_WORD_START */
    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_WORD_START,
                           0, "This ", 0, 5);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_WORD_START,
                           4, "This ", 0, 5);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_WORD_START,
                           10, "test. ", 10, 16);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_WORD_START,
                           58, "third.", 58, 64);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_WORD_START,
                           5, "This ", 0, 5);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_WORD_START,
                           7, "This ", 0, 5);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_WORD_START,
                           0, "is ", 5, 8);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_WORD_START,
                           4, "is ", 5, 8);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_WORD_START,
                           3, "is ", 5, 8);

    /* ATK_TEXT_BOUNDARY_WORD_END */
    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           0, "This", 0, 4);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           4, " is", 4, 7);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           5, " is", 4, 7);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           9, " test", 9, 14);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           5, "This", 0, 4);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           4, "This", 0, 4);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           7, " is", 4, 7);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           5, " a", 7, 9);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           4, " a", 7, 9);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_WORD_END,
                           58, " third", 57, 63);

    /* ATK_TEXT_BOUNDARY_SENTENCE_START */
    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_SENTENCE_START,
                           0, "This is a test. ", 0, 16);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_SENTENCE_START,
                           15, "This is a test. ", 0, 16);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_SENTENCE_START,
                           0, "This is the second sentence. ", 16, 45);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_SENTENCE_START,
                           15, "This is the second sentence. ", 16, 45);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_SENTENCE_START,
                           16, "This is a test. ", 0, 16);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_SENTENCE_START,
                           44, "This is a test. ", 0, 16);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_SENTENCE_START,
                           15, "", 0, 0);

    /* ATK_TEXT_BOUNDARY_SENTENCE_END */
    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           0, "This is a test.", 0, 15);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           15, " This is the second sentence.", 15, 44);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           16, " This is the second sentence.", 15, 44);

    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           17, " This is the second sentence.", 15, 44);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           0, " This is the second sentence.", 15, 44);

    test_get_text_function(text_obj, atk_text_get_text_after_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           15, " And this the third.", 44, 64);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           16, "This is a test.", 0, 15);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           15, "This is a test.", 0, 15);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           14, "", 0, 0);

    test_get_text_function(text_obj, atk_text_get_text_before_offset, ATK_TEXT_BOUNDARY_SENTENCE_END,
                           44, " This is the second sentence.", 15, 44);

    /* It's trick to test these properly right now, since our a11y
       implementation splits different lines in different a11y
       items */
    /* ATK_TEXT_BOUNDARY_LINE_START */
    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_LINE_START,
                           0, "This is a test. This is the second sentence. And this the third.", 0, 64);

    /* ATK_TEXT_BOUNDARY_LINE_END */
    test_get_text_function(text_obj, atk_text_get_text_at_offset, ATK_TEXT_BOUNDARY_LINE_END,
                           0, "This is a test. This is the second sentence. And this the third.", 0, 64);
}

static void test_webkit_atk_get_text_at_offset_forms(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    GMainLoop* loop;
    AtkText* text_obj;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, contents, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_timeout_add(100, (GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    /* Get to the inner AtkText object */
    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);
    obj = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj);

    text_obj = ATK_TEXT(obj);
    g_assert(ATK_IS_TEXT(text_obj));

    run_get_text_tests(text_obj);

    g_object_unref(webView);
}

static void test_webkit_atk_get_text_at_offset(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    GMainLoop* loop;
    AtkText* text_obj;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, contents, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_timeout_add(100, (GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    /* Get to the inner AtkText object */
    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);
    obj = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj);

    text_obj = ATK_TEXT(obj);
    g_assert(ATK_IS_TEXT(text_obj));

    run_get_text_tests(text_obj);

    g_object_unref(webView);
}

static void test_webkit_atk_get_text_at_offset_newlines(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    GMainLoop* loop;
    AtkText* text_obj;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, contentsWithNewlines, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_timeout_add(100, (GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    /* Get to the inner AtkText object */
    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);
    obj = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj);

    text_obj = ATK_TEXT(obj);
    g_assert(ATK_IS_TEXT(text_obj));

    run_get_text_tests(text_obj);

    g_object_unref(webView);
}

static void test_webkit_atk_get_text_at_offset_textarea(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    GMainLoop* loop;
    AtkText* text_obj;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, contentsInTextarea, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_timeout_add(100, (GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    /* Get to the inner AtkText object */
    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);
    obj = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj);
    obj = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj);

    text_obj = ATK_TEXT(obj);
    g_assert(ATK_IS_TEXT(text_obj));

    run_get_text_tests(text_obj);

    g_object_unref(webView);
}

static void test_webkit_atk_get_text_at_offset_text_input(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    GMainLoop* loop;
    AtkText* text_obj;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, contentsInTextInput, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_timeout_add(100, (GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    /* Get to the inner AtkText object */
    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);
    obj = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj);
    obj = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj);

    text_obj = ATK_TEXT(obj);
    g_assert(ATK_IS_TEXT(text_obj));

    run_get_text_tests(text_obj);

    g_object_unref(webView);
}

static void testWebkitAtkGetTextInParagraphAndBodySimple(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    AtkObject* obj1;
    AtkObject* obj2;
    GMainLoop* loop;
    AtkText* textObj1;
    AtkText* textObj2;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, contentsInParagraphAndBodySimple, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_timeout_add(100, (GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    /* Get to the inner AtkText object */
    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);
    obj1 = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj1);
    obj2 = atk_object_ref_accessible_child(obj, 1);
    g_assert(obj2);

    textObj1 = ATK_TEXT(obj1);
    g_assert(ATK_IS_TEXT(textObj1));
    textObj2 = ATK_TEXT(obj2);
    g_assert(ATK_IS_TEXT(textObj2));

    char *text = atk_text_get_text(textObj1, 0, -1);
    g_assert_cmpstr(text, ==, "This is a test.");

    text = atk_text_get_text(textObj2, 0, 12);
    g_assert_cmpstr(text, ==, "Hello world.");

    g_object_unref(obj1);
    g_object_unref(obj2);
    g_object_unref(webView);
}

static void testWebkitAtkGetTextInParagraphAndBodyModerate(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    AtkObject* obj1;
    AtkObject* obj2;
    GMainLoop* loop;
    AtkText* textObj1;
    AtkText* textObj2;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, contentsInParagraphAndBodyModerate, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_timeout_add(100, (GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    /* Get to the inner AtkText object */
    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);
    obj1 = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj1);
    obj2 = atk_object_ref_accessible_child(obj, 1);
    g_assert(obj2);

    textObj1 = ATK_TEXT(obj1);
    g_assert(ATK_IS_TEXT(textObj1));
    textObj2 = ATK_TEXT(obj2);
    g_assert(ATK_IS_TEXT(textObj2));

    char *text = atk_text_get_text(textObj1, 0, -1);
    g_assert_cmpstr(text, ==, "This is a test.");

    text = atk_text_get_text(textObj2, 0, 53);
    g_assert_cmpstr(text, ==, "Hello world.\nThis sentence is green.\nThis one is not.");

    g_object_unref(obj1);
    g_object_unref(obj2);
    g_object_unref(webView);
}

int main(int argc, char** argv)
{
    g_thread_init(NULL);
    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");
    g_test_add_func("/webkit/atk/get_text_at_offset", test_webkit_atk_get_text_at_offset);
    g_test_add_func("/webkit/atk/get_text_at_offset_forms", test_webkit_atk_get_text_at_offset_forms);
    g_test_add_func("/webkit/atk/get_text_at_offset_newlines", test_webkit_atk_get_text_at_offset_newlines);
    g_test_add_func("/webkit/atk/get_text_at_offset_textarea", test_webkit_atk_get_text_at_offset_textarea);
    g_test_add_func("/webkit/atk/get_text_at_offset_text_input", test_webkit_atk_get_text_at_offset_text_input);
    g_test_add_func("/webkit/atk/getTextInParagraphAndBodySimple", testWebkitAtkGetTextInParagraphAndBodySimple);
    g_test_add_func("/webkit/atk/getTextInParagraphAndBodyModerate", testWebkitAtkGetTextInParagraphAndBodyModerate);
    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
