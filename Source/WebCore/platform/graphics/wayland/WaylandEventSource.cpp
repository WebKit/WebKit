/*
 * Copyright (C) 2014 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WaylandEventSource.h"

#if PLATFORM(WAYLAND)

namespace WebCore {

class GLibSource {
public:
    void initialize(struct wl_display*);

    gboolean check();
    gboolean dispatch();

private:
    GSource m_source;
    GPollFD m_pollFileDescriptor;
    struct wl_display* m_display;
};

void GLibSource::initialize(struct wl_display* display)
{
    m_display = display;
    struct wl_event_loop* loop = wl_display_get_event_loop(display);
    m_pollFileDescriptor.fd = wl_event_loop_get_fd(loop);
    m_pollFileDescriptor.events = G_IO_IN | G_IO_ERR | G_IO_HUP;
    g_source_add_poll(&m_source, &m_pollFileDescriptor);

    g_source_set_name(&m_source, "Nested Wayland compositor display event source");
    g_source_set_priority(&m_source, G_PRIORITY_DEFAULT);
    g_source_set_can_recurse(&m_source, TRUE);
    g_source_attach(&m_source, nullptr);
}

gboolean GLibSource::check()
{
    return m_pollFileDescriptor.revents;
}

gboolean GLibSource::dispatch()
{
    if (m_pollFileDescriptor.revents & G_IO_IN) {
        struct wl_event_loop* loop = wl_display_get_event_loop(m_display);
        wl_event_loop_dispatch(loop, -1);
        wl_display_flush_clients(m_display);
        m_pollFileDescriptor.revents = 0;
    }

    if (m_pollFileDescriptor.revents & (G_IO_ERR | G_IO_HUP))
        g_warning("Wayland Display Event Source: lost connection to nested Wayland compositor");

    return TRUE;
}

static gboolean prepareCallback(GSource*, gint* timeout)
{
    *timeout = -1;
    return FALSE;
}

static gboolean checkCallback(GSource* base)
{
    auto source = reinterpret_cast<GLibSource*>(base);
    return source->check();
}

static gboolean dispatchCallback(GSource* base, GSourceFunc, gpointer)
{
    auto source = reinterpret_cast<GLibSource*>(base);
    return source->dispatch();
}

static GSourceFuncs waylandGLibSourceFuncs = {
    prepareCallback,
    checkCallback,
    dispatchCallback,
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

GSource* WaylandEventSource::createDisplayEventSource(struct wl_display* display)
{
    GSource* base = g_source_new(&waylandGLibSourceFuncs, sizeof(GLibSource));

    auto source = reinterpret_cast<GLibSource*>(base);
    source->initialize(display);

    return base;
}

} // namespace WebCore

#endif // PLATFORM(WAYLAND)
