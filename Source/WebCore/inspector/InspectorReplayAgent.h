/*
 * Copyright (C) 2011-2013 University of Washington. All rights reserved.
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef InspectorReplayAgent_h
#define InspectorReplayAgent_h

#if ENABLE(INSPECTOR) && ENABLE(WEB_REPLAY)

#include "InspectorWebAgentBase.h"
#include "InspectorWebBackendDispatchers.h"
#include "InspectorWebFrontendDispatchers.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class DocumentLoader;
class Frame;
class InspectorPageAgent;
class InstrumentingAgents;
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
    , public Inspector::InspectorReplayBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorReplayAgent);
public:
    InspectorReplayAgent(InstrumentingAgents*, InspectorPageAgent*);
    ~InspectorReplayAgent();

    virtual void didCreateFrontendAndBackend(Inspector::InspectorFrontendChannel*, Inspector::InspectorBackendDispatcher*) override;
    virtual void willDestroyFrontendAndBackend(Inspector::InspectorDisconnectReason) override;

    // Callbacks from InspectorInstrumentation.
    void frameNavigated(DocumentLoader*);
    void frameDetached(Frame*);

    // Notifications from ReplayController.
    void sessionCreated(PassRefPtr<ReplaySession>);
    // This is called internally (when adding/removing) and by ReplayController during capture.
    void sessionModified(PassRefPtr<ReplaySession>);
    void sessionLoaded(PassRefPtr<ReplaySession>);

    void segmentCreated(PassRefPtr<ReplaySessionSegment>);
    void segmentCompleted(PassRefPtr<ReplaySessionSegment>);
    void segmentLoaded(PassRefPtr<ReplaySessionSegment>);
    void segmentUnloaded();

    void captureStarted();
    void captureStopped();

    void playbackStarted();
    void playbackPaused(const ReplayPosition&);
    void playbackHitPosition(const ReplayPosition&);

    // Calls from the Inspector frontend.
    virtual void startCapturing(ErrorString*) override;
    virtual void stopCapturing(ErrorString*) override;

    virtual void replayToPosition(ErrorString*, const RefPtr<Inspector::InspectorObject>&, bool shouldFastForward) override;
    virtual void replayToCompletion(ErrorString*, bool shouldFastForward) override;
    virtual void pausePlayback(ErrorString*) override;
    virtual void cancelPlayback(ErrorString*) override;

    virtual void switchSession(ErrorString*, SessionIdentifier) override;
    virtual void insertSessionSegment(ErrorString*, SessionIdentifier, SegmentIdentifier, int segmentIndex) override;
    virtual void removeSessionSegment(ErrorString*, SessionIdentifier, int segmentIndex) override;

    virtual void getAvailableSessions(ErrorString*, RefPtr<Inspector::TypeBuilder::Array<SessionIdentifier>>&) override;
    virtual void getSerializedSession(ErrorString*, SessionIdentifier, RefPtr<Inspector::TypeBuilder::Replay::ReplaySession>&) override;
    virtual void getSerializedSegment(ErrorString*, SegmentIdentifier, RefPtr<Inspector::TypeBuilder::Replay::SessionSegment>&) override;

private:
    PassRefPtr<ReplaySession> findSession(ErrorString*, SessionIdentifier);
    PassRefPtr<ReplaySessionSegment> findSegment(ErrorString*, SegmentIdentifier);
    SessionState sessionState() const;

    std::unique_ptr<Inspector::InspectorReplayFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::InspectorReplayBackendDispatcher> m_backendDispatcher;
    Page& m_page;

    HashMap<int, RefPtr<ReplaySession>, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int>> m_sessionsMap;
    HashMap<int, RefPtr<ReplaySessionSegment>, WTF::IntHash<int>, WTF::UnsignedWithZeroKeyHashTraits<int>> m_segmentsMap;
};

} // namespace WebCore

#endif // ENABLE(INSPECTOR) && ENABLE(WEB_REPLAY)

#endif // InspectorReplayAgent_h
