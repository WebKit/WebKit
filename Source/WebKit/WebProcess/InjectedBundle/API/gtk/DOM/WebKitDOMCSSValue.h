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

#ifndef WebKitDOMCSSValue_h
#define WebKitDOMCSSValue_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMObject.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_CSS_VALUE            (webkit_dom_css_value_get_type())
#define WEBKIT_DOM_CSS_VALUE(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_CSS_VALUE, WebKitDOMCSSValue))
#define WEBKIT_DOM_CSS_VALUE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_CSS_VALUE, WebKitDOMCSSValueClass)
#define WEBKIT_DOM_IS_CSS_VALUE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_CSS_VALUE))
#define WEBKIT_DOM_IS_CSS_VALUE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_CSS_VALUE))
#define WEBKIT_DOM_CSS_VALUE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_CSS_VALUE, WebKitDOMCSSValueClass))

#ifndef WEBKIT_DISABLE_DEPRECATED

/**
 * WEBKIT_DOM_CSS_VALUE_CSS_INHERIT:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_VALUE_CSS_INHERIT 0

/**
 * WEBKIT_DOM_CSS_VALUE_CSS_PRIMITIVE_VALUE:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_VALUE_CSS_PRIMITIVE_VALUE 1

/**
 * WEBKIT_DOM_CSS_VALUE_CSS_VALUE_LIST:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_VALUE_CSS_VALUE_LIST 2

/**
 * WEBKIT_DOM_CSS_VALUE_CSS_CUSTOM:
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
 */
#define WEBKIT_DOM_CSS_VALUE_CSS_CUSTOM 3

#endif /* WEBKIT_DISABLE_DEPRECATED */

struct _WebKitDOMCSSValue {
    WebKitDOMObject parent_instance;
};

struct _WebKitDOMCSSValueClass {
    WebKitDOMObjectClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_css_value_get_type(void);

/**
 * webkit_dom_css_value_get_css_text:
 * @self: A #WebKitDOMCSSValue
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_css_value_get_css_text(WebKitDOMCSSValue* self);

/**
 * webkit_dom_css_value_set_css_text:
 * @self: A #WebKitDOMCSSValue
 * @value: A #gchar
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_css_value_set_css_text(WebKitDOMCSSValue* self, const gchar* value, GError** error);

/**
 * webkit_dom_css_value_get_css_value_type:
 * @self: A #WebKitDOMCSSValue
 *
 * Returns: A #gushort
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gushort
webkit_dom_css_value_get_css_value_type(WebKitDOMCSSValue* self);

G_END_DECLS

#endif /* WebKitDOMCSSValue_h */
