/*
 * Copyright (C) 2023 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WPEBuffer.h"

#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * WPEBuffer:
 *
 */
struct _WPEBufferPrivate {
    GRefPtr<WPEView> view;
    int width;
    int height;

    struct {
        gpointer data;
        GDestroyNotify destroyFunction;
    } userData;
};

WEBKIT_DEFINE_ABSTRACT_TYPE(WPEBuffer, wpe_buffer, G_TYPE_OBJECT)

/**
 * wpe_buffer_error_quark:
 *
 * Gets the WPEBuffer Error Quark.
 *
 * Returns: a #GQuark.
 **/
G_DEFINE_QUARK(wpe-buffer-error-quark, wpe_buffer_error)

enum {
    PROP_0,

    PROP_VIEW,
    PROP_WIDTH,
    PROP_HEIGHT,

    N_PROPERTIES
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

static void wpeBufferSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* buffer = WPE_BUFFER(object);

    switch (propId) {
    case PROP_VIEW:
        buffer->priv->view = WPE_VIEW(g_value_get_object(value));
        break;
    case PROP_WIDTH:
        buffer->priv->width = g_value_get_int(value);
        break;
    case PROP_HEIGHT:
        buffer->priv->height = g_value_get_int(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeBufferGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* buffer = WPE_BUFFER(object);

    switch (propId) {
    case PROP_VIEW:
        g_value_set_object(value, wpe_buffer_get_view(buffer));
        break;
    case PROP_WIDTH:
        g_value_set_int(value, wpe_buffer_get_width(buffer));
        break;
    case PROP_HEIGHT:
        g_value_set_int(value, wpe_buffer_get_height(buffer));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeBufferDispose(GObject* object)
{
    wpe_buffer_set_user_data(WPE_BUFFER(object), nullptr, nullptr);

    G_OBJECT_CLASS(wpe_buffer_parent_class)->dispose(object);
}

static void wpe_buffer_class_init(WPEBufferClass* bufferClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(bufferClass);
    objectClass->set_property = wpeBufferSetProperty;
    objectClass->get_property = wpeBufferGetProperty;
    objectClass->dispose = wpeBufferDispose;

    /**
     * WPEBuffer:view:
     *
     * The #WPEView of the buffer.
     */
    sObjProperties[PROP_VIEW] =
        g_param_spec_object(
            "view",
            nullptr, nullptr,
            WPE_TYPE_VIEW,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WPEBuffer:width:
     *
     * The buffer width
     */
    sObjProperties[PROP_WIDTH] =
        g_param_spec_int(
            "width",
            nullptr, nullptr,
            0, G_MAXINT, 0,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WPEBuffer:height:
     *
     * The buffer height
     */
    sObjProperties[PROP_HEIGHT] =
        g_param_spec_int(
            "height",
            nullptr, nullptr,
            0, G_MAXINT, 0,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties);
}

/**
 * wpe_buffer_get_view:
 * @buffer: a #WPEBuffer
 *
 * Get the #WPEView of @buffer
 *
 * Returns: (transfer none): a #WPEView
 */
WPEView* wpe_buffer_get_view(WPEBuffer* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), nullptr);

    return buffer->priv->view.get();
}

/**
 * wpe_buffer_get_width:
 * @buffer: a #WPEBuffer
 *
 * Get the @buffer width
 *
 * Returns: the buffer width
 */
int wpe_buffer_get_width(WPEBuffer* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), 0);

    return buffer->priv->width;
}

/**
 * wpe_buffer_get_height:
 * @buffer: a #WPEBuffer
 *
 * Get the @buffer height
 *
 * Returns: the buffer height
 */
int wpe_buffer_get_height(WPEBuffer* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), 0);

    return buffer->priv->height;
}

/**
 * wpe_buffer_set_user_data:
 * @buffer: a #WPEBuffer
 * @user_data: data to associate with @buffer
 * @destroy_func: (nullable): the function to call to release @user_data
 *
 * Set @user_data to @buffer.
 * When @buffer is destroyed @destroy_func is called with @user_data.
 */
void wpe_buffer_set_user_data(WPEBuffer* buffer, gpointer userData, GDestroyNotify destroyFunction)
{
    g_return_if_fail(WPE_IS_BUFFER(buffer));

    auto* priv = buffer->priv;
    if (priv->userData.data == userData && priv->userData.destroyFunction == destroyFunction)
        return;

    if (priv->userData.destroyFunction)
        priv->userData.destroyFunction(priv->userData.data);

    priv->userData.data = userData;
    priv->userData.destroyFunction = destroyFunction;
}

/**
 * wpe_buffer_get_user_data:
 * @buffer: a #WPEBuffer
 *
 * Get user data previously set with wpe_buffer_set_user_data.
 *
 * Returns: (nullable): the @buffer user data, or %NULL
 */
gpointer wpe_buffer_get_user_data(WPEBuffer* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), nullptr);

    return buffer->priv->userData.data;
}

/**
 * wpe_buffer_import_to_egl_image:
 * @buffer: a #WPEBuffer
 * @error: return location for error or %NULL to ignore
 *
 * Import @buffer into a EGL image.
 *
 * Returns: (transfer none): an EGL image, or %NULL in case of error
 */
gpointer wpe_buffer_import_to_egl_image(WPEBuffer* buffer, GError** error)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), nullptr);

    auto* wpeBufferClass = WPE_BUFFER_GET_CLASS(buffer);
    if (!wpeBufferClass->import_to_egl_image) {
        g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_NOT_SUPPORTED, "Operation not supported");
        return nullptr;
    }
    return wpeBufferClass->import_to_egl_image(buffer, error);
}

/**
 * wpe_buffer_import_to_pixels:
 * @buffer: a #WPEBuffer
 * @error: return location for error or %NULL to ignore
 *
 * Import @buffer into a pixels buffer.
 *
 * Returns: (transfer none): a #GBytes with the pixels data, or %NULL in case of error
 */
GBytes* wpe_buffer_import_to_pixels(WPEBuffer* buffer, GError** error)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), nullptr);

    auto* wpeBufferClass = WPE_BUFFER_GET_CLASS(buffer);
    if (!wpeBufferClass->import_to_pixels) {
        g_set_error_literal(error, WPE_BUFFER_ERROR, WPE_BUFFER_ERROR_NOT_SUPPORTED, "Operation not supported");
        return nullptr;
    }
    return wpeBufferClass->import_to_pixels(buffer, error);
}
