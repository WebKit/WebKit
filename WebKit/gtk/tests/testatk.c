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

static const char* centeredContents = "<html><body><p style='text-align: center;'>Short line</p><p style='text-align: center;'>Long-size line with some foo bar baz content</p><p style='text-align: center;'>Short line</p><p style='text-align: center;'>This is a multi-line paragraph<br />where the first line<br />is the biggest one</p></body></html>";

static const char* contents = "<html><body><p>This is a test. This is the second sentence. And this the third.</p></body></html>";

static const char* contentsWithNewlines = "<html><body><p>This is a test. \n\nThis\n is the second sentence. And this the third.</p></body></html>";

static const char* contentsInTextarea = "<html><body><textarea cols='80'>This is a test. This is the second sentence. And this the third.</textarea></body></html>";

static const char* contentsInTextInput = "<html><body><input type='text' size='80' value='This is a test. This is the second sentence. And this the third.'/></body></html>";

static const char* contentsInParagraphAndBodySimple = "<html><body><p>This is a test.</p>Hello world.</body></html>";

static const char* contentsInParagraphAndBodyModerate = "<html><body><p>This is a test.</p>Hello world.<br /><font color='#00cc00'>This sentence is green.</font><br />This one is not.</body></html>";

static const char* contentsInTable = "<html><body><table><tr><td>foo</td><td>bar</td></tr></table></body></html>";

static const char* contentsInTableWithHeaders = "<html><body><table><tr><th>foo</th><th>bar</th><th colspan='2'>baz</th></tr><tr><th>qux</th><td>1</td><td>2</td><td>3</td></tr><tr><th rowspan='2'>quux</th><td>4</td><td>5</td><td>6</td></tr><tr><td>6</td><td>7</td><td>8</td></tr><tr><th>corge</th><td>9</td><td>10</td><td>11</td></tr></table><table><tr><td>1</td><td>2</td></tr><tr><td>3</td><td>4</td></tr></table></body></html>";

static const char* formWithTextInputs = "<html><body><form><input type='text' name='entry' /></form></body></html>";

static const char* listsOfItems = "<html><body><ul><li>text only</li><li><a href='foo'>link only</a></li><li>text and a <a href='bar'>link</a></li></ul><ol><li>text only</li><li><a href='foo'>link only</a></li><li>text and a <a href='bar'>link</a></li></ol></body></html>";

static const char* textForSelections = "<html><body><p>A paragraph with plain text</p><p>A paragraph with <a href='http://webkit.org'>a link</a> in the middle</p></body></html>";

static const char* textWithAttributes = "<html><head><style>.st1 {font-family: monospace; color:rgb(120,121,122);} .st2 {text-decoration:underline; background-color:rgb(80,81,82);}</style></head><body><p style=\"font-size:14; text-align:right;\">This is the <i>first</i><b> sentence of this text.</b></p><p class=\"st1\">This sentence should have an style applied <span class=\"st2\">and this part should have another one</span>.</p><p>x<sub>1</sub><sup>2</sup>=x<sub>2</sub><sup>3</sup></p><p style=\"text-align:center;\">This sentence is the <strike>last</strike> one.</p></body></html>";

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

    g_idle_add((GSourceFunc)bail_out, loop);
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

    g_idle_add((GSourceFunc)bail_out, loop);
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

    g_idle_add((GSourceFunc)bail_out, loop);
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

    g_idle_add((GSourceFunc)bail_out, loop);
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

    g_idle_add((GSourceFunc)bail_out, loop);
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

    g_idle_add((GSourceFunc)bail_out, loop);
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

    g_idle_add((GSourceFunc)bail_out, loop);
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

