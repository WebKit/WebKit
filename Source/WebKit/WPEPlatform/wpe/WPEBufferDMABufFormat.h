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

#ifndef WPEBufferDMABufFormat_h
#define WPEBufferDMABufFormat_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

#define WPE_TYPE_BUFFER_DMA_BUF_FORMAT (wpe_buffer_dma_buf_format_get_type())

typedef struct _WPEBufferDMABufFormat WPEBufferDMABufFormat;

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

WPE_API GType                      wpe_buffer_dma_buf_format_get_type      (void);
WPE_API WPEBufferDMABufFormat     *wpe_buffer_dma_buf_format_new           (WPEBufferDMABufFormatUsage usage,
                                                                            guint32                    fourcc,
                                                                            GArray                    *modifiers);
WPE_API WPEBufferDMABufFormat     *wpe_buffer_dma_buf_format_copy          (WPEBufferDMABufFormat     *format);
WPE_API void                       wpe_buffer_dma_buf_format_free          (WPEBufferDMABufFormat     *format);
WPE_API WPEBufferDMABufFormatUsage wpe_buffer_dma_buf_format_get_usage     (WPEBufferDMABufFormat     *format);
WPE_API guint32                    wpe_buffer_dma_buf_format_get_fourcc    (WPEBufferDMABufFormat     *format);
WPE_API GArray                    *wpe_buffer_dma_buf_format_get_modifiers (WPEBufferDMABufFormat     *format);

G_END_DECLS

#endif /* WPEBuffer_h */
