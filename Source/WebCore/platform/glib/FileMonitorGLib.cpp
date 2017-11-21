/*
 * Copyright (C) 2017 Igalia S.L.
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
#include "FileMonitor.h"

#include "FileSystem.h"
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

FileMonitor::FileMonitor(const String& path, Ref<WorkQueue>&& handlerQueue, WTF::Function<void(FileChangeType)>&& modificationHandler)
    : m_handlerQueue(WTFMove(handlerQueue))
    , m_modificationHandler(WTFMove(modificationHandler))
{
    if (path.isEmpty() || !m_modificationHandler)
        return;

    m_cancellable = adoptGRef(g_cancellable_new());
    m_handlerQueue->dispatch([this, cancellable = m_cancellable, path = path.isolatedCopy()] {
        if (g_cancellable_is_cancelled(cancellable.get()))
            return;
        auto file = adoptGRef(g_file_new_for_path(FileSystem::fileSystemRepresentation(path).data()));
        GUniqueOutPtr<GError> error;
        m_platformMonitor = adoptGRef(g_file_monitor(file.get(), G_FILE_MONITOR_NONE, m_cancellable.get(), &error.outPtr()));
        if (!m_platformMonitor) {
            WTFLogAlways("Failed to create a monitor for path %s: %s", path.utf8().data(), error->message);
            return;
        }
        g_signal_connect(m_platformMonitor.get(), "changed", G_CALLBACK(fileChangedCallback), this);
    });
}

FileMonitor::~FileMonitor()
{
    g_cancellable_cancel(m_cancellable.get());
}

void FileMonitor::fileChangedCallback(GFileMonitor*, GFile*, GFile*, GFileMonitorEvent event, FileMonitor* monitor)
{
    switch (event) {
    case G_FILE_MONITOR_EVENT_DELETED:
        monitor->didChange(FileChangeType::Removal);
        break;
    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
    case G_FILE_MONITOR_EVENT_CREATED:
        monitor->didChange(FileChangeType::Modification);
        break;
    default:
        break;
    }
}

void FileMonitor::didChange(FileChangeType type)
{
    ASSERT(!isMainThread());
    if (g_cancellable_is_cancelled(m_cancellable.get())) {
        m_platformMonitor = nullptr;
        return;
    }

    m_modificationHandler(type);
    if (type == FileChangeType::Removal)
        m_platformMonitor = nullptr;
}

} // namespace WebCore
