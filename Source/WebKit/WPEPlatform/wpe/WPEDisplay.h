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

#ifndef WPEDisplay_h
#define WPEDisplay_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEBufferDMABufFormats.h>
#include <wpe/WPEDefines.h>
#include <wpe/WPEInputMethodContext.h>
#include <wpe/WPEKeymap.h>
#include <wpe/WPEScreen.h>
#include <wpe/WPEView.h>

G_BEGIN_DECLS

#define WPE_DISPLAY_EXTENSION_POINT_NAME "wpe-platform-display"

#define WPE_TYPE_DISPLAY (wpe_display_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEDisplay, wpe_display, WPE, DISPLAY, GObject)

struct _WPEDisplayClass
{
    GObjectClass parent_class;

    gboolean                (* connect)                       (WPEDisplay *display,
                                                               GError    **error);
    WPEView                *(* create_view)                   (WPEDisplay *display);
    gpointer                (* get_egl_display)               (WPEDisplay *display,
                                                               GError    **error);
    WPEKeymap              *(* get_keymap)                    (WPEDisplay *display,
                                                               GError    **error);
    WPEBufferDMABufFormats *(* get_preferred_dma_buf_formats) (WPEDisplay *display);
    guint                   (* get_n_screens)                 (WPEDisplay *display);
    WPEScreen              *(* get_screen)                    (WPEDisplay *display,
                                                               guint       index);
    const char             *(* get_drm_device)                (WPEDisplay *display);
    const char             *(* get_drm_render_node)           (WPEDisplay *display);
    gboolean                (* use_explicit_sync)             (WPEDisplay *display);

    WPEInputMethodContext   *(* create_input_method_context)    (WPEDisplay *display);

    gpointer padding[32];
};

#define WPE_DISPLAY_ERROR (wpe_display_error_quark())

/**
 * WPEDisplayError:
 * @WPE_DISPLAY_ERROR_NOT_SUPPORTED: Operation not supported
 * @WPE_DISPLAY_ERROR_CONNECTION_FAILED: Failed to connect to the native system
 *
 * #WPEDisplay errors
 */
typedef enum {
    WPE_DISPLAY_ERROR_NOT_SUPPORTED,
    WPE_DISPLAY_ERROR_CONNECTION_FAILED
} WPEDisplayError;

WPE_API GQuark                  wpe_display_error_quark                   (void);
WPE_API WPEDisplay             *wpe_display_get_default                   (void);
WPE_API WPEDisplay             *wpe_display_get_primary                   (void);
WPE_API void                    wpe_display_set_primary                   (WPEDisplay *display);
WPE_API gboolean                wpe_display_connect                       (WPEDisplay *display,
                                                                           GError    **error);
WPE_API gpointer                wpe_display_get_egl_display               (WPEDisplay *display,
                                                                           GError    **error);
WPE_API WPEKeymap              *wpe_display_get_keymap                    (WPEDisplay *display,
                                                                           GError    **error);
WPE_API WPEBufferDMABufFormats *wpe_display_get_preferred_dma_buf_formats (WPEDisplay *display);
WPE_API guint                   wpe_display_get_n_screens                 (WPEDisplay *display);
WPE_API WPEScreen              *wpe_display_get_screen                    (WPEDisplay *display,
                                                                           guint       index);
WPE_API void                    wpe_display_screen_added                  (WPEDisplay *display,
                                                                           WPEScreen *screen);
WPE_API void                    wpe_display_screen_removed                (WPEDisplay *display,
                                                                           WPEScreen *screen);
WPE_API const char             *wpe_display_get_drm_device                (WPEDisplay *display);
WPE_API const char             *wpe_display_get_drm_render_node           (WPEDisplay *display);
WPE_API gboolean                wpe_display_use_explicit_sync             (WPEDisplay *display);

G_END_DECLS

#endif /* WPEDisplay_h */
