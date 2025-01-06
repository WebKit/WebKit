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

#ifndef WPEDisplayDRM_h
#define WPEDisplayDRM_h

#if !defined(__WPE_DRM_H_INSIDE__) && !defined(BUILDING_WEBKIT)
#error "Only <wpe/drm/wpe-drm.h> can be included directly."
#endif

#include <gbm.h>
#include <glib-object.h>
#include <wpe/wpe-platform.h>
#include <xf86drmMode.h>

G_BEGIN_DECLS

#define WPE_TYPE_DISPLAY_DRM (wpe_display_drm_get_type())
WPE_API G_DECLARE_FINAL_TYPE (WPEDisplayDRM, wpe_display_drm, WPE, DISPLAY_DRM, WPEDisplay)

WPE_API WPEDisplay         *wpe_display_drm_new                (void);
WPE_API gboolean            wpe_display_drm_connect            (WPEDisplayDRM *display,
                                                                const char    *name,
                                                                GError       **error);
WPE_API struct gbm_device  *wpe_display_drm_get_device         (WPEDisplayDRM *display);
WPE_API gboolean            wpe_display_drm_supports_atomic    (WPEDisplayDRM *display);
WPE_API gboolean            wpe_display_drm_supports_modifiers (WPEDisplayDRM *display);

G_END_DECLS

#endif /* WPEDisplayDRM_h */
