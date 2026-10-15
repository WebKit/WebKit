/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "DebuggerPauseCoordinator.h"

#include "MessageSenderInlines.h"
#include "WebInspectorBackendMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DebuggerPauseCoordinator);

DebuggerPauseCoordinator::DebuggerPauseCoordinator(WebPageProxy& page)
    : m_page(page)
{
}

DebuggerPauseCoordinator::~DebuggerPauseCoordinator()
{
    closeSession();
}

void DebuggerPauseCoordinator::frameDidPause(WebProcessProxy& process, WebCore::FrameIdentifier frameID)
{
    m_stoppingFrames.add({ process.coreProcessIdentifier(), frameID });
    if (!m_sessionID)
        openSession(process);
}

void DebuggerPauseCoordinator::frameDidContinue(WebProcessProxy& process, WebCore::FrameIdentifier frameID)
{
    if (m_stoppingFrames.remove({ process.coreProcessIdentifier(), frameID }) && m_stoppingFrames.isEmpty())
        closeSession();
}

// A frame that leaves its process, or is removed, may never report that it continued: the process can lose the
// page that would send the report, and stay alive.
void DebuggerPauseCoordinator::frameDidChangeProcess(WebCore::FrameIdentifier frameID, WebCore::ProcessIdentifier newProcessID)
{
    removeStoppingFramesIf([&](auto& entry) {
        return entry.second == frameID && entry.first != newProcessID;
    });
}

void DebuggerPauseCoordinator::frameWillBeDestroyed(WebCore::FrameIdentifier frameID)
{
    removeStoppingFramesIf([&](auto& entry) {
        return entry.second == frameID;
    });
}

void DebuggerPauseCoordinator::processDidTerminate(WebProcessProxy& process)
{
    m_suspendedPages.removeAllMatching([&](auto& entry) {
        return !entry.first || entry.first.get() == &process;
    });

    // A process that dies while one of its frames is paused never reports that the frame continued.
    removeStoppingFramesIf([&](auto& entry) {
        return entry.first == process.coreProcessIdentifier();
    });
}

template<typename Predicate>
void DebuggerPauseCoordinator::removeStoppingFramesIf(NOESCAPE const Predicate& predicate)
{
    if (m_stoppingFrames.removeIf(predicate) && m_stoppingFrames.isEmpty())
        closeSession();
}

void DebuggerPauseCoordinator::openSession(WebProcessProxy& pausingProcess)
{
    ASSERT(!m_sessionID);
    auto sessionID = WebCore::DebuggerPauseSessionIdentifier::generate();
    m_sessionID = sessionID;

    Ref page = m_page.get();
    page->forEachWebContentProcess([&](WebProcessProxy& process, WebCore::PageIdentifier pageID) {
        // The pausing process already holds every page in it while its frame is paused.
        if (&process == &pausingProcess)
            return;
        process.send(Messages::WebInspectorBackend::SuspendForDebugger(sessionID), pageID);
        m_suspendedPages.append({ process, pageID });
    });
}

void DebuggerPauseCoordinator::closeSession()
{
    auto sessionID = std::exchange(m_sessionID, std::nullopt);
    m_stoppingFrames.clear();
    if (!sessionID)
        return;

    for (auto& [weakProcess, pageID] : std::exchange(m_suspendedPages, { })) {
        if (RefPtr process = weakProcess.get())
            process->send(Messages::WebInspectorBackend::ResumeFromDebugger(*sessionID), pageID);
    }
}

} // namespace WebKit
