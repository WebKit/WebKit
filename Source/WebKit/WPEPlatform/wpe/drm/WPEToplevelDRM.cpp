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

#include "config.h"
#include "WPEToplevelDRM.h"

#include "WPEDisplayDRMPrivate.h"
#include "WPEScreenDRMPrivate.h"
#include <wtf/glib/WTFGType.h>

/**
 * WPEToplevelDRM:
 *
 */
struct _WPEToplevelDRMPrivate {
};
WEBKIT_DEFINE_FINAL_TYPE(WPEToplevelDRM, wpe_toplevel_drm, WPE_TYPE_TOPLEVEL, WPEToplevel)

static void wpeToplevelDRMConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_toplevel_drm_parent_class)->constructed(object);

    auto* toplevel = WPE_TOPLEVEL(object);
    auto* display = WPE_DISPLAY_DRM(wpe_toplevel_get_display(toplevel));
    auto* screen = wpeDisplayDRMGetScreen(display);
    auto* mode = wpeScreenDRMGetMode(WPE_SCREEN_DRM(screen));
    double scale = wpe_screen_get_scale(screen);
    wpe_toplevel_resized(toplevel, mode->hdisplay / scale, mode->vdisplay / scale);
    wpe_toplevel_scale_changed(toplevel, scale);
    wpe_toplevel_state_changed(toplevel, static_cast<WPEToplevelState>(WPE_TOPLEVEL_STATE_FULLSCREEN | WPE_TOPLEVEL_STATE_ACTIVE));
}

static WPEScreen* wpeToplevelDRMGetScreen(WPEToplevel* toplevel)
{
    if (auto* display = wpe_toplevel_get_display(toplevel))
        return wpeDisplayDRMGetScreen(WPE_DISPLAY_DRM(display));
    return nullptr;
}

static void wpe_toplevel_drm_class_init(WPEToplevelDRMClass* toplevelDRMClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(toplevelDRMClass);
    objectClass->constructed = wpeToplevelDRMConstructed;

    WPEToplevelClass* toplevelClass = WPE_TOPLEVEL_CLASS(toplevelDRMClass);
    toplevelClass->get_screen = wpeToplevelDRMGetScreen;
}

/**
 * wpe_toplevel_drm_new:
 * @display: a #WPEDisplayDRM
 *
 * Create a new #WPEToplevel on @display.
 *
 * Returns: (transfer full): a #WPEToplevel
 */
WPEToplevel* wpe_toplevel_drm_new(WPEDisplayDRM* display)
{
    return WPE_TOPLEVEL(g_object_new(WPE_TYPE_TOPLEVEL_DRM, "display", display, nullptr));
}
