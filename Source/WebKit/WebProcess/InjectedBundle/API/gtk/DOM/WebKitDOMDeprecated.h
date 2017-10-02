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

#ifndef WebKitDOMDeprecated_h
#define WebKitDOMDeprecated_h

#if !defined(WEBKIT_DISABLE_DEPRECATED)

#include <glib.h>
#include <webkitdom/WebKitDOMNode.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

/**
 * webkit_dom_html_element_get_inner_html:
 * @self: a #WebKitDOMHTMLElement
 *
 * Returns: a #gchar
 *
 * Deprecated: 2.8: Use webkit_dom_element_get_inner_html() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_inner_html) gchar*
webkit_dom_html_element_get_inner_html(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_set_inner_html:
 * @self: a #WebKitDOMHTMLElement
 * @contents: a #gchar with contents to set
 * @error: a #GError or %NULL
 *
 * Deprecated: 2.8: Use webkit_dom_element_set_inner_html() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_set_inner_html) void
webkit_dom_html_element_set_inner_html(WebKitDOMHTMLElement* self, const gchar* contents, GError** error);

/**
 * webkit_dom_html_element_get_outer_html:
 * @self: a #WebKitDOMHTMLElement
 *
 * Returns: a #gchar
 *
 * Deprecated: 2.8: Use webkit_dom_element_get_outer_html() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_outer_html) gchar*
webkit_dom_html_element_get_outer_html(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_html_element_set_outer_html:
 * @self: a #WebKitDOMHTMLElement
 * @contents: a #gchar with contents to set
 * @error: a #GError or %NULL
 *
 * Deprecated: 2.8: Use webkit_dom_element_set_outer_html() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_set_outer_html) void
webkit_dom_html_element_set_outer_html(WebKitDOMHTMLElement* self, const gchar* contents, GError** error);

/**
 * webkit_dom_html_element_get_children:
 * @self: A #WebKitDOMHTMLElement
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.10: Use webkit_dom_element_get_children() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_children) WebKitDOMHTMLCollection*
webkit_dom_html_element_get_children(WebKitDOMHTMLElement* self);

/**
 * webkit_dom_document_get_elements_by_tag_name:
 * @self: A #WebKitDOMDocument
 * @tag_name: a #gchar with the tag name
 *
 * Returns: (transfer full): a #WebKitDOMNodeList
 *
 * Deprecated: 2.12: Use webkit_dom_document_get_elements_by_tag_name_as_html_collection() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_get_elements_by_tag_name_as_html_collection) WebKitDOMNodeList*
webkit_dom_document_get_elements_by_tag_name(WebKitDOMDocument* self, const gchar* tag_name);

/**
 * webkit_dom_document_get_elements_by_tag_name_ns:
 * @self: A #WebKitDOMDocument
 * @namespace_uri: a #gchar with the namespace URI
 * @tag_name: a #gchar with the tag name
 *
 * Returns: (transfer full): a #WebKitDOMNodeList
 *
 * Deprecated: 2.12: Use webkit_dom_document_get_elements_by_tag_name_ns_as_html_collection() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_get_elements_by_tag_name_as_html_collection) WebKitDOMNodeList*
webkit_dom_document_get_elements_by_tag_name_ns(WebKitDOMDocument* self, const gchar* namespace_uri, const gchar* tag_name);


/**
 * webkit_dom_document_get_elements_by_class_name:
 * @self: A #WebKitDOMDocument
 * @class_name: a #gchar with the tag name
 *
 * Returns: (transfer full): a #WebKitDOMNodeList
 *
 * Deprecated: 2.12: Use webkit_dom_document_get_elements_by_class_name_as_html_collection() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_get_elements_by_class_name_as_html_collection) WebKitDOMNodeList*
webkit_dom_document_get_elements_by_class_name(WebKitDOMDocument* self, const gchar* class_name);

/**
 * webkit_dom_element_get_elements_by_tag_name:
 * @self: A #WebKitDOMElement
 * @tag_name: a #gchar with the tag name
 *
 * Returns: (transfer full): a #WebKitDOMNodeList
 *
 * Deprecated: 2.12: Use webkit_dom_element_get_elements_by_tag_name_as_html_collection() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_elements_by_tag_name_as_html_collection) WebKitDOMNodeList*
webkit_dom_element_get_elements_by_tag_name(WebKitDOMElement* self, const gchar* tag_name);

/**
 * webkit_dom_element_get_elements_by_tag_name_ns:
 * @self: A #WebKitDOMElement
 * @namespace_uri: a #gchar with the namespace URI
 * @tag_name: a #gchar with the tag name
 *
 * Returns: (transfer full): a #WebKitDOMNodeList
 *
 * Deprecated: 2.12: Use webkit_dom_element_get_elements_by_tag_name_ns_as_html_collection() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_elements_by_tag_name_as_html_collection) WebKitDOMNodeList*
webkit_dom_element_get_elements_by_tag_name_ns(WebKitDOMElement* self, const gchar* namespace_uri, const gchar* tag_name);


/**
 * webkit_dom_element_get_elements_by_class_name:
 * @self: A #WebKitDOMElement
 * @class_name: a #gchar with the tag name
 *
 * Returns: (transfer full): a #WebKitDOMNodeList
 *
 * Deprecated: 2.12: Use webkit_dom_element_get_elements_by_class_name_as_html_collection() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_get_elements_by_class_name_as_html_collection) WebKitDOMNodeList*
webkit_dom_element_get_elements_by_class_name(WebKitDOMElement* self, const gchar* class_name);

/**
 * webkit_dom_node_clone_node:
 * @self: A #WebKitDOMNode
 * @deep: A #gboolean
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMNode
 *
 * Deprecated: 2.14: Use webkit_dom_node_clone_node_with_error() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_node_clone_node_with_error) WebKitDOMNode*
webkit_dom_node_clone_node(WebKitDOMNode* self, gboolean deep, GError** error);


/**
 * webkit_dom_document_get_default_charset:
 * @self: A #WebKitDOMDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.14
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_document_get_default_charset(WebKitDOMDocument* self);

/**
 * webkit_dom_text_replace_whole_text:
 * @self: A #WebKitDOMText
 * @content: A #gchar
 * @error: #GError
 *
 * Returns: (transfer none): A #WebKitDOMText
 *
 * Deprecated: 2.14
 */
