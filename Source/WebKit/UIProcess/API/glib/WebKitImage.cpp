/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "WebKitImage.h"

#if ENABLE(2022_GLIB_API)

#include "WebKitImagePrivate.h"

#if USE(SKIA)
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <skia/core/SkData.h>
#include <skia/core/SkImage.h>
#include <skia/core/SkImageInfo.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // Skia port
#include <skia/encode/SkPngEncoder.h>
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#else
#include <WebCore/NotImplemented.h>
#endif
#include <wtf/Assertions.h>
#include <wtf/Hasher.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/glib/WTFGType.h>

static void webkit_image_gicon_interface_init(GIconIface*);
static void webkit_image_gloadable_icon_interface_init(GLoadableIconIface*);

struct _WebKitImagePrivate {
    int width;
    int height;
    unsigned stride;

    GRefPtr<GBytes> bytes;
};

/**
 * WebKitImage:
 *
 * Represents an image as a buffer containing pixel data.
 *
 * Image objects are always created by WebKit, and considered immutable:
 * a copy of the image data needs to be made before modifying the image.
 * Pixel data can be obtained with [id@webkit_image_as_bytes].
 *
 * Since: 2.52
 */
WEBKIT_DEFINE_FINAL_TYPE_WITH_CODE(
    WebKitImage, webkit_image, G_TYPE_OBJECT, GObject,
    G_IMPLEMENT_INTERFACE(G_TYPE_ICON, webkit_image_gicon_interface_init)
    G_IMPLEMENT_INTERFACE(G_TYPE_LOADABLE_ICON, webkit_image_gloadable_icon_interface_init))


enum {
    PROP_0,

    PROP_WIDTH,
    PROP_HEIGHT,
    PROP_STRIDE,

    N_PROPERTIES
};

static constexpr uint8_t RGBA8BytesPerPixel = 4;

static std::array<GParamSpec*, N_PROPERTIES> sObjProperties;

