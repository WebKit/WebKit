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
#include "WPEBufferSHM.h"

#include "WPEEnumTypes.h"
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

/**
 * WPEBufferSHM:
 *
 */
struct _WPEBufferSHMPrivate {
    WPEPixelFormat format;
    GRefPtr<GBytes> data;
    unsigned stride;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEBufferSHM, wpe_buffer_shm, WPE_TYPE_BUFFER, WPEBuffer)

enum {
    PROP_0,

    PROP_FORMAT,
    PROP_DATA,
    PROP_STRIDE,

    N_PROPERTIES
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

static void wpeBufferSHMSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* buffer = WPE_BUFFER_SHM(object);

    switch (propId) {
    case PROP_FORMAT:
        buffer->priv->format = static_cast<WPEPixelFormat>(g_value_get_enum(value));
        break;
    case PROP_DATA:
        buffer->priv->data = static_cast<GBytes*>(g_value_get_boxed(value));
        break;
    case PROP_STRIDE:
        buffer->priv->stride = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void wpeBufferSHMGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* buffer = WPE_BUFFER_SHM(object);

    switch (propId) {
    case PROP_FORMAT:
        g_value_set_enum(value, wpe_buffer_shm_get_format(buffer));
        break;
    case PROP_DATA:
        g_value_set_boxed(value, wpe_buffer_shm_get_data(buffer));
        break;
    case PROP_STRIDE:
        g_value_set_uint(value, wpe_buffer_shm_get_stride(buffer));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static GBytes* wpeBufferSHMImportToPixels(WPEBuffer* buffer, GError**)
{
    return WPE_BUFFER_SHM(buffer)->priv->data.get();
}

static void wpe_buffer_shm_class_init(WPEBufferSHMClass* bufferSHMClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(bufferSHMClass);
    objectClass->set_property = wpeBufferSHMSetProperty;
    objectClass->get_property = wpeBufferSHMGetProperty;

    WPEBufferClass* bufferClass = WPE_BUFFER_CLASS(bufferSHMClass);
    bufferClass->import_to_pixels = wpeBufferSHMImportToPixels;

    /**
     * WPEBufferSHM:format:
     *
     * The buffer pixel format
     */
    sObjProperties[PROP_FORMAT] =
        g_param_spec_enum(
            "format",
            nullptr, nullptr,
            WPE_TYPE_PIXEL_FORMAT,
            WPE_PIXEL_FORMAT_ARGB8888,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WPEBufferSHM:data:
     *
     * The buffer pixel data
     */
    sObjProperties[PROP_DATA] =
        g_param_spec_boxed(
            "data",
            nullptr, nullptr,
            G_TYPE_BYTES,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WPEBufferSHM:stride:
     *
     * The buffer stride
     */
    sObjProperties[PROP_STRIDE] =
        g_param_spec_uint(
            "stride",
            nullptr, nullptr,
            0, G_MAXUINT, 0,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties);
}

/**
 * wpe_buffer_shm_new:
 * @view: a #WPEView
 * @width: the buffer width
 * @height: the buffer height
 * @format: the buffer format
 * @data: the buffer data
 * @stride: the buffer stride
 *
 * Crerate a new #WPEBufferSHM for the given parameters.
 *
 * Returns: (transfer full): a #WPEBufferSHM
 */
WPEBufferSHM* wpe_buffer_shm_new(WPEView* view, int width, int height, WPEPixelFormat format, GBytes* data, guint stride)
{
    g_return_val_if_fail(WPE_IS_VIEW(view), nullptr);
    g_return_val_if_fail(data, nullptr);

    return WPE_BUFFER_SHM(g_object_new(WPE_TYPE_BUFFER_SHM,
        "view", view,
        "width", width,
        "height", height,
        "format", format,
        "data", data,
        "stride", stride,
        nullptr));
}

/**
 * wpe_buffer_shm_get_format:
 * @buffer: a #WPEBufferSHM
 *
 * Get the @buffer pixel format
 *
 * Returns: a #WPEPixelFormat
 */
WPEPixelFormat wpe_buffer_shm_get_format(WPEBufferSHM* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_SHM(buffer), WPE_PIXEL_FORMAT_ARGB8888);

    return buffer->priv->format;
}

/**
 * wpe_buffer_shm_get_data:
 * @buffer: a #WPEBufferSHM
 *
 * Get the @buffer data
 *
 * Returns: (transfer none): a #GBytes
 */
GBytes* wpe_buffer_shm_get_data(WPEBufferSHM* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_SHM(buffer), nullptr);

    return buffer->priv->data.get();
}

/**
 * wpe_buffer_shm_get_stride:
 * @buffer: a #WPEBufferSHM
 *
 * Get the @buffer stride
 *
 * Returns: the buffer stride
 */
guint wpe_buffer_shm_get_stride(WPEBufferSHM* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER_SHM(buffer), 0);

    return buffer->priv->stride;
}