WEBKIT_DEPRECATED WebKitDOMText*
webkit_dom_text_replace_whole_text(WebKitDOMText* self, const gchar* content, GError** error);

/**
 * webkit_dom_html_input_element_get_capture:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.14: Use webkit_dom_html_input_element_get_capture_type() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_html_input_element_get_capture_type) gboolean
webkit_dom_html_input_element_get_capture(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_document_get_design_mode:
 * @self: A #WebKitDOMHTMLDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.14: Use webkit_dom_document_get_design_mode() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_get_design_mode) gchar*
webkit_dom_html_document_get_design_mode(WebKitDOMHTMLDocument* self);

/**
 * webkit_dom_html_document_set_design_mode:
 * @self: A #WebKitDOMHTMLDocument
 * @value: A #gchar
 *
 * Deprecated: 2.14: Use webkit_dom_document_set_design_mode() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_set_design_mode) void
webkit_dom_html_document_set_design_mode(WebKitDOMHTMLDocument* self, const gchar* value);

/**
 * webkit_dom_html_document_get_compat_mode:
 * @self: A #WebKitDOMHTMLDocument
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.14: Use webkit_dom_document_get_compat_mode() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_get_compat_mode) gchar*
webkit_dom_html_document_get_compat_mode(WebKitDOMHTMLDocument* self);

/**
 * webkit_dom_html_document_get_embeds:
 * @self: A #WebKitDOMHTMLDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.14: Use webkit_dom_document_get_embeds() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_get_embeds) WebKitDOMHTMLCollection*
webkit_dom_html_document_get_embeds(WebKitDOMHTMLDocument* self);

/**
 * webkit_dom_html_document_get_plugins:
 * @self: A #WebKitDOMHTMLDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.14: Use webkit_dom_document_get_plugins() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_get_plugins) WebKitDOMHTMLCollection*
webkit_dom_html_document_get_plugins(WebKitDOMHTMLDocument* self);

/**
 * webkit_dom_html_document_get_scripts:
 * @self: A #WebKitDOMHTMLDocument
 *
 * Returns: (transfer full): A #WebKitDOMHTMLCollection
 *
 * Deprecated: 2.14: Use webkit_dom_document_get_scripts() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_document_get_scripts) WebKitDOMHTMLCollection*
webkit_dom_html_document_get_scripts(WebKitDOMHTMLDocument* self);

/**
 * webkit_dom_node_get_namespace_uri:
 * @self: A #WebKitDOMNode
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.14: Use webkit_dom_attr_get_namespace_uri() or webkit_dom_element_get_namespace_uri() instead.
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_node_get_namespace_uri(WebKitDOMNode* self);

/**
 * webkit_dom_node_get_prefix:
 * @self: A #WebKitDOMNode
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.14: Use webkit_dom_attr_get_prefix() or webkit_dom_element_get_prefix() instead.
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_node_get_prefix(WebKitDOMNode* self);

/**
 * webkit_dom_node_set_prefix:
 * @self: A #WebKitDOMNode
 * @value: A #gchar
 * @error: #GError
 *
 * Deprecated: 2.14
 */
