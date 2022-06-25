/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)

#include "GroupActivitiesSession.h"
#include "MediaSessionCoordinatorProxyPrivate.h"
#include <wtf/CompletionHandler.h>

OBJC_CLASS AVDelegatingPlaybackCoordinator;
OBJC_CLASS AVDelegatingPlaybackCoordinatorPlayCommand;
OBJC_CLASS AVDelegatingPlaybackCoordinatorPauseCommand;
OBJC_CLASS AVDelegatingPlaybackCoordinatorSeekCommand;
OBJC_CLASS AVDelegatingPlaybackCoordinatorBufferingCommand;
OBJC_CLASS AVDelegatingPlaybackCoordinatorPrepareTransitionCommand;
OBJC_CLASS WKGroupActivitiesCoordinatorDelegate;

namespace WebKit {

class GroupActivitiesCoordinator final : public MediaSessionCoordinatorProxyPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<GroupActivitiesCoordinator> create(GroupActivitiesSession&);
    ~GroupActivitiesCoordinator();

    using CommandCompletionHandler = Function<void()>;
    void issuePlayCommand(AVDelegatingPlaybackCoordinatorPlayCommand *, CommandCompletionHandler&&);
    void issuePauseCommand(AVDelegatingPlaybackCoordinatorPauseCommand *, CommandCompletionHandler&&);
    void issueSeekCommand(AVDelegatingPlaybackCoordinatorSeekCommand *, CommandCompletionHandler&&);
    void issueBufferingCommand(AVDelegatingPlaybackCoordinatorBufferingCommand *, CommandCompletionHandler&&);
    void issuePrepareTransitionCommand(AVDelegatingPlaybackCoordinatorPrepareTransitionCommand *);

private:
    GroupActivitiesCoordinator(GroupActivitiesSession&);

    void sessionStateChanged(const GroupActivitiesSession&, GroupActivitiesSession::State);

    using CoordinatorCompletionHandler = CompletionHandler<void(std::optional<WebCore::ExceptionData>&&)>;
    String identifier() const final;
    void join(CoordinatorCompletionHandler&&) final;
    void leave() final;
    void seekTo(double, CoordinatorCompletionHandler&&) final;
    void play(CoordinatorCompletionHandler&&) final;
    void pause(CoordinatorCompletionHandler&&) final;
    void setTrack(const String&, CoordinatorCompletionHandler&&) final;
    void positionStateChanged(const std::optional<WebCore::MediaPositionState>&) final;
    void readyStateChanged(WebCore::MediaSessionReadyState) final;
    void playbackStateChanged(WebCore::MediaSessionPlaybackState) final;
    void trackIdentifierChanged(const String&) final;

    Ref<GroupActivitiesSession> m_session;
    RetainPtr<WKGroupActivitiesCoordinatorDelegate> m_delegate;
    RetainPtr<AVDelegatingPlaybackCoordinator> m_playbackCoordinator;

    std::optional<WebCore::MediaPositionState> m_positionState;
    std::optional<WebCore::MediaSessionReadyState> m_readyState;
    std::optional<WebCore::MediaSessionPlaybackState> m_playbackState;

    GroupActivitiesSession::StateChangeObserver m_stateChangeObserver;
};

}

#endif
