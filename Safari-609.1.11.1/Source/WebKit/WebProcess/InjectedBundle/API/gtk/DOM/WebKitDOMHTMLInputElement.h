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

#ifndef WebKitDOMHTMLInputElement_h
#define WebKitDOMHTMLInputElement_h

#include <glib-object.h>
#include <webkitdom/WebKitDOMHTMLElement.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_HTML_INPUT_ELEMENT            (webkit_dom_html_input_element_get_type())
#define WEBKIT_DOM_HTML_INPUT_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_HTML_INPUT_ELEMENT, WebKitDOMHTMLInputElement))
#define WEBKIT_DOM_HTML_INPUT_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_HTML_INPUT_ELEMENT, WebKitDOMHTMLInputElementClass)
#define WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_HTML_INPUT_ELEMENT))
#define WEBKIT_DOM_IS_HTML_INPUT_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_HTML_INPUT_ELEMENT))
#define WEBKIT_DOM_HTML_INPUT_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_HTML_INPUT_ELEMENT, WebKitDOMHTMLInputElementClass))

struct _WebKitDOMHTMLInputElement {
    WebKitDOMHTMLElement parent_instance;
};

struct _WebKitDOMHTMLInputElementClass {
    WebKitDOMHTMLElementClass parent_class;
};

WEBKIT_DEPRECATED GType
webkit_dom_html_input_element_get_type(void);

/**
 * webkit_dom_html_input_element_select:
 * @self: A #WebKitDOMHTMLInputElement
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_select(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_get_accept:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_accept(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_accept:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_accept(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_alt:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_alt(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_alt:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_alt(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_autofocus:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_input_element_get_autofocus(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_autofocus:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_autofocus(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_input_element_get_default_checked:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_input_element_get_default_checked(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_default_checked:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_default_checked(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_input_element_get_checked:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_input_element_get_checked(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_checked:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_checked(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_input_element_get_disabled:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_input_element_get_disabled(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_disabled:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_disabled(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_input_element_get_form:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: (transfer none): A #WebKitDOMHTMLFormElement
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMHTMLFormElement*
webkit_dom_html_input_element_get_form(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_get_files:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: (transfer full): A #WebKitDOMFileList
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED WebKitDOMFileList*
webkit_dom_html_input_element_get_files(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_files:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #WebKitDOMFileList
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_files(WebKitDOMHTMLInputElement* self, WebKitDOMFileList* value);

/**
 * webkit_dom_html_input_element_get_height:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_html_input_element_get_height(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_height:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gulong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_height(WebKitDOMHTMLInputElement* self, gulong value);

/**
 * webkit_dom_html_input_element_get_indeterminate:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_input_element_get_indeterminate(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_indeterminate:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_indeterminate(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_input_element_get_max_length:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #glong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED glong
webkit_dom_html_input_element_get_max_length(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_max_length:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #glong
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_max_length(WebKitDOMHTMLInputElement* self, glong value, GError** error);

/**
 * webkit_dom_html_input_element_get_multiple:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_input_element_get_multiple(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_multiple:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_multiple(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_input_element_get_name:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_name(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_name:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_name(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_read_only:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_input_element_get_read_only(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_read_only:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_read_only(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_input_element_get_size:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_html_input_element_get_size(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_size:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gulong
 * @error: #GError
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_size(WebKitDOMHTMLInputElement* self, gulong value, GError** error);

/**
 * webkit_dom_html_input_element_get_src:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_src(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_src:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_src(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_input_type:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_input_type(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_input_type:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_input_type(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_default_value:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_default_value(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_default_value:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_default_value(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_value:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_value(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_value:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_value(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_width:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gulong
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gulong
webkit_dom_html_input_element_get_width(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_width:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gulong
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_width(WebKitDOMHTMLInputElement* self, gulong value);

/**
 * webkit_dom_html_input_element_get_will_validate:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gboolean
webkit_dom_html_input_element_get_will_validate(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_get_align:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_align(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_align:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_align(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_use_map:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_use_map(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_use_map:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_use_map(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_get_capture_type:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gchar
 *
 * Since: 2.14
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED gchar*
webkit_dom_html_input_element_get_capture_type(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_capture_type:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use JavaScriptCore API instead
**/
WEBKIT_DEPRECATED void
webkit_dom_html_input_element_set_capture_type(WebKitDOMHTMLInputElement* self, const gchar* value);

/**
 * webkit_dom_html_input_element_is_edited:
 * @input: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Deprecated: 2.22: Use webkit_dom_element_html_input_element_is_user_edited() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_html_input_element_is_user_edited) gboolean
webkit_dom_html_input_element_is_edited(WebKitDOMHTMLInputElement* input);

/**
 * webkit_dom_html_input_element_get_auto_filled:
 * @self: A #WebKitDOMHTMLInputElement
 *
 * Returns: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use webkit_dom_element_html_input_element_get_auto_filled() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_html_input_element_get_auto_filled) gboolean
webkit_dom_html_input_element_get_auto_filled(WebKitDOMHTMLInputElement* self);

/**
 * webkit_dom_html_input_element_set_auto_filled:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gboolean
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use webkit_dom_element_html_input_element_set_auto_filled() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_html_input_element_set_auto_filled) void
webkit_dom_html_input_element_set_auto_filled(WebKitDOMHTMLInputElement* self, gboolean value);

/**
 * webkit_dom_html_input_element_set_editing_value:
 * @self: A #WebKitDOMHTMLInputElement
 * @value: A #gchar
 *
 * Since: 2.16
 *
 * Deprecated: 2.22: Use webkit_dom_element_html_input_element_set_editing_value() instead.
 */
WEBKIT_DEPRECATED_FOR(webkit_dom_element_html_input_element_set_editing_value) void
webkit_dom_html_input_element_set_editing_value(WebKitDOMHTMLInputElement* self, const gchar* value);

G_END_DECLS

#endif /* WebKitDOMHTMLInputElement_h */
