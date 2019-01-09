/*
 * Copyright (C) 2014 Igalia S.L.
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

typedef struct _WebKitXPathNSResolver {
    GObject parent;
} WebKitXPathNSResolver;

typedef struct _WebKitXPathNSResolverClass {
    GObjectClass parentClass;
} WebKitXPathNSResolverClass;

static char* webkitXPathNSResolverLookupNamespaceURI(WebKitDOMXPathNSResolver* resolver, const char* prefix)
{
    if (!g_strcmp0(prefix, "foo"))
        return g_strdup("http://www.example.com");

    return nullptr;
}

static void webkitXPathNSResolverDOMXPathNSResolverIfaceInit(WebKitDOMXPathNSResolverIface* iface)
{
    iface->lookup_namespace_uri = webkitXPathNSResolverLookupNamespaceURI;
}

G_DEFINE_TYPE_WITH_CODE(WebKitXPathNSResolver, webkit_xpath_ns_resolver, G_TYPE_OBJECT, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_XPATH_NS_RESOLVER, webkitXPathNSResolverDOMXPathNSResolverIfaceInit))

static void webkit_xpath_ns_resolver_init(WebKitXPathNSResolver*)
{
}

static void webkit_xpath_ns_resolver_class_init(WebKitXPathNSResolverClass*)
{
}

class WebKitDOMXPathNSResolverTest : public WebProcessTest {
public:
    static std::unique_ptr<WebProcessTest> create() { return std::unique_ptr<WebProcessTest>(new WebKitDOMXPathNSResolverTest()); }

private:
    void evaluateFooChildTextAndCheckResult(WebKitDOMDocument* document, WebKitDOMXPathNSResolver* resolver)
    {
        WebKitDOMElement* documentElement = webkit_dom_document_get_document_element(document);
        g_assert_true(WEBKIT_DOM_IS_ELEMENT(documentElement));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(documentElement));

        GRefPtr<WebKitDOMXPathResult> result = adoptGRef(webkit_dom_document_evaluate(document, "foo:child/text()", WEBKIT_DOM_NODE(documentElement), resolver, WEBKIT_DOM_XPATH_RESULT_ORDERED_NODE_ITERATOR_TYPE, nullptr, nullptr));
        g_assert_true(WEBKIT_DOM_IS_XPATH_RESULT(result.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(result.get()));

        WebKitDOMNode* nodeResult = webkit_dom_xpath_result_iterate_next(result.get(), nullptr);
        g_assert_true(WEBKIT_DOM_IS_NODE(nodeResult));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(nodeResult));

        GUniquePtr<char> nodeValue(webkit_dom_node_get_node_value(nodeResult));
        g_assert_cmpstr(nodeValue.get(), ==, "SUCCESS");
    }

    bool testXPathNSResolverNative(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        GRefPtr<WebKitDOMXPathNSResolver> resolver = adoptGRef(webkit_dom_document_create_ns_resolver(document, WEBKIT_DOM_NODE(webkit_dom_document_get_document_element(document))));
        g_assert_true(WEBKIT_DOM_IS_XPATH_NS_RESOLVER(resolver.get()));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(resolver.get()));
        evaluateFooChildTextAndCheckResult(document, resolver.get());

        return true;
    }

    bool testXPathNSResolverCustom(WebKitWebPage* page)
    {
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert_true(WEBKIT_DOM_IS_DOCUMENT(document));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(document));

        GRefPtr<WebKitDOMXPathNSResolver> resolver = adoptGRef(WEBKIT_DOM_XPATH_NS_RESOLVER(g_object_new(webkit_xpath_ns_resolver_get_type(), nullptr)));
        assertObjectIsDeletedWhenTestFinishes(G_OBJECT(resolver.get()));
        evaluateFooChildTextAndCheckResult(document, resolver.get());

        return true;
    }

    bool runTest(const char* testName, WebKitWebPage* page) override
    {
        if (!strcmp(testName, "native"))
            return testXPathNSResolverNative(page);
        if (!strcmp(testName, "custom"))
            return testXPathNSResolverCustom(page);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitDOMXPathNSResolverTest, "WebKitDOMXPathNSResolver/native");
    REGISTER_TEST(WebKitDOMXPathNSResolverTest, "WebKitDOMXPathNSResolver/custom");
}

G_GNUC_END_IGNORE_DEPRECATIONS;
