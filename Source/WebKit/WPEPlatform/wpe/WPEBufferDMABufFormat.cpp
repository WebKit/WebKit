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
#include "WPEBufferDMABufFormat.h"

#include <wtf/FastMalloc.h>
#include <wtf/glib/GRefPtr.h>

/**
 * WPEBufferDMABufFormat: (copy-func wpe_buffer_dma_buf_format_copy) (free-func wpe_buffer_dma_buf_format_free)
 *
 */
struct _WPEBufferDMABufFormat {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    WPEBufferDMABufFormatUsage usage { WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING };
    guint32 fourcc { 0 };
    GRefPtr<GArray> modifiers;
};

G_DEFINE_BOXED_TYPE(WPEBufferDMABufFormat, wpe_buffer_dma_buf_format, wpe_buffer_dma_buf_format_copy, wpe_buffer_dma_buf_format_free)

/**
 * wpe_buffer_dma_buf_format_new:
 * @usage: a #WPEBufferDMABufFormatUsage
 * @fourcc: the DRM format
 * @modifiers: (transfer none) (element-type guint64): a #GArray for modifiers
 *
 * Create a new #WPEBufferDMABufFormat.
 *
 * Returns: (transfer full): a new allocated #WPEBufferDMABufFormat.
 */
WPEBufferDMABufFormat* wpe_buffer_dma_buf_format_new(WPEBufferDMABufFormatUsage usage, guint32 fourcc, GArray* modifiers)
{
    return new _WPEBufferDMABufFormat { usage, fourcc, modifiers };
}

/**
 * wpe_buffer_dma_buf_format_copy:
 * @format: a #WPEBufferDMABufFormat
 *
 * Copy @format into a new #WPEBufferDMABufFormat
 *
 * Returns: (transfer full): a new allocated #WPEBufferDMABufFormat.
 */
WPEBufferDMABufFormat* wpe_buffer_dma_buf_format_copy(WPEBufferDMABufFormat* format)
{
    g_return_val_if_fail(format, nullptr);

    return new _WPEBufferDMABufFormat { format->usage, format->fourcc, format->modifiers };
}

/**
 * wpe_buffer_dma_buf_format_free:
 * @format: a #WPEBufferDMABufFormat
 *
 * Free @format
 */
void wpe_buffer_dma_buf_format_free(WPEBufferDMABufFormat* format)
{
    g_return_if_fail(format);

    delete format;
}

/**
 * wpe_buffer_dma_buf_format_get_usage:
 * @format: a #WPEBufferDMABufFormat
 *
 * Get the #WPEBufferDMABufFormatUsage of @format
 *
 * Returns: a #WPEBufferDMABufFormatUsage.
 */
WPEBufferDMABufFormatUsage wpe_buffer_dma_buf_format_get_usage(WPEBufferDMABufFormat* format)
{
    g_return_val_if_fail(format, WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING);

    return format->usage;
}

/**
 * wpe_buffer_dma_buf_format_get_fourcc:
 * @format: a #WPEBufferDMABufFormat
 *
 * Get the DRM format of @format
 *
 * Returns: the DRM fourcc
 */
guint32 wpe_buffer_dma_buf_format_get_fourcc(WPEBufferDMABufFormat* format)
{
    g_return_val_if_fail(format, 0);

    return format->fourcc;
}

/**
 * wpe_buffer_dma_buf_format_get_modifiers:
 * @format: a #WPEBufferDMABufFormat
 *
 * Get the list of modifiers of @format
 *
 * Returns: (transfer none) (element-type guint64): a #GArray of #guint64
 */
GArray* wpe_buffer_dma_buf_format_get_modifiers(WPEBufferDMABufFormat* format)
{
    g_return_val_if_fail(format, nullptr);

    return format->modifiers.get();
}
