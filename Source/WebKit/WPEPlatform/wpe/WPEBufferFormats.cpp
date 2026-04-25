/*
 * Copyright (C) 2024 Igalia S.L.
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
#include "WPEBufferFormats.h"

#include "GRefPtrWPE.h"
#include <wtf/FastMalloc.h>
#include <wtf/Vector.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

struct BufferFormat {
    explicit BufferFormat(guint32 fourcc)
        : fourcc(fourcc)
        , modifiers(adoptGRef(g_array_new(FALSE, TRUE, sizeof(guint64))))
    {
    }

    BufferFormat(const BufferFormat&) = delete;
    BufferFormat& operator=(const BufferFormat&) = delete;
    BufferFormat(BufferFormat&& other)
        : fourcc(other.fourcc)
        , modifiers(WTF::move(other.modifiers))
    {
    }

    guint32 fourcc { 0 };
    GRefPtr<GArray> modifiers;
};

struct BufferFormatsGroup {
    BufferFormatsGroup(WPEDRMDevice* device, WPEBufferFormatUsage usage)
        : device(device)
        , usage(usage)
    {
    }

    BufferFormatsGroup(const BufferFormatsGroup&) = delete;
    BufferFormatsGroup& operator=(const BufferFormatsGroup&) = delete;
    BufferFormatsGroup(BufferFormatsGroup&& other)
        : device(other.device)
        , usage(other.usage)
        , formats(WTF::move(other.formats))
    {
        other.device = nullptr;
        other.usage = WPE_BUFFER_FORMAT_USAGE_RENDERING;
    }

    GRefPtr<WPEDRMDevice> device;
    WPEBufferFormatUsage usage { WPE_BUFFER_FORMAT_USAGE_RENDERING };
    Vector<BufferFormat> formats;
};

/**
 * WPEBufferFormats:
 *
 * List of supported buffer formats
 */
struct _WPEBufferFormatsPrivate {
    GRefPtr<WPEDRMDevice> device;
    Vector<BufferFormatsGroup> groups;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEBufferFormats, wpe_buffer_formats, G_TYPE_OBJECT, GObject)

static void wpe_buffer_formats_class_init(WPEBufferFormatsClass*)
{
}

/**
 * wpe_buffer_formats_get_device:
 * @formats: a #WPEBufferFormats
 *
 * Get the main DRM device to be used to allocate buffer for @formats
 *
 * Returns: (transfer none) (nullable): a #WPEDRMDevice or %NULL
 */
WPEDRMDevice* wpe_buffer_formats_get_device(WPEBufferFormats* formats)
{
    g_return_val_if_fail(WPE_IS_BUFFER_FORMATS(formats), nullptr);

    return formats->priv->device.get();
}

/**
 * wpe_buffer_formats_get_n_groups:
 * @formats: a #WPEBufferFormats
 *
 * Get the number of groups in @formats
 *
 * Returns: the number of groups
 */
guint wpe_buffer_formats_get_n_groups(WPEBufferFormats* formats)
{
    g_return_val_if_fail(WPE_IS_BUFFER_FORMATS(formats), 0);

    return formats->priv->groups.size();
}

/**
 * wpe_buffer_formats_get_group_usage:
 * @formats: a #WPEBufferFormats
 * @group: a group index
 *
 * Get the #WPEBufferFormatUsage of @group in @formats
 *
 * Returns: a #WPEBufferFormatUsage.
 */
WPEBufferFormatUsage wpe_buffer_formats_get_group_usage(WPEBufferFormats* formats, guint group)
{
    g_return_val_if_fail(WPE_IS_BUFFER_FORMATS(formats), WPE_BUFFER_FORMAT_USAGE_RENDERING);
    g_return_val_if_fail(group < formats->priv->groups.size(), WPE_BUFFER_FORMAT_USAGE_RENDERING);

    if (group >= formats->priv->groups.size())
        return WPE_BUFFER_FORMAT_USAGE_RENDERING;

    return formats->priv->groups[group].usage;
}

