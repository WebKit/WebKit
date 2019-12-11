/*
 *  Copyright (C) 2017 Aidan Holm <aidanholm@gmail.com>
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
#include "WebKitDOMClientRect.h"

#include "ConvertToUTF8String.h"
#include "DOMObjectCache.h"
#include "WebKitDOMClientRectPrivate.h"
#include "WebKitDOMPrivate.h"
#include <WebCore/CSSImportRule.h>
#include <WebCore/Document.h>
#include <WebCore/ExceptionCode.h>
#include <WebCore/JSExecState.h>
#include <wtf/GetPtr.h>
#include <wtf/RefPtr.h>

#define WEBKIT_DOM_CLIENT_RECT_GET_PRIVATE(obj) G_TYPE_INSTANCE_GET_PRIVATE(obj, WEBKIT_DOM_TYPE_CLIENT_RECT, WebKitDOMClientRectPrivate)

typedef struct _WebKitDOMClientRectPrivate {
    RefPtr<WebCore::DOMRect> coreObject;
} WebKitDOMClientRectPrivate;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS;

namespace WebKit {

WebKitDOMClientRect* kit(WebCore::DOMRect* obj)
{
    if (!obj)
        return nullptr;

    if (gpointer ret = DOMObjectCache::get(obj))
        return WEBKIT_DOM_CLIENT_RECT(ret);

    return wrapClientRect(obj);
}

WebCore::DOMRect* core(WebKitDOMClientRect* request)
{
    return request ? static_cast<WebCore::DOMRect*>(WEBKIT_DOM_OBJECT(request)->coreObject) : nullptr;
}

WebKitDOMClientRect* wrapClientRect(WebCore::DOMRect* coreObject)
{
    ASSERT(coreObject);
    return WEBKIT_DOM_CLIENT_RECT(g_object_new(WEBKIT_DOM_TYPE_CLIENT_RECT, "core-object", coreObject, nullptr));
}

} // namespace WebKit

G_DEFINE_TYPE(WebKitDOMClientRect, webkit_dom_client_rect, WEBKIT_DOM_TYPE_OBJECT)

enum {
    DOM_CLIENT_RECT_PROP_0,
    DOM_CLIENT_RECT_PROP_TOP,
    DOM_CLIENT_RECT_PROP_RIGHT,
    DOM_CLIENT_RECT_PROP_BOTTOM,
    DOM_CLIENT_RECT_PROP_LEFT,
    DOM_CLIENT_RECT_PROP_WIDTH,
    DOM_CLIENT_RECT_PROP_HEIGHT,
};

static void webkit_dom_client_rect_finalize(GObject* object)
{
    WebKitDOMClientRectPrivate* priv = WEBKIT_DOM_CLIENT_RECT_GET_PRIVATE(object);

    WebKit::DOMObjectCache::forget(priv->coreObject.get());

    priv->~WebKitDOMClientRectPrivate();
    G_OBJECT_CLASS(webkit_dom_client_rect_parent_class)->finalize(object);
}

static void webkit_dom_client_rect_get_property(GObject* object, guint propertyId, GValue* value, GParamSpec* pspec)
{
    WebKitDOMClientRect* self = WEBKIT_DOM_CLIENT_RECT(object);

    switch (propertyId) {
    case DOM_CLIENT_RECT_PROP_TOP:
        g_value_set_float(value, webkit_dom_client_rect_get_top(self));
        break;
    case DOM_CLIENT_RECT_PROP_RIGHT:
        g_value_set_float(value, webkit_dom_client_rect_get_right(self));
        break;
    case DOM_CLIENT_RECT_PROP_BOTTOM:
        g_value_set_float(value, webkit_dom_client_rect_get_bottom(self));
        break;
    case DOM_CLIENT_RECT_PROP_LEFT:
        g_value_set_float(value, webkit_dom_client_rect_get_left(self));
        break;
    case DOM_CLIENT_RECT_PROP_WIDTH:
        g_value_set_float(value, webkit_dom_client_rect_get_width(self));
        break;
    case DOM_CLIENT_RECT_PROP_HEIGHT:
        g_value_set_float(value, webkit_dom_client_rect_get_height(self));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyId, pspec);
        break;
    }
}

static void webkit_dom_client_rect_constructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_dom_client_rect_parent_class)->constructed(object);

    WebKitDOMClientRectPrivate* priv = WEBKIT_DOM_CLIENT_RECT_GET_PRIVATE(object);
    priv->coreObject = static_cast<WebCore::DOMRect*>(WEBKIT_DOM_OBJECT(object)->coreObject);
    WebKit::DOMObjectCache::put(priv->coreObject.get(), object);
}

static void webkit_dom_client_rect_class_init(WebKitDOMClientRectClass* requestClass)
{
    GObjectClass* gobjectClass = G_OBJECT_CLASS(requestClass);
    g_type_class_add_private(gobjectClass, sizeof(WebKitDOMClientRectPrivate));
    gobjectClass->constructed = webkit_dom_client_rect_constructed;
    gobjectClass->finalize = webkit_dom_client_rect_finalize;
    gobjectClass->get_property = webkit_dom_client_rect_get_property;

    g_object_class_install_property(
        gobjectClass,
        DOM_CLIENT_RECT_PROP_TOP,
        g_param_spec_float(
            "top",
            "ClientRect:top",
            "read-only gfloat ClientRect:top",
            -G_MAXFLOAT, G_MAXFLOAT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CLIENT_RECT_PROP_RIGHT,
        g_param_spec_float(
            "right",
            "ClientRect:right",
            "read-only gfloat ClientRect:right",
            -G_MAXFLOAT, G_MAXFLOAT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CLIENT_RECT_PROP_BOTTOM,
        g_param_spec_float(
            "bottom",
            "ClientRect:bottom",
            "read-only gfloat ClientRect:bottom",
            -G_MAXFLOAT, G_MAXFLOAT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CLIENT_RECT_PROP_LEFT,
        g_param_spec_float(
            "left",
            "ClientRect:left",
            "read-only gfloat ClientRect:left",
            -G_MAXFLOAT, G_MAXFLOAT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CLIENT_RECT_PROP_WIDTH,
        g_param_spec_float(
            "width",
            "ClientRect:width",
            "read-only gfloat ClientRect:width",
            0, G_MAXFLOAT, 0,
            WEBKIT_PARAM_READABLE));

    g_object_class_install_property(
        gobjectClass,
        DOM_CLIENT_RECT_PROP_HEIGHT,
        g_param_spec_float(
            "height",
            "ClientRect:height",
            "read-only gfloat ClientRect:height",
            0, G_MAXFLOAT, 0,
            WEBKIT_PARAM_READABLE));

}

static void webkit_dom_client_rect_init(WebKitDOMClientRect* request)
{
    WebKitDOMClientRectPrivate* priv = WEBKIT_DOM_CLIENT_RECT_GET_PRIVATE(request);
    new (priv) WebKitDOMClientRectPrivate();
}

gfloat webkit_dom_client_rect_get_top(WebKitDOMClientRect* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CLIENT_RECT(self), 0);
    return WebKit::core(self)->top();
}

gfloat webkit_dom_client_rect_get_right(WebKitDOMClientRect* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CLIENT_RECT(self), 0);
    return WebKit::core(self)->right();
}

gfloat webkit_dom_client_rect_get_bottom(WebKitDOMClientRect* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CLIENT_RECT(self), 0);
    return WebKit::core(self)->bottom();
}

gfloat webkit_dom_client_rect_get_left(WebKitDOMClientRect* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CLIENT_RECT(self), 0);
    return WebKit::core(self)->left();
}

gfloat webkit_dom_client_rect_get_width(WebKitDOMClientRect* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CLIENT_RECT(self), 0);
    return WebKit::core(self)->width();
}

gfloat webkit_dom_client_rect_get_height(WebKitDOMClientRect* self)
{
    WebCore::JSMainThreadNullState state;
    g_return_val_if_fail(WEBKIT_DOM_IS_CLIENT_RECT(self), 0);
    return WebKit::core(self)->height();
}
G_GNUC_END_IGNORE_DEPRECATIONS;
