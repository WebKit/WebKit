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
#include "WPEToplevelHeadless.h"

#include <wtf/glib/WTFGType.h>

/**
 * WPEToplevelHeadless:
 *
 */
struct _WPEToplevelHeadlessPrivate {
};
WEBKIT_DEFINE_FINAL_TYPE(WPEToplevelHeadless, wpe_toplevel_headless, WPE_TYPE_TOPLEVEL, WPEToplevel)

static void wpeToplevelHeadlessConstructed(GObject* object)
{
    G_OBJECT_CLASS(wpe_toplevel_headless_parent_class)->constructed(object);

    wpe_toplevel_state_changed(WPE_TOPLEVEL(object), WPE_TOPLEVEL_STATE_ACTIVE);
}

static gboolean wpeToplevelHeadlessResize(WPEToplevel* toplevel, int width, int height)
{
    wpe_toplevel_resized(toplevel, width, height);
    wpe_toplevel_foreach_view(toplevel, [](WPEToplevel* toplevel, WPEView* view, gpointer) -> gboolean {
        int width, height;
        wpe_toplevel_get_size(toplevel, &width, &height);
        wpe_view_resized(view, width, height);
        return FALSE;
    }, nullptr);
    return TRUE;
}

static gboolean wpeToplevelHeadlessSetFullscreen(WPEToplevel* toplevel, gboolean fullscreen)
{
    auto state = wpe_toplevel_get_state(toplevel);
    if (fullscreen)
        state = static_cast<WPEToplevelState>(state | WPE_TOPLEVEL_STATE_FULLSCREEN);
    else
        state = static_cast<WPEToplevelState>(state & ~WPE_TOPLEVEL_STATE_FULLSCREEN);
    wpe_toplevel_state_changed(toplevel, state);
    return TRUE;
}

static void wpe_toplevel_headless_class_init(WPEToplevelHeadlessClass* toplevelHeadlessClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(toplevelHeadlessClass);
    objectClass->constructed = wpeToplevelHeadlessConstructed;

    WPEToplevelClass* toplevelClass = WPE_TOPLEVEL_CLASS(toplevelHeadlessClass);
    toplevelClass->resize = wpeToplevelHeadlessResize;
    toplevelClass->set_fullscreen = wpeToplevelHeadlessSetFullscreen;
}

/**
 * wpe_toplevel_headless_new:
 * @display: a #WPEDisplayHeadless
 *
 * Create a new #WPEToplevel on @display.
 *
 * Returns: (transfer full): a #WPEToplevel
 */
WPEToplevel* wpe_toplevel_headless_new(WPEDisplayHeadless* display)
{
    return WPE_TOPLEVEL(g_object_new(WPE_TYPE_TOPLEVEL_HEADLESS, "display", display, nullptr));
}
