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

#include "config.h"
#include "WPEWaylandCursor.h"

#include "WPEDisplayWaylandPrivate.h"
#include "WPEWaylandCursorTheme.h"

namespace WPE {

WaylandCursor::WaylandCursor(WPEDisplayWayland* display)
    : m_display(display)
    , m_surface(wl_compositor_create_surface(wpe_display_wayland_get_wl_compositor(m_display)))
    , m_theme(WaylandCursorTheme::create(wpe_display_wayland_get_wl_shm(m_display)))
{
}

WaylandCursor::~WaylandCursor()
{
    if (m_surface)
        wl_surface_destroy(m_surface);
}

void WaylandCursor::setFromName(const char* name, double scale)
{
    if (!m_theme)
        return;

    if (!g_strcmp0(m_name.get(), name))
        return;

    m_name.reset(g_strdup(name));
    if (!g_strcmp0(m_name.get(), "none")) {
        update();
        wl_surface_attach(m_surface, nullptr, 0, 0);
        wl_surface_commit(m_surface);
        return;
    }

    // FIXME: support animated cursors.
    const auto& cursor = m_theme->cursor(name, scale, 1);
    if (cursor.isEmpty()) {
        g_warning("Cursor %s not found in theme", name);
        return;
    }

    m_hotspot.x = cursor[0].hotspotX;
    m_hotspot.y = cursor[0].hotspotY;
    update();

    wl_surface_attach(m_surface, cursor[0].buffer, 0, 0);
    wl_surface_damage(m_surface, 0, 0, cursor[0].width, cursor[0].height);
    wl_surface_commit(m_surface);
}

void WaylandCursor::setFromBuffer(struct wl_buffer* buffer, uint32_t width, uint32_t height, uint32_t hotspotX, uint32_t hotspotY)
{
    m_name = nullptr;
    m_hotspot.x = hotspotX;
    m_hotspot.y = hotspotY;
    update();

    wl_surface_attach(m_surface, buffer, 0, 0);
    wl_surface_damage(m_surface, 0, 0, width, height);
    wl_surface_commit(m_surface);
}

void WaylandCursor::update() const
{
    if (auto* seat = wpeDisplayWaylandGetSeat(m_display))
        seat->setCursor(m_surface, m_hotspot.x, m_hotspot.y);
}

} // namespace WPE
