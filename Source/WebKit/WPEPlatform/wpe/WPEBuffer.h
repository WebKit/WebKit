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

#ifndef WPEBuffer_h
#define WPEBuffer_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>
#include <wpe/WPEDisplay.h>

G_BEGIN_DECLS

#define WPE_TYPE_BUFFER (wpe_buffer_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEBuffer, wpe_buffer, WPE, BUFFER, GObject)

struct _WPEBufferClass
{
    GObjectClass parent_class;

    gpointer (* import_to_egl_image) (WPEBuffer *buffer,
                                      GError   **error);
    GBytes  *(* import_to_pixels)    (WPEBuffer *buffer,
                                      GError   **error);

    gpointer padding[32];
};

#define WPE_BUFFER_ERROR (wpe_buffer_error_quark())

/**
 * WPEBufferError:
 * @WPE_BUFFER_ERROR_NOT_SUPPORTED: Operation not supported
 * @WPE_BUFFER_ERROR_IMPORT_FAILED: Import buffer operation failed
 *
 * #WPEBuffer errors
 */
typedef enum {
    WPE_BUFFER_ERROR_NOT_SUPPORTED,
    WPE_BUFFER_ERROR_IMPORT_FAILED
} WPEBufferError;

WPE_API GQuark      wpe_buffer_error_quark          (void);
WPE_API WPEDisplay *wpe_buffer_get_display          (WPEBuffer     *buffer);
WPE_API int         wpe_buffer_get_width            (WPEBuffer     *buffer);
WPE_API int         wpe_buffer_get_height           (WPEBuffer     *buffer);
WPE_API void        wpe_buffer_set_user_data        (WPEBuffer     *buffer,
                                                     gpointer       user_data,
                                                     GDestroyNotify destroy_func);
WPE_API gpointer    wpe_buffer_get_user_data        (WPEBuffer     *buffer);
WPE_API gpointer    wpe_buffer_import_to_egl_image  (WPEBuffer     *buffer,
                                                     GError       **error);
WPE_API GBytes     *wpe_buffer_import_to_pixels     (WPEBuffer     *buffer,
                                                     GError       **error);
WPE_API void        wpe_buffer_set_rendering_fence  (WPEBuffer     *buffer,
                                                     int            fd);
WPE_API int         wpe_buffer_get_rendering_fence  (WPEBuffer     *buffer);
WPE_API int         wpe_buffer_take_rendering_fence (WPEBuffer     *buffer);
WPE_API void        wpe_buffer_set_release_fence    (WPEBuffer     *buffer,
                                                     int            fd);
WPE_API int         wpe_buffer_get_release_fence    (WPEBuffer     *buffer);
WPE_API int         wpe_buffer_take_release_fence   (WPEBuffer     *buffer);

G_END_DECLS

#endif /* WPEBuffer_h */
