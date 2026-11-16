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

#ifndef WebKitDOMCSSRule_h
#define WebKitDOMCSSRule_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_CSS_RULE            (webkit_dom_css_rule_get_type())
#define WEBKIT_DOM_CSS_RULE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_CSS_RULE, WebKitDOMCSSRule))
#define WEBKIT_DOM_CSS_RULE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_CSS_RULE, WebKitDOMCSSRuleClass)
#define WEBKIT_DOM_IS_CSS_RULE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_CSS_RULE))
#define WEBKIT_DOM_IS_CSS_RULE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_CSS_RULE))
#define WEBKIT_DOM_CSS_RULE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_CSS_RULE, WebKitDOMCSSRuleClass))

#ifndef WEBKIT_DISABLE_DEPRECATED

/**
 * WEBKIT_DOM_CSS_RULE_UNKNOWN_RULE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_RULE_UNKNOWN_RULE 0

/**
 * WEBKIT_DOM_CSS_RULE_STYLE_RULE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_RULE_STYLE_RULE 1

/**
 * WEBKIT_DOM_CSS_RULE_CHARSET_RULE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_RULE_CHARSET_RULE 2

/**
 * WEBKIT_DOM_CSS_RULE_IMPORT_RULE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_RULE_IMPORT_RULE 3

/**
 * WEBKIT_DOM_CSS_RULE_MEDIA_RULE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_RULE_MEDIA_RULE 4

/**
 * WEBKIT_DOM_CSS_RULE_FONT_FACE_RULE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_RULE_FONT_FACE_RULE 5

/**
 * WEBKIT_DOM_CSS_RULE_PAGE_RULE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_RULE_PAGE_RULE 6

#endif /* WEBKIT_DISABLE_DEPRECATED */

struct _WebKitDOMCSSRule {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMCSSRuleClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_css_rule_get_type(void);

/**
 * webkit_dom_css_rule_get_rule_type:
 * @self: A #WebKitDOMCSSRule
 *
 * Returns: A #gushort
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gushort
webkit_dom_css_rule_get_rule_type(WebKitDOMCSSRule* self);

/**
 * webkit_dom_css_rule_get_css_text:
 * @self: A #WebKitDOMCSSRule
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_css_rule_get_css_text(WebKitDOMCSSRule* self);

/**
 * webkit_dom_css_rule_set_css_text:
 * @self: A #WebKitDOMCSSRule
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_css_rule_set_css_text(WebKitDOMCSSRule* self, const gchar* value, GError** error);

/**
 * webkit_dom_css_rule_get_parent_style_sheet:
 * @self: A #WebKitDOMCSSRule
 *
 * Returns: (transfer full): A #WebKitDOMCSSStyleSheet
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSStyleSheet*
webkit_dom_css_rule_get_parent_style_sheet(WebKitDOMCSSRule* self);

/**
 * webkit_dom_css_rule_get_parent_rule:
 * @self: A #WebKitDOMCSSRule
 *
 * Returns: (transfer full): A #WebKitDOMCSSRule
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMCSSRule*
webkit_dom_css_rule_get_parent_rule(WebKitDOMCSSRule* self);

G_END_DECLS

#endif /* WebKitDOMCSSRule_h */
