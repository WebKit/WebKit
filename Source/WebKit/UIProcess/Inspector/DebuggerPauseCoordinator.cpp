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

#include "BrowsingContextGroup.h"
#include "FrameInspectorTarget.h"
#include "MessageSenderInlines.h"
#include "ProvisionalPageProxy.h"
#include "WebInspectorBackendMessages.h"
#include "WebPageInspectorController.h"
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
    m_isDestroying = true;
    closeSession();
}

void DebuggerPauseCoordinator::frameDidPause(WebProcessProxy& process, WebCore::FrameIdentifier frameID)
{
    bool isNewStoppingFrame = m_stoppingFrames.add({ process.coreProcessIdentifier(), frameID }).isNewEntry;
    if (!m_isEnabled)
        return;
    if (!m_sessionID)
        openSession();
    else if (isNewStoppingFrame)
        notifySessionChanged(true);
}

void DebuggerPauseCoordinator::frameDidContinue(WebProcessProxy& process, WebCore::FrameIdentifier frameID)
{
    if (!m_stoppingFrames.remove({ process.coreProcessIdentifier(), frameID }))
        return;
    stoppingFramesDidShrink();
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
    if (!m_stoppingFrames.removeIf(predicate))
        return;
    stoppingFramesDidShrink();
}

// A session that was interrupted is the user's pause, not a frame's, so it stays open with no stopping frame until it is resumed.
void DebuggerPauseCoordinator::stoppingFramesDidShrink()
{
    if (!m_sessionID)
        return;
    if (m_stoppingFrames.isEmpty() && !m_isInterrupted)
        closeSession();
    else
        notifySessionChanged(true);
}

// forEachWebContentProcess() skips the provisional page, which can commit during the session.
static void forEachProcessOfPage(WebPageProxy& page, NOESCAPE const Function<void(WebProcessProxy&, WebCore::PageIdentifier)>& function)
{
    page.forEachWebContentProcess(function);
    if (RefPtr provisionalPage = page.provisionalPageProxy())
        function(protect(provisionalPage->process()), provisionalPage->webPageID());
}

void DebuggerPauseCoordinator::openSession()
{
    ASSERT(!m_sessionID);
    m_sessionID = WebCore::DebuggerPauseSessionIdentifier::generate();

    Ref group = protect(m_page.get())->browsingContextGroup();
    group->forEachPage([&](WebPageProxy& page) {
        page.forEachWebContentProcess([&](WebProcessProxy& process, WebCore::PageIdentifier pageID) {
            suspendPage(process, pageID);
        });

        // forEachWebContentProcess() skips the provisional page, which can commit during the session, unless its navigation leaves the group.
        if (RefPtr provisionalPage = page.provisionalPageProxy(); provisionalPage && &provisionalPage->browsingContextGroup() == group.ptr())
            suspendPage(protect(provisionalPage->process()), provisionalPage->webPageID());
    });

    notifySessionChanged(true);
}

void DebuggerPauseCoordinator::notifySessionChanged(bool isPaused)
{
    ASSERT(m_sessionID);
    if (m_isDestroying)
        return;

    Vector<String> stoppingTargetIDs;
    if (isPaused) {
        for (auto& [processID, frameID] : m_stoppingFrames)
            stoppingTargetIDs.append(FrameInspectorTarget::toTargetID(frameID, processID));
        std::ranges::sort(stoppingTargetIDs, WTF::codePointCompareLessThan);
    }
    protect(m_page)->inspectorController().debuggerPauseSessionDidChange(*m_sessionID, isPaused, stoppingTargetIDs);
}

void DebuggerPauseCoordinator::pageWasAdded(WebProcessProxy& process, WebCore::PageIdentifier pageID)
{
    if (!m_sessionID)
        return;
    suspendPage(process, pageID);
}

void DebuggerPauseCoordinator::suspendPage(WebProcessProxy& process, WebCore::PageIdentifier pageID)
{
    // A provisional page can reuse a remote page's identifier and start from scratch, so always send but track once.
    process.send(Messages::WebInspectorBackend::SuspendForDebugger(*m_sessionID), pageID);
    bool isTracked = m_suspendedPages.containsIf([&](auto& entry) {
        return entry.first.get() == &process && entry.second == pageID;
    });
    if (!isTracked)
        m_suspendedPages.append({ process, pageID });
}

// While global pause is off the stopping frames are still tracked, so that turning it on again holds the other frames while one is still stopped.
void DebuggerPauseCoordinator::setEnabled(bool enabled)
{
    if (m_isEnabled == enabled)
        return;
    m_isEnabled = enabled;
    if (!enabled)
        endSession();
    else if (!m_stoppingFrames.isEmpty())
        openSession();
}

// A session with no stopping frame holds every process of an idle page. A session that is already open is reported again, for a frontend that connected after it opened.
void DebuggerPauseCoordinator::interrupt()
{
    m_isInterrupted = true;
    if (!m_sessionID)
        openSession();
    else
        notifySessionChanged(true);
}

// The session closes as the stopping frames report that they continued, so the other frames stay held until the resumed code goes idle, as when a frame is resumed from its own target.
void DebuggerPauseCoordinator::resumeSession()
{
    if (!m_sessionID)
        return;

    m_isInterrupted = false;
    if (m_stoppingFrames.isEmpty()) {
        closeSession();
        return;
    }

    HashSet<WebCore::ProcessIdentifier> stoppingProcesses;
    for (auto& [processID, frameID] : m_stoppingFrames)
        stoppingProcesses.add(processID);

    forEachProcessOfPage(protect(m_page.get()), [&](WebProcessProxy& process, WebCore::PageIdentifier pageID) {
        if (stoppingProcesses.contains(process.coreProcessIdentifier()))
            process.send(Messages::WebInspectorBackend::ResumeStoppedFramesForDebugger(), pageID);
    });
}

void DebuggerPauseCoordinator::closeSession()
{
    m_stoppingFrames.clear();
    endSession();
}

void DebuggerPauseCoordinator::endSession()
{
    m_isInterrupted = false;
    if (!m_sessionID)
        return;

    notifySessionChanged(false);
    auto sessionID = std::exchange(m_sessionID, std::nullopt);

    for (auto& [weakProcess, pageID] : std::exchange(m_suspendedPages, { })) {
        if (RefPtr process = weakProcess.get())
            process->send(Messages::WebInspectorBackend::ResumeFromDebugger(*sessionID), pageID);
    }
}

} // namespace WebKit