/**
 * wpe_buffer_formats_get_group_device:
 * @formats: a #WPEBufferFormats
 * @group: a group index
 *
 * Get the target DRM device of @group in @formats
 *
 * Returns: (transfer none) (nullable): a #WPEDRMDevice or %NULL
 */
WPEDRMDevice* wpe_buffer_formats_get_group_device(WPEBufferFormats* formats, guint group)
{
    g_return_val_if_fail(WPE_IS_BUFFER_FORMATS(formats), nullptr);
    g_return_val_if_fail(group < formats->priv->groups.size(), nullptr);

    if (group >= formats->priv->groups.size())
        return nullptr;

    return formats->priv->groups[group].device.get();
}

/**
 * wpe_buffer_formats_get_group_n_formats:
 * @formats: a #WPEBufferFormats
 * @group: a group index
 *
 * Get the number of formats in @group in @formats
 *
 * Returns: the number of formats
 */
guint wpe_buffer_formats_get_group_n_formats(WPEBufferFormats* formats, guint group)
{
    g_return_val_if_fail(WPE_IS_BUFFER_FORMATS(formats), 0);
    g_return_val_if_fail(group < formats->priv->groups.size(), 0);

    if (group >= formats->priv->groups.size())
        return 0;

    return formats->priv->groups[group].formats.size();
}

/**
 * wpe_buffer_formats_get_format_fourcc:
 * @formats: a #WPEBufferFormats
 * @group: a group index
 * @format: a format index
 *
 * Get the DRM fourcc of @format in @group in @formats
 *
 * Returns: the DRM fourcc
 */
guint32 wpe_buffer_formats_get_format_fourcc(WPEBufferFormats* formats, guint group, guint format)
{
    g_return_val_if_fail(WPE_IS_BUFFER_FORMATS(formats), 0);
    g_return_val_if_fail(group < formats->priv->groups.size(), 0);
    g_return_val_if_fail(format < formats->priv->groups[group].formats.size(), 0);

    if (group >= formats->priv->groups.size() || format >= formats->priv->groups[group].formats.size())
        return 0;

    return formats->priv->groups[group].formats[format].fourcc;
}

/**
 * wpe_buffer_formats_get_format_modifiers:
 * @formats: a #WPEBufferFormats
 * @group: a group index
 * @format: a format index
 *
 * Get the list of modifiers of @format in @group in @formats
 *
 * Returns: (transfer none) (element-type guint64): a #GArray of #guint64
 */
GArray* wpe_buffer_formats_get_format_modifiers(WPEBufferFormats* formats, guint group, guint format)
{
    g_return_val_if_fail(WPE_IS_BUFFER_FORMATS(formats), nullptr);
    g_return_val_if_fail(group < formats->priv->groups.size(), nullptr);
    g_return_val_if_fail(format < formats->priv->groups[group].formats.size(), nullptr);

    if (group >= formats->priv->groups.size() || format >= formats->priv->groups[group].formats.size())
        return nullptr;

    return formats->priv->groups[group].formats[format].modifiers.get();
}

/**
 * WPEBufferFormatsBuilder:
 *
 * Helper type to build a #WPEBufferFormats
 */
struct _WPEBufferFormatsBuilder {
    WTF_DEPRECATED_MAKE_STRUCT_FAST_ALLOCATED(_WPEBufferFormatsBuilder);

    _WPEBufferFormatsBuilder(WPEDRMDevice* mainDevice)
        : device(mainDevice)
    {
    }

    GRefPtr<WPEDRMDevice> device;
    Vector<BufferFormatsGroup> groups;
    int referenceCount { 1 };
};
G_DEFINE_BOXED_TYPE(WPEBufferFormatsBuilder, wpe_buffer_formats_builder, wpe_buffer_formats_builder_ref, wpe_buffer_formats_builder_unref)