static void webkitImageSetProperty(GObject* object, guint propId, const GValue* value, GParamSpec* paramSpec)
{
    auto* image = WEBKIT_IMAGE(object);

    switch (propId) {
    case PROP_WIDTH:
        image->priv->width = g_value_get_int(value);
        break;
    case PROP_HEIGHT:
        image->priv->height = g_value_get_int(value);
        break;
    case PROP_STRIDE:
        image->priv->stride = g_value_get_uint(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkitImageGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    auto* image = WEBKIT_IMAGE(object);

    switch (propId) {
    case PROP_WIDTH:
        g_value_set_int(value, webkit_image_get_width(image));
        break;
    case PROP_HEIGHT:
        g_value_set_int(value, webkit_image_get_height(image));
        break;
    case PROP_STRIDE:
        g_value_set_uint(value, webkit_image_get_stride(image));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_image_class_init(WebKitImageClass* imageClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(imageClass);
    objectClass->set_property = webkitImageSetProperty;
    objectClass->get_property = webkitImageGetProperty;

    /**
     * WebKitImage:width: (getter get_width) (attributes org.gtk.Property.get=webkit_image_get_width):
     *
     * The image width in pixels.
     *
     * Since: 2.52
     */
    sObjProperties[PROP_WIDTH] =
    g_param_spec_int(
        "width",
        nullptr, nullptr,
        1, G_MAXINT, 1,
        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitImage:height: (getter get_height) (attributes org.gtk.Property.get=webkit_image_get_height):
     *
     * The image height in pixels.
     *
     * Since: 2.52
     */
    sObjProperties[PROP_HEIGHT] =
    g_param_spec_int(
        "height",
        nullptr, nullptr,
        1, G_MAXINT, 1,
        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    /**
     * WebKitImage:stride: (getter get_stride) (attributes org.gtk.Property.get=webkit_image_get_stride):
     *
     * The image stride, in bytes. This value indicates the amount of
     * memory occupied by each row of pixels in the image. Note that
     * the stride may be larger than the image width multiplied by the
     * amount of bytes used to represent each pixel.
     *
     * Since: 2.52
     */
    sObjProperties[PROP_STRIDE] =
    g_param_spec_uint(
        "stride",
        nullptr, nullptr,
        RGBA8BytesPerPixel, G_MAXUINT, 4,
        static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_properties(objectClass, N_PROPERTIES, sObjProperties.data());
}

WebKitImage* webkitImageNew(int width, int height, guint stride, GRefPtr<GBytes>&& bytes)
{
    RELEASE_ASSERT(bytes);
    RELEASE_ASSERT(static_cast<gsize>(width) * RGBA8BytesPerPixel <= stride);

    auto* image = WEBKIT_IMAGE(g_object_new(WEBKIT_TYPE_IMAGE,
        "width", width,
        "height", height,
        "stride", stride,
        nullptr));
    image->priv->bytes = WTFMove(bytes);

    return image;
}

/**
 * webkit_image_get_width: (get-property width):
 * @image: a #WebKitImage
 *
 * Get the @image width in pixels.
 *
 * Returns: the image width
 *
 * Since: 2.52
 */
int webkit_image_get_width(WebKitImage* image)
{
    g_return_val_if_fail(WEBKIT_IS_IMAGE(image), 0);
    return image->priv->width;
}

/**
 * webkit_image_get_height: (get-property height):
 * @image: a #WebKitImage
 *
 * Get the @image height in pixels.
 *
 * Returns: the image height
 *
 * Since: 2.52
 */
int webkit_image_get_height(WebKitImage* image)
{
    g_return_val_if_fail(WEBKIT_IS_IMAGE(image), 0);
    return image->priv->height;
}

/**
 * webkit_image_get_stride: (get-property stride):
 * @image: a #WebKitImage
 *
 * Get the @image stride.
 *
 * Returns: the image stride
 *
 * Since: 2.52
 */
guint webkit_image_get_stride(WebKitImage* image)
{
    g_return_val_if_fail(WEBKIT_IS_IMAGE(image), 0);
    return image->priv->stride;
}

/**
 * webkit_image_as_bytes:
 * @image: a #WebKitImage
 *
 * Get the @image pixel data as an array of bytes.
 *
 * The pixel format for the returned byte buffer is 32-bit per pixel
 * with 8-bit premultiplied alpha, in the preferred byte order for
 * the architecture.
 *
 * Returns: (transfer none): a #GBytes
 *
 * Since: 2.52
 */
GBytes* webkit_image_as_bytes(WebKitImage* image)
{
    g_return_val_if_fail(WEBKIT_IS_IMAGE(image), nullptr);

    return image->priv->bytes.get();
}

static guint webkitImageHash(GIcon* icon)
{
    g_return_val_if_fail(WEBKIT_IS_IMAGE(icon), 0);

    auto* image = WEBKIT_IMAGE(icon);

    Hasher hasher;

    auto* bytes = webkit_image_as_bytes(image);
    auto dataSpan = span(bytes);

    gsize rowBytes = image->priv->width * RGBA8BytesPerPixel;

    ASSERT(dataSpan.size() >= image->priv->stride * (image->priv->height - 1) + rowBytes);

    for (int y = 0; y < image->priv->height; ++y) {
        auto row = consumeSpan(dataSpan, image->priv->stride);
        WTF::add(hasher, row.first(rowBytes));
    }

    WTF::add(hasher, image->priv->width, image->priv->height);
    return hasher.hash();
}

static gboolean webkitImageEqual(GIcon* icon1, GIcon* icon2)
{
    g_return_val_if_fail(WEBKIT_IS_IMAGE(icon1), false);

    if (!WEBKIT_IS_IMAGE(icon2))
        return false;

    auto* image1 = WEBKIT_IMAGE(icon1);
    auto* image2 = WEBKIT_IMAGE(icon2);

    if (image1->priv->width != image2->priv->width
        || image1->priv->height != image2->priv->height)
        return false;

    auto* bytes1 = webkit_image_as_bytes(image1);
    auto* bytes2 = webkit_image_as_bytes(image2);

    auto dataSpan1 = span(bytes1);
    auto dataSpan2 = span(bytes2);

    gsize rowBytes = image1->priv->width * RGBA8BytesPerPixel;

    ASSERT(dataSpan1.size() >= image1->priv->stride * (image1->priv->height - 1) + rowBytes);
    ASSERT(dataSpan2.size() >= image2->priv->stride * (image1->priv->height - 1) + rowBytes);

    for (int y = 0; y < image1->priv->height; ++y) {
        auto row1 = consumeSpan(dataSpan1, image1->priv->stride);
        auto row2 = consumeSpan(dataSpan2, image2->priv->stride);

        if (!equalSpans(row1.first(rowBytes), row2.first(rowBytes)))
            return false;
    }

    return true;
}

static GInputStream* webkitImageLoad(GLoadableIcon* icon, int size, char** type, GCancellable* cancellable, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_IMAGE(icon), nullptr);

    auto* image = WEBKIT_IMAGE(icon);
    auto* bytes = webkit_image_as_bytes(image);
    if (!bytes) {
        LOG_ERROR("Failed to retrieve image RGBA bytes");
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to encode image as PNG");
        return nullptr;
    }

#if USE(SKIA)
    gsize dataSize = 0;
    auto* data = g_bytes_get_data(bytes,  &dataSize);
    sk_sp<SkData> skData = SkData::MakeWithoutCopy(data, dataSize);
    SkImageInfo info = SkImageInfo::Make(
        image->priv->width,
        image->priv->height,
        kBGRA_8888_SkColorType,
        kUnpremul_SkAlphaType
    );

    sk_sp<SkImage> skImage = SkImages::RasterFromData(info, skData, image->priv->stride);

    if (!skImage) {
        LOG_ERROR("Failed to create SkImage from data");
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to encode image as PNG");
        return nullptr;
    }

    sk_sp<SkData> pngData = SkPngEncoder::Encode(nullptr, skImage.get(), { });
    if (!pngData) {
        LOG_ERROR("Failed to encode SkImage as PNG");
        g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Failed to encode image as PNG");
        return nullptr;
    }

    pngData->ref();
    GRefPtr<GBytes> pngBytes = adoptGRef(g_bytes_new_with_free_func(
        pngData->data(),
        pngData->size(),
        [](gpointer userData) {
            static_cast<SkData*>(userData)->unref();
        },
        pngData.get()
    ));

    GInputStream* stream = g_memory_input_stream_new_from_bytes(pngBytes.get());

    if (type)
        *type = g_strdup("image/png"_s);

    return stream;
#else
    notImplemented();
    g_set_error_literal(error, G_IO_ERROR, G_IO_ERROR_FAILED, "Operation unimplemented");
    return nullptr;
#endif
}

struct LoadTaskData {
    int size;
    GLoadableIcon* icon;
};
WEBKIT_DEFINE_ASYNC_DATA_STRUCT(LoadTaskData)

static void webkitImageLoadAsync(GLoadableIcon* icon, int size, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    auto taskData = createLoadTaskData();
    taskData->icon = icon;
    taskData->size = size;
    GRefPtr<GTask> task = adoptGRef(g_task_new(icon, cancellable, callback, userData));
    GTaskThreadFunc taskHandler = [](GTask* task, gpointer, gpointer taskData, GCancellable* cancellable) {
        if (g_task_return_error_if_cancelled(task))
            return;

        auto* data = static_cast<LoadTaskData*>(taskData);
        GUniqueOutPtr<GError> error;
        GRefPtr<GInputStream> stream = webkitImageLoad(data->icon, data->size, nullptr, cancellable, &error.outPtr());
        if (error)
            g_task_return_error(task, error.release());
        else
            g_task_return_pointer(task, stream.leakRef(), g_object_unref);
    };
    g_task_set_task_data(task.get(), taskData, reinterpret_cast<GDestroyNotify>(destroyLoadTaskData));
    g_task_run_in_thread(task.get(), taskHandler);
}

static GInputStream* webkitImageLoadFinish(GLoadableIcon* icon, GAsyncResult* res, char** type, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_IMAGE(icon), nullptr);

    if (type)
        *type = g_strdup("image/png"_s);
    return G_INPUT_STREAM(g_task_propagate_pointer(G_TASK(res), error));
}

static void webkit_image_gicon_interface_init(GIconIface* iface)
{
    iface->hash = webkitImageHash;
    iface->equal = webkitImageEqual;
}

static void webkit_image_gloadable_icon_interface_init(GLoadableIconIface* iface)
{
    iface->load = webkitImageLoad;
    iface->load_async = webkitImageLoadAsync;
    iface->load_finish = webkitImageLoadFinish;
}

#endif // ENABLE(2022_GLIB_API)
