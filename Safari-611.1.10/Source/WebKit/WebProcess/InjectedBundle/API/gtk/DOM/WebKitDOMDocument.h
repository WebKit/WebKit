/*
 *  This file is part of the WebKit open source project.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#if !defined(__WEBKITDOM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <webkitdom/webkitdom.h> can be included directly."
#endif

#ifndef WebKitDOMDocument_h
#define WebKitDOMDocument_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMNode.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_DOCUMENT            (webkit_dom_document_get_type())
#define WEBKIT_DOM_DOCUMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_DOCUMENT, WebKitDOMDocument))
#define WEBKIT_DOM_DOCUMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_DOCUMENT, WebKitDOMDocumentClass)
#define WEBKIT_DOM_IS_DOCUMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_DOCUMENT))
#define WEBKIT_DOM_IS_DOCUMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_DOCUMENT))
#define WEBKIT_DOM_DOCUMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_DOCUMENT, WebKitDOMDocumentClass))

struct _WebKitDOMDocument {
    WebKitDOMNode parent_instance;
};

struct _WebKitDOMDocumentClass {
    WebKitDOMNodeClass parent_class;
};

WEBKIT_API GType
webkit_dom_document_get_type(void);

/**
 * webkit_dom_document_create_element:
 * @self: A #WebKitDOMDocument
 * @tagName: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_create_element(WebKitDOMDocument* self, const gchar* tagName, GError** error);

/**
 * webkit_dom_document_create_document_fragment:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMDocumentFragment
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocumentFragment*
webkit_dom_document_create_document_fragment(WebKitDOMDocument* self);

/**
 * webkit_dom_document_create_text_node:
 * @self: A #WebKitDOMDocument
 * @data: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMText
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMText*
webkit_dom_document_create_text_node(WebKitDOMDocument* self, const gchar* data);

/**
 * webkit_dom_document_create_comment:
 * @self: A #WebKitDOMDocument
 * @data: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMComment
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMComment*
webkit_dom_document_create_comment(WebKitDOMDocument* self, const gchar* data);

/**
 * webkit_dom_document_create_cdata_section:
 * @self: A #WebKitDOMDocument
 * @data: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMCDATASection
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCDATASection*
webkit_dom_document_create_cdata_section(WebKitDOMDocument* self, const gchar* data, GError** error);

/**
 * webkit_dom_document_create_processing_instruction:
 * @self: A #WebKitDOMDocument
 * @target: A #gchar
 * @data: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMProcessingInstruction
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMProcessingInstruction*
webkit_dom_document_create_processing_instruction(WebKitDOMDocument* self, const gchar* target, const gchar* data, GError** error);

/**
 * webkit_dom_document_create_attribute:
 * @self: A #WebKitDOMDocument
 * @name: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMAttr
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMAttr*
webkit_dom_document_create_attribute(WebKitDOMDocument* self, const gchar* name, GError** error);

/**
 * webkit_dom_document_get_elements_by_tag_name_as_html_collection:
 * @self: A #WebKitDOMDocument
 * @tagname: A #gchar
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.12
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_elements_by_tag_name_as_html_collection(WebKitDOMDocument* self, const gchar* tagname);

/**
 * webkit_dom_document_import_node:
 * @self: A #WebKitDOMDocument
 * @importedNode: A #WebKitDOMNode
 * @deep: A #gboolean
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_document_import_node(WebKitDOMDocument* self, WebKitDOMNode* importedNode, gboolean deep, GError** error);

/**
 * webkit_dom_document_create_element_ns:
 * @self: A #WebKitDOMDocument
 * @namespaceURI: (allow-none): A #gchar
 * @qualifiedName: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_create_element_ns(WebKitDOMDocument* self, const gchar* namespaceURI, const gchar* qualifiedName, GError** error);

/**
 * webkit_dom_document_create_attribute_ns:
 * @self: A #WebKitDOMDocument
 * @namespaceURI: (allow-none): A #gchar
 * @qualifiedName: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMAttr
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMAttr*
webkit_dom_document_create_attribute_ns(WebKitDOMDocument* self, const gchar* namespaceURI, const gchar* qualifiedName, GError** error);

/**
 * webkit_dom_document_get_elements_by_tag_name_ns_as_html_collection:
 * @self: A #WebKitDOMDocument
 * @namespaceURI: A #gchar
 * @localName: A #gchar
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.12
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_elements_by_tag_name_ns_as_html_collection(WebKitDOMDocument* self, const gchar* namespaceURI, const gchar* localName);

/**
 * webkit_dom_document_adopt_node:
 * @self: A #WebKitDOMDocument
 * @source: A #WebKitDOMNode
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNode*
webkit_dom_document_adopt_node(WebKitDOMDocument* self, WebKitDOMNode* source, GError** error);

/**
 * webkit_dom_document_create_event:
 * @self: A #WebKitDOMDocument
 * @eventType: A #gchar
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMEvent
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMEvent*
webkit_dom_document_create_event(WebKitDOMDocument* self, const gchar* eventType, GError** error);

/**
 * webkit_dom_document_create_range:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMRange
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMRange*
webkit_dom_document_create_range(WebKitDOMDocument* self);

/**
 * webkit_dom_document_create_node_iterator:
 * @self: A #WebKitDOMDocument
 * @root: A #WebKitDOMNode
 * @whatToShow: A #gulong
 * @filter: (allow-none): A #WebKitDOMNodeFilter
 * @expandEntityReferences: A #gboolean
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMNodeIterator
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNodeIterator*
webkit_dom_document_create_node_iterator(WebKitDOMDocument* self, WebKitDOMNode* root, gulong whatToShow, WebKitDOMNodeFilter* filter, gboolean expandEntityReferences, GError** error);

/**
 * webkit_dom_document_create_tree_walker:
 * @self: A #WebKitDOMDocument
 * @root: A #WebKitDOMNode
 * @whatToShow: A #gulong
 * @filter: (allow-none): A #WebKitDOMNodeFilter
 * @expandEntityReferences: A #gboolean
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMTreeWalker
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMTreeWalker*
webkit_dom_document_create_tree_walker(WebKitDOMDocument* self, WebKitDOMNode* root, gulong whatToShow, WebKitDOMNodeFilter* filter, gboolean expandEntityReferences, GError** error);

/**
 * webkit_dom_document_get_override_style:
 * @self: A #WebKitDOMDocument
 * @element: A #WebKitDOMElement
 * @pseudoElement: (allow-none): A #gchar
 *
 * Returns: (transfer full): A #WebKitDOMCSSStyleDeclaration
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSStyleDeclaration*
webkit_dom_document_get_override_style(WebKitDOMDocument* self, WebKitDOMElement* element, const gchar* pseudoElement);

/**
 * webkit_dom_document_create_expression:
 * @self: A #WebKitDOMDocument
 * @expression: A #gchar
 * @resolver: A #WebKitDOMXPathNSResolver
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMXPathExpression
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMXPathExpression*
webkit_dom_document_create_expression(WebKitDOMDocument* self, const gchar* expression, WebKitDOMXPathNSResolver* resolver, GError** error);

/**
 * webkit_dom_document_create_ns_resolver:
 * @self: A #WebKitDOMDocument
 * @nodeResolver: A #WebKitDOMNode
 *
 * Returns: (transfer full): A #WebKitDOMXPathNSResolver
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMXPathNSResolver*
webkit_dom_document_create_ns_resolver(WebKitDOMDocument* self, WebKitDOMNode* nodeResolver);

/**
 * webkit_dom_document_evaluate:
 * @self: A #WebKitDOMDocument
 * @expression: A #gchar
 * @contextNode: A #WebKitDOMNode
 * @resolver: (allow-none): A #WebKitDOMXPathNSResolver
 * @type: A #gushort
 * @inResult: (allow-none): A #WebKitDOMXPathResult
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMXPathResult
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMXPathResult*
webkit_dom_document_evaluate(WebKitDOMDocument* self, const gchar* expression, WebKitDOMNode* contextNode, WebKitDOMXPathNSResolver* resolver, gushort type, WebKitDOMXPathResult* inResult, GError** error);

/**
 * webkit_dom_document_exec_command:
 * @self: A #WebKitDOMDocument
 * @command: A #gchar
 * @userInterface: A #gboolean
 * @value: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_exec_command(WebKitDOMDocument* self, const gchar* command, gboolean userInterface, const gchar* value);

/**
 * webkit_dom_document_query_command_enabled:
 * @self: A #WebKitDOMDocument
 * @command: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_query_command_enabled(WebKitDOMDocument* self, const gchar* command);

/**
 * webkit_dom_document_query_command_indeterm:
 * @self: A #WebKitDOMDocument
 * @command: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_query_command_indeterm(WebKitDOMDocument* self, const gchar* command);

/**
 * webkit_dom_document_query_command_state:
 * @self: A #WebKitDOMDocument
 * @command: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_query_command_state(WebKitDOMDocument* self, const gchar* command);

/**
 * webkit_dom_document_query_command_supported:
 * @self: A #WebKitDOMDocument
 * @command: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_query_command_supported(WebKitDOMDocument* self, const gchar* command);

/**
 * webkit_dom_document_query_command_value:
 * @self: A #WebKitDOMDocument
 * @command: A #gchar
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_query_command_value(WebKitDOMDocument* self, const gchar* command);

/**
 * webkit_dom_document_get_elements_by_name:
 * @self: A #WebKitDOMDocument
 * @elementName: A #gchar
 *
 * Returns: (transfer full): A #WebKitDOMNodeList
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNodeList*
webkit_dom_document_get_elements_by_name(WebKitDOMDocument* self, const gchar* elementName);

/**
 * webkit_dom_document_element_from_point:
 * @self: A #WebKitDOMDocument
 * @x: A #glong
 * @y: A #glong
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_element_from_point(WebKitDOMDocument* self, glong x, glong y);

/**
 * webkit_dom_document_create_css_style_declaration:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMCSSStyleDeclaration
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSStyleDeclaration*
webkit_dom_document_create_css_style_declaration(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_elements_by_class_name_as_html_collection:
 * @self: A #WebKitDOMDocument
 * @classNames: A #gchar
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.12
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_elements_by_class_name_as_html_collection(WebKitDOMDocument* self, const gchar* classNames);

/**
 * webkit_dom_document_has_focus:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_has_focus(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_element_by_id:
 * @self: A #WebKitDOMDocument
 * @elementId: A #gchar
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_get_element_by_id(WebKitDOMDocument* self, const gchar* elementId);

/**
 * webkit_dom_document_query_selector:
 * @self: A #WebKitDOMDocument
 * @selectors: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_query_selector(WebKitDOMDocument* self, const gchar* selectors, GError** error);

/**
 * webkit_dom_document_query_selector_all:
 * @self: A #WebKitDOMDocument
 * @selectors: A #gchar
 * @error: #GError
 *
 * Returns: (transfer full): A #WebKitDOMNodeList
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMNodeList*
webkit_dom_document_query_selector_all(WebKitDOMDocument* self, const gchar* selectors, GError** error);

/**
 * webkit_dom_document_get_doctype:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMDocumentType
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDocumentType*
webkit_dom_document_get_doctype(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_implementation:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMDOMImplementation
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDOMImplementation*
webkit_dom_document_get_implementation(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_document_element:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_get_document_element(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_input_encoding:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_input_encoding(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_xml_encoding:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_xml_encoding(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_xml_version:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_xml_version(WebKitDOMDocument* self);

/**
 * webkit_dom_document_set_xml_version:
 * @self: A #WebKitDOMDocument
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_xml_version(WebKitDOMDocument* self, const gchar* value, GError** error);

/**
 * webkit_dom_document_get_xml_standalone:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_get_xml_standalone(WebKitDOMDocument* self);

/**
 * webkit_dom_document_set_xml_standalone:
 * @self: A #WebKitDOMDocument
 * @value: A #gboolean
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_xml_standalone(WebKitDOMDocument* self, gboolean value, GError** error);

/**
 * webkit_dom_document_get_document_uri:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_document_uri(WebKitDOMDocument* self);

/**
 * webkit_dom_document_set_document_uri:
 * @self: A #WebKitDOMDocument
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_document_uri(WebKitDOMDocument* self, const gchar* value);

/**
 * webkit_dom_document_get_default_view:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMDOMWindow
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMDOMWindow*
webkit_dom_document_get_default_view(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_style_sheets:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMStyleSheetList
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMStyleSheetList*
webkit_dom_document_get_style_sheets(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_title:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_title(WebKitDOMDocument* self);

/**
 * webkit_dom_document_set_title:
 * @self: A #WebKitDOMDocument
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_title(WebKitDOMDocument* self, const gchar* value);

/**
 * webkit_dom_document_get_design_mode:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_design_mode(WebKitDOMDocument* self);

/**
 * webkit_dom_document_set_design_mode:
 * @self: A #WebKitDOMDocument
 * @value: A #gchar
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_design_mode(WebKitDOMDocument* self, const gchar* value);

/**
 * webkit_dom_document_get_referrer:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_referrer(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_domain:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_domain(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_url:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_url(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_cookie:
 * @self: A #WebKitDOMDocument
 * @error: #GError
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_cookie(WebKitDOMDocument* self, GError** error);

/**
 * webkit_dom_document_set_cookie:
 * @self: A #WebKitDOMDocument
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_cookie(WebKitDOMDocument* self, const gchar* value, GError** error);

/**
 * webkit_dom_document_get_body:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMHTMLElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLElement*
webkit_dom_document_get_body(WebKitDOMDocument* self);

/**
 * webkit_dom_document_set_body:
 * @self: A #WebKitDOMDocument
 * @value: A #WebKitDOMHTMLElement
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_body(WebKitDOMDocument* self, WebKitDOMHTMLElement* value, GError** error);

/**
 * webkit_dom_document_get_head:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMHTMLHeadElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLHeadElement*
webkit_dom_document_get_head(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_images:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_images(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_applets:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_applets(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_links:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_links(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_forms:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_forms(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_anchors:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_anchors(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_embeds:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_embeds(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_plugins:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_plugins(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_scripts:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_scripts(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_last_modified:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_last_modified(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_charset:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_charset(WebKitDOMDocument* self);

/**
 * webkit_dom_document_set_charset:
 * @self: A #WebKitDOMDocument
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_charset(WebKitDOMDocument* self, const gchar* value);

/**
 * webkit_dom_document_get_ready_state:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_ready_state(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_character_set:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_character_set(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_preferred_stylesheet_set:
 * @self: A #WebKitDOMDocument
 *
 * This function has been removed and does nothing.
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_preferred_stylesheet_set(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_selected_stylesheet_set:
 * @self: A #WebKitDOMDocument
 *
 * This function has been removed and does nothing.
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_selected_stylesheet_set(WebKitDOMDocument* self);

/**
 * webkit_dom_document_set_selected_stylesheet_set:
 * @self: A #WebKitDOMDocument
 * @value: A #gchar
 *
 * This function has been removed and does nothing.
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_selected_stylesheet_set(WebKitDOMDocument* self, const gchar* value);

/**
 * webkit_dom_document_get_active_element:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_get_active_element(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_compat_mode:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_compat_mode(WebKitDOMDocument* self);

/**
 * webkit_dom_document_caret_range_from_point:
 * @self: A #WebKitDOMDocument
 * @x: A #glong
 * @y: A #glong
 *
 * Returns: (transfer full): A #WebKitDOMRange
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMRange*
webkit_dom_document_caret_range_from_point(WebKitDOMDocument* self, glong x, glong y);

/**
 * webkit_dom_document_webkit_cancel_fullscreen:
 * @self: A #WebKitDOMDocument
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_webkit_cancel_fullscreen(WebKitDOMDocument* self);

/**
 * webkit_dom_document_webkit_exit_fullscreen:
 * @self: A #WebKitDOMDocument
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_webkit_exit_fullscreen(WebKitDOMDocument* self);

/**
 * webkit_dom_document_exit_pointer_lock:
 * @self: A #WebKitDOMDocument
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_exit_pointer_lock(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_content_type:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_content_type(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_dir:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_dir(WebKitDOMDocument* self);

/**
 * webkit_dom_document_set_dir:
 * @self: A #WebKitDOMDocument
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_document_set_dir(WebKitDOMDocument* self, const gchar* value);

/**
 * webkit_dom_document_get_webkit_is_fullscreen:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_get_webkit_is_fullscreen(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_webkit_fullscreen_keyboard_input_allowed:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_get_webkit_fullscreen_keyboard_input_allowed(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_webkit_current_fullscreen_element:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_get_webkit_current_fullscreen_element(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_webkit_fullscreen_enabled:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_get_webkit_fullscreen_enabled(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_webkit_fullscreen_element:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_get_webkit_fullscreen_element(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_pointer_lock_element:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_get_pointer_lock_element(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_visibility_state:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_visibility_state(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_hidden:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_document_get_hidden(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_current_script:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMHTMLScriptElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLScriptElement*
webkit_dom_document_get_current_script(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_origin:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_origin(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_scrolling_element:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_get_scrolling_element(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_children:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLCollection*
webkit_dom_document_get_children(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_first_element_child:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_get_first_element_child(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_last_element_child:
 * @self: A #WebKitDOMDocument
 *
 * Returns: (transfer none): A #WebKitDOMElement
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMElement*
webkit_dom_document_get_last_element_child(WebKitDOMDocument* self);

/**
 * webkit_dom_document_get_child_element_count:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gulong
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_document_get_child_element_count(WebKitDOMDocument* self);

G_END_DECLS

#endif /* WebKitDOMDocument_h */
