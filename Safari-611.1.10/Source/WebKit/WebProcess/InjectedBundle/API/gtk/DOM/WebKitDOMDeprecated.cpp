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

#include "ConvertToUTF8String.h"
#include <WebCore/DOMException.h>
#include <WebCore/Document.h>
#include <WebCore/Element.h>
#include <WebCore/JSExecState.h>
#include <WebCore/HTMLCollection.h>
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMHTMLDocumentPrivate.h"
#include "WebKitDOMHTMLInputElementPrivate.h"
#include "WebKitDOMHTMLTitleElement.h"
#include "WebKitDOMNodeListPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMTextPrivate.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

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
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(document->getElementsByTagName(String::fromUTF8(tagName)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_document_get_elements_by_tag_name_ns(WebKitDOMDocument* self, const gchar* namespaceURI, const gchar* tagName)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), nullptr);
    g_return_val_if_fail(namespaceURI, nullptr);
    g_return_val_if_fail(tagName, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Document* document = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(document->getElementsByTagNameNS(String::fromUTF8(namespaceURI), String::fromUTF8(tagName)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_document_get_elements_by_class_name(WebKitDOMDocument* self, const gchar* className)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), nullptr);
    g_return_val_if_fail(className, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Document* document = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(document->getElementsByClassName(String::fromUTF8(className)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_element_get_elements_by_tag_name(WebKitDOMElement* self, const gchar* tagName)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), nullptr);
    g_return_val_if_fail(tagName, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Element* element = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(element->getElementsByTagName(String::fromUTF8(tagName)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_element_get_elements_by_tag_name_ns(WebKitDOMElement* self, const gchar* namespaceURI, const gchar* tagName)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), nullptr);
    g_return_val_if_fail(namespaceURI, nullptr);
    g_return_val_if_fail(tagName, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Element* element = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(element->getElementsByTagNameNS(String::fromUTF8(namespaceURI), String::fromUTF8(tagName)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNodeList* webkit_dom_element_get_elements_by_class_name(WebKitDOMElement* self, const gchar* className)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(self), nullptr);
    g_return_val_if_fail(className, nullptr);

    WebCore::JSMainThreadNullState state;
    WebCore::Element* element = WebKit::core(self);
    RefPtr<WebCore::NodeList> nodeList = WTF::getPtr(element->getElementsByClassName(String::fromUTF8(className)));
    return WebKit::kit(nodeList.get());
}

WebKitDOMNode* webkit_dom_node_clone_node(WebKitDOMNode* self, gboolean deep)
{
    return webkit_dom_node_clone_node_with_error(self, deep, nullptr);
}

gchar* webkit_dom_document_get_default_charset(WebKitDOMDocument* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOCUMENT(self), nullptr);
    return convertToUTF8String(WebKit::core(self)->defaultCharsetForLegacyBindings());
}

WebKitDOMText* webkit_dom_text_replace_whole_text(WebKitDOMText* self, const gchar* content, GError** error)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_TEXT(self), nullptr);
    g_return_val_if_fail(content, nullptr);
    g_return_val_if_fail(!error || !*error, nullptr);

    WebCore::JSMainThreadNullState state;
    return WebKit::kit(WebKit::core(self)->replaceWholeText(WTF::String::fromUTF8(content)).get());
}

gboolean webkit_dom_html_input_element_get_capture(WebKitDOMHTMLInputElement* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);

#if ENABLE(MEDIA_CAPTURE)
    WebCore::JSMainThreadNullState state;
    WebCore::HTMLInputElement* item = WebKit::core(self);
    return item->mediaCaptureType() != WebCore::MediaCaptureTypeNone;
#else
    UNUSED_PARAM(self);
    WEBKIT_WARN_FEATURE_NOT_PRESENT("Media Capture")
    return FALSE;
#endif /* ENABLE(MEDIA_CAPTURE) */
}

gchar* webkit_dom_html_document_get_design_mode(WebKitDOMHTMLDocument* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), nullptr);
    return webkit_dom_document_get_design_mode(WEBKIT_DOM_DOCUMENT(self));
}


void webkit_dom_html_document_set_design_mode(WebKitDOMHTMLDocument* self, const gchar* value)
{
    g_return_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self));
    webkit_dom_document_set_design_mode(WEBKIT_DOM_DOCUMENT(self), value);
}

gchar* webkit_dom_html_document_get_compat_mode(WebKitDOMHTMLDocument* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), nullptr);
    return webkit_dom_document_get_compat_mode(WEBKIT_DOM_DOCUMENT(self));
}

WebKitDOMHTMLCollection* webkit_dom_html_document_get_embeds(WebKitDOMHTMLDocument* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), nullptr);
    return webkit_dom_document_get_embeds(WEBKIT_DOM_DOCUMENT(self));
}

WebKitDOMHTMLCollection* webkit_dom_html_document_get_plugins(WebKitDOMHTMLDocument* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), nullptr);
    return webkit_dom_document_get_plugins(WEBKIT_DOM_DOCUMENT(self));
}

WebKitDOMHTMLCollection* webkit_dom_html_document_get_scripts(WebKitDOMHTMLDocument* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_DOCUMENT(self), nullptr);
    return webkit_dom_document_get_scripts(WEBKIT_DOM_DOCUMENT(self));
}

gchar* webkit_dom_node_get_namespace_uri(WebKitDOMNode* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(self), nullptr);

    WebCore::JSMainThreadNullState state;
    return convertToUTF8String(WebKit::core(self)->namespaceURI());
}

gchar* webkit_dom_node_get_prefix(WebKitDOMNode* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(self), nullptr);
    WebCore::JSMainThreadNullState state;
    return convertToUTF8String(WebKit::core(self)->prefix());
}

void webkit_dom_node_set_prefix(WebKitDOMNode* self, const gchar* value, GError** error)
{
    g_return_if_fail(WEBKIT_DOM_IS_NODE(self));
    g_return_if_fail(value);
    g_return_if_fail(!error || !*error);

    g_warning("%s: prefix is now a readonly property according to the DOM spec.", __func__);

    WebCore::JSMainThreadNullState state;
    WebCore::Node* item = WebKit::core(self);
    WTF::String convertedValue = WTF::String::fromUTF8(value);
    auto result = item->setPrefix(convertedValue);
    if (result.hasException()) {
        auto description = WebCore::DOMException::description(result.releaseException().code());
        g_set_error_literal(error, g_quark_from_string("WEBKIT_DOM"), description.legacyCode, description.name);
    }
}

gchar* webkit_dom_node_get_local_name(WebKitDOMNode* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_NODE(self), nullptr);
    WebCore::JSMainThreadNullState state;
    return convertToUTF8String(WebKit::core(self)->localName());
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

gchar* webkit_dom_element_get_webkit_region_overset(WebKitDOMElement*)
{
    g_warning("%s: CSS Regions support has been removed, this function does nothing.", __func__);
    return nullptr;
}
G_GNUC_END_IGNORE_DEPRECATIONS;
