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

#ifndef WPEView_h
#define WPEView_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>
#include <wpe/WPEGestureController.h>
#include <wpe/WPEToplevel.h>

G_BEGIN_DECLS

#define WPE_TYPE_VIEW (wpe_view_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEView, wpe_view, WPE, VIEW, GObject)

typedef struct _WPEBuffer WPEBuffer;
typedef struct _WPEBufferDMABufFormats WPEBufferDMABufFormats;
typedef struct _WPEDisplay WPEDisplay;
typedef struct _WPEEvent WPEEvent;
typedef struct _WPEScreen WPEScreen;
typedef struct _WPERectangle WPERectangle;

struct _WPEViewClass
{
    GObjectClass parent_class;

    gboolean (* render_buffer)         (WPEView            *view,
                                        WPEBuffer          *buffer,
                                        const WPERectangle *damage_rects,
                                        guint               n_damage_rects,
                                        GError            **error);
    gboolean (* lock_pointer)          (WPEView            *view);
    gboolean (* unlock_pointer)        (WPEView            *view);
    void     (* set_cursor_from_name)  (WPEView            *view,
                                        const char         *name);
    void     (* set_cursor_from_bytes) (WPEView            *view,
                                        GBytes             *bytes,
                                        guint               width,
                                        guint               height,
                                        guint               stride,
                                        guint               hotspot_x,
                                        guint               hotspot_y);
    void     (* set_opaque_rectangles) (WPEView            *view,
                                        WPERectangle       *rects,
                                        guint               n_rects);
    gboolean (* can_be_mapped)         (WPEView            *view);

    gpointer padding[32];
};

#define WPE_VIEW_ERROR (wpe_view_error_quark())

/**
 * WPEViewError:
 * @WPE_VIEW_ERROR_RENDER_FAILED: Failed to render
 *
 * #WPEView errors
 */
typedef enum {
    WPE_VIEW_ERROR_RENDER_FAILED
} WPEViewError;

WPE_API GQuark                  wpe_view_error_quark                   (void);

WPE_API WPEView                *wpe_view_new                           (WPEDisplay         *display);
WPE_API WPEDisplay             *wpe_view_get_display                   (WPEView            *view);
WPE_API WPEToplevel            *wpe_view_get_toplevel                  (WPEView            *view);
WPE_API void                    wpe_view_set_toplevel                  (WPEView            *view,
                                                                        WPEToplevel        *toplevel);
WPE_API int                     wpe_view_get_width                     (WPEView            *view);
WPE_API int                     wpe_view_get_height                    (WPEView            *view);
WPE_API void                    wpe_view_closed                        (WPEView            *view);
WPE_API void                    wpe_view_resized                       (WPEView            *view,
                                                                        int                 width,
                                                                        int                 height);
WPE_API gdouble                 wpe_view_get_scale                     (WPEView            *view);
WPE_API gboolean                wpe_view_get_visible                   (WPEView            *view);
WPE_API void                    wpe_view_set_visible                   (WPEView            *view,
                                                                        gboolean            visible);
WPE_API gboolean                wpe_view_get_mapped                    (WPEView            *view);
WPE_API void                    wpe_view_map                           (WPEView            *view);
WPE_API void                    wpe_view_unmap                         (WPEView            *view);
WPE_API gboolean                wpe_view_lock_pointer                  (WPEView            *view);
WPE_API gboolean                wpe_view_unlock_pointer                (WPEView            *view);
WPE_API void                    wpe_view_set_cursor_from_name          (WPEView            *view,
                                                                        const char         *name);
WPE_API void                    wpe_view_set_cursor_from_bytes         (WPEView            *view,
                                                                        GBytes             *bytes,
                                                                        guint               width,
                                                                        guint               height,
                                                                        guint               stride,
                                                                        guint               hotspot_x,
                                                                        guint               hotspot_y);
WPE_API WPEToplevelState        wpe_view_get_toplevel_state            (WPEView            *view);
WPE_API WPEScreen              *wpe_view_get_screen                    (WPEView            *view);
WPE_API gboolean                wpe_view_render_buffer                 (WPEView            *view,
                                                                        WPEBuffer          *buffer,
                                                                        const WPERectangle *damage_rects,
                                                                        guint               n_damage_rects,
                                                                        GError            **error);
WPE_API void                    wpe_view_buffer_rendered               (WPEView            *view,
                                                                        WPEBuffer          *buffer);
WPE_API void                    wpe_view_buffer_released               (WPEView            *view,
                                                                        WPEBuffer          *buffer);
WPE_API void                    wpe_view_event                         (WPEView            *view,
                                                                        WPEEvent           *event);
WPE_API guint                   wpe_view_compute_press_count           (WPEView            *view,
                                                                        gdouble             x,
                                                                        gdouble             y,
                                                                        guint               button,
                                                                        guint32             time);
WPE_API void                    wpe_view_focus_in                      (WPEView            *view);
WPE_API void                    wpe_view_focus_out                     (WPEView            *view);
WPE_API gboolean                wpe_view_get_has_focus                 (WPEView            *view);
WPE_API WPEBufferDMABufFormats *wpe_view_get_preferred_dma_buf_formats (WPEView            *view);
WPE_API void                    wpe_view_set_opaque_rectangles         (WPEView            *view,
                                                                        WPERectangle       *rects,
                                                                        guint               n_rects);
WPE_API void                    wpe_view_set_gesture_controller        (WPEView            *view,
                                                                        WPEGestureController *controller);
WPE_API WPEGestureController   *wpe_view_get_gesture_controller        (WPEView            *view);

G_END_DECLS

#endif /* WPEView_h */