static void testWebkitAtkGetTextInTable(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    GMainLoop* loop;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, contentsInTable, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_idle_add((GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);
    obj = atk_object_ref_accessible_child(obj, 0);
    g_assert(obj);

    /* Tables should not implement AtkText */
    g_assert(G_TYPE_INSTANCE_GET_INTERFACE(obj, ATK_TYPE_TEXT, AtkTextIface) == NULL);

    g_object_unref(obj);
    g_object_unref(webView);
}

static void testWebkitAtkGetHeadersInTable(void)
{
    WebKitWebView* webView;
    AtkObject* axWebView;
    AtkObject* table;
    AtkObject* colHeader;
    AtkObject* rowHeader;
    GMainLoop* loop;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, contentsInTableWithHeaders, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_idle_add((GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    axWebView = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(axWebView);

    // Check table with both column and row headers
    table = atk_object_ref_accessible_child(axWebView, 0);
    g_assert(table);
    g_assert(atk_object_get_role(table) == ATK_ROLE_TABLE);

    colHeader = atk_table_get_column_header(ATK_TABLE(table), 0);
    g_assert(colHeader);
    g_assert(atk_object_get_role(colHeader) == ATK_ROLE_TABLE_CELL);
    g_assert(atk_object_get_index_in_parent(colHeader) == 0);

    colHeader = atk_table_get_column_header(ATK_TABLE(table), 1);
    g_assert(colHeader);
    g_assert(atk_object_get_role(colHeader) == ATK_ROLE_TABLE_CELL);
    g_assert(atk_object_get_index_in_parent(colHeader) == 1);

    colHeader = atk_table_get_column_header(ATK_TABLE(table), 2);
    g_assert(colHeader);
    g_assert(atk_object_get_role(colHeader) == ATK_ROLE_TABLE_CELL);
    g_assert(atk_object_get_index_in_parent(colHeader) == 2);

    colHeader = atk_table_get_column_header(ATK_TABLE(table), 3);
    g_assert(colHeader);
    g_assert(atk_object_get_role(colHeader) == ATK_ROLE_TABLE_CELL);
    g_assert(atk_object_get_index_in_parent(colHeader) == 2);

    rowHeader = atk_table_get_row_header(ATK_TABLE(table), 0);
    g_assert(rowHeader);
    g_assert(atk_object_get_role(rowHeader) == ATK_ROLE_TABLE_CELL);
    g_assert(atk_object_get_index_in_parent(rowHeader) == 0);

    rowHeader = atk_table_get_row_header(ATK_TABLE(table), 1);
    g_assert(rowHeader);
    g_assert(atk_object_get_role(rowHeader) == ATK_ROLE_TABLE_CELL);
    g_assert(atk_object_get_index_in_parent(rowHeader) == 3);

    rowHeader = atk_table_get_row_header(ATK_TABLE(table), 2);
    g_assert(rowHeader);
    g_assert(atk_object_get_role(rowHeader) == ATK_ROLE_TABLE_CELL);
    g_assert(atk_object_get_index_in_parent(rowHeader) == 7);

    rowHeader = atk_table_get_row_header(ATK_TABLE(table), 3);
    g_assert(rowHeader);
    g_assert(atk_object_get_role(rowHeader) == ATK_ROLE_TABLE_CELL);
    g_assert(atk_object_get_index_in_parent(rowHeader) == 7);

    g_object_unref(table);

    // Check table with no headers at all
    table = atk_object_ref_accessible_child(axWebView, 1);
    g_assert(table);
    g_assert(atk_object_get_role(table) == ATK_ROLE_TABLE);

    colHeader = atk_table_get_column_header(ATK_TABLE(table), 0);
    g_assert(colHeader == 0);

    colHeader = atk_table_get_column_header(ATK_TABLE(table), 1);
    g_assert(colHeader == 0);

    rowHeader = atk_table_get_row_header(ATK_TABLE(table), 0);
    g_assert(rowHeader == 0);

    rowHeader = atk_table_get_row_header(ATK_TABLE(table), 1);
    g_assert(rowHeader == 0);

    g_object_unref(table);
    g_object_unref(webView);
}

static gint compAtkAttribute(AtkAttribute* a1, AtkAttribute* a2)
{
    gint strcmpVal;
    strcmpVal = g_strcmp0(a1->name, a2->name);
    if (strcmpVal)
        return strcmpVal;
    return g_strcmp0(a1->value, a2->value);
}

static gint compAtkAttributeName(AtkAttribute* a1, AtkAttribute* a2)
{
    return g_strcmp0(a1->name, a2->name);
}

static gboolean atkAttributeSetAttributeHasValue(AtkAttributeSet* set, AtkTextAttribute attribute, const gchar* value)
{
    GSList *element;
    AtkAttribute at;
    gboolean result;
    at.name = (gchar *)atk_text_attribute_get_name(attribute);
    element = g_slist_find_custom(set, &at, (GCompareFunc)compAtkAttributeName);
    result = element && !g_strcmp0(((AtkAttribute*)(element->data))->value, value);
    return result;
}

static gboolean atkAttributeSetAreEqual(AtkAttributeSet* set1, AtkAttributeSet* set2)
{
    if (!set1)
        return !set2;

    set1 = g_slist_sort(set1, (GCompareFunc)compAtkAttribute);
    set2 = g_slist_sort(set2, (GCompareFunc)compAtkAttribute);

    while (set1) {
        if (!set2 || compAtkAttribute(set1->data, set2->data))
            return FALSE;

        set1 = set1->next;
        set2 = set2->next;
    }

    return (!set2);
}

static void testWebkitAtkTextAttributes(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    AtkObject* child;
    GMainLoop* loop;
    AtkText* childText;
    AtkAttributeSet* set1;
    AtkAttributeSet* set2;
    AtkAttributeSet* set3;
    AtkAttributeSet* set4;
    gint startOffset, endOffset;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);

    webkit_web_view_load_string(webView, textWithAttributes, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_idle_add((GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);

    child = atk_object_ref_accessible_child(obj, 0);
    g_assert(child && ATK_IS_TEXT(child));
    childText = ATK_TEXT(child);
    set1 = atk_text_get_run_attributes(childText, 0, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 0);
    g_assert_cmpint(endOffset, ==, 12);
    g_assert(atkAttributeSetAreEqual(set1, NULL));

    set2 = atk_text_get_run_attributes(childText, 15, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 12);
    g_assert_cmpint(endOffset, ==, 17);
    g_assert(atkAttributeSetAttributeHasValue(set2, ATK_TEXT_ATTR_STYLE, "italic"));

    set3 = atk_text_get_run_attributes(childText, 17, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 17);
    g_assert_cmpint(endOffset, ==, 40);
    g_assert(atkAttributeSetAttributeHasValue(set3, ATK_TEXT_ATTR_WEIGHT, "700"));

    set4 = atk_text_get_default_attributes(childText);
    g_assert(atkAttributeSetAttributeHasValue(set4, ATK_TEXT_ATTR_STYLE, "normal"));
    g_assert(atkAttributeSetAttributeHasValue(set4, ATK_TEXT_ATTR_JUSTIFICATION, "right"));
    g_assert(atkAttributeSetAttributeHasValue(set4, ATK_TEXT_ATTR_SIZE, "14"));
    atk_attribute_set_free(set1);
    atk_attribute_set_free(set2);
    atk_attribute_set_free(set3);
    atk_attribute_set_free(set4);

    child = atk_object_ref_accessible_child(obj, 1);
    g_assert(child && ATK_IS_TEXT(child));
    childText = ATK_TEXT(child);

    set1 = atk_text_get_default_attributes(childText);
    g_assert(atkAttributeSetAttributeHasValue(set1, ATK_TEXT_ATTR_FAMILY_NAME, "monospace"));
    g_assert(atkAttributeSetAttributeHasValue(set1, ATK_TEXT_ATTR_STYLE, "normal"));
    g_assert(atkAttributeSetAttributeHasValue(set1, ATK_TEXT_ATTR_STRIKETHROUGH, "false"));
    g_assert(atkAttributeSetAttributeHasValue(set1, ATK_TEXT_ATTR_WEIGHT, "400"));
    g_assert(atkAttributeSetAttributeHasValue(set1, ATK_TEXT_ATTR_FG_COLOR, "120,121,122"));

    set2 = atk_text_get_run_attributes(childText, 43, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 43);
    g_assert_cmpint(endOffset, ==, 80);
    // Checks that default attributes of text are not returned when called to atk_text_get_run_attributes
    g_assert(!atkAttributeSetAttributeHasValue(set2, ATK_TEXT_ATTR_FG_COLOR, "120,121,122"));
    g_assert(atkAttributeSetAttributeHasValue(set2, ATK_TEXT_ATTR_UNDERLINE, "single"));
    g_assert(atkAttributeSetAttributeHasValue(set2, ATK_TEXT_ATTR_BG_COLOR, "80,81,82"));
    atk_attribute_set_free(set1);
    atk_attribute_set_free(set2);

    child = atk_object_ref_accessible_child(obj, 2);
    g_assert(child && ATK_IS_TEXT(child));
    childText = ATK_TEXT(child);

    set1 = atk_text_get_run_attributes(childText, 0, &startOffset, &endOffset);
    set2 = atk_text_get_run_attributes(childText, 3, &startOffset, &endOffset);
    g_assert(atkAttributeSetAreEqual(set1, set2));
    atk_attribute_set_free(set2);

    set2 = atk_text_get_run_attributes(childText, 1, &startOffset, &endOffset);
    set3 = atk_text_get_run_attributes(childText, 5, &startOffset, &endOffset);
    g_assert(atkAttributeSetAreEqual(set2, set3));
    g_assert(!atkAttributeSetAreEqual(set1, set2));
    atk_attribute_set_free(set3);

    set3 = atk_text_get_run_attributes(childText, 2, &startOffset, &endOffset);
    set4 = atk_text_get_run_attributes(childText, 6, &startOffset, &endOffset);
    g_assert(atkAttributeSetAreEqual(set3, set4));
    g_assert(!atkAttributeSetAreEqual(set1, set3));
    g_assert(!atkAttributeSetAreEqual(set2, set3));
    atk_attribute_set_free(set1);
    atk_attribute_set_free(set2);
    atk_attribute_set_free(set3);
    atk_attribute_set_free(set4);

    child = atk_object_ref_accessible_child(obj, 3);
    g_assert(child && ATK_IS_TEXT(child));
    childText = ATK_TEXT(child);
    set1 = atk_text_get_run_attributes(childText, 24, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 21);
    g_assert_cmpint(endOffset, ==, 25);
    g_assert(atkAttributeSetAttributeHasValue(set1, ATK_TEXT_ATTR_STRIKETHROUGH, "true"));

    set2 = atk_text_get_run_attributes(childText, 25, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 25);
    g_assert_cmpint(endOffset, ==, 30);
    g_assert(atkAttributeSetAreEqual(set2, NULL));

    set3 = atk_text_get_default_attributes(childText);
    g_assert(atkAttributeSetAttributeHasValue(set3, ATK_TEXT_ATTR_JUSTIFICATION, "center"));
    atk_attribute_set_free(set1);
    atk_attribute_set_free(set2);
    atk_attribute_set_free(set3);
}

static void testWekitAtkTextSelections(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    GMainLoop* loop;
    gchar* selectedText;
    gint startOffset;
    gint endOffset;
    gboolean result;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, textForSelections, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_idle_add((GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);

    AtkText* paragraph1 = ATK_TEXT(atk_object_ref_accessible_child(obj, 0));
    g_assert(ATK_IS_TEXT(paragraph1));
    AtkText* paragraph2 = ATK_TEXT(atk_object_ref_accessible_child(obj, 1));
    g_assert(ATK_IS_TEXT(paragraph2));
    AtkText* link = ATK_TEXT(atk_object_ref_accessible_child(ATK_OBJECT(paragraph2), 0));
    g_assert(ATK_IS_TEXT(link));

    // First paragraph (simple text)

    // Basic initial checks
    g_assert_cmpint(atk_text_get_n_selections(paragraph1), ==, 0);
    selectedText = atk_text_get_selection(paragraph1, 0, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 0);
    g_assert_cmpint(endOffset, ==, 0);
    g_assert_cmpstr(selectedText, ==, NULL);
    g_free (selectedText);
    // Try removing a non existing (yet) selection
    result = atk_text_remove_selection(paragraph1, 0);
    g_assert(!result);
    // Try setting a 0-char selection
    result = atk_text_set_selection(paragraph1, 0, 5, 5);
    g_assert(result);

    // Make a selection and test it
    result = atk_text_set_selection(paragraph1, 0, 5, 25);
    g_assert(result);
    g_assert_cmpint(atk_text_get_n_selections(paragraph1), ==, 1);
    selectedText = atk_text_get_selection(paragraph1, 0, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 5);
    g_assert_cmpint(endOffset, ==, 25);
    g_assert_cmpstr(selectedText, ==, "agraph with plain te");
    g_free (selectedText);
    // Try removing the selection from other AtkText object (should fail)
    result = atk_text_remove_selection(paragraph2, 0);
    g_assert(!result);

    // Remove the selection and test everything again
    result = atk_text_remove_selection(paragraph1, 0);
    g_assert(result);
    g_assert_cmpint(atk_text_get_n_selections(paragraph1), ==, 0);
    selectedText = atk_text_get_selection(paragraph1, 0, &startOffset, &endOffset);
    // Now offsets should be the same, set to the last position of the caret
    g_assert_cmpint(startOffset, ==, endOffset);
    g_assert_cmpint(startOffset, ==, 25);
    g_assert_cmpint(endOffset, ==, 25);
    g_assert_cmpstr(selectedText, ==, NULL);
    g_free (selectedText);

    // Second paragraph (text + link + text)

    // Set a selection partially covering the link and test it
    result = atk_text_set_selection(paragraph2, 0, 7, 21);
    g_assert(result);

    // Test the paragraph first
    g_assert_cmpint(atk_text_get_n_selections(paragraph2), ==, 1);
    selectedText = atk_text_get_selection(paragraph2, 0, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 7);
    g_assert_cmpint(endOffset, ==, 21);
    g_assert_cmpstr(selectedText, ==, "raph with a li");
    g_free (selectedText);

    // Now test just the link
    g_assert_cmpint(atk_text_get_n_selections(link), ==, 1);
    selectedText = atk_text_get_selection(link, 0, &startOffset, &endOffset);
    g_assert_cmpint(startOffset, ==, 0);
    g_assert_cmpint(endOffset, ==, 4);
    g_assert_cmpstr(selectedText, ==, "a li");
    g_free (selectedText);

    // Remove selections and text everything again
    result = atk_text_remove_selection(paragraph2, 0);
    g_assert(result);
    g_assert_cmpint(atk_text_get_n_selections(paragraph2), ==, 0);
    selectedText = atk_text_get_selection(paragraph2, 0, &startOffset, &endOffset);
    // Now offsets should be the same (no selection)
    g_assert_cmpint(startOffset, ==, endOffset);
    g_assert_cmpstr(selectedText, ==, NULL);
    g_free (selectedText);

    g_assert_cmpint(atk_text_get_n_selections(link), ==, 0);
    selectedText = atk_text_get_selection(link, 0, &startOffset, &endOffset);
    // Now offsets should be the same (no selection)
    g_assert_cmpint(startOffset, ==, endOffset);
    g_assert_cmpstr(selectedText, ==, NULL);
    g_free (selectedText);

    g_object_unref(paragraph1);
    g_object_unref(paragraph2);
    g_object_unref(webView);
}

static void testWebkitAtkGetExtents(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    GMainLoop* loop;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, centeredContents, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_idle_add((GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);

    AtkText* short_text1 = ATK_TEXT(atk_object_ref_accessible_child(obj, 0));
    g_assert(ATK_IS_TEXT(short_text1));
    AtkText* long_text = ATK_TEXT(atk_object_ref_accessible_child(obj, 1));
    g_assert(ATK_IS_TEXT(long_text));
    AtkText* short_text2 = ATK_TEXT(atk_object_ref_accessible_child(obj, 2));
    g_assert(ATK_IS_TEXT(short_text2));
    AtkText* multiline_text = ATK_TEXT(atk_object_ref_accessible_child(obj, 3));
    g_assert(ATK_IS_TEXT(multiline_text));

    // Start with window extents.
    AtkTextRectangle sline_window1, sline_window2, lline_window, mline_window;
    atk_text_get_range_extents(short_text1, 0, 10, ATK_XY_WINDOW, &sline_window1);
    atk_text_get_range_extents(long_text, 0, 44, ATK_XY_WINDOW, &lline_window);
    atk_text_get_range_extents(short_text2, 0, 10, ATK_XY_WINDOW, &sline_window2);
    atk_text_get_range_extents(multiline_text, 0, 60, ATK_XY_WINDOW, &mline_window);

    // Check vertical line position.
    g_assert_cmpint(sline_window1.y + sline_window1.height, <=, lline_window.y);
    g_assert_cmpint(lline_window.y + lline_window.height + sline_window2.height, <=, mline_window.y);

    // Paragraphs 1 and 3 have identical text and alignment.
    g_assert_cmpint(sline_window1.x, ==, sline_window2.x);
    g_assert_cmpint(sline_window1.width, ==, sline_window2.width);
    g_assert_cmpint(sline_window1.height, ==, sline_window2.height);

    // All lines should be the same height; line 2 is the widest line.
    g_assert_cmpint(sline_window1.height, ==, lline_window.height);
    g_assert_cmpint(sline_window1.width, <, lline_window.width);

    // Make sure the character extents jive with the range extents.
    gint x, y, width, height;

    // First paragraph (short text)
    atk_text_get_character_extents(short_text1, 0, &x, &y, &width, &height, ATK_XY_WINDOW);
    g_assert_cmpint(x, ==, sline_window1.x);
    g_assert_cmpint(y, ==, sline_window1.y);
    g_assert_cmpint(height, ==, sline_window1.height);

    atk_text_get_character_extents(short_text1, 9, &x, &y, &width, &height, ATK_XY_WINDOW);
    g_assert_cmpint(x, ==, sline_window1.x + sline_window1.width - width);
    g_assert_cmpint(y, ==, sline_window1.y);
    g_assert_cmpint(height, ==, sline_window1.height);

    // Second paragraph (long text)
    atk_text_get_character_extents(long_text, 0, &x, &y, &width, &height, ATK_XY_WINDOW);
    g_assert_cmpint(x, ==, lline_window.x);
    g_assert_cmpint(y, ==, lline_window.y);
    g_assert_cmpint(height, ==, lline_window.height);

    atk_text_get_character_extents(long_text, 43, &x, &y, &width, &height, ATK_XY_WINDOW);
    g_assert_cmpint(x, ==, lline_window.x + lline_window.width - width);
    g_assert_cmpint(y, ==, lline_window.y);
    g_assert_cmpint(height, ==, lline_window.height);

    // Third paragraph (short text)
    atk_text_get_character_extents(short_text2, 0, &x, &y, &width, &height, ATK_XY_WINDOW);
    g_assert_cmpint(x, ==, sline_window2.x);
    g_assert_cmpint(y, ==, sline_window2.y);
    g_assert_cmpint(height, ==, sline_window2.height);

    atk_text_get_character_extents(short_text2, 9, &x, &y, &width, &height, ATK_XY_WINDOW);
    g_assert_cmpint(x, ==, sline_window2.x + sline_window2.width - width);
    g_assert_cmpint(y, ==, sline_window2.y);
    g_assert_cmpint(height, ==, sline_window2.height);

    // Four paragraph (3 lines multi-line text)
    atk_text_get_character_extents(multiline_text, 0, &x, &y, &width, &height, ATK_XY_WINDOW);
    g_assert_cmpint(x, ==, mline_window.x);
    g_assert_cmpint(y, ==, mline_window.y);
    g_assert_cmpint(3 * height, ==, mline_window.height);

    atk_text_get_character_extents(multiline_text, 59, &x, &y, &width, &height, ATK_XY_WINDOW);
    // Last line won't fill the whole width of the rectangle
    g_assert_cmpint(x, <=, mline_window.x + mline_window.width - width);
    g_assert_cmpint(y, ==, mline_window.y + mline_window.height - height);
    g_assert_cmpint(height, <=, mline_window.height);

    g_object_unref(short_text1);
    g_object_unref(short_text2);
    g_object_unref(long_text);
    g_object_unref(multiline_text);
    g_object_unref(webView);
}

static void testWebkitAtkListsOfItems(void)
{
    WebKitWebView* webView;
    AtkObject* obj;
    AtkObject* uList;
    AtkObject* oList;
    AtkObject* item1;
    AtkObject* item2;
    AtkObject* item3;
    GMainLoop* loop;

    webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, listsOfItems, NULL, NULL, NULL);
    loop = g_main_loop_new(NULL, TRUE);

    g_idle_add((GSourceFunc)bail_out, loop);
    g_main_loop_run(loop);

    obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);

    // Unordered list

    uList = atk_object_ref_accessible_child(obj, 0);
    g_assert(ATK_OBJECT(uList));
    g_assert(atk_object_get_role(uList) == ATK_ROLE_LIST);
    g_assert_cmpint(atk_object_get_n_accessible_children(uList), ==, 3);

    item1 = atk_object_ref_accessible_child(uList, 0);
    item2 = atk_object_ref_accessible_child(uList, 1);
    item3 = atk_object_ref_accessible_child(uList, 2);

    g_assert_cmpint(atk_object_get_n_accessible_children(item1), ==, 0);
    g_assert_cmpint(atk_object_get_n_accessible_children(item2), ==, 1);
    g_assert_cmpint(atk_object_get_n_accessible_children(item3), ==, 1);

    g_assert_cmpstr(atk_text_get_text(ATK_TEXT(item1), 0, -1), ==, "\342\200\242 text only");
    g_assert_cmpstr(atk_text_get_text(ATK_TEXT(item2), 0, -1), ==, "\342\200\242 link only");
    g_assert_cmpstr(atk_text_get_text(ATK_TEXT(item3), 0, -1), ==, "\342\200\242 text and a link");

    g_object_unref(item1);
    g_object_unref(item2);
    g_object_unref(item3);

    // Ordered list

    oList = atk_object_ref_accessible_child(obj, 1);
    g_assert(ATK_OBJECT(oList));
    g_assert(atk_object_get_role(oList) == ATK_ROLE_LIST);
    g_assert_cmpint(atk_object_get_n_accessible_children(oList), ==, 3);

    item1 = atk_object_ref_accessible_child(oList, 0);
    item2 = atk_object_ref_accessible_child(oList, 1);
    item3 = atk_object_ref_accessible_child(oList, 2);

    g_assert_cmpstr(atk_text_get_text(ATK_TEXT(item1), 0, -1), ==, "1 text only");
    g_assert_cmpstr(atk_text_get_text(ATK_TEXT(item2), 0, -1), ==, "2 link only");
    g_assert_cmpstr(atk_text_get_text(ATK_TEXT(item3), 0, -1), ==, "3 text and a link");

    g_assert_cmpint(atk_object_get_n_accessible_children(item1), ==, 0);
    g_assert_cmpint(atk_object_get_n_accessible_children(item2), ==, 1);
    g_assert_cmpint(atk_object_get_n_accessible_children(item3), ==, 1);

    g_object_unref(item1);
    g_object_unref(item2);
    g_object_unref(item3);

    g_object_unref(uList);
    g_object_unref(oList);
    g_object_unref(webView);
}

static gboolean textInserted = FALSE;
static gboolean textDeleted = FALSE;

static void textChangedCb(AtkText* text, gint pos, gint len, const gchar* detail)
{
    g_assert(text && ATK_IS_OBJECT(text));

    if (!g_strcmp0(detail, "insert"))
        textInserted = TRUE;
    else if (!g_strcmp0(detail, "delete"))
        textDeleted = TRUE;
}

static gboolean checkTextChanges(gpointer unused)
{
    g_assert_cmpint(textInserted, ==, TRUE);
    g_assert_cmpint(textDeleted, ==, TRUE);
    return FALSE;
}

static void testWebkitAtkTextChangedNotifications(void)
{
    WebKitWebView* webView = WEBKIT_WEB_VIEW(webkit_web_view_new());
    g_object_ref_sink(webView);
    GtkAllocation alloc = { 0, 0, 800, 600 };
    gtk_widget_size_allocate(GTK_WIDGET(webView), &alloc);
    webkit_web_view_load_string(webView, formWithTextInputs, 0, 0, 0);

    // Manually spin the main context to get the accessible objects
    while (g_main_context_pending(0))
        g_main_context_iteration(0, TRUE);

    AtkObject* obj = gtk_widget_get_accessible(GTK_WIDGET(webView));
    g_assert(obj);

    AtkObject* form = atk_object_ref_accessible_child(obj, 0);
    g_assert(ATK_IS_OBJECT(form));

    AtkObject* textEntry = atk_object_ref_accessible_child(form, 0);
    g_assert(ATK_IS_EDITABLE_TEXT(textEntry));
    g_assert(atk_object_get_role(ATK_OBJECT(textEntry)) == ATK_ROLE_ENTRY);

    g_signal_connect(textEntry, "text-changed::insert",
                     G_CALLBACK(textChangedCb),
                     (gpointer)"insert");
    g_signal_connect(textEntry, "text-changed::delete",
                     G_CALLBACK(textChangedCb),
                     (gpointer)"delete");

    gint pos = 0;
    atk_editable_text_insert_text(ATK_EDITABLE_TEXT(textEntry), "foo bar baz", 11, &pos);
    atk_editable_text_delete_text(ATK_EDITABLE_TEXT(textEntry), 4, 7);
    textInserted = FALSE;
    textDeleted = FALSE;

    g_idle_add((GSourceFunc)checkTextChanges, 0);

    g_object_unref(form);
    g_object_unref(textEntry);
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
    g_test_add_func("/webkit/atk/getTextInTable", testWebkitAtkGetTextInTable);
    g_test_add_func("/webkit/atk/getHeadersInTable", testWebkitAtkGetHeadersInTable);
    g_test_add_func("/webkit/atk/textAttributes", testWebkitAtkTextAttributes);
    g_test_add_func("/webkit/atk/textSelections", testWekitAtkTextSelections);
    g_test_add_func("/webkit/atk/getExtents", testWebkitAtkGetExtents);
    g_test_add_func("/webkit/atk/listsOfItems", testWebkitAtkListsOfItems);
    g_test_add_func("/webkit/atk/textChangedNotifications", testWebkitAtkTextChangedNotifications);
    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
