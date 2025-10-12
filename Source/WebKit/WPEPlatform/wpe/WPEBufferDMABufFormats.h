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

#ifndef WPEBufferDMABufFormats_h
#define WPEBufferDMABufFormats_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDRMDevice.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

#define WPE_TYPE_BUFFER_DMA_BUF_FORMATS (wpe_buffer_dma_buf_formats_get_type())
WPE_API G_DECLARE_FINAL_TYPE (WPEBufferDMABufFormats, wpe_buffer_dma_buf_formats, WPE, BUFFER_DMA_BUF_FORMATS, GObject)

/**
 * WPEBufferDMABufFormatUsage:
 * @WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING: format should be used for rendering.
 * @WPE_BUFFER_DMA_BUF_FORMAT_USAGE_MAPPING: format should be used for mapping buffer.
 * @WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT: format should be used for scanout.
 *
 * Enum values to indicate the best usage of a #WPEBufferDMABufFormat.
 */
typedef enum {
    WPE_BUFFER_DMA_BUF_FORMAT_USAGE_RENDERING,
    WPE_BUFFER_DMA_BUF_FORMAT_USAGE_MAPPING,
    WPE_BUFFER_DMA_BUF_FORMAT_USAGE_SCANOUT
} WPEBufferDMABufFormatUsage;

WPE_API WPEDRMDevice              *wpe_buffer_dma_buf_formats_get_device           (WPEBufferDMABufFormats *formats);
WPE_API guint                      wpe_buffer_dma_buf_formats_get_n_groups         (WPEBufferDMABufFormats *formats);
WPE_API WPEBufferDMABufFormatUsage wpe_buffer_dma_buf_formats_get_group_usage      (WPEBufferDMABufFormats *formats,
                                                                                    guint                   group);
WPE_API WPEDRMDevice              *wpe_buffer_dma_buf_formats_get_group_device     (WPEBufferDMABufFormats *formats,
                                                                                    guint                   group);
WPE_API guint                      wpe_buffer_dma_buf_formats_get_group_n_formats  (WPEBufferDMABufFormats *formats,
                                                                                    guint                   group);
WPE_API guint32                    wpe_buffer_dma_buf_formats_get_format_fourcc    (WPEBufferDMABufFormats *formats,
                                                                                    guint                   group,
                                                                                    guint                   format);
WPE_API GArray                    *wpe_buffer_dma_buf_formats_get_format_modifiers (WPEBufferDMABufFormats *formats,
                                                                                    guint                   group,
                                                                                    guint                   format);

#define WPE_TYPE_BUFFER_DMA_BUF_FORMATS_BUILDER (wpe_buffer_dma_buf_formats_builder_get_type())
typedef struct _WPEBufferDMABufFormatsBuilder WPEBufferDMABufFormatsBuilder;

WPE_API GType                          wpe_buffer_dma_buf_formats_builder_get_type      (void);
WPE_API WPEBufferDMABufFormatsBuilder *wpe_buffer_dma_buf_formats_builder_new           (WPEDRMDevice                  *device);
WPE_API WPEBufferDMABufFormatsBuilder *wpe_buffer_dma_buf_formats_builder_ref           (WPEBufferDMABufFormatsBuilder *builder);
WPE_API void                           wpe_buffer_dma_buf_formats_builder_unref         (WPEBufferDMABufFormatsBuilder *builder);
WPE_API void                           wpe_buffer_dma_buf_formats_builder_append_group  (WPEBufferDMABufFormatsBuilder *builder,
                                                                                         WPEDRMDevice                  *device,
                                                                                         WPEBufferDMABufFormatUsage     usage);
WPE_API void                           wpe_buffer_dma_buf_formats_builder_append_format (WPEBufferDMABufFormatsBuilder *builder,
                                                                                         guint32                        fourcc,
                                                                                         guint64                        modifier);
WPE_API WPEBufferDMABufFormats        *wpe_buffer_dma_buf_formats_builder_end           (WPEBufferDMABufFormatsBuilder *builder);

G_END_DECLS

#endif /* WPEBufferDMABufFormats_h */
