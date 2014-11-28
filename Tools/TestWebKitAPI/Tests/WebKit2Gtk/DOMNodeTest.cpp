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
#include <wtf/gobject/GUniquePtr.h>

class WebKitDOMNodeTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebKitDOMNodeTest>(new WebKitDOMNodeTest()); }

private:
    bool testHierarchyNavigation(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));

        WebKitDOMHTMLHeadElement* head = webkit_dom_document_get_head(document);
        g_assert(WEBKIT_DOM_IS_HTML_HEAD_ELEMENT(head));

        // Title, head's child.
        g_assert(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(head)));
        GRefPtr<WebKitDOMNodeList> list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(head)));
        g_assert(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 1);
        WebKitDOMNode* node = webkit_dom_node_list_item(list.get(), 0);
        g_assert(WEBKIT_DOM_IS_HTML_TITLE_ELEMENT(node));

        // Body, Head sibling.
        node = webkit_dom_node_get_next_sibling(WEBKIT_DOM_NODE(head));
        g_assert(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(node));
        WebKitDOMHTMLBodyElement* body = WEBKIT_DOM_HTML_BODY_ELEMENT(node);

        // There is no third sibling
        g_assert(!webkit_dom_node_get_next_sibling(node));

        // Body's previous sibling is Head.
        node = webkit_dom_node_get_previous_sibling(WEBKIT_DOM_NODE(body));
        g_assert(WEBKIT_DOM_IS_HTML_HEAD_ELEMENT(node));

        // Body has 3 children.
        g_assert(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        unsigned long length = webkit_dom_node_list_get_length(list.get());
        g_assert_cmpint(length, ==, 3);

        // The three of them are P tags.
        for (unsigned long i = 0; i < length; i++) {
            node = webkit_dom_node_list_item(list.get(), i);
            g_assert(WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT(node));
        }

        // Go backwards
        unsigned i;
        for (i = 0; node; node = webkit_dom_node_get_previous_sibling(node), i++) { }
        g_assert_cmpint(i, ==, 3);

        return true;
    }

    bool testInsertion(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));

        WebKitDOMHTMLElement* body = webkit_dom_document_get_body(document);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(body));

        // Body shouldn't have any children at this point.
        g_assert(!webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));

        // Insert one P element.
        WebKitDOMElement* p = webkit_dom_document_create_element(document, "P", 0);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(p));
        webkit_dom_node_append_child(WEBKIT_DOM_NODE(body), WEBKIT_DOM_NODE(p), 0);

        // Now it should have one, the same that we inserted.
        g_assert(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        GRefPtr<WebKitDOMNodeList> list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 1);
        WebKitDOMNode* node = webkit_dom_node_list_item(list.get(), 0);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        g_assert(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(p), node));

        // Replace the P tag with a DIV tag.
        WebKitDOMElement* div = webkit_dom_document_create_element(document, "DIV", 0);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(div));
        webkit_dom_node_replace_child(WEBKIT_DOM_NODE(body), WEBKIT_DOM_NODE(div), WEBKIT_DOM_NODE(p), 0);
        g_assert(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 1);
        node = webkit_dom_node_list_item(list.get(), 0);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        g_assert(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(div), node));

        // Now remove the tag.
        webkit_dom_node_remove_child(WEBKIT_DOM_NODE(body), node, 0);
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 0);

        // Test insert before. If refChild is null, insert newChild as last element of parent.
        div = webkit_dom_document_create_element(document, "DIV", 0);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(div));
        webkit_dom_node_insert_before(WEBKIT_DOM_NODE(body), WEBKIT_DOM_NODE(div), 0, 0);
        g_assert(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 1);
        node = webkit_dom_node_list_item(list.get(), 0);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        g_assert(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(div), node));

        // Now insert a 'p' before 'div'.
        p = webkit_dom_document_create_element(document, "P", 0);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(p));
        webkit_dom_node_insert_before(WEBKIT_DOM_NODE(body), WEBKIT_DOM_NODE(p), WEBKIT_DOM_NODE(div), 0);
        g_assert(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 2);
        node = webkit_dom_node_list_item(list.get(), 0);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        g_assert(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(p), node));
        node = webkit_dom_node_list_item(list.get(), 1);
        g_assert(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        g_assert(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(div), node));

        return true;
    }

    bool testTagNames(WebKitWebPage* page)
    {
        static const char* expectedTagNames[] = { "HTML", "HEAD", "BODY", "VIDEO", "SOURCE", "VIDEO", "SOURCE", "INPUT" };

        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));

        WebKitDOMNodeList* list = webkit_dom_document_get_elements_by_tag_name(document, "*");
        gulong nodeCount = webkit_dom_node_list_get_length(list);
        g_assert_cmpuint(nodeCount, ==, G_N_ELEMENTS(expectedTagNames));
        for (unsigned i = 0; i < nodeCount; i++) {
            WebKitDOMNode* node = webkit_dom_node_list_item(list, i);
            g_assert(WEBKIT_DOM_IS_NODE(node));
            GUniquePtr<char> tagName(webkit_dom_node_get_node_name(node));
            g_assert_cmpstr(tagName.get(), ==, expectedTagNames[i]);
        }

        return true;
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "hierarchy-navigation"))
            return testHierarchyNavigation(page);
        if (!strcmp(testName, "insertion"))
            return testInsertion(page);
        if (!strcmp(testName, "tag-names"))
            return testTagNames(page);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitDOMNodeTest, "WebKitDOMNode/hierarchy-navigation");
    REGISTER_TEST(WebKitDOMNodeTest, "WebKitDOMNode/insertion");
    REGISTER_TEST(WebKitDOMNodeTest, "WebKitDOMNode/tag-names");
}