WEBKIT_DEPRECATED void
webkit_dom_node_set_prefix(WebKitDOMNode* self, const gchar* value, GError** error);

/**
 * webkit_dom_node_get_local_name:
 * @self: A #WebKitDOMNode
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.14: Use webkit_dom_attr_get_local_name() or webkit_dom_element_get_local_name() instead.
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_node_get_local_name(WebKitDOMNode* self);

#define WEBKIT_DOM_TYPE_ENTITY_REFERENCE            (webkit_dom_entity_reference_get_type())
#define WEBKIT_DOM_ENTITY_REFERENCE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_ENTITY_REFERENCE, WebKitDOMEntityReference))
#define WEBKIT_DOM_ENTITY_REFERENCE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_ENTITY_REFERENCE, WebKitDOMEntityReferenceClass)
#define WEBKIT_DOM_IS_ENTITY_REFERENCE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_ENTITY_REFERENCE))
#define WEBKIT_DOM_IS_ENTITY_REFERENCE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_ENTITY_REFERENCE))
#define WEBKIT_DOM_ENTITY_REFERENCE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_ENTITY_REFERENCE, WebKitDOMEntityReferenceClass))

typedef struct _WebKitDOMEntityReference WebKitDOMEntityReference;
typedef struct _WebKitDOMEntityReferenceClass WebKitDOMEntityReferenceClass;

struct _WebKitDOMEntityReference {
    WebKitDOMNode parent_instance;
};

struct _WebKitDOMEntityReferenceClass {
    WebKitDOMNodeClass parent_class;
};

WEBKIT_DEPRECATED GType webkit_dom_entity_reference_get_type(void);

/**
 * webkit_dom_node_iterator_get_expand_entity_references:
 * @self: A #WebKitDOMNodeIterator
 *
 * This function has been removed from the DOM spec and it just returns %FALSE.
 *
 * Returns: A #gboolean                                                                                                                                                                       *
 * Deprecated: 2.12
 */
WEBKIT_DEPRECATED gboolean webkit_dom_node_iterator_get_expand_entity_references(WebKitDOMNodeIterator* self);

/**
 * webkit_dom_tree_walker_get_expand_entity_references:
 * @self: A #WebKitDOMTreeWalker
 *
 * This function has been removed from the DOM spec and it just returns %FALSE.
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.12
 */
WEBKIT_DEPRECATED gboolean webkit_dom_tree_walker_get_expand_entity_references(WebKitDOMTreeWalker* self);

/**
 * webkit_dom_document_create_entity_reference:
 * @self: A #WebKitDOMDocument
 * @name: (allow-none): A #gchar
 * @error: #GError
 *
 * This function has been removed from the DOM spec and it just returns %NULL.
 *
 * Returns: (transfer none): A #WebKitDOMEntityReference
 *
 * Deprecated: 2.12
 */
