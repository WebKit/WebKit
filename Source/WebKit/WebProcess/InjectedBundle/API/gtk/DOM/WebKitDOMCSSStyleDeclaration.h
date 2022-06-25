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

#ifndef WebKitDOMCSSStyleDeclaration_h
#define WebKitDOMCSSStyleDeclaration_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_CSS_STYLE_DECLARATION            (webkit_dom_css_style_declaration_get_type())
#define WEBKIT_DOM_CSS_STYLE_DECLARATION(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_CSS_STYLE_DECLARATION, WebKitDOMCSSStyleDeclaration))
#define WEBKIT_DOM_CSS_STYLE_DECLARATION_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_CSS_STYLE_DECLARATION, WebKitDOMCSSStyleDeclarationClass)
#define WEBKIT_DOM_IS_CSS_STYLE_DECLARATION(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_CSS_STYLE_DECLARATION))
#define WEBKIT_DOM_IS_CSS_STYLE_DECLARATION_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_CSS_STYLE_DECLARATION))
#define WEBKIT_DOM_CSS_STYLE_DECLARATION_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_CSS_STYLE_DECLARATION, WebKitDOMCSSStyleDeclarationClass))

struct _WebKitDOMCSSStyleDeclaration {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMCSSStyleDeclarationClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_css_style_declaration_get_type(void);

/**
 * webkit_dom_css_style_declaration_get_property_value:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @propertyName: A #gchar
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_css_style_declaration_get_property_value(WebKitDOMCSSStyleDeclaration* self, const gchar* propertyName);

/**
 * webkit_dom_css_style_declaration_remove_property:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @propertyName: A #gchar
 * @error: #GError
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_css_style_declaration_remove_property(WebKitDOMCSSStyleDeclaration* self, const gchar* propertyName, GError** error);

/**
 * webkit_dom_css_style_declaration_get_property_priority:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @propertyName: A #gchar
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_css_style_declaration_get_property_priority(WebKitDOMCSSStyleDeclaration* self, const gchar* propertyName);

/**
 * webkit_dom_css_style_declaration_set_property:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @propertyName: A #gchar
 * @value: A #gchar
 * @priority: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_css_style_declaration_set_property(WebKitDOMCSSStyleDeclaration* self, const gchar* propertyName, const gchar* value, const gchar* priority, GError** error);

/**
 * webkit_dom_css_style_declaration_item:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @index: A #gulong
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_css_style_declaration_item(WebKitDOMCSSStyleDeclaration* self, gulong index);

/**
 * webkit_dom_css_style_declaration_get_property_shorthand:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @propertyName: A #gchar
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_css_style_declaration_get_property_shorthand(WebKitDOMCSSStyleDeclaration* self, const gchar* propertyName);

/**
 * webkit_dom_css_style_declaration_is_property_implicit:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @propertyName: A #gchar
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_css_style_declaration_is_property_implicit(WebKitDOMCSSStyleDeclaration* self, const gchar* propertyName);

/**
 * webkit_dom_css_style_declaration_get_css_text:
 * @self: A #WebKitDOMCSSStyleDeclaration
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_css_style_declaration_get_css_text(WebKitDOMCSSStyleDeclaration* self);

/**
 * webkit_dom_css_style_declaration_set_css_text:
 * @self: A #WebKitDOMCSSStyleDeclaration
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_css_style_declaration_set_css_text(WebKitDOMCSSStyleDeclaration* self, const gchar* value, GError** error);

/**
 * webkit_dom_css_style_declaration_get_length:
 * @self: A #WebKitDOMCSSStyleDeclaration
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_css_style_declaration_get_length(WebKitDOMCSSStyleDeclaration* self);

/**
 * webkit_dom_css_style_declaration_get_parent_rule:
 * @self: A #WebKitDOMCSSStyleDeclaration
 *
 * Returns: (transfer full): A #WebKitDOMCSSRule
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSRule*
webkit_dom_css_style_declaration_get_parent_rule(WebKitDOMCSSStyleDeclaration* self);

G_END_DECLS

#endif /* WebKitDOMCSSStyleDeclaration_h */
