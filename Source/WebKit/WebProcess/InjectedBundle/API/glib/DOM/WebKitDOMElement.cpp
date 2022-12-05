/*
 *  Copyright (C) 2018 Igalia S.L.
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

#include "config.h"
#include "WebKitDOMElement.h"

#include "DOMObjectCache.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitDOMPrivate.h"
#include <WebCore/HTMLInputElement.h>
#include <WebCore/HTMLTextAreaElement.h>

#if PLATFORM(GTK)
#include "WebKitDOMEventTarget.h"
#endif

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMElement* kit(WebCore::Element* obj)
{
    return WEBKIT_DOM_ELEMENT(kit(static_cast<WebCore::Node*>(obj)));
}

WebCore::Element* core(WebKitDOMElement* element)
{
    return element ? static_cast<WebCore::Element*>(webkitDOMNodeGetCoreObject(WEBKIT_DOM_NODE(element))) : nullptr;
}

WebKitDOMElement* wrapElement(WebCore::Element* coreObject)
{
    ASSERT(coreObject);
#if PLATFORM(GTK)
    return WEBKIT_DOM_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_ELEMENT, "core-object", coreObject, nullptr));
#else
    auto* element = WEBKIT_DOM_ELEMENT(g_object_new(WEBKIT_DOM_TYPE_ELEMENT, nullptr));
    webkitDOMNodeSetCoreObject(WEBKIT_DOM_NODE(element), static_cast<WebCore::Node*>(coreObject));
    return element;
#endif
}

} // namespace WebKit

#if PLATFORM(GTK)
G_DEFINE_TYPE_WITH_CODE(WebKitDOMElement, webkit_dom_element, WEBKIT_DOM_TYPE_NODE, G_IMPLEMENT_INTERFACE(WEBKIT_DOM_TYPE_EVENT_TARGET, webkitDOMElementDOMEventTargetInit))
#else
G_DEFINE_TYPE(WebKitDOMElement, webkit_dom_element, WEBKIT_DOM_TYPE_NODE)
#endif

static void webkit_dom_element_class_init(WebKitDOMElementClass* elementClass)
{
#if PLATFORM(GTK)
    GObjectClass* gobjectClass = G_OBJECT_CLASS(elementClass);
    webkitDOMElementInstallProperties(gobjectClass);
#endif
}

static void webkit_dom_element_init(WebKitDOMElement*)
{
}

/**
 * webkit_dom_element_html_input_element_is_user_edited:
 * @element: a #WebKitDOMElement
 *
 * Get whether @element is an HTML text input element that has been edited by a user action.
 *
 * Returns: whether @element has been edited by a user action.
 *
 * Since: 2.22
 *
 * Deprecated: 2.40: Use webkit_web_form_manager_input_element_is_user_edited() instead.
 */
gboolean webkit_dom_element_html_input_element_is_user_edited(WebKitDOMElement* element)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(element), FALSE);

    auto* node = webkitDOMNodeGetCoreObject(WEBKIT_DOM_NODE(element));
    if (is<WebCore::HTMLInputElement>(node))
        return downcast<WebCore::HTMLInputElement>(*node).lastChangeWasUserEdit();

    if (is<WebCore::HTMLTextAreaElement>(node))
        return downcast<WebCore::HTMLTextAreaElement>(*node).lastChangeWasUserEdit();

    return FALSE;
}

/**
 * webkit_dom_element_html_input_element_get_auto_filled:
 * @element: a #WebKitDOMElement
 *
 * Get whether the element is an HTML input element that has been filled automatically.
 *
 * Returns: whether @element has been filled automatically.
 *
 * Since: 2.22
 *
 * Deprecated: 2.40: Use webkit_web_form_manager_input_element_is_auto_filled() instead.
 */
gboolean webkit_dom_element_html_input_element_get_auto_filled(WebKitDOMElement* element)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_ELEMENT(element), FALSE);

    auto* node = webkitDOMNodeGetCoreObject(WEBKIT_DOM_NODE(element));
    if (!is<WebCore::HTMLInputElement>(node))
        return false;

    return downcast<WebCore::HTMLInputElement>(*node).isAutoFilled();
}

/**
 * webkit_dom_element_html_input_element_set_auto_filled:
 * @element: a #WebKitDOMElement
 * @auto_filled: value to set
 *
 * Set whether the element is an HTML input element that has been filled automatically.
 * If @element is not an HTML input element this function does nothing.
 *
 * Since: 2.22
 *
 * Deprecated: 2.40: Use webkit_web_form_manager_input_element_auto_fill() instead.
 */
void webkit_dom_element_html_input_element_set_auto_filled(WebKitDOMElement* element, gboolean autoFilled)
{
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(element));

    auto* node = webkitDOMNodeGetCoreObject(WEBKIT_DOM_NODE(element));
    if (!is<WebCore::HTMLInputElement>(node))
        return;

    downcast<WebCore::HTMLInputElement>(*node).setAutoFilled(autoFilled);
}

/**
 * webkit_dom_element_html_input_element_set_editing_value:
 * @element: a #WebKitDOMElement
 * @value: the text to set
 *
 * Set the value of an HTML input element as if it had been edited by
 * the user, triggering a change event. If @element is not an HTML input
 * element this function does nothing.
 *
 * Since: 2.22
 *
 * Deprecated: 2.40: Use webkit_web_form_manager_input_element_auto_fill() instead.
 */
void webkit_dom_element_html_input_element_set_editing_value(WebKitDOMElement* element, const char* value)
{
    g_return_if_fail(WEBKIT_DOM_IS_ELEMENT(element));

    auto* node = webkitDOMNodeGetCoreObject(WEBKIT_DOM_NODE(element));
    if (!is<WebCore::HTMLInputElement>(node))
        return;

    downcast<WebCore::HTMLInputElement>(*node).setValueForUser(String::fromUTF8(value));
}

G_GNUC_END_IGNORE_DEPRECATIONS;
