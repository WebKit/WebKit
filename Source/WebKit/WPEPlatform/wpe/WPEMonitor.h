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

#ifndef WPEMonitor_h
#define WPEMonitor_h

#if !defined(__WPE_PLATFORM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/wpe-platform.h> can be included directly."
#endif

#include <glib-object.h>
#include <wpe/WPEDefines.h>

G_BEGIN_DECLS

#define WPE_TYPE_MONITOR (wpe_monitor_get_type())
WPE_DECLARE_DERIVABLE_TYPE (WPEMonitor, wpe_monitor, WPE, MONITOR, GObject)

struct _WPEMonitorClass
{
    GObjectClass parent_class;

    void (* invalidate) (WPEMonitor *monitor);

    gpointer padding[32];
};

WPE_API guint32 wpe_monitor_get_id              (WPEMonitor *monitor);
WPE_API void    wpe_monitor_invalidate          (WPEMonitor *monitor);
WPE_API int     wpe_monitor_get_x               (WPEMonitor *monitor);
WPE_API int     wpe_monitor_get_y               (WPEMonitor *monitor);
WPE_API void    wpe_monitor_set_position        (WPEMonitor *monitor,
                                                 int         x,
                                                 int         y);
WPE_API int     wpe_monitor_get_width           (WPEMonitor *monitor);
WPE_API int     wpe_monitor_get_height          (WPEMonitor *monitor);
WPE_API void    wpe_monitor_set_size            (WPEMonitor *monitor,
                                                 int         width,
                                                 int         height);
WPE_API int     wpe_monitor_get_physical_width  (WPEMonitor *monitor);
WPE_API int     wpe_monitor_get_physical_height (WPEMonitor *monitor);
WPE_API void    wpe_monitor_set_physical_size   (WPEMonitor *monitor,
                                                 int         width,
                                                 int         height);
WPE_API gdouble wpe_monitor_get_scale           (WPEMonitor *monitor);
WPE_API void    wpe_monitor_set_scale           (WPEMonitor *monitor,
                                                 gdouble     scale);
WPE_API int     wpe_monitor_get_refresh_rate    (WPEMonitor *monitor);
WPE_API void    wpe_monitor_set_refresh_rate    (WPEMonitor *monitor,
                                                 int         refresh_rate);

G_END_DECLS

#endif /* WPEMonitor_h */
