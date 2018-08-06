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

#include "config.h"
#include "WebKitDOMUIEvent.h"

#include <WebCore/CSSImportRule.h>
#include "DOMObjectCache.h"
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include <WebCore/KeyboardEvent.h>
#include "WebKitDOMDOMWindowPrivate.h"
#include "WebKitDOMEventPrivate.h"
#include "WebKitDOMPrivate.h"
#include "WebKitDOMUIEventPrivate.h"
#include "ConvertToUTF8String.h"
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMUIEvent* kit(WebCore::UIEvent* obj)
{
    return WEBKIT_DOM_UI_EVENT(kit(static_cast<WebCore::Event*>(obj)));
}

WebCore::UIEvent* core(WebKitDOMUIEvent* request)
{
    return request ? static_cast<WebCore::UIEvent*>(WEBKIT_DOM_OBJECT(request)->coreObject) : 0;
}

WebKitDOMUIEvent* wrapUIEvent(WebCore::UIEvent* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_UI_EVENT(g_object_new(WEBKIT_DOM_TYPE_UI_EVENT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMUIEvent, webkit_dom_ui_event, WEBKIT_DOM_TYPE_EVENT)

enum {
    DOM_UI_EVENT_PROP_0,
    DOM_UI_EVENT_PROP_VIEW,
    DOM_UI_EVENT_PROP_DETAIL,
    DOM_UI_EVENT_PROP_KEY_CODE,
    DOM_UI_EVENT_PROP_CHAR_CODE,
    DOM_UI_EVENT_PROP_LAYER_X,
    DOM_UI_EVENT_PROP_LAYER_Y,
    DOM_UI_EVENT_PROP_PAGE_X,
    DOM_UI_EVENT_PROP_PAGE_Y,
};

static void webkit_dom_ui_event_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMUIEvent* self = WEBKIT_DOM_UI_EVENT(object);

    switch (propertyId) {
    case DOM_UI_EVENT_PROP_VIEW:
        g_value_set_object(value, webkit_dom_ui_event_get_view(self));
        break;
    case DOM_UI_EVENT_PROP_DETAIL:
        g_value_set_long(value, webkit_dom_ui_event_get_detail(self));
        break;
    case DOM_UI_EVENT_PROP_KEY_CODE:
        g_value_set_long(value, webkit_dom_ui_event_get_key_code(self));
        break;
    case DOM_UI_EVENT_PROP_CHAR_CODE:
        g_value_set_long(value, webkit_dom_ui_event_get_char_code(self));
        break;
    case DOM_UI_EVENT_PROP_LAYER_X:
        g_value_set_long(value, webkit_dom_ui_event_get_layer_x(self));
        break;
    case DOM_UI_EVENT_PROP_LAYER_Y:
        g_value_set_long(value, webkit_dom_ui_event_get_layer_y(self));
        break;
    case DOM_UI_EVENT_PROP_PAGE_X:
        g_value_set_long(value, webkit_dom_ui_event_get_page_x(self));
        break;
    case DOM_UI_EVENT_PROP_PAGE_Y:
        g_value_set_long(value, webkit_dom_ui_event_get_page_y(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_ui_event_class_init(WebKitDOMUIEventClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    gobjectClass->get_property = webkit_dom_ui_event_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_UI_EVENT_PROP_VIEW,
        g_param_spec_object(
            "view",
            "UIEvent:view",
            "read-only WebKitDOMDOMWindow* UIEvent:view",
            WEBKIT_DOM_TYPE_DOM_WINDOW,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_UI_EVENT_PROP_DETAIL,
        g_param_spec_long(
            "detail",
            "UIEvent:detail",
            "read-only glong UIEvent:detail",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_UI_EVENT_PROP_KEY_CODE,
        g_param_spec_long(
            "key-code",
            "UIEvent:key-code",
            "read-only glong UIEvent:key-code",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_UI_EVENT_PROP_CHAR_CODE,
        g_param_spec_long(
            "char-code",
            "UIEvent:char-code",
            "read-only glong UIEvent:char-code",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_UI_EVENT_PROP_LAYER_X,
        g_param_spec_long(
            "layer-x",
            "UIEvent:layer-x",
            "read-only glong UIEvent:layer-x",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_UI_EVENT_PROP_LAYER_Y,
        g_param_spec_long(
            "layer-y",
            "UIEvent:layer-y",
            "read-only glong UIEvent:layer-y",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_UI_EVENT_PROP_PAGE_X,
        g_param_spec_long(
            "page-x",
            "UIEvent:page-x",
            "read-only glong UIEvent:page-x",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_UI_EVENT_PROP_PAGE_Y,
        g_param_spec_long(
            "page-y",
            "UIEvent:page-y",
            "read-only glong UIEvent:page-y",
            G_MINLONG, G_MAXLONG, 0,
            WEBKIT_PARAM_READABLE));
}

static void webkit_dom_ui_event_init(WebKitDOMUIEvent* request)
{
    UNUSED_PARAM(request);
}

void webkit_dom_ui_event_init_ui_event(WebKitDOMUIEvent* self, const gchar* type, gboolean canBubble, gboolean cancelable, WebKitDOMDOMWindow* view, glong detail)
{
    WebCore::JSMainThreadNullState state;
    g_return_if_fail(WEBKIT_DOM_IS_UI_EVENT(self));
    g_return_if_fail(type);
    g_return_if_fail(WEBKIT_DOM_IS_DOM_WINDOW(view));
    WebCore::UIEvent* item = WebKit::core(self);
    WTF::String convertedType = WTF::String::fromUTF8(type);
    item->initUIEvent(convertedType, canBubble, cancelable, WebKit::toWindowProxy(view), detail);
}

WebKitDOMDOMWindow* webkit_dom_ui_event_get_view(WebKitDOMUIEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_UI_EVENT(self), 0);
    WebCore::UIEvent* item = WebKit::core(self);
    return WebKit::kit(item->view());
}

glong webkit_dom_ui_event_get_detail(WebKitDOMUIEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_UI_EVENT(self), 0);
    WebCore::UIEvent* item = WebKit::core(self);
    glong result = item->detail();
    return result;
}

glong webkit_dom_ui_event_get_key_code(WebKitDOMUIEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_UI_EVENT(self), 0);
    WebCore::UIEvent* item = WebKit::core(self);
    glong result = is<WebCore::KeyboardEvent>(*item) ? downcast<WebCore::KeyboardEvent>(*item).keyCode() : 0;
    return result;
}

glong webkit_dom_ui_event_get_char_code(WebKitDOMUIEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_UI_EVENT(self), 0);
    WebCore::UIEvent* item = WebKit::core(self);
    glong result = is<WebCore::KeyboardEvent>(*item) ? downcast<WebCore::KeyboardEvent>(*item).charCode() : 0;
    return result;
}

glong webkit_dom_ui_event_get_layer_x(WebKitDOMUIEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_UI_EVENT(self), 0);
    WebCore::UIEvent* item = WebKit::core(self);
    glong result = item->layerX();
    return result;
}

glong webkit_dom_ui_event_get_layer_y(WebKitDOMUIEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_UI_EVENT(self), 0);
    WebCore::UIEvent* item = WebKit::core(self);
    glong result = item->layerY();
    return result;
}

glong webkit_dom_ui_event_get_page_x(WebKitDOMUIEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_UI_EVENT(self), 0);
    WebCore::UIEvent* item = WebKit::core(self);
    glong result = item->pageX();
    return result;
}

glong webkit_dom_ui_event_get_page_y(WebKitDOMUIEvent* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_UI_EVENT(self), 0);
    WebCore::UIEvent* item = WebKit::core(self);
    glong result = item->pageY();
    return result;
}
G_GNUC_END_IGNORE_DEPRECATIONS;
