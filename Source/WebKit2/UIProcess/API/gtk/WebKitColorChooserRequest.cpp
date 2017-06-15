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
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>

using namespace WebKit;
using namespace WebCore;

/**
 * SECTION: WebKitColorChooserRequest
 * @Short_description: A request to open a color chooser
 * @Title: WebKitColorChooserRequest
 * @See_also: #WebKitWebView
 *
 * Whenever the user interacts with an &lt;input type='color' /&gt;
 * HTML element, WebKit will need to show a dialog to choose a color. For that
 * to happen in a general way, instead of just opening a #GtkColorChooser
 * (which might be not desirable in some cases, which could prefer to use their
 * own color chooser dialog), WebKit will fire the
 * #WebKitWebView::run-color-chooser signal with a #WebKitColorChooserRequest
 * object, which will allow the client application to specify the color to be
 * selected, to inspect the details of the request (e.g. to get initial color)
 * and to cancel the request, in case nothing was selected.
 *
 * In case the client application does not wish to handle this signal,
 * WebKit will provide a default handler which will asynchronously run
 * a regular #GtkColorChooserDialog for the user to interact with.
 */

enum {
    PROP_0,

    PROP_RGBA
};

enum {
    FINISHED,

    LAST_SIGNAL
};

struct _WebKitColorChooserRequestPrivate {
    WebKitColorChooser* colorChooser;
    GdkRGBA rgba;
    bool handled;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitColorChooserRequest, webkit_color_chooser_request, G_TYPE_OBJECT)

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
    case PROP_RGBA:
        g_value_set_boxed(value, &request->priv->rgba);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propertyID, paramSpec);
    }
}

static void webkitColorChooserRequestSetProperty(GObject* object, guint propertyID, const GValue* value, GParamSpec* paramSpec)
{
    WebKitColorChooserRequest* request = WEBKIT_COLOR_CHOOSER_REQUEST(object);

    switch (propertyID) {
    case PROP_RGBA:
        webkit_color_chooser_request_set_rgba(request, static_cast<GdkRGBA*>(g_value_get_boxed(value)));
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

    /**
     * WebKitWebView:rgba:
     *
     * The #GdkRGBA color of the request
     *
     * Since: 2.8
     */
    g_object_class_install_property(objectClass,
        PROP_RGBA,
        g_param_spec_boxed("rgba",
            _("Current RGBA color"),
            _("The current RGBA color for the request"),
            GDK_TYPE_RGBA,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT)));

    /**
     * WebKitColorChooserRequest::finished:
     * @request: the #WebKitColorChooserRequest on which the signal is emitted
     *
     * Emitted when the @request finishes. This signal can be emitted because the
     * user completed the @request calling webkit_color_chooser_request_finish(),
     * or cancelled it with webkit_color_chooser_request_cancel() or because the
     * color input element is removed from the DOM.
     *
     * Since: 2.8
     */
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

/**
 * webkit_color_chooser_request_set_rgba:
 * @request: a #WebKitFileChooserRequest
 * @rgba: a pointer #GdkRGBA
 *
 * Sets the current #GdkRGBA color of @request
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
    g_object_notify(G_OBJECT(request), "rgba");
}

/**
 * webkit_color_chooser_request_get_rgba:
 * @request: a #WebKitColorChooserRequest
 * @rgba: (out): a #GdkRGBA to fill in with the current color.
 *
 * Gets the current #GdkRGBA color of @request
 *
 * Since: 2.8
 */
void webkit_color_chooser_request_get_rgba(WebKitColorChooserRequest* request, GdkRGBA* rgba)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));
    g_return_if_fail(rgba);

    *rgba = request->priv->rgba;
}

/**
 * webkit_color_chooser_request_get_element_rectangle:
 * @request: a #WebKitColorChooserRequest
 * @rect: (out): a #GdkRectangle to fill in with the element area
 *
 * Gets the bounding box of the color input element.
 *
 * Since: 2.8
 */
void webkit_color_chooser_request_get_element_rectangle(WebKitColorChooserRequest* request, GdkRectangle* rect)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));
    g_return_if_fail(rect);

    *rect = request->priv->colorChooser->elementRect();
}

/**
 * webkit_color_chooser_request_finish:
 * @request: a #WebKitColorChooserRequest
 *
 * Finishes @request and the input element keeps the current value of
 * #WebKitColorChooserRequest:rgba.
 * The signal #WebKitColorChooserRequest::finished
 * is emitted to notify that the request has finished.
 *
 * Since: 2.8
 */
void webkit_color_chooser_request_finish(WebKitColorChooserRequest* request)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));

    if (request->priv->handled)
        return;

    request->priv->handled = true;
    g_signal_emit(request, signals[FINISHED], 0);
}

/**
 * webkit_color_chooser_request_cancel:
 * @request: a #WebKitColorChooserRequest
 *
 * Cancels @request and the input element changes to use the initial color
 * it has before the request started.
 * The signal #WebKitColorChooserRequest::finished
 * is emitted to notify that the request has finished.
 *
 * Since: 2.8
 */
void webkit_color_chooser_request_cancel(WebKitColorChooserRequest* request)
{
    g_return_if_fail(WEBKIT_IS_COLOR_CHOOSER_REQUEST(request));

    if (request->priv->handled)
        return;

    request->priv->handled = true;
    request->priv->colorChooser->cancel();
    g_signal_emit(request, signals[FINISHED], 0);
}

WebKitColorChooserRequest* webkitColorChooserRequestCreate(WebKitColorChooser* colorChooser)
{
    WebKitColorChooserRequest* request = WEBKIT_COLOR_CHOOSER_REQUEST(
        g_object_new(WEBKIT_TYPE_COLOR_CHOOSER_REQUEST, "rgba", colorChooser->initialColor(), nullptr));
    request->priv->colorChooser = colorChooser;
    return request;
}
