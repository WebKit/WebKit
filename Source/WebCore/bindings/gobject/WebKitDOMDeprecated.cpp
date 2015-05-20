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

#include "WebKitDOMHTMLElement.h"

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
