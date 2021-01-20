/*
 * Copyright (C) 2013 Igalia S.L.
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

#include "config.h"

#include "WebProcessTest.h"
#include <gio/gio.h>
#include <webkit2/webkit-web-extension.h>
#include <wtf/glib/GUniquePtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

typedef struct _WebKitNodeFilter {
    GObject parent;
} WebKitNodeFilter;

typedef struct _WebKitNodeFilterClass {
    GObjectClass parentClass;
} WebKitNodeFilterClass;

static short webkitNodeFilterAcceptNode(WebKitDOMNodeFilter*, WebKitDOMNode* node)
{
    // Filter out input elements.
    return WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(node) ? WEBKIT_DOM_NODE_FILTER_REJECT : WEBKIT_DOM_NODE_FILTER_ACCEPT;
}

static void webkitNodeFilterDOMNodeFilterIfaceInit(WebKitDOMNodeFilterIface* iface)
{
    iface->accept_node = webkitNodeFilterAcceptNode;
}

G_DEFINE_TYPE_WITH_CODE(WebKitNodeFilter, webkit_node_filter, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_NODE_FILTER, webkitNodeFilterDOMNodeFilterIfaceInit))

static void webkit_node_filter_init(WebKitNodeFilter*)
{
}

static void webkit_node_filter_class_init(WebKitNodeFilterClass*)
{
}

static const char* expectedNodesAll[] = { "HTML", "HEAD", "TITLE", "#text", "BODY", "INPUT", "INPUT", "BR" };
static const char* expectedNodesNoInput[] = { "HTML", "HEAD", "TITLE", "#text", "BODY", "BR" };
static const char* expectedElementsNoInput[] = { "HTML", "HEAD", "TITLE", "BODY", "BR" };

class WebKitDOMNodeFilterTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebProcessTest>(new WebKitDOMNodeFilterTest()); }

private:
    bool testTreeWalker(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        WebKitDOMElement* root = webkit_dom_document_get_element_by_id(document, "root");
        g_assert_true(WEBKIT_DOM_IS_NODE(root));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(root));

        // No filter.
        GRefPtr<WebKitDOMTreeWalker> walker = adoptGRef(webkit_dom_document_create_tree_walker(document, WEBKIT_DOM_NODE(root), WEBKIT_DOM_NODE_FILTER_SHOW_ALL, nullptr, FALSE, nullptr));
        g_assert_true(WEBKIT_DOM_IS_TREE_WALKER(walker.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(walker.get()));
        g_assert_null(webkit_dom_tree_walker_get_filter(walker.get()));

        unsigned i = 0;
        for (WebKitDOMNode* node = WEBKIT_DOM_NODE(root); node; node = webkit_dom_tree_walker_next_node(walker.get()), ++i) {
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
            g_assert_cmpuint(i, <, G_N_ELEMENTS(expectedNodesAll));
            GUniquePtr<char> nodeName(webkit_dom_node_get_node_name(node));
            g_assert_cmpstr(nodeName.get(), ==, expectedNodesAll[i]);
        }
        g_assert_cmpuint(i, ==, G_N_ELEMENTS(expectedNodesAll));

        // Input elements filter.
        GRefPtr<WebKitDOMNodeFilter> filter = adoptGRef(static_cast<WebKitDOMNodeFilter*>(g_object_new(webkit_node_filter_get_type(), nullptr)));
        walker = adoptGRef(webkit_dom_document_create_tree_walker(document, WEBKIT_DOM_NODE(root), WEBKIT_DOM_NODE_FILTER_SHOW_ALL, filter.get(), FALSE, nullptr));
        g_assert_true(WEBKIT_DOM_IS_TREE_WALKER(walker.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(filter.get()));
        g_assert_true(webkit_dom_tree_walker_get_filter(walker.get()) == filter.get());

        i = 0;
        for (WebKitDOMNode* node = WEBKIT_DOM_NODE(root); node; node = webkit_dom_tree_walker_next_node(walker.get()), ++i) {
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
            g_assert_cmpuint(i, <, G_N_ELEMENTS(expectedNodesNoInput));
            GUniquePtr<char> nodeName(webkit_dom_node_get_node_name(node));
            g_assert_cmpstr(nodeName.get(), ==, expectedNodesNoInput[i]);
        }
        g_assert_cmpuint(i, ==, G_N_ELEMENTS(expectedNodesNoInput));

        // Show only elements, reusing the input filter.
        walker = adoptGRef(webkit_dom_document_create_tree_walker(document, WEBKIT_DOM_NODE(root), WEBKIT_DOM_NODE_FILTER_SHOW_ELEMENT, filter.get(), FALSE, nullptr));
        g_assert_true(WEBKIT_DOM_IS_TREE_WALKER(walker.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(walker.get()));
        g_assert_true(webkit_dom_tree_walker_get_filter(walker.get()) == filter.get());

        i = 0;
        for (WebKitDOMNode* node = WEBKIT_DOM_NODE(root); node; node = webkit_dom_tree_walker_next_node(walker.get()), ++i) {
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
            g_assert_cmpuint(i, <, G_N_ELEMENTS(expectedElementsNoInput));
            GUniquePtr<char> nodeName(webkit_dom_node_get_node_name(node));
            g_assert_cmpstr(nodeName.get(), ==, expectedElementsNoInput[i]);
        }
        g_assert_cmpuint(i, ==, G_N_ELEMENTS(expectedElementsNoInput));

        return true;
    }

    bool testNodeIterator(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        WebKitDOMElement* root = webkit_dom_document_get_element_by_id(document, "root");
        g_assert_true(WEBKIT_DOM_IS_NODE(root));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(root));

        // No filter.
        GRefPtr<WebKitDOMNodeIterator> iter = adoptGRef(webkit_dom_document_create_node_iterator(document, WEBKIT_DOM_NODE(root), WEBKIT_DOM_NODE_FILTER_SHOW_ALL, nullptr, FALSE, nullptr));
        g_assert_true(WEBKIT_DOM_IS_NODE_ITERATOR(iter.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(iter.get()));
        g_assert_null(webkit_dom_node_iterator_get_filter(iter.get()));

        unsigned i = 0;
        while (WebKitDOMNode* node = webkit_dom_node_iterator_next_node(iter.get(), nullptr)) {
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
            g_assert_cmpuint(i, <, G_N_ELEMENTS(expectedNodesAll));
            GUniquePtr<char> nodeName(webkit_dom_node_get_node_name(node));
            g_assert_cmpstr(nodeName.get(), ==, expectedNodesAll[i]);
            i++;
        }
        g_assert_cmpuint(i, ==, G_N_ELEMENTS(expectedNodesAll));

        // Input elements filter.
        GRefPtr<WebKitDOMNodeFilter> filter = adoptGRef(static_cast<WebKitDOMNodeFilter*>(g_object_new(webkit_node_filter_get_type(), nullptr)));
        iter = adoptGRef(webkit_dom_document_create_node_iterator(document, WEBKIT_DOM_NODE(root), WEBKIT_DOM_NODE_FILTER_SHOW_ALL, filter.get(), FALSE, nullptr));
        g_assert_true(WEBKIT_DOM_IS_NODE_ITERATOR(iter.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(iter.get()));
        g_assert_true(webkit_dom_node_iterator_get_filter(iter.get()) == filter.get());

        i = 0;
        while (WebKitDOMNode* node = webkit_dom_node_iterator_next_node(iter.get(), nullptr)) {
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
            g_assert_cmpuint(i, <, G_N_ELEMENTS(expectedNodesNoInput));
            GUniquePtr<char> nodeName(webkit_dom_node_get_node_name(node));
            g_assert_cmpstr(nodeName.get(), ==, expectedNodesNoInput[i]);
            i++;
        }
        g_assert_cmpuint(i, ==, G_N_ELEMENTS(expectedNodesNoInput));

        // Show only elements, reusing the input filter.
        iter = adoptGRef(webkit_dom_document_create_node_iterator(document, WEBKIT_DOM_NODE(root), WEBKIT_DOM_NODE_FILTER_SHOW_ELEMENT, filter.get(), FALSE, nullptr));
        g_assert_true(WEBKIT_DOM_IS_NODE_ITERATOR(iter.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(iter.get()));
        g_assert_true(webkit_dom_node_iterator_get_filter(iter.get()) == filter.get());

        i = 0;
        while (WebKitDOMNode* node = webkit_dom_node_iterator_next_node(iter.get(), nullptr)) {
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
            g_assert_cmpuint(i, <, G_N_ELEMENTS(expectedElementsNoInput));
            GUniquePtr<char> nodeName(webkit_dom_node_get_node_name(node));
            g_assert_cmpstr(nodeName.get(), ==, expectedElementsNoInput[i]);
            i++;
        }
        g_assert_cmpuint(i, ==, G_N_ELEMENTS(expectedElementsNoInput));

        return true;
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "tree-walker"))
            return testTreeWalker(page);
        if (!strcmp(testName, "node-iterator"))
            return testNodeIterator(page);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitDOMNodeFilterTest, "WebKitDOMNodeFilter/tree-walker");
    REGISTER_TEST(WebKitDOMNodeFilterTest, "WebKitDOMNodeFilter/node-iterator");
}

G_GNUC_END_IGNORE_DEPRECATIONS;
