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

#ifndef WPEScreen_h
#define WPEScreen_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

#define WPE_TYPE_SCREEN (wpe_screen_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEScreen, wpe_screen, WPE, SCREEN, GObject)

struct _WPEScreenClass
{
    GObjectClass parent_class;

    void (* invalidate) (WPEScreen *screen);

    gpointer padding[32];
};

WPE_API guint32 wpe_screen_get_id               (WPEScreen *screen);
WPE_API void    wpe_screen_invalidate           (WPEScreen *screen);
WPE_API int     wpe_screen_get_x                (WPEScreen *screen);
WPE_API int     wpe_screen_get_y                (WPEScreen *screen);
WPE_API void    wpe_screen_set_position         (WPEScreen *screen,
                                                 int         x,
                                                 int         y);
WPE_API int     wpe_screen_get_width            (WPEScreen *screen);
WPE_API int     wpe_screen_get_height           (WPEScreen *screen);
WPE_API void    wpe_screen_set_size             (WPEScreen *screen,
                                                 int         width,
                                                 int         height);
WPE_API int     wpe_screen_get_physical_width   (WPEScreen *screen);
WPE_API int     wpe_screen_get_physical_height  (WPEScreen *screen);
WPE_API void    wpe_screen_set_physical_size    (WPEScreen *screen,
                                                 int         width,
                                                 int         height);
WPE_API gdouble wpe_screen_get_scale            (WPEScreen *screen);
WPE_API void    wpe_screen_set_scale            (WPEScreen *screen,
                                                 gdouble     scale);
WPE_API int     wpe_screen_get_refresh_rate     (WPEScreen *screen);
WPE_API void    wpe_screen_set_refresh_rate     (WPEScreen *screen,
                                                 int         refresh_rate);

G_END_DECLS

#endif /* WPEScreen_h */
