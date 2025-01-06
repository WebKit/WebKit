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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FenceMonitor.h"

#if PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
#include <glib-unix.h>

#if PLATFORM(GTK)
#include <gtk/gtk.h>
#endif

namespace WebKit {

FenceMonitor::FenceMonitor(Function<void()>&& callback)
    : m_callback(WTFMove(callback))
{
}

FenceMonitor::~FenceMonitor()
{
    if (m_source)
        g_source_destroy(m_source.get());
}

struct FenceSource {
    static GSourceFuncs sourceFuncs;

    GSource base;
    gpointer tag;
};

GSourceFuncs FenceSource::sourceFuncs = {
    nullptr, // prepare
    nullptr, // check
    // dispatch
    [](GSource* base, GSourceFunc callback, gpointer userData) -> gboolean
    {
        auto& source = *reinterpret_cast<FenceSource*>(base);
        g_source_remove_unix_fd(&source.base, source.tag);
        source.tag = nullptr;

        callback(userData);
        return G_SOURCE_CONTINUE;
    },
    nullptr, // finalize
    nullptr, // closure_callback
    nullptr, // closure_marshall
};

void FenceMonitor::ensureSource()
{
    if (LIKELY(m_source))
        return;

    m_source = adoptGRef(g_source_new(&FenceSource::sourceFuncs, sizeof(FenceSource)));
    g_source_set_name(m_source.get(), "[WebKit] Fence monitor");
#if PLATFORM(GTK)
    g_source_set_priority(m_source.get(), GDK_PRIORITY_REDRAW - 1);
#endif
    g_source_set_callback(m_source.get(), [](gpointer userData) -> gboolean {
        auto& monitor = *static_cast<FenceMonitor*>(userData);
        monitor.m_fd = { };
        monitor.m_callback();
        return G_SOURCE_CONTINUE;
    }, this, nullptr);
    g_source_attach(m_source.get(), g_main_context_get_thread_default());
}

static bool isFileDescriptorReadable(int fd)
{
    GPollFD pollFD = { fd, G_IO_IN, 0 };
    if (!g_poll(&pollFD, 1, 0))
        return FALSE;

    return pollFD.revents & (G_IO_IN | G_IO_NVAL);
}

void FenceMonitor::addFileDescriptor(UnixFileDescriptor&& fd)
{
    RELEASE_ASSERT(!m_fd);

    if (!fd || isFileDescriptorReadable(fd.value())) {
        m_callback();
        return;
    }

    m_fd = WTFMove(fd);

    ensureSource();
    auto& source = *reinterpret_cast<FenceSource*>(m_source.get());
    if (source.tag)
        g_source_remove_unix_fd(&source.base, source.tag);
    source.tag = g_source_add_unix_fd(&source.base, m_fd.value(), G_IO_IN);
}

} // namespace WebKit

#endif // PLATFORM(GTK) || (PLATFORM(WPE) && ENABLE(WPE_PLATFORM))
