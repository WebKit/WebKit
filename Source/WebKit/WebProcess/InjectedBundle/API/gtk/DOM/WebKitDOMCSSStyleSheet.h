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

#ifndef WebKitDOMCSSStyleSheet_h
#define WebKitDOMCSSStyleSheet_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMStyleSheet.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_CSS_STYLE_SHEET            (webkit_dom_css_style_sheet_get_type())
#define WEBKIT_DOM_CSS_STYLE_SHEET(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_CSS_STYLE_SHEET, WebKitDOMCSSStyleSheet))
#define WEBKIT_DOM_CSS_STYLE_SHEET_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_CSS_STYLE_SHEET, WebKitDOMCSSStyleSheetClass)
#define WEBKIT_DOM_IS_CSS_STYLE_SHEET(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_CSS_STYLE_SHEET))
#define WEBKIT_DOM_IS_CSS_STYLE_SHEET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_CSS_STYLE_SHEET))
#define WEBKIT_DOM_CSS_STYLE_SHEET_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_CSS_STYLE_SHEET, WebKitDOMCSSStyleSheetClass))

struct _WebKitDOMCSSStyleSheet {
    WebKitDOMStyleSheet parent_instance;
};

struct _WebKitDOMCSSStyleSheetClass {
    WebKitDOMStyleSheetClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_css_style_sheet_get_type(void);

/**
 * webkit_dom_css_style_sheet_insert_rule:
 * @self: A #WebKitDOMCSSStyleSheet
 * @rule: A #gchar
 * @index: A #gulong
 * @error: #GError
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_css_style_sheet_insert_rule(WebKitDOMCSSStyleSheet* self, const gchar* rule, gulong index, GError** error);

/**
 * webkit_dom_css_style_sheet_delete_rule:
 * @self: A #WebKitDOMCSSStyleSheet
 * @index: A #gulong
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_css_style_sheet_delete_rule(WebKitDOMCSSStyleSheet* self, gulong index, GError** error);

/**
 * webkit_dom_css_style_sheet_add_rule:
 * @self: A #WebKitDOMCSSStyleSheet
 * @selector: A #gchar
 * @style: A #gchar
 * @index: A #gulong
 * @error: #GError
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_css_style_sheet_add_rule(WebKitDOMCSSStyleSheet* self, const gchar* selector, const gchar* style, gulong index, GError** error);

/**
 * webkit_dom_css_style_sheet_remove_rule:
 * @self: A #WebKitDOMCSSStyleSheet
 * @index: A #gulong
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_css_style_sheet_remove_rule(WebKitDOMCSSStyleSheet* self, gulong index, GError** error);

/**
 * webkit_dom_css_style_sheet_get_owner_rule:
 * @self: A #WebKitDOMCSSStyleSheet
 *
 * Returns: (transfer full): A #WebKitDOMCSSRule
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSRule*
webkit_dom_css_style_sheet_get_owner_rule(WebKitDOMCSSStyleSheet* self);

/**
 * webkit_dom_css_style_sheet_get_css_rules:
 * @self: A #WebKitDOMCSSStyleSheet
 *
 * Returns: (transfer full): A #WebKitDOMCSSRuleList
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSRuleList*
webkit_dom_css_style_sheet_get_css_rules(WebKitDOMCSSStyleSheet* self);

/**
 * webkit_dom_css_style_sheet_get_rules:
 * @self: A #WebKitDOMCSSStyleSheet
 *
 * Returns: (transfer full): A #WebKitDOMCSSRuleList
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSRuleList*
webkit_dom_css_style_sheet_get_rules(WebKitDOMCSSStyleSheet* self);

G_END_DECLS

#endif /* WebKitDOMCSSStyleSheet_h */
