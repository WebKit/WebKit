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

static const char* contentsInTable = "<html><body><table><tr><td>foo</td><td>bar</td></tr></table></body></html>";

static const char* contentsInTableWithHeaders = "<html><body><table><tr><th>foo</th><th>bar</th><th colspan='2'>baz</th></tr><tr><th>qux</th><td>1</td><td>2</td><td>3</td></tr><tr><th rowspan='2'>quux</th><td>4</td><td>5</td><td>6</td></tr><tr><td>6</td><td>7</td><td>8</td></tr><tr><th>corge</th><td>9</td><td>10</td><td>11</td></tr></table><table><tr><td>1</td><td>2</td></tr><tr><td>3</td><td>4</td></tr></table></body></html>";

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
    return g_test_run ();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
