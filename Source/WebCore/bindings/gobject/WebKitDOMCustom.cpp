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

#include "config.h"
#include "WebKitDOMCustom.h"

#include "DOMTokenList.h"
#include "JSMainThreadExecState.h"
#include "SerializedScriptValue.h"
#include "WebKitDOMDOMWindowPrivate.h"
#include "WebKitDOMHTMLInputElement.h"
#include "WebKitDOMHTMLInputElementPrivate.h"
#include "WebKitDOMHTMLLinkElementPrivate.h"
#include "WebKitDOMHTMLTextAreaElement.h"
#include "WebKitDOMHTMLTextAreaElementPrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMUserMessageHandlerPrivate.h"
#include "WebKitDOMUserMessageHandlersNamespacePrivate.h"
#include "WebKitDOMWebKitNamespacePrivate.h"

using namespace WebKit;

gboolean webkit_dom_html_text_area_element_is_edited(WebKitDOMHTMLTextAreaElement* area)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_TEXT_AREA_ELEMENT(area), FALSE);

    return core(area)->lastChangeWasUserEdit();
}

gboolean webkit_dom_html_input_element_is_edited(WebKitDOMHTMLInputElement* input)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(input), FALSE);

    return core(input)->lastChangeWasUserEdit();
}

WebKitDOMWebKitNamespace* webkit_dom_dom_window_get_webkit_namespace(WebKitDOMDOMWindow* window)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(window), nullptr);

    WebCore::DOMWindow* domWindow = core(window);
    if (!domWindow->shouldHaveWebKitNamespaceForWorld(WebCore::mainThreadNormalWorld()))
        return nullptr;
    return kit(domWindow->webkitNamespace());
}

WebKitDOMUserMessageHandler* webkit_dom_user_message_handlers_namespace_get_handler(WebKitDOMUserMessageHandlersNamespace* handlersNamespace, const gchar* name)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_USER_MESSAGE_HANDLERS_NAMESPACE(handlersNamespace), nullptr);
    g_return_val_if_fail(name, nullptr);

    return kit(core(handlersNamespace)->handler(String::fromUTF8(name), WebCore::mainThreadNormalWorld()));
}

gboolean webkit_dom_dom_window_webkit_message_handlers_post_message(WebKitDOMDOMWindow* window, const gchar* handlerName, const gchar* message)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(window), FALSE);
    g_return_val_if_fail(handlerName, FALSE);
    g_return_val_if_fail(message, FALSE);

    WebCore::DOMWindow* domWindow = core(window);
    if (!domWindow->shouldHaveWebKitNamespaceForWorld(WebCore::mainThreadNormalWorld()))
        return FALSE;

    auto webkitNamespace = domWindow->webkitNamespace();
    if (!webkitNamespace)
        return FALSE;

    auto handler = webkitNamespace->messageHandlers()->handler(String::fromUTF8(handlerName), WebCore::mainThreadNormalWorld());
    if (!handler)
        return FALSE;

    WebCore::JSMainThreadNullState state;
    WebCore::ExceptionCode ec = 0;
    handler->postMessage(WebCore::SerializedScriptValue::create(String::fromUTF8(message)), ec);
    if (ec)
        return FALSE;

    return TRUE;
}

void webkit_dom_html_link_element_set_sizes(WebKitDOMHTMLLinkElement* linkElement, const gchar* value)
{
    g_return_if_fail(WEBKIT_DOM_IS_HTML_LINK_ELEMENT(linkElement));
    g_return_if_fail(value);

    core(linkElement)->sizes().setValue(String::fromUTF8(value));
}

gboolean webkit_dom_html_input_element_get_auto_filled(WebKitDOMHTMLInputElement* self)
{
    g_return_val_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self), FALSE);

    return WebKit::core(self)->isAutoFilled();
}

void webkit_dom_html_input_element_set_auto_filled(WebKitDOMHTMLInputElement* self, gboolean value)
{
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));

    WebKit::core(self)->setAutoFilled(value);
}

void webkit_dom_html_input_element_set_editing_value(WebKitDOMHTMLInputElement* self, const gchar* value)
{
    g_return_if_fail(WEBKIT_DOM_IS_HTML_INPUT_ELEMENT(self));
    g_return_if_fail(value);

    WebKit::core(self)->setEditingValue(WTF::String::fromUTF8(value));
}
