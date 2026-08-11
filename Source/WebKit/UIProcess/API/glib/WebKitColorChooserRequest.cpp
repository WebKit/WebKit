/*
 * Copyright (C) 2015 Igalia S.L.
 * Copyright (c) 2012, Samsung Electronics
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 *   list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebKitColorChooserRequest.h"

#include "WebKitColorChooserRequestPrivate.h"
#include <wtf/glib/WTFGType.h>

#if PLATFORM(GTK)
#include "GtkUtilities.h"
#elif PLATFORM(WPE)
#include "WebKitColorPrivate.h"
#endif

enum {
    PROP_0,
    PROP_COLOR,
    N_PROPERTIES,
};

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

enum {
    FINISHED,

    LAST_SIGNAL
};

struct _WebKitColorChooserRequestPrivate {
    WeakPtr<WebKit::WebColorPicker> colorPicker;
    WebCore::Color initialColor;
    WebCore::IntRect elementRect;
#if PLATFORM(GTK)
    GdkRGBA rgba;
#elif PLATFORM(WPE)
    WebKitColor color;
#endif
    bool handled;
};

static std::array<unsigned, LAST_SIGNAL> signals;

WEBKIT_DEFINE_FINAL_TYPE(WebKitColorChooserRequest, webkit_color_chooser_request, G_TYPE_OBJECT, GObject)

static void webkitColorChooserRequestSelectedColorChanged(WebKitColorChooserRequest* request, const WebCore::Color& color)
{
    if (RefPtr colorPicker = request->priv->colorPicker.get())
        colorPicker->setSelectedColor(color);

    g_object_notify_by_pspec(G_OBJECT(request), sObjProperties[PROP_COLOR]);
}

static void webkitColorChooserRequestDispose(GObject* object)
{
    WebKitColorChooserRequest* request = WEBKIT_COLOR_CHOOSER_REQUEST(object);
    if (!request->priv->handled)
        webkit_color_chooser_request_finish(request);

    G_OBJECT_CLASS(webkit_color_chooser_request_parent_class)->dispose(object);
}

static void webkitColorChooserRequestGetProperty(GObject* object, guint propertyID, GValue* value, GParamSpec* paramSpec)
{
    WebKitColorChooserRequest* request = WEBKIT_COLOR_CHOOSER_REQUEST(object);

    switch (propertyID) {
    case PROP_COLOR:
#if PLATFORM(GTK)
        g_value_set_boxed(value, &request->priv->rgba);
#elif PLATFORM(WPE)
        g_value_set_boxed(value, &request->priv->color);
#endif
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyID, paramSpec);
    }
}

static void webkitColorChooserRequestSetProperty(GObject* object, guint propertyID, const GValue* value, GParamSpec* paramSpec)
{
    WebKitColorChooserRequest* request = WEBKIT_COLOR_CHOOSER_REQUEST(object);

    switch (propertyID) {
    case PROP_COLOR:
#if PLATFORM(GTK)
        webkit_color_chooser_request_set_rgba(request, static_cast<GdkRGBA*>(g_value_get_boxed(value)));
#elif PLATFORM(WPE)
        webkit_color_chooser_request_set_color(request, static_cast<WebKitColor*>(g_value_get_boxed(value)));
#endif
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyID, paramSpec);
    }
}

static void webkit_color_chooser_request_class_init(WebKitColorChooserRequestClass* requestClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(requestClass);
    objectClass->dispose = webkitColorChooserRequestDispose;
    objectClass->get_property = webkitColorChooserRequestGetProperty;
    objectClass->set_property = webkitColorChooserRequestSetProperty;

#if PLATFORM(GTK)
    /**
     * WebKitColorChooserRequest:rgba:
     *
     * The [struct@Gdk.RGBA] color of the request.
     *
     * Since: 2.8
     */
    sObjProperties[PROP_COLOR] =
        g_param_spec_boxed("rgba",
            nullptr, nullptr,
            GDK_TYPE_RGBA,
            WEBKIT_PARAM_READWRITE);
#elif PLATFORM(WPE)
    /**
     * WebKitColorChooserRequest:color:
     *
     * The [struct@Color] of the request.
     *
     * Since: 2.56
     */
    sObjProperties[PROP_COLOR] =
        g_param_spec_boxed("color",
            nullptr, nullptr,
            WEBKIT_TYPE_COLOR,
            WEBKIT_PARAM_READWRITE);
