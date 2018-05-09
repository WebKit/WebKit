/*
 * Copyright (C) 2015 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GSocketMonitor.h"

#include <gio/gio.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace IPC {

GSocketMonitor::~GSocketMonitor()
{
    stop();
}

gboolean GSocketMonitor::socketSourceCallback(GSocket*, GIOCondition condition, GSocketMonitor* monitor)
{
    if (g_cancellable_is_cancelled(monitor->m_cancellable.get()))
        return G_SOURCE_REMOVE;
    return monitor->m_callback(condition);
}

void GSocketMonitor::start(GSocket* socket, GIOCondition condition, RunLoop& runLoop, Function<gboolean (GIOCondition)>&& callback)
{
    stop();

    m_cancellable = adoptGRef(g_cancellable_new());
    m_source = adoptGRef(g_socket_create_source(socket, condition, m_cancellable.get()));
    g_source_set_name(m_source.get(), "[WebKit] Socket monitor");
    m_callback = WTFMove(callback);
    g_source_set_callback(m_source.get(), reinterpret_cast<GSourceFunc>(reinterpret_cast<GCallback>(socketSourceCallback)), this, nullptr);
    g_source_set_priority(m_source.get(), RunLoopSourcePriority::RunLoopDispatcher);
    g_source_attach(m_source.get(), runLoop.mainContext());
}

void GSocketMonitor::stop()
{
    if (!m_source)
        return;

    g_cancellable_cancel(m_cancellable.get());
    m_cancellable = nullptr;
    g_source_destroy(m_source.get());
    m_source = nullptr;
    m_callback = nullptr;
}

} // namespace IPC
