/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "config.h"
#import "FileMonitor.h"

#import "FileSystem.h"
#import "Logging.h"
#import <dispatch/dispatch.h>
#import <wtf/DispatchPtr.h>

namespace WebCore {
    
constexpr unsigned monitorMask = DISPATCH_VNODE_DELETE | DISPATCH_VNODE_WRITE | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_REVOKE;
constexpr unsigned fileUnavailableMask = DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME | DISPATCH_VNODE_REVOKE;

void FileMonitor::startMonitoring()
{
    if (m_platformMonitor)
        return;

    if (m_path.isEmpty())
        return;

    if (!m_modificationHandler)
        return;

    auto handle = openFile(m_path, OpenForEventsOnly);
    if (handle == invalidPlatformFileHandle) {
        RELEASE_LOG_ERROR(ResourceLoadStatistics, "Failed to open statistics file for monitoring: %s", m_path.utf8().data());
        return;
    }

    m_platformMonitor = adoptDispatch(dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, handle, monitorMask, m_handlerQueue->dispatchQueue()));

    LOG(ResourceLoadStatistics, "Creating monitor %p", m_platformMonitor.get());

    dispatch_source_set_event_handler(m_platformMonitor.get(), [this, protectedThis = makeRefPtr(this), fileMonitor = m_platformMonitor.get()] () mutable {
        // If this is getting called after the monitor was cancelled, just drop the notification.
        if (dispatch_source_testcancel(fileMonitor))
            return;

        unsigned flag = dispatch_source_get_data(fileMonitor);
        LOG(ResourceLoadStatistics, "File event %#X for monitor %p", flag, fileMonitor);
        if (flag & fileUnavailableMask) {
            m_modificationHandler(FileChangeType::Removal);
            dispatch_source_cancel(fileMonitor);
        } else {
            ASSERT(flag & DISPATCH_VNODE_WRITE);
            m_modificationHandler(FileChangeType::Modification);
        }
    });
    
    dispatch_source_set_cancel_handler(m_platformMonitor.get(), [fileMonitor = m_platformMonitor.get()] {
        auto handle = static_cast<PlatformFileHandle>(dispatch_source_get_handle(fileMonitor));
        closeFile(handle);
    });
    
    dispatch_resume(m_platformMonitor.get());
}
    
void FileMonitor::stopMonitoring()
{
    if (!m_platformMonitor)
        return;

    LOG(ResourceLoadStatistics, "Stopping monitor %p", m_platformMonitor.get());

    dispatch_source_cancel(m_platformMonitor.get());
    m_platformMonitor = nullptr;
}

} // namespace WebCore
