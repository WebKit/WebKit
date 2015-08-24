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
#include "WebKitDOMHTMLElement.h"
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