WEBKIT_DEPRECATED WebKitDOMEntityReference* webkit_dom_document_create_entity_reference(WebKitDOMDocument* self, const gchar* name, GError** error);

#define WEBKIT_DOM_TYPE_HTML_BASE_FONT_ELEMENT            (webkit_dom_html_base_font_element_get_type())
#define WEBKIT_DOM_HTML_BASE_FONT_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_BASE_FONT_ELEMENT, WebKitDOMHTMLBaseFontElement))
#define WEBKIT_DOM_HTML_BASE_FONT_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_BASE_FONT_ELEMENT, WebKitDOMHTMLBaseFontElementClass)
#define WEBKIT_DOM_IS_HTML_BASE_FONT_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_BASE_FONT_ELEMENT))
#define WEBKIT_DOM_IS_HTML_BASE_FONT_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_BASE_FONT_ELEMENT))
#define WEBKIT_DOM_HTML_BASE_FONT_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_BASE_FONT_ELEMENT, WebKitDOMHTMLBaseFontElementClass))

typedef struct _WebKitDOMHTMLBaseFontElement WebKitDOMHTMLBaseFontElement;
typedef struct _WebKitDOMHTMLBaseFontElementClass WebKitDOMHTMLBaseFontElementClass;

struct _WebKitDOMHTMLBaseFontElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLBaseFontElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_base_font_element_get_type(void);

/**
 * webkit_dom_html_base_font_element_get_color:
 * @self: A #WebKitDOMHTMLBaseFontElement
 *
 * This function has been removed from the DOM spec and it just returns %NULL.
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.12
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_html_base_font_element_get_color(WebKitDOMHTMLBaseFontElement* self);

/**
 * webkit_dom_html_base_font_element_set_color:
 * @self: A #WebKitDOMHTMLBaseFontElement
 * @value: A #gchar
 *
 * This function has been removed from the DOM spec and it does nothing.
 *
 * Deprecated: 2.12
 */
WEBKIT_DEPRECATED void
webkit_dom_html_base_font_element_set_color(WebKitDOMHTMLBaseFontElement* self, const gchar* value);

/**
 * webkit_dom_html_base_font_element_get_face:
 * @self: A #WebKitDOMHTMLBaseFontElement
 *
 * This function has been removed from the DOM spec and it just returns %NULL.
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.12
 */
WEBKIT_DEPRECATED gchar*
webkit_dom_html_base_font_element_get_face(WebKitDOMHTMLBaseFontElement* self);

/**
 * webkit_dom_html_base_font_element_set_face:
 * @self: A #WebKitDOMHTMLBaseFontElement
 * @value: A #gchar
 *
 * This function has been removed from the DOM spec and it does nothing.
 *
 * Deprecated: 2.12
 */
WEBKIT_DEPRECATED void
webkit_dom_html_base_font_element_set_face(WebKitDOMHTMLBaseFontElement* self, const gchar* value);

/**
 * webkit_dom_html_base_font_element_get_size:
 * @self: A #WebKitDOMHTMLBaseFontElement
 *
 * This function has been removed from the DOM spec and it just returns 0.
 *
 * Returns: A #glong
 *
 * Deprecated: 2.12
 */
WEBKIT_DEPRECATED glong
webkit_dom_html_base_font_element_get_size(WebKitDOMHTMLBaseFontElement* self);

/**
 * webkit_dom_html_base_font_element_set_size:
 * @self: A #WebKitDOMHTMLBaseFontElement
 * @value: A #glong
 *
 * This function has been removed from the DOM spec and it does nothing.
 *
 * Deprecated: 2.12
 */
WEBKIT_DEPRECATED void
webkit_dom_html_base_font_element_set_size(WebKitDOMHTMLBaseFontElement* self, glong value);

/**
 * webkit_dom_element_get_webkit_region_overset:
 * @self: A #WebKitDOMElement
 *
 * CSS Regions support has been removed. This function does nothing.
 *
 * Returns: %NULL
 *
 * Deprecated: 2.20
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_element_get_webkit_region_overset(WebKitDOMElement* self);

G_END_DECLS

#endif /* WEBKIT_DISABLE_DEPRECATED */

#endif
