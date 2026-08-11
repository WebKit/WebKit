/*
 * Copyright (C) 2026 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FilePathWatcher.h"

#include <wtf/TZoneMallocInlines.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/WTFString.h>

namespace WTF {

WTF_MAKE_TZONE_ALLOCATED_IMPL(FilePathWatcher);

FilePathWatcher::FilePathWatcher(const String& path, Function<void()>&& handler)
    : m_handler(WTF::move(handler))
{
    if (path.isEmpty() || !m_handler)
        return;

    auto pathUtf8 = path.utf8();
    GRefPtr file = adoptGRef(g_file_new_for_path(pathUtf8.data()));
    GUniqueOutPtr<GError> error;
    m_monitor = adoptGRef(g_file_monitor_file(file.get(), G_FILE_MONITOR_WATCH_HARD_LINKS, nullptr, &error.outPtr()));
    if (!m_monitor) {
        LOG_ERROR("FilePathWatcher: failed to monitor %s: %s", pathUtf8.data(), error.get() ? error->message : "unknown error");
        return;
    }
    // GFileMonitor coalesces events within a rate-limit window that defaults to
    // 800 ms; the local (inotify) backend honors this property. Tighten it so a
    // rare change is reported promptly instead of after the default delay.
    g_file_monitor_set_rate_limit(m_monitor.get(), 50);
    m_changedHandlerID = g_signal_connect(m_monitor.get(), "changed", G_CALLBACK(fileChangedCallback), this);
}

FilePathWatcher::~FilePathWatcher()
{
    if (!m_monitor)
        return;
    if (m_changedHandlerID)
        g_signal_handler_disconnect(m_monitor.get(), m_changedHandlerID);
    g_file_monitor_cancel(m_monitor.get());
}

void FilePathWatcher::fileChangedCallback(GFileMonitor*, GFile*, GFile*, GFileMonitorEvent event, FilePathWatcher* watcher)
{
    // Without G_FILE_MONITOR_WATCH_MOVES, GIO terminates every observable mutation
    // (rename, symlink swap, in-place write) with CREATED or CHANGES_DONE_HINT —
    // a bare DELETED is never the tail of an atomic update, so suppressing it
    // avoids reacting to the transient mid-rename state.
    switch (event) {
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED:
        watcher->m_handler();
        break;
    default:
        break;
    }
}

} // namespace WTF
