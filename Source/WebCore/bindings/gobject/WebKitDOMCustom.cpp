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

#include "JSMainThreadExecState.h"
#include "WebKitDOMDOMWindowPrivate.h"
#include "WebKitDOMHTMLInputElement.h"
#include "WebKitDOMHTMLInputElementPrivate.h"
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

