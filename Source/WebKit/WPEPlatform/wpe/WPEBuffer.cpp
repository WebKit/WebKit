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
#include <wtf/glib/GWeakPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/unix/UnixFileDescriptor.h>

/**
 * WPEBuffer:
 *
 */
struct _WPEBufferPrivate {
    GWeakPtr<WPEDisplay> display;
    int width;
    int height;
    UnixFileDescriptor renderingFence;
    UnixFileDescriptor releaseFence;

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

    PROP_DISPLAY,
    PROP_WIDTH,
    PROP_HEIGHT,

    N_PROPERTIES
};

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

static void wpeBufferSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* buffer = WPE_BUFFER(object);

    switch (propId) {
    case PROP_DISPLAY:
        buffer->priv->display.reset(WPE_DISPLAY(g_value_get_object(value)));
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
    case PROP_DISPLAY:
        g_value_set_object(value, wpe_buffer_get_display(buffer));
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
     * WPEBuffer:display:
     *
     * The #WPEDisplay of the buffer.
     */
    sObjProperties[PROP_DISPLAY] =
        g_param_spec_object(
            "display",
            nullptr, nullptr,
            WPE_TYPE_DISPLAY,
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

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());
}

/**
 * wpe_buffer_get_display:
 * @buffer: a #WPEBuffer
 *
 * Get the #WPEDisplay of @buffer
 *
 * Returns: (transfer none) (nullable): a #WPEDisplay or %NULL
 */
WPEDisplay* wpe_buffer_get_display(WPEBuffer* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), nullptr);

    return buffer->priv->display.get();
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
 * Import @buffer into a pixels buffer. The returned data is owned by the @buffer and it's
 * not a copy, so if you are not going to use it immediately or need to modify the data, you
 * should make a copy.
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

/**
 * wpe_buffer_set_rendering_fence:
 * @buffer: a #WPEBuffer
 * @fd: a file descriptor, or -1
 *
 * Set the rendering fence file descriptor to use for the @buffer. The fence
 * will be used to wait before rendering the buffer when the display supports
 * explicit sync, see wpe_display_use_explicit_sync().
 * The buffer takes the ownership of the file descriptor.
 */
void wpe_buffer_set_rendering_fence(WPEBuffer* buffer, int fd)
{
    g_return_if_fail(WPE_IS_BUFFER(buffer));

    if (buffer->priv->renderingFence.value() == fd)
        return;

    buffer->priv->renderingFence = UnixFileDescriptor { fd, UnixFileDescriptor::Adopt };
}

/**
 * wpe_buffer_get_rendering_fence:
 * @buffer: a #WPEBuffer
 *
 * Get the rendering fence file descriptor of @buffer.
 *
 * Returns: a file descriptor, or -1
 */
int wpe_buffer_get_rendering_fence(WPEBuffer* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), -1);

    return buffer->priv->renderingFence.value();
}

/**
 * wpe_buffer_take_rendering_fence:
 * @buffer: a #WPEBuffer
 *
 * Get the rendering fence file descriptor of @buffer and set it to -1.
 *
 * Returns: a file descriptor, or -1
 */
int wpe_buffer_take_rendering_fence(WPEBuffer* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), -1);

    return buffer->priv->renderingFence.release();
}

 /**
 * wpe_buffer_set_release_fence:
 * @buffer: a #WPEBuffer
 * @fd: a file descriptor, or -1
 *
 * Set the release fence file descriptor to use for the @buffer. The fence
 * will be used to wait before releasing the buffer to be destroyed or reused.
 * The buffer takes the ownership of the file descriptor.
 */
void wpe_buffer_set_release_fence(WPEBuffer* buffer, int fd)
{
    g_return_if_fail(WPE_IS_BUFFER(buffer));

    if (buffer->priv->releaseFence.value() == fd)
        return;

    buffer->priv->releaseFence = UnixFileDescriptor { fd, UnixFileDescriptor::Adopt };
}

/**
 * wpe_buffer_get_release_fence:
 * @buffer: a #WPEBuffer
 *
 * Get the release fence file descriptor of @buffer.
 *
 * Returns: a file descriptor, or -1
 */
int wpe_buffer_get_release_fence(WPEBuffer* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), -1);

    return buffer->priv->releaseFence.value();
}

/**
 * wpe_buffer_take_release_fence:
 * @buffer: a #WPEBuffer
 *
 * Get the release fence file descriptor of @buffer and set it to -1.
 *
 * Returns: a file descriptor, or -1
 */
int wpe_buffer_take_release_fence(WPEBuffer* buffer)
{
    g_return_val_if_fail(WPE_IS_BUFFER(buffer), -1);

    return buffer->priv->releaseFence.release();
}
