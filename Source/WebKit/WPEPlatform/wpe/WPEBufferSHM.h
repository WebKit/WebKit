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

#ifndef WPEBufferSHM_h
#define WPEBufferSHM_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>
#include <wpe/WPEBuffer.h>

G_BEGIN_DECLS

#define WPE_TYPE_BUFFER_SHM (wpe_buffer_shm_get_type())
WPE_API G_DECLARE_FINAL_TYPE (WPEBufferSHM, wpe_buffer_shm, WPE, BUFFER_SHM, WPEBuffer)

/**
 * WPEPixelFormat:
 * @WPE_PIXEL_FORMAT_ARGB8888: 32-bit ARGB format
 *
 * Enum with the supported pixel formats for memory buffers.
 */
typedef enum {
    WPE_PIXEL_FORMAT_ARGB8888
} WPEPixelFormat;

WPE_API WPEBufferSHM  *wpe_buffer_shm_new        (WPEDisplay    *display,
                                                  int            width,
                                                  int            height,
                                                  WPEPixelFormat format,
                                                  GBytes        *data,
                                                  guint          stride);
WPE_API WPEPixelFormat wpe_buffer_shm_get_format (WPEBufferSHM  *buffer);
WPE_API GBytes        *wpe_buffer_shm_get_data   (WPEBufferSHM  *buffer);
WPE_API guint          wpe_buffer_shm_get_stride (WPEBufferSHM  *buffer);

G_END_DECLS

#endif /* WPEBuffer_h */
