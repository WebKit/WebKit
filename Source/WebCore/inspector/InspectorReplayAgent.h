/*
 * Copyright (C) 2011-2013 University of Washington. All rights reserved.
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(WEB_REPLAY)

#include "InspectorWebAgentBase.h"
#include <inspector/InspectorBackendDispatchers.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class DocumentLoader;
class Event;
class Frame;
class InspectorPageAgent;
class Page;
class ReplaySession;
class ReplaySessionSegment;

enum class SessionState;

struct ReplayPosition;

typedef String ErrorString;
typedef int SessionIdentifier;
typedef int SegmentIdentifier;

class InspectorReplayAgent final
    : public InspectorAgentBase
    , public Inspector::ReplayBackendDispatcherHandler {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(InspectorReplayAgent);
public:
    InspectorReplayAgent(PageAgentContext&);
    virtual ~InspectorReplayAgent();

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // InspectorInstrumentation callbacks.
    void frameNavigated(DocumentLoader*);
    void frameDetached(Frame&);
    void willDispatchEvent(const Event&, Frame*);

    // Notifications from ReplayController.
    void sessionCreated(RefPtr<ReplaySession>&&);
    // This is called internally (when adding/removing) and by ReplayController during capture.
    void sessionModified(RefPtr<ReplaySession>&&);
    void sessionLoaded(RefPtr<ReplaySession>&&);

    void segmentCreated(RefPtr<ReplaySessionSegment>&&);
    void segmentCompleted(RefPtr<ReplaySessionSegment>&&);
    void segmentLoaded(RefPtr<ReplaySessionSegment>&&);
    void segmentUnloaded();

    void captureStarted();
    void captureStopped();

    void playbackStarted();
    void playbackPaused(const ReplayPosition&);
    void playbackHitPosition(const ReplayPosition&);
    void playbackFinished();

    // Calls from the Inspector frontend.
    void startCapturing(ErrorString&) override;
    void stopCapturing(ErrorString&) override;

    void replayToPosition(ErrorString&, const Inspector::InspectorObject& position, bool shouldFastForward) override;
    void replayToCompletion(ErrorString&, bool shouldFastForward) override;
    void pausePlayback(ErrorString&) override;
    void cancelPlayback(ErrorString&) override;

    void switchSession(ErrorString&, SessionIdentifier) override;
    void insertSessionSegment(ErrorString&, Inspector::Protocol::Replay::SessionIdentifier, Inspector::Protocol::Replay::SegmentIdentifier, int segmentIndex) override;
    void removeSessionSegment(ErrorString&, Inspector::Protocol::Replay::SessionIdentifier, int segmentIndex) override;

    void currentReplayState(ErrorString&, Inspector::Protocol::Replay::SessionIdentifier*, Inspector::Protocol::OptOutput<Inspector::Protocol::Replay::SegmentIdentifier>*, Inspector::Protocol::Replay::SessionState*, Inspector::Protocol::Replay::SegmentState* segmentState, RefPtr<Inspector::Protocol::Replay::ReplayPosition>&) override;
    void getAvailableSessions(ErrorString&, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::Replay::SessionIdentifier>>&) override;
    void getSessionData(ErrorString&, Inspector::Protocol::Replay::SessionIdentifier, RefPtr<Inspector::Protocol::Replay::ReplaySession>&) override;
    void getSegmentData(ErrorString&, Inspector::Protocol::Replay::SegmentIdentifier, RefPtr<Inspector::Protocol::Replay::SessionSegment>&) override;

private:
    RefPtr<ReplaySession> findSession(ErrorString&, SessionIdentifier);
    RefPtr<ReplaySessionSegment> findSegment(ErrorString&, SegmentIdentifier);
    WebCore::SessionState sessionState() const;

    std::unique_ptr<Inspector::ReplayFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::ReplayBackendDispatcher> m_backendDispatcher;
    Page& m_page;

    HashMap<int, RefPtr<ReplaySession>, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int>> m_sessionsMap;
    HashMap<int, RefPtr<ReplaySessionSegment>, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int>> m_segmentsMap;
};

} // namespace WebCore

#endif // ENABLE(WEB_REPLAY)
