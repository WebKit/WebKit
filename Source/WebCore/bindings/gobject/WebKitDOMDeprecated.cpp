/*
 *  Copyright (C) 2014 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitDOMDeprecated.h"

#include "Document.h"
#include "Element.h"
#include "JSMainThreadExecState.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMNodeListPrivate.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

gchar* webkit_dom_html_element_get_inner_html(WebKitDOMHTMLElement* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), nullptr);
    return webkit_dom_element_get_inner_html(WEBKIT_DOM_ELEMENT(self));
}

void webkit_dom_html_element_set_inner_html(WebKitDOMHTMLElement* self, const gchar* contents, GError** error)
{
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    webkit_dom_element_set_inner_html(WEBKIT_DOM_ELEMENT(self), contents, error);
}

gchar* webkit_dom_html_element_get_outer_html(WebKitDOMHTMLElement* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), nullptr);
    return webkit_dom_element_get_outer_html(WEBKIT_DOM_ELEMENT(self));
}

void webkit_dom_html_element_set_outer_html(WebKitDOMHTMLElement* self, const gchar* contents, GError** error)
{
    g_return_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self));
    webkit_dom_element_set_outer_html(WEBKIT_DOM_ELEMENT(self), contents, error);
}

WebKitDOMHTMLCollection* webkit_dom_html_element_get_children(WebKitDOMHTMLElement* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_ELEMENT(self), nullptr);
    return webkit_dom_element_get_children(WEBKIT_DOM_ELEMENT(self));
}

WebKitDOMNodeList* webkit_dom_document_get_elements_by_tag_name(WebKitDOMDocument* self, const gchar* tagName)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), nullptr);
    g_return_val_if_fail(tagName, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Document* document = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(document->getElementsByTagNameForObjC(String::fromUTF8(tagName)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_document_get_elements_by_tag_name_ns(WebKitDOMDocument* self, const gchar* namespaceURI, const gchar* tagName)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), nullptr);
    g_return_val_if_fail(namespaceURI, nullptr);
    g_return_val_if_fail(tagName, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Document* document = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(document->getElementsByTagNameNSForObjC(String::fromUTF8(namespaceURI), String::fromUTF8(tagName)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_document_get_elements_by_class_name(WebKitDOMDocument* self, const gchar* className)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), nullptr);
    g_return_val_if_fail(className, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Document* document = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(document->getElementsByClassNameForObjC(String::fromUTF8(className)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_element_get_elements_by_tag_name(WebKitDOMElement* self, const gchar* tagName)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), nullptr);
    g_return_val_if_fail(tagName, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Element* element = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(element->getElementsByTagNameForObjC(String::fromUTF8(tagName)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_element_get_elements_by_tag_name_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* tagName)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), nullptr);
    g_return_val_if_fail(namespaceURI, nullptr);
    g_return_val_if_fail(tagName, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Element* element = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(element->getElementsByTagNameNSForObjC(String::fromUTF8(namespaceURI), String::fromUTF8(tagName)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_element_get_elements_by_class_name(WebKitDOMElement* self, const gchar* className)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), nullptr);
    g_return_val_if_fail(className, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Element* element = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(element->getElementsByClassNameForObjC(String::fromUTF8(className)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNode* webkit_dom_node_clone_node(WebKitDOMNode* self, gboolean deep)
{
    return webkit_dom_node_clone_node_with_error(self, deep, nullptr);
}

G_DEFINE_TYPE(WebKitDOMEntityReference, webkit_dom_entity_reference, WEBKIT_DOM_TYPE_NODE)

static void webkit_dom_entity_reference_init(WebKitDOMEntityReference*)
{
}

static void webkit_dom_entity_reference_class_init(WebKitDOMEntityReferenceClass*)
{
}

gboolean webkit_dom_node_iterator_get_expand_entity_references(WebKitDOMNodeIterator*)
{
    g_warning("%s: EntityReference has been removed from DOM spec, this function does nothing.", __func__);
    return FALSE;
}

gboolean webkit_dom_tree_walker_get_expand_entity_references(WebKitDOMTreeWalker*)
{
    g_warning("%s: EntityReference has been removed from DOM spec, this function does nothing.", __func__);
    return FALSE;
}

WebKitDOMEntityReference* webkit_dom_document_create_entity_reference(WebKitDOMDocument*, const gchar*, GError**)
{
    g_warning("%s: EntityReference has been removed from DOM spec, this function does nothing.", __func__);
    return nullptr;
}

G_DEFINE_TYPE(WebKitDOMHTMLBaseFontElement, webkit_dom_html_base_font_element, WEBKIT_DOM_TYPE_HTML_ELEMENT)

static void webkit_dom_html_base_font_element_init(WebKitDOMHTMLBaseFontElement*)
{
}

static void webkit_dom_html_base_font_element_class_init(WebKitDOMHTMLBaseFontElementClass*)
{
}

gchar* webkit_dom_html_base_font_element_get_color(WebKitDOMHTMLBaseFontElement*)
{
    g_warning("%s: HTMLBaseFont has been removed from DOM spec, this function does nothing.", __func__);
    return nullptr;
}

void webkit_dom_html_base_font_element_set_color(WebKitDOMHTMLBaseFontElement*, const gchar*)
{
    g_warning("%s: HTMLBaseFont has been removed from DOM spec, this function does nothing.", __func__);
}

gchar* webkit_dom_html_base_font_element_get_face(WebKitDOMHTMLBaseFontElement*)
{
    g_warning("%s: HTMLBaseFont has been removed from DOM spec, this function does nothing.", __func__);
    return nullptr;
}

void webkit_dom_html_base_font_element_set_face(WebKitDOMHTMLBaseFontElement*, const gchar*)
{
    g_warning("%s: HTMLBaseFont has been removed from DOM spec, this function does nothing.", __func__);
}

glong webkit_dom_html_base_font_element_get_size(WebKitDOMHTMLBaseFontElement*)
{
    g_warning("%s: HTMLBaseFont has been removed from DOM spec, this function does nothing.", __func__);
    return 0;
}

void webkit_dom_html_base_font_element_set_size(WebKitDOMHTMLBaseFontElement*, glong)
{
    g_warning("%s: HTMLBaseFont has been removed from DOM spec, this function does nothing.", __func__);
}
