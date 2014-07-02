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
#include <wtf/gobject/GUniquePtr.h>

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
    static PassOwnPtr<WebProcessTest> create() { return adoptPtr(new WebKitDOMXPathNSResolverTest()); }

private:
    guint64 webPageFromArgs(GVariant* args)
    {
        GVariantIter iter;
        g_variant_iter_init(&iter, args);

        const char* key;
        GVariant* value;
        while (g_variant_iter_loop(&iter, "{&sv}", &key, &value)) {
            if (!strcmp(key, "pageID") && g_variant_classify(value) == G_VARIANT_CLASS_UINT64)
                return g_variant_get_uint64(value);
        }

        g_assert_not_reached();
        return 0;
    }

    void evaluateFooChildTextAndCheckResult(WebKitDOMDocument* document, WebKitDOMXPathNSResolver* resolver)
    {
        WebKitDOMElement* documentElement = webkit_dom_document_get_document_element(document);
        g_assert(WEBKIT_DOM_IS_ELEMENT(documentElement));

        WebKitDOMXPathResult* result = webkit_dom_document_evaluate(document, "foo:child/text()", WEBKIT_DOM_NODE(documentElement), resolver, WEBKIT_DOM_XPATH_RESULT_ORDERED_NODE_ITERATOR_TYPE, nullptr, nullptr);
        g_assert(WEBKIT_DOM_IS_XPATH_RESULT(result));

        WebKitDOMNode* nodeResult = webkit_dom_xpath_result_iterate_next(result, nullptr);
        g_assert(WEBKIT_DOM_IS_NODE(nodeResult));

        GUniquePtr<char> nodeValue(webkit_dom_node_get_node_value(nodeResult));
        g_assert_cmpstr(nodeValue.get(), ==, "SUCCESS");
    }

    bool testXPathNSResolverNative(WebKitWebExtension* extension, GVariant* args)
    {
        WebKitWebPage* page = webkit_web_extension_get_page(extension, webPageFromArgs(args));
        g_assert(WEBKIT_IS_WEB_PAGE(page));
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));

        WebKitDOMXPathNSResolver* resolver = webkit_dom_document_create_ns_resolver(document, WEBKIT_DOM_NODE(webkit_dom_document_get_document_element(document)));
        g_assert(WEBKIT_DOM_IS_XPATH_NS_RESOLVER(resolver));
        evaluateFooChildTextAndCheckResult(document, resolver);

        return true;
    }

    bool testXPathNSResolverCustom(WebKitWebExtension* extension, GVariant* args)
    {
        WebKitWebPage* page = webkit_web_extension_get_page(extension, webPageFromArgs(args));
        g_assert(WEBKIT_IS_WEB_PAGE(page));
        WebKitDOMDocument* document = webkit_web_page_get_dom_document(page);
        g_assert(WEBKIT_DOM_IS_DOCUMENT(document));

        GRefPtr<WebKitDOMXPathNSResolver> resolver = adoptGRef(WEBKIT_DOM_XPATH_NS_RESOLVER(g_object_new(webkit_xpath_ns_resolver_get_type(), nullptr)));
        evaluateFooChildTextAndCheckResult(document, resolver.get());

        return true;
    }

    virtual bool runTest(const char* testName, WebKitWebExtension* extension, GVariant* args)
    {
        if (!strcmp(testName, "native"))
            return testXPathNSResolverNative(extension, args);
        if (!strcmp(testName, "custom"))
            return testXPathNSResolverCustom(extension, args);

        g_assert_not_reached();
        return false;
    }
};

static void __attribute__((constructor)) registerTests()
{
    REGISTER_TEST(WebKitDOMXPathNSResolverTest, "WebKitDOMXPathNSResolver/native");
    REGISTER_TEST(WebKitDOMXPathNSResolverTest, "WebKitDOMXPathNSResolver/custom");
}
