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

class WebKitDOMNodeTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebKitDOMNodeTest>(new WebKitDOMNodeTest()); }

private:
    bool testHierarchyNavigation(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        WebKitDOMHTMLHeadElement* head = webkit_dom_document_get_head(document);
        g_assert_true(WEBKIT_DOM_IS_HTML_HEAD_ELEMENT(head));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(head));

        // Title, head's child.
        g_assert_true(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(head)));
        GRefPtr<WebKitDOMNodeList> list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(head)));
        g_assert_true(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 1);
        WebKitDOMNode* node = webkit_dom_node_list_item(list.get(), 0);
        g_assert_true(WEBKIT_DOM_IS_HTML_TITLE_ELEMENT(node));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));

        // Body, Head sibling.
        node = webkit_dom_node_get_next_sibling(WEBKIT_DOM_NODE(head));
        g_assert_true(WEBKIT_DOM_IS_HTML_BODY_ELEMENT(node));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
        WebKitDOMHTMLBodyElement* body = WEBKIT_DOM_HTML_BODY_ELEMENT(node);

        // There is no third sibling
        g_assert_null(webkit_dom_node_get_next_sibling(node));

        // Body's previous sibling is Head.
        node = webkit_dom_node_get_previous_sibling(WEBKIT_DOM_NODE(body));
        g_assert_true(WEBKIT_DOM_IS_HTML_HEAD_ELEMENT(node));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));

        // Body has 3 children.
        g_assert_true(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert_true(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(list.get()));
        unsigned long length = webkit_dom_node_list_get_length(list.get());
        g_assert_cmpint(length, ==, 3);

        // The three of them are P tags.
        for (unsigned long i = 0; i < length; i++) {
            node = webkit_dom_node_list_item(list.get(), i);
            g_assert_true(WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT(node));
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
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
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        WebKitDOMHTMLElement* body = webkit_dom_document_get_body(document);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(body));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(body));

        // Body shouldn't have any children at this point.
        g_assert_false(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));

        // The value of a non-existent attribute should be null, not an empty string
        g_assert_null(webkit_dom_html_body_element_get_background(WEBKIT_DOM_HTML_BODY_ELEMENT(body)));

        // Insert one P element.
        WebKitDOMElement* p = webkit_dom_document_create_element(document, "P", 0);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(p));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(p));
        webkit_dom_node_append_child(WEBKIT_DOM_NODE(body), WEBKIT_DOM_NODE(p), 0);

        // Now it should have one, the same that we inserted.
        g_assert_true(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        GRefPtr<WebKitDOMNodeList> list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert_true(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 1);
        WebKitDOMNode* node = webkit_dom_node_list_item(list.get(), 0);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
        g_assert_true(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(p), node));

        // Replace the P tag with a DIV tag.
        WebKitDOMElement* div = webkit_dom_document_create_element(document, "DIV", 0);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(div));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(div));
        webkit_dom_node_replace_child(WEBKIT_DOM_NODE(body), WEBKIT_DOM_NODE(div), WEBKIT_DOM_NODE(p), 0);
        g_assert_true(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert_true(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 1);
        node = webkit_dom_node_list_item(list.get(), 0);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
        g_assert_true(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(div), node));

        // Now remove the tag.
        webkit_dom_node_remove_child(WEBKIT_DOM_NODE(body), node, 0);
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert_true(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 0);

        // Test insert before. If refChild is null, insert newChild as last element of parent.
        div = webkit_dom_document_create_element(document, "DIV", 0);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(div));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(div));
        webkit_dom_node_insert_before(WEBKIT_DOM_NODE(body), WEBKIT_DOM_NODE(div), 0, 0);
        g_assert_true(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert_true(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 1);
        node = webkit_dom_node_list_item(list.get(), 0);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
        g_assert_true(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(div), node));

        // Now insert a 'p' before 'div'.
        p = webkit_dom_document_create_element(document, "P", 0);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(p));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(p));
        webkit_dom_node_insert_before(WEBKIT_DOM_NODE(body), WEBKIT_DOM_NODE(p), WEBKIT_DOM_NODE(div), 0);
        g_assert_true(webkit_dom_node_has_child_nodes(WEBKIT_DOM_NODE(body)));
        list = adoptGRef(webkit_dom_node_get_child_nodes(WEBKIT_DOM_NODE(body)));
        g_assert_true(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(list.get()));
        g_assert_cmpint(webkit_dom_node_list_get_length(list.get()), ==, 2);
        node = webkit_dom_node_list_item(list.get(), 0);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
        g_assert_true(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(p), node));
        node = webkit_dom_node_list_item(list.get(), 1);
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(node));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
        g_assert_true(webkit_dom_node_is_same_node(WEBKIT_DOM_NODE(div), node));

        return true;
    }

    bool testTagNamesNodeList(WebKitWebPage* page)
    {
        static const char* expectedTagNames[] = { "HTML", "HEAD", "BODY", "VIDEO", "SOURCE", "VIDEO", "SOURCE", "INPUT" };

        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        GRefPtr<WebKitDOMNodeList> list = adoptGRef(webkit_dom_document_query_selector_all(document, "*", nullptr));
        g_assert_true(WEBKIT_DOM_IS_NODE_LIST(list.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(list.get()));
        gulong nodeCount = webkit_dom_node_list_get_length(list.get());
        g_assert_cmpuint(nodeCount, ==, G_N_ELEMENTS(expectedTagNames));
        for (unsigned i = 0; i < nodeCount; i++) {
            WebKitDOMNode* node = webkit_dom_node_list_item(list.get(), i);
            g_assert_true(WEBKIT_DOM_IS_NODE(node));
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
            GUniquePtr<char> tagName(webkit_dom_node_get_node_name(node));
            g_assert_cmpstr(tagName.get(), ==, expectedTagNames[i]);
        }

        return true;
    }

    bool testTagNamesHTMLCollection(WebKitWebPage* page)
    {
        static const char* expectedTagNames[] = { "HTML", "HEAD", "BODY", "VIDEO", "SOURCE", "VIDEO", "SOURCE", "INPUT" };

        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        GRefPtr<WebKitDOMHTMLCollection> collection = adoptGRef(webkit_dom_document_get_elements_by_tag_name_as_html_collection(document, "*"));
        g_assert_true(WEBKIT_DOM_IS_HTML_COLLECTION(collection.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(collection.get()));
        gulong nodeCount = webkit_dom_html_collection_get_length(collection.get());
        g_assert_cmpuint(nodeCount, ==, G_N_ELEMENTS(expectedTagNames));
        for (unsigned i = 0; i < nodeCount; i++) {
            WebKitDOMNode* node = webkit_dom_html_collection_item(collection.get(), i);
            g_assert_true(WEBKIT_DOM_IS_NODE(node));
            assertObjectIsDeletedWhenTestFinishes(G_OBJECT(node));
            GUniquePtr<char> tagName(webkit_dom_node_get_node_name(node));
            g_assert_cmpstr(tagName.get(), ==, expectedTagNames[i]);
        }

        return true;
    }

    bool testDOMCache(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        // DOM objects already in the document should be automatically handled by the cache.
        WebKitDOMElement* div = webkit_dom_document_get_element_by_id(document, "container");
        g_assert_true(WEBKIT_DOM_IS_HTML_ELEMENT(div));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(div));

        // Get the same elment twice should return the same pointer.
        g_assert_true(div == webkit_dom_document_get_element_by_id(document, "container"));

        // A new DOM object created that is derived from Node should be automatically handled by the cache.
        WebKitDOMElement* p = webkit_dom_document_create_element(document, "P", nullptr);
        g_assert_true(WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT(p));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(p));

        // A new DOM object created that isn't derived from Node should be manually handled.
        GRefPtr<WebKitDOMNodeIterator> iter = adoptGRef(webkit_dom_document_create_node_iterator(document, WEBKIT_DOM_NODE(div), WEBKIT_DOM_NODE_FILTER_SHOW_ALL, nullptr, FALSE, nullptr));
        g_assert_true(WEBKIT_DOM_IS_NODE_ITERATOR(iter.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(iter.get()));

        // We can also manually handle a DOM object handled by the cache.
        GRefPtr<WebKitDOMElement> p2 = adoptGRef(webkit_dom_document_create_element(document, "P", nullptr));
        g_assert_true(WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT(p2.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(p2.get()));

        // Manually handling a DOM object owned by the cache shouldn't crash when the cache has more than one reference.
        GRefPtr<WebKitDOMElement> p3 = adoptGRef(webkit_dom_document_create_element(document, "P", nullptr));
        g_assert_true(WEBKIT_DOM_IS_HTML_PARAGRAPH_ELEMENT(p3.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(p3.get()));
        webkit_dom_node_append_child(WEBKIT_DOM_NODE(div), WEBKIT_DOM_NODE(p3.get()), nullptr);

        // DOM objects removed from the document are also correctly handled by the cache.
        WebKitDOMElement* a = webkit_dom_document_create_element(document, "A", nullptr);
        g_assert_true(WEBKIT_DOM_IS_ELEMENT(a));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(a));
        webkit_dom_node_remove_child(WEBKIT_DOM_NODE(div), WEBKIT_DOM_NODE(a), nullptr);

        return true;
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "hierarchy-navigation"))
            return testHierarchyNavigation(page);
        if (!strcmp(testName, "insertion"))
            return testInsertion(page);
        if (!strcmp(testName, "tag-names-node-list"))
            return testTagNamesNodeList(page);
        if (!strcmp(testName, "tag-names-html-collection"))
            return testTagNamesHTMLCollection(page);
        if (!strcmp(testName, "dom-cache"))
            return testDOMCache(page);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitDOMNodeTest, "WebKitDOMNode/hierarchy-navigation");
    REGISTER_TEST(WebKitDOMNodeTest, "WebKitDOMNode/insertion");
    REGISTER_TEST(WebKitDOMNodeTest, "WebKitDOMNode/tag-names-node-list");
    REGISTER_TEST(WebKitDOMNodeTest, "WebKitDOMNode/tag-names-html-collection");
    REGISTER_TEST(WebKitDOMNodeTest, "WebKitDOMNode/dom-cache");
}

G_GNUC_END_IGNORE_DEPRECATIONS;
