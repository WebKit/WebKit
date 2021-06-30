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

#import "config.h"
#import "GroupActivitiesSession.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)

#import "WKGroupSession.h"

namespace WebKit {

Ref<GroupActivitiesSession> GroupActivitiesSession::create(RetainPtr<WKGroupSession>&& session)
{
    return adoptRef(*new GroupActivitiesSession(WTFMove(session)));
}

GroupActivitiesSession::GroupActivitiesSession(RetainPtr<WKGroupSession>&& session)
    : m_groupSession(WTFMove(session))
{
    m_groupSession.get().newActivityCallback = [this] (WKURLActivity *activity) {
        m_fallbackURLObservers.forEach([this, activity] (auto& observer) {
            observer(*this, activity.fallbackURL);
        });
    };

    m_groupSession.get().stateChangedCallback = [this] (WKGroupSessionState state) {
        static_assert(static_cast<size_t>(State::Waiting) == static_cast<size_t>(WKGroupSessionStateWaiting), "MediaSessionCoordinatorState::Waiting != WKGroupSessionStateWaiting");
        static_assert(static_cast<size_t>(State::Joined) == static_cast<size_t>(WKGroupSessionStateJoined), "MediaSessionCoordinatorState::Joined != WKGroupSessionStateJoined");
        static_assert(static_cast<size_t>(State::Invalidated) == static_cast<size_t>(WKGroupSessionStateInvalidated), "MediaSessionCoordinatorState::Closed != WKGroupSessionStateInvalidated");
        m_stateChangeObservers.forEach([this, state] (auto& observer) {
            observer(*this, static_cast<State>(state));
        });
    };
}

GroupActivitiesSession::~GroupActivitiesSession()
{
    m_groupSession.get().newActivityCallback = nil;
    m_groupSession.get().stateChangedCallback = nil;
}

void GroupActivitiesSession::join()
{
    [m_groupSession join];
}

void GroupActivitiesSession::leave()
{
    [m_groupSession leave];
}

auto GroupActivitiesSession::state() const -> State
{
    static_assert(static_cast<size_t>(State::Waiting) == static_cast<size_t>(WKGroupSessionStateWaiting), "State::Waiting != WKGroupSessionStateWaiting");
    static_assert(static_cast<size_t>(State::Joined) == static_cast<size_t>(WKGroupSessionStateJoined), "State::Joined != WKGroupSessionStateJoined");
    static_assert(static_cast<size_t>(State::Invalidated) == static_cast<size_t>(WKGroupSessionStateInvalidated), "State::Invalidated != WKGroupSessionStateInvalidated");
    return static_cast<State>([m_groupSession state]);
}

String GroupActivitiesSession::uuid() const
{
    return [m_groupSession uuid].UUIDString;
}

URL GroupActivitiesSession::fallbackURL() const
{
    return [m_groupSession activity].fallbackURL;
}

void GroupActivitiesSession::addStateChangeObserver(const StateChangeObserver& observer)
{
    m_stateChangeObservers.add(observer);
}

void GroupActivitiesSession::addFallbackURLObserver(const FallbackURLObserver& observer)
{
    m_fallbackURLObservers.add(observer);
}

}

#endif