#endif

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());

    signals[FINISHED] =
        g_signal_new(
            "finished",
            G_TYPE_FROM_CLASS(requestClass),
            G_SIGNAL_RUN_LAST,
            0, 0,
            nullptr,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
}

#if PLATFORM(GTK)
/**
 * webkit_color_chooser_request_set_rgba:
 * @request: a [class@ColorChooserRequest]
 * @rgba: a pointer to a [struct@Gdk.RGBA]
 *
 * Sets the current [struct@Gdk.RGBA] color of @request.
 *
 * Since: 2.8
 */
void webkit_color_chooser_request_set_rgba(WebKitColorChooserRequest* request, const GdkRGBA* rgba)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));
    g_return_if_fail(rgba);

    if (gdk_rgba_equal(&request->priv->rgba, rgba))
        return;

    request->priv->rgba = *rgba;
    webkitColorChooserRequestSelectedColorChanged(request, WebKit::gdkRGBAToColor(*rgba));
}

/**
 * webkit_color_chooser_request_get_rgba:
 * @request: a [class@ColorChooserRequest]
 * @rgba: (out): a [struct@Gdk.RGBA] to fill in with the current color
 *
 * Gets the current [struct@Gdk.RGBA] color of @request.
 *
 * Since: 2.8
 */
void webkit_color_chooser_request_get_rgba(WebKitColorChooserRequest* request, GdkRGBA* rgba)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));
    g_return_if_fail(rgba);

    *rgba = request->priv->rgba;
}

void webkit_color_chooser_request_get_element_rectangle(WebKitColorChooserRequest* request, GdkRectangle* rect)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));
    g_return_if_fail(rect);

    *rect = request->priv->elementRect;
}
#elif PLATFORM(WPE)
/**
 * webkit_color_chooser_request_set_color:
 * @request: a [class@ColorChooserRequest]
 * @color: a pointer to a [struct@Color]
 *
 * Sets the current [struct@Color] of @request.
 *
 * Since: 2.56
 */
void webkit_color_chooser_request_set_color(WebKitColorChooserRequest* request, const WebKitColor* color)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));
    g_return_if_fail(color);

    const auto& current = request->priv->color;
    if (current.red == color->red && current.green == color->green && current.blue == color->blue && current.alpha == color->alpha)
        return;

    request->priv->color = *color;
    webkitColorChooserRequestSelectedColorChanged(request, webkitColorToWebCoreColor(&request->priv->color));
}

/**
 * webkit_color_chooser_request_get_color:
 * @request: a [class@ColorChooserRequest]
 * @color: (out): a [struct@Color] to fill in with the current color
 *
 * Gets the current [struct@Color] of @request.
 *
 * Since: 2.56
 */
void webkit_color_chooser_request_get_color(WebKitColorChooserRequest* request, WebKitColor* color)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));
    g_return_if_fail(color);

    *color = request->priv->color;
}

void webkit_color_chooser_request_get_element_rectangle(WebKitColorChooserRequest* request, WebKitRectangle* rect)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));
    g_return_if_fail(rect);

    const auto& elementRect = request->priv->elementRect;
    *rect = { elementRect.x(), elementRect.y(), elementRect.width(), elementRect.height() };
}
#endif

void webkit_color_chooser_request_finish(WebKitColorChooserRequest* request)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));

    if (request->priv->handled)
        return;

    request->priv->handled = true;
    g_signal_emit(request, signals[FINISHED], 0);
}

void webkit_color_chooser_request_cancel(WebKitColorChooserRequest* request)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));

    if (request->priv->handled)
        return;

    request->priv->handled = true;
    if (RefPtr colorPicker = request->priv->colorPicker.get())
        colorPicker->setSelectedColor(request->priv->initialColor);
    g_signal_emit(request, signals[FINISHED], 0);
}

WebKitColorChooserRequest* webkitColorChooserRequestCreate(WebKit::WebColorPicker& colorPicker, const WebCore::Color& initialColor, const WebCore::IntRect& elementRect)
{
    WebKitColorChooserRequest* request = WEBKIT_COLOR_CHOOSER_REQUEST(g_object_new(WEBKIT_TYPE_COLOR_CHOOSER_REQUEST, nullptr));
    request->priv->colorPicker = colorPicker;
    request->priv->initialColor = initialColor;
    request->priv->elementRect = elementRect;
#if PLATFORM(GTK)
    request->priv->rgba = WebKit::colorToGdkRGBA(initialColor);
#elif PLATFORM(WPE)
    webkitColorFillFromWebCoreColor(initialColor, &request->priv->color);
#endif
    return request;
}