/**
 * wpe_buffer_formats_builder_new:
 * @device: (nullable): the main DRM device
 *
 * Create a new #WPEBufferFormatsBuilder
 *
 * Returns: (transfer full): a new #WPEBufferFormatsBuilder
 */
WPEBufferFormatsBuilder* wpe_buffer_formats_builder_new(WPEDRMDevice* device)
{
    auto* builder = static_cast<WPEBufferFormatsBuilder*>(fastMalloc(sizeof(WPEBufferFormatsBuilder)));
    new (builder) WPEBufferFormatsBuilder(device);
    return builder;
}

/**
 * wpe_buffer_formats_builder_ref:
 * @builder: a #WPEBufferFormatsBuilder
 *
 * Atomically acquires a reference on the given @builder.
 *
 * This function is MT-safe and may be called from any thread.
 *
 * Returns: The same @builder with an additional reference.
 */
WPEBufferFormatsBuilder* wpe_buffer_formats_builder_ref(WPEBufferFormatsBuilder* builder)
{
    g_return_val_if_fail(builder, nullptr);

    g_atomic_int_inc(&builder->referenceCount);
    return builder;
}

/**
 * wpe_buffer_formats_builder_unref:
 * @builder: a #WPEBufferFormatsBuilder
 *
 * Atomically releases a reference on the given @builder.
 *
 * If the reference was the last, the resources associated to the
 * @builder are freed. This function is MT-safe and may be called from
 * any thread.
 */
void wpe_buffer_formats_builder_unref(WPEBufferFormatsBuilder* builder)
{
    g_return_if_fail(builder);

    if (g_atomic_int_dec_and_test(&builder->referenceCount)) {
        builder->~WPEBufferFormatsBuilder();
        fastFree(builder);
    }
}

/**
 * wpe_buffer_formats_builder_append_group:
 * @builder: a #WPEBufferFormatsBuilder
 * @device: (nullable): a #WPEDRMDevice
 * @usage: a #WPEBufferFormatUsage
 *
 * Append a new group for @device and @usage to @builder.
 * If @device is %NULL, the main device passed to wpe_buffer_formats_builder_new()
 * should be used.
 */
void wpe_buffer_formats_builder_append_group(WPEBufferFormatsBuilder* builder, WPEDRMDevice* device, WPEBufferFormatUsage usage)
{
    g_return_if_fail(builder);

    builder->groups.append(BufferFormatsGroup(device, usage));
}

/**
 * wpe_buffer_formats_builder_append_format:
 * @builder: a #WPEBufferFormatsBuilder
 * @fourcc: a DRM fourcc
 * @modifier: a DRM modifier
 *
 * Append a new pair of @format and @modifier to the last group added to @builder
 */
void wpe_buffer_formats_builder_append_format(WPEBufferFormatsBuilder* builder, guint32 fourcc, guint64 modifier)
{
    g_return_if_fail(builder);

    auto& group = builder->groups.last();
    if (group.formats.isEmpty() || group.formats.last().fourcc != fourcc)
        group.formats.append(BufferFormat(fourcc));
    g_array_append_val(group.formats.last().modifiers.get(), modifier);
}

/**
 * wpe_buffer_formats_builder_end:
 * @builder: (transfer full): a #WPEBufferFormatsBuilder
 *
 * End the builder process and return the constructed #WPEBufferFormats.
 * This function calls wpe_buffer_formats_builder_unref() on @builder.
 *
 * Returns: (transfer full): a new #WPEBufferFormats.
 */
WPEBufferFormats* wpe_buffer_formats_builder_end(WPEBufferFormatsBuilder* builder)
{
    g_return_val_if_fail(builder, nullptr);

    auto* formats = WPE_BUFFER_FORMATS(g_object_new(WPE_TYPE_BUFFER_FORMATS, nullptr));
    formats->priv->device = WTF::move(builder->device);
    formats->priv->groups = WTF::move(builder->groups);
    wpe_buffer_formats_builder_unref(builder);

    return formats;
}
