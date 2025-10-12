/*
 * Copyright (C) 2025 Igalia S.L.
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
#include "WPEToplevelMock.h"

#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/WTFGType.h>

struct _WPEToplevelMockPrivate {
    unsigned currentScreen;
    int savedWidth;
    int savedHeight;
    bool isActive;
    bool isFullscreen;
    bool isMaximized;
};
WEBKIT_DEFINE_FINAL_TYPE(WPEToplevelMock, wpe_toplevel_mock, WPE_TYPE_TOPLEVEL, WPEToplevel)

static void wpeToplevelMockUpdateState(WPEToplevelMock* toplevel)
{
    unsigned state = WPE_TOPLEVEL_STATE_NONE;
    if (toplevel->priv->isActive)
        state |= WPE_TOPLEVEL_STATE_ACTIVE;
    if (toplevel->priv->isFullscreen)
        state |= WPE_TOPLEVEL_STATE_FULLSCREEN;
    if (toplevel->priv->isMaximized)
        state |= WPE_TOPLEVEL_STATE_MAXIMIZED;
    wpe_toplevel_state_changed(WPE_TOPLEVEL(toplevel), static_cast<WPEToplevelState>(state));
}

static void wpeToplevelMockSetTitle(WPEToplevel*, const char*)
{
}

static WPEScreen* wpeToplevelMockGetScreen(WPEToplevel* toplevel)
{
    auto* priv = WPE_TOPLEVEL_MOCK(toplevel)->priv;
    if (auto* display = wpe_toplevel_get_display(toplevel))
        return wpe_display_get_screen(display, priv->currentScreen);
    return nullptr;
}

static gboolean wpeToplevelMockResize(WPEToplevel* toplevel, int width, int height)
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

static gboolean wpeToplevelMockSetFullscreen(WPEToplevel* toplevel, gboolean fullscreen)
{
    auto* mockToplevel = WPE_TOPLEVEL_MOCK(toplevel);
    auto* priv = mockToplevel->priv;
    if (fullscreen) {
        if (!priv->isFullscreen && !priv->isMaximized)
            wpe_toplevel_get_size(toplevel, &priv->savedWidth, &priv->savedHeight);
        priv->isFullscreen = true;
        wpe_toplevel_resize(toplevel, 1920, 1080);
    } else {
        priv->isFullscreen = false;
        if (!priv->isMaximized) {
            wpe_toplevel_resize(toplevel, priv->savedWidth, priv->savedHeight);
            priv->savedWidth = 0;
            priv->savedHeight = 0;
        }
    }
    wpeToplevelMockUpdateState(mockToplevel);

    return TRUE;
}

static gboolean wpeToplevelMockSetMaximized(WPEToplevel* toplevel, gboolean maximized)
{
    auto* mockToplevel = WPE_TOPLEVEL_MOCK(toplevel);
    auto* priv = mockToplevel->priv;
    if (maximized) {
        if (!priv->isFullscreen && !priv->isMaximized)
            wpe_toplevel_get_size(toplevel, &priv->savedWidth, &priv->savedHeight);
        priv->isMaximized = true;
        wpe_toplevel_resize(toplevel, 1920, 1040);
    } else {
        priv->isMaximized = false;
        if (!priv->isFullscreen) {
            wpe_toplevel_resize(toplevel, priv->savedWidth, priv->savedHeight);
            priv->savedWidth = 0;
            priv->savedHeight = 0;
        }
    }
    wpeToplevelMockUpdateState(mockToplevel);

    return TRUE;
}

static gboolean wpeToplevelMockSetMinimized(WPEToplevel*)
{
    return FALSE;
}

static WPEBufferDMABufFormats* wpeToplevelMockGetPreferredDMABufFormats(WPEToplevel*)
{
    return nullptr;
}

static void wpe_toplevel_mock_class_init(WPEToplevelMockClass* toplevelMockClass)
{
    WPEToplevelClass* toplevelClass = WPE_TOPLEVEL_CLASS(toplevelMockClass);
    toplevelClass->set_title = wpeToplevelMockSetTitle;
    toplevelClass->get_screen = wpeToplevelMockGetScreen;
    toplevelClass->resize = wpeToplevelMockResize;
    toplevelClass->set_fullscreen = wpeToplevelMockSetFullscreen;
    toplevelClass->set_maximized = wpeToplevelMockSetMaximized;
    toplevelClass->set_minimized = wpeToplevelMockSetMinimized;
    toplevelClass->get_preferred_dma_buf_formats = wpeToplevelMockGetPreferredDMABufFormats;
}

WPEToplevel* wpeToplevelMockNew(WPEDisplayMock* display, guint maxViews)
{
    return WPE_TOPLEVEL(g_object_new(WPE_TYPE_TOPLEVEL_MOCK, "display", display, "max-views", maxViews, nullptr));
}

void wpeToplevelMockSwitchToScreen(WPEToplevelMock* toplevel, guint screen)
{
    toplevel->priv->currentScreen = screen;
    if (auto* screen = wpeToplevelMockGetScreen(WPE_TOPLEVEL(toplevel)))
        wpe_toplevel_scale_changed(WPE_TOPLEVEL(toplevel), wpe_screen_get_scale(screen));
    wpe_toplevel_screen_changed(WPE_TOPLEVEL(toplevel));
}

void wpeToplevelMockSetActive(WPEToplevelMock* toplevel, gboolean active)
{
    toplevel->priv->isActive = !!active;
    wpeToplevelMockUpdateState(toplevel);
}
