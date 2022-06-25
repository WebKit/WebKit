/*
 * Copyright (C) 2013 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#if !defined(__WEBKITDOM_H_INSIDE__) && !defined(WEBKIT2_COMPILATION)
#error "Only <wpe/webkitdom.h> can be included directly."
#endif

#ifndef WebKitDOMElement_h
#define WebKitDOMElement_h

#include <glib-object.h>
#include <wpe/WebKitDOMNode.h>
#include <wpe/WebKitDOMDefines.h>

G_BEGIN_DECLS

#define WEBKIT_DOM_TYPE_ELEMENT            (webkit_dom_element_get_type())
#define WEBKIT_DOM_ELEMENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj), WEBKIT_DOM_TYPE_ELEMENT, WebKitDOMElement))
#define WEBKIT_DOM_ELEMENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),  WEBKIT_DOM_TYPE_ELEMENT, WebKitDOMElementClass)
#define WEBKIT_DOM_IS_ELEMENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj), WEBKIT_DOM_TYPE_ELEMENT))
#define WEBKIT_DOM_IS_ELEMENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),  WEBKIT_DOM_TYPE_ELEMENT))
#define WEBKIT_DOM_ELEMENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj),  WEBKIT_DOM_TYPE_ELEMENT, WebKitDOMElementClass))

typedef struct _WebKitDOMElement WebKitDOMElement;
typedef struct _WebKitDOMElementClass WebKitDOMElementClass;

struct _WebKitDOMElement {
    WebKitDOMNode parent_instance;
};

struct _WebKitDOMElementClass {
    WebKitDOMNodeClass parent_class;
};

WEBKIT_API GType
webkit_dom_element_get_type                             (void);

WEBKIT_API gboolean
webkit_dom_element_html_input_element_is_user_edited    (WebKitDOMElement *element);

WEBKIT_API gboolean
webkit_dom_element_html_input_element_get_auto_filled   (WebKitDOMElement *element);

WEBKIT_API void
webkit_dom_element_html_input_element_set_auto_filled   (WebKitDOMElement *element,
                                                         gboolean          auto_filled);
WEBKIT_API void
webkit_dom_element_html_input_element_set_editing_value (WebKitDOMElement *element,
                                                         const char       *value);

G_END_DECLS

#endif /* WebKitDOMElement_h */
