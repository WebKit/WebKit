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

#ifndef WPEToplevel_h
#define WPEToplevel_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

#define WPE_TYPE_TOPLEVEL (wpe_toplevel_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEToplevel, wpe_toplevel, WPE, TOPLEVEL, GObject)

typedef struct _WPEBufferFormats WPEBufferFormats;
typedef struct _WPEDisplay WPEDisplay;
typedef struct _WPEScreen WPEScreen;
typedef struct _WPEView WPEView;

struct _WPEToplevelClass
{
    GObjectClass parent_class;

    void              (* set_title)                    (WPEToplevel *toplevel,
                                                        const char  *title);
    WPEScreen        *(* get_screen)                   (WPEToplevel *toplevel);
    gboolean          (* resize)                       (WPEToplevel *toplevel,
                                                        int          width,
                                                        int          height);
    gboolean          (* set_fullscreen)               (WPEToplevel *toplevel,
                                                        gboolean     fullscreen);
    gboolean          (* set_maximized)                (WPEToplevel *toplevel,
                                                        gboolean     maximized);
    gboolean          (* set_minimized)                (WPEToplevel *toplevel);
    WPEBufferFormats *(* get_preferred_buffer_formats) (WPEToplevel *toplevel);

    gpointer padding[32];
};

/**
 * WPEToplevelState:
 * @WPE_TOPLEVEL_STATE_NONE: the toplevel is in normal state
 * @WPE_TOPLEVEL_STATE_FULLSCREEN: the toplevel is fullscreen
 * @WPE_TOPLEVEL_STATE_MAXIMIZED: the toplevel is maximized
 * @WPE_TOPLEVEL_STATE_ACTIVE: the toplevel is active
 *
 * The current state of a #WPEToplevel.
 */
typedef enum {
    WPE_TOPLEVEL_STATE_NONE       = 0,
    WPE_TOPLEVEL_STATE_FULLSCREEN = (1 << 0),
    WPE_TOPLEVEL_STATE_MAXIMIZED  = (1 << 1),
    WPE_TOPLEVEL_STATE_ACTIVE     = (1 << 2)
} WPEToplevelState;

/**
 * WPEToplevelForeachViewFunc:
 * @toplevel: a #WPEToplevel
 * @view: a #WPEView
 * @user_data: (closure): user data
 *
 * A function used by wpe_toplevel_foreach_view().
 *
 * Returns: %TRUE to stop the iteration, or %FALSE to continue
 */
typedef gboolean (* WPEToplevelForeachViewFunc) (WPEToplevel *toplevel,
                                                 WPEView     *view,
                                                 gpointer     user_data);

WPE_API GList            *wpe_toplevel_list                             (void);
WPE_API WPEDisplay       *wpe_toplevel_get_display                      (WPEToplevel               *toplevel);
WPE_API void              wpe_toplevel_set_title                        (WPEToplevel               *toplevel,
                                                                         const char                *title);
WPE_API guint             wpe_toplevel_get_max_views                    (WPEToplevel               *toplevel);
WPE_API guint             wpe_toplevel_get_n_views                      (WPEToplevel               *toplevel);
WPE_API void              wpe_toplevel_foreach_view                     (WPEToplevel               *toplevel,
                                                                         WPEToplevelForeachViewFunc func,
                                                                         gpointer                   user_data);
WPE_API void              wpe_toplevel_closed                           (WPEToplevel               *toplevel);
WPE_API void              wpe_toplevel_get_size                         (WPEToplevel               *toplevel,
                                                                         int                       *width,
                                                                         int                       *height);
WPE_API gboolean          wpe_toplevel_resize                           (WPEToplevel               *toplevel,
                                                                         int                        width,
                                                                         int                        height);
WPE_API void              wpe_toplevel_resized                          (WPEToplevel               *toplevel,
                                                                         int                        width,
                                                                         int                        height);
WPE_API WPEToplevelState  wpe_toplevel_get_state                        (WPEToplevel               *toplevel);
WPE_API void              wpe_toplevel_state_changed                    (WPEToplevel               *toplevel,
                                                                         WPEToplevelState           state);
WPE_API gdouble           wpe_toplevel_get_scale                        (WPEToplevel               *toplevel);
WPE_API void              wpe_toplevel_scale_changed                    (WPEToplevel               *toplevel,
                                                                         gdouble                    scale);
WPE_API WPEScreen        *wpe_toplevel_get_screen                       (WPEToplevel               *toplevel);
WPE_API void              wpe_toplevel_screen_changed                   (WPEToplevel               *toplevel);
WPE_API gboolean          wpe_toplevel_fullscreen                       (WPEToplevel               *toplevel);
WPE_API gboolean          wpe_toplevel_unfullscreen                     (WPEToplevel               *toplevel);
WPE_API gboolean          wpe_toplevel_maximize                         (WPEToplevel               *toplevel);
WPE_API gboolean          wpe_toplevel_unmaximize                       (WPEToplevel               *toplevel);
WPE_API gboolean          wpe_toplevel_minimize                         (WPEToplevel               *toplevel);
WPE_API WPEBufferFormats *wpe_toplevel_get_preferred_buffer_formats     (WPEToplevel               *toplevel);
WPE_API void              wpe_toplevel_preferred_buffer_formats_changed (WPEToplevel               *toplevel);

G_END_DECLS

#endif /* WPEToplevel_h */
