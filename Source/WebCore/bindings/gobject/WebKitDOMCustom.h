/*
 *  Copyright (C) 2011 Igalia S.L.
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

#ifndef WebKitDOMCustom_h
#define WebKitDOMCustom_h

#include <glib.h>
#include <webkitdom/webkitdomdefines.h>

G_BEGIN_DECLS

WEBKIT_API gboolean webkit_dom_html_text_area_element_is_edited(WebKitDOMHTMLTextAreaElement*);
WEBKIT_API gboolean webkit_dom_html_input_element_is_edited(WebKitDOMHTMLInputElement*);

/* Compatibility */
WEBKIT_API WebKitDOMBlob* webkit_dom_blob_webkit_slice(WebKitDOMBlob* self, gint64 start, gint64 end, const gchar* content_type);
WEBKIT_API gchar* webkit_dom_html_element_get_class_name(WebKitDOMHTMLElement* element);
WEBKIT_API void webkit_dom_html_element_set_class_name(WebKitDOMHTMLElement* element, const gchar* value);
WEBKIT_API WebKitDOMDOMTokenList* webkit_dom_html_element_get_class_list(WebKitDOMHTMLElement* element);
WEBKIT_API void webkit_dom_html_form_element_dispatch_form_change(WebKitDOMHTMLFormElement* self);
WEBKIT_API void webkit_dom_html_form_element_dispatch_form_input(WebKitDOMHTMLFormElement* self);
WEBKIT_API gboolean webkit_dom_webkit_named_flow_get_overflow(WebKitDOMWebKitNamedFlow* flow);
WEBKIT_API gchar* webkit_dom_element_get_webkit_region_overflow(WebKitDOMElement* element);
WEBKIT_API WebKitDOMNodeList* webkit_dom_webkit_named_flow_get_content_nodes(WebKitDOMWebKitNamedFlow* namedFlow);
WEBKIT_API WebKitDOMNodeList* webkit_dom_webkit_named_flow_get_regions_by_content_node(WebKitDOMWebKitNamedFlow* namedFlow, WebKitDOMNode* contentNode);

G_END_DECLS

#endif
