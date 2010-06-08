/*
 * Copyright (C) 2010 Igalia S.L.
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

#include "test_utils.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <gtk/gtk.h>
#include <webkit/webkit.h>

#if GLIB_CHECK_VERSION(2, 16, 0) && GTK_CHECK_VERSION(2, 14, 0)

#define HTML_DOCUMENT_HIERARCHY_NAVIGATION "<html><head><title>This is the title</title></head><body><p>1</p><p>2</p><p>3</p></body></html>"

typedef struct {
    GtkWidget* webView;
    GMainLoop* loop;
} DomNodeFixture;

static gboolean finish_loading(DomNodeFixture* fixture)
{
    if (g_main_loop_is_running(fixture->loop))
        g_main_loop_quit(fixture->loop);

    return FALSE;
}

static void dom_node_fixture_setup(DomNodeFixture* fixture, gconstpointer data)
{
    fixture->loop = g_main_loop_new(NULL, TRUE);
    fixture->webView = webkit_web_view_new();
    g_object_ref_sink(fixture->webView);

    if (data != NULL)
        webkit_web_view_load_string(WEBKIT_WEB_VIEW(fixture->webView), (const char*)data, NULL, NULL, NULL);

    g_idle_add((GSourceFunc)finish_loading, fixture);
    g_main_loop_run(fixture->loop);
}

static void dom_node_fixture_teardown(DomNodeFixture* fixture, gconstpointer data)
{
    g_object_unref(fixture->webView);
    g_main_loop_unref(fixture->loop);
}

static void test_dom_node_hierarchy_navigation(DomNodeFixture* fixture, gconstpointer data)
{
    WebKitDOMDocument* document;
    WebKitDOMHTMLHeadElement* head;
    WebKitDOMHTMLBodyElement* body;
    WebKitDOMNodeList* list;
    WebKitDOMNode* ptr;
    gulong i, length;

    document = webkit_web_view_get_dom_document(WEBKIT_WEB_VIEW(fixture->webView));
    g_assert(document);
    g_assert(WEBKIT_DOM_IS_DOCUMENT(document));
    head = webkit_dom_document_get_head(document);
    g_assert(head);
    g_assert(WEBKIT_DOM_IS_HTML_HEAD_ELEMENT(head));

    /* Title, head's child */
    g_assert(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(head)));
    list = webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(head));
    g_assert_cmpint(webkit_dom_node_list_get_length(list), ==, 1);
    ptr = webkit_dom_node_list_item(list, 0);
    g_assert(ptr);
    g_assert(WEBKIT_DOM_IS_HTML_TITLE_ELEMENT(ptr));

    /* Body, Head sibling */
    ptr = webkit_dom_node_get_next_sibling(WEBKIT_DOM_NODE(head));
    g_assert(ptr);
    body = WEBKIT_DOM_HTML_BODY_ELEMENT(ptr);
    g_assert(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(body));

    /* There is no third sibling */
    ptr = webkit_dom_node_get_next_sibling(ptr);
    g_assert(ptr == NULL);

    /* Body's previous sibling is Head */
    ptr = webkit_dom_node_get_previous_sibling(WEBKIT_DOM_NODE(body));
    g_assert(ptr);
    g_assert(WEBKIT_DOM_IS_HTML_HEAD_ELEMENT(ptr));

    /* Body has 3 children */
    g_assert(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
    list = webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body));
    length = webkit_dom_node_list_get_length(list);
    g_assert_cmpint(length, ==, 3);

    /* The three of them are P tags */
    for (i = 0; i < length; i++) {
        ptr = webkit_dom_node_list_item(list, i);
        g_assert(ptr);
        g_assert(WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT(ptr));
    }

    /* Go backwards */
    for (i = 0; ptr; ptr = webkit_dom_node_get_previous_sibling(ptr), i++)
        /* Nothing */;

    g_assert_cmpint(i, ==, 3);
}

int main(int argc, char** argv)
{
    if (!g_thread_supported())
        g_thread_init(NULL);

    gtk_test_init(&argc, &argv, NULL);

    g_test_bug_base("https://bugs.webkit.org/");

    g_test_add("/webkit/domnode/test_hierarchy_navigation",
               DomNodeFixture, HTML_DOCUMENT_HIERARCHY_NAVIGATION,
               dom_node_fixture_setup,
               test_dom_node_hierarchy_navigation,
               dom_node_fixture_teardown);

    return g_test_run();
}

#else
int main(int argc, char** argv)
{
    g_critical("You will need at least glib-2.16.0 and gtk-2.14.0 to run the unit tests. Doing nothing now.");
    return 0;
}

#endif
