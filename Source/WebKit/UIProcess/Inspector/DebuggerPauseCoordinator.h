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

#pragma once

#include <WebCore/DebuggerPauseSessionIdentifier.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/ProcessIdentifier.h>
#include <wtf/CheckedPtr.h>
#include <wtf/Function.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/WeakRef.h>

namespace WebKit {

class WebPageProxy;
class WebProcessProxy;

class DebuggerPauseCoordinator final : public CanMakeCheckedPtr<DebuggerPauseCoordinator> {
    WTF_MAKE_TZONE_ALLOCATED(DebuggerPauseCoordinator);
    WTF_MAKE_NONCOPYABLE(DebuggerPauseCoordinator);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DebuggerPauseCoordinator);
public:
    explicit DebuggerPauseCoordinator(WebPageProxy&);
    ~DebuggerPauseCoordinator();

    void frameDidPause(WebProcessProxy&, WebCore::FrameIdentifier);
    void frameDidContinue(WebProcessProxy&, WebCore::FrameIdentifier);

    void pageWasAdded(WebProcessProxy&, WebCore::PageIdentifier);
    void frameDidChangeProcess(WebCore::FrameIdentifier, WebCore::ProcessIdentifier newProcessID);
    void frameWillBeDestroyed(WebCore::FrameIdentifier);
    void processDidTerminate(WebProcessProxy&);
    bool isEnabled() const { return m_isEnabled; }
    void setEnabled(bool);
    void interrupt();
    void closeSession();
    void resumeSession();

private:
    void openSession();
    void endSession();
    void notifySessionChanged(bool isPaused);
    void stoppingFramesDidShrink();
    void suspendPage(WebProcessProxy&, WebCore::PageIdentifier);
    template<typename Predicate> void removeStoppingFramesIf(NOESCAPE const Predicate&);

    WeakRef<WebPageProxy> m_page;
    std::optional<WebCore::DebuggerPauseSessionIdentifier> m_sessionID;
    bool m_isEnabled { true };
    bool m_isInterrupted { false };
    bool m_isDestroying { false };
    // A frame can be stopping in two processes at once: its old document has not reported continuing yet when its new document pauses.
    HashSet<std::pair<WebCore::ProcessIdentifier, WebCore::FrameIdentifier>> m_stoppingFrames;
    Vector<std::pair<WeakPtr<WebProcessProxy>, WebCore::PageIdentifier>> m_suspendedPages;
};

} // namespace WebKit
