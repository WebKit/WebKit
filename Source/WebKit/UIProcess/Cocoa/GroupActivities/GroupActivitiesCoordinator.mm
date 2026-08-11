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
#import "GroupActivitiesCoordinator.h"

#if ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)

#import "WKGroupSession.h"
#import <WebCore/NotImplemented.h>
#import <wtf/BlockPtr.h>
#import <wtf/TZoneMallocInlines.h>
#import <wtf/darwin/DispatchExtras.h>

#import <pal/cocoa/AVFoundationSoftLink.h>
#import <pal/cf/CoreMediaSoftLink.h>

@interface WKGroupActivitiesCoordinatorDelegate : NSObject<AVPlaybackCoordinatorPlaybackControlDelegate> {
    WeakPtr<WebKit::GroupActivitiesCoordinator> _parent;
}
- (id)initWithParent:(WebKit::GroupActivitiesCoordinator&)parent;
@end

@implementation WKGroupActivitiesCoordinatorDelegate
- (id)initWithParent:(WebKit::GroupActivitiesCoordinator&)parent
{
    self = [super init];
    if (!self)
        return nil;

    _parent = parent;
    return self;
}

-(void)playbackCoordinator:(AVDelegatingPlaybackCoordinator *)coordinator didIssuePlayCommand:(AVDelegatingPlaybackCoordinatorPlayCommand *)playCommand completionHandler:(void (^)(void))completionHandler {
    dispatch_async(mainDispatchQueueSingleton(), ^{
        RefPtr parent = _parent.get();
        if (!parent) {
            completionHandler();
            return;
        }

        parent->issuePlayCommand(playCommand, [completionHandler = makeBlockPtr(completionHandler)] {
            completionHandler();
        });
    });
}

-(void)playbackCoordinator:(AVDelegatingPlaybackCoordinator *)coordinator didIssuePauseCommand:(AVDelegatingPlaybackCoordinatorPauseCommand *)pauseCommand completionHandler:(void (^)(void))completionHandler {
    dispatch_async(mainDispatchQueueSingleton(), ^{
        RefPtr parent = _parent.get();
        if (!parent) {
            completionHandler();
            return;
        }

        parent->issuePauseCommand(pauseCommand, [completionHandler = makeBlockPtr(completionHandler)] {
            completionHandler();
        });
    });
}

-(void)playbackCoordinator:(AVDelegatingPlaybackCoordinator *)coordinator didIssueSeekCommand:(AVDelegatingPlaybackCoordinatorSeekCommand *)seekCommand completionHandler:(void (^)(void))completionHandler {
    dispatch_async(mainDispatchQueueSingleton(), ^{
        RefPtr parent = _parent.get();
        if (!parent) {
            completionHandler();
            return;
        }

        parent->issueSeekCommand(seekCommand, [completionHandler = makeBlockPtr(completionHandler)] {
            completionHandler();
        });
    });
}

-(void)playbackCoordinator:(AVDelegatingPlaybackCoordinator *)coordinator didIssueBufferingCommand:(AVDelegatingPlaybackCoordinatorBufferingCommand *)bufferingCommand completionHandler:(void (^)(void))completionHandler {
    dispatch_async(mainDispatchQueueSingleton(), ^{
        RefPtr parent = _parent.get();
        if (!parent) {
            completionHandler();
            return;
        }

        parent->issueBufferingCommand(bufferingCommand, [completionHandler = makeBlockPtr(completionHandler)] {
            completionHandler();
        });
    });
}

-(void)playbackCoordinator:(AVDelegatingPlaybackCoordinator *)coordinator didIssuePrepareTransitionCommand:(AVDelegatingPlaybackCoordinatorPrepareTransitionCommand *)prepareTransitionCommand {
    dispatch_async(mainDispatchQueueSingleton(), ^{
        RefPtr parent = _parent.get();
        if (!parent)
            return;
        parent->issuePrepareTransitionCommand(prepareTransitionCommand);
    });
}
@end

namespace WebKit {

using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(GroupActivitiesCoordinator);

Ref<GroupActivitiesCoordinator> GroupActivitiesCoordinator::create(GroupActivitiesSession& session)
{
    return adoptRef(*new GroupActivitiesCoordinator(session));
}

GroupActivitiesCoordinator::GroupActivitiesCoordinator(GroupActivitiesSession& session)
    : m_session(session)
    , m_delegate(adoptNS([[WKGroupActivitiesCoordinatorDelegate alloc] initWithParent:*this]))
    , m_playbackCoordinator(adoptNS([PAL::allocAVDelegatingPlaybackCoordinatorInstance() initWithPlaybackControlDelegate:m_delegate.get()]))
    , m_stateChangeObserver(GroupActivitiesSession::StateChangeObserver::create([weakThis = WeakPtr { *this }] (auto& session, auto state) {
        if (RefPtr protectedThis = weakThis.get())
            protectedThis->sessionStateChanged(session, state);
    }))
{
    [protect(session.groupSession()) coordinateWithCoordinator:m_playbackCoordinator.get()];
    session.addStateChangeObserver(m_stateChangeObserver);
}

GroupActivitiesCoordinator::~GroupActivitiesCoordinator()
{
    protect(m_session->groupSession()).get().newActivityCallback = nil;
    protect(m_session->groupSession()).get().stateChangedCallback = nil;
}

void GroupActivitiesCoordinator::sessionStateChanged(const GroupActivitiesSession& session, GroupActivitiesSession::State state)
{
    RefPtr client = this->client();
    if (!client)
        return;

    static_assert(static_cast<size_t>(MediaSessionCoordinatorState::Waiting) == static_cast<size_t>(GroupActivitiesSession::State::Waiting), "MediaSessionCoordinatorState::Waiting != WKGroupSessionStateWaiting");
    static_assert(static_cast<size_t>(MediaSessionCoordinatorState::Joined) == static_cast<size_t>(GroupActivitiesSession::State::Joined), "MediaSessionCoordinatorState::Joined != WKGroupSessionStateJoined");
    static_assert(static_cast<size_t>(MediaSessionCoordinatorState::Closed) == static_cast<size_t>(GroupActivitiesSession::State::Invalidated), "MediaSessionCoordinatorState::Closed != WKGroupSessionStateInvalidated");
    client->coordinatorStateChanged(static_cast<MediaSessionCoordinatorState>(state));
}

String GroupActivitiesCoordinator::identifier() const
{
    return m_session->uuid();
}

void GroupActivitiesCoordinator::join(CoordinatorCompletionHandler&& callback)
{
    m_session->join();
    callback(std::nullopt);
}

void GroupActivitiesCoordinator::leave()
{
    m_session->leave();
}

void GroupActivitiesCoordinator::seekTo(double time, CoordinatorCompletionHandler&& callback)
{
    [m_playbackCoordinator coordinateSeekToTime:PAL::CMTimeMakeWithSeconds(time, 1000) options:0];
    callback(std::nullopt);
}

void GroupActivitiesCoordinator::play(CoordinatorCompletionHandler&& callback)
{
    [m_playbackCoordinator coordinateRateChangeToRate:1 options:0];
    callback(std::nullopt);
}

void GroupActivitiesCoordinator::pause(CoordinatorCompletionHandler&& callback)
{
    [m_playbackCoordinator coordinateRateChangeToRate:0 options:0];
    callback(std::nullopt);
}

void GroupActivitiesCoordinator::setTrack(const String& track, CoordinatorCompletionHandler&& callback)
{
    callback(std::nullopt);
}

void GroupActivitiesCoordinator::positionStateChanged(const std::optional<MediaPositionState>& positionState)
{
    if (m_positionState == positionState)
        return;
    m_positionState = positionState;
}

void GroupActivitiesCoordinator::readyStateChanged(MediaSessionReadyState readyState)
{
    if (m_readyState == readyState)
        return;
    m_readyState = readyState;
}

void GroupActivitiesCoordinator::playbackStateChanged(MediaSessionPlaybackState playbackState)
{
    if (m_playbackState == playbackState)
        return;
    m_playbackState = playbackState;
}

void GroupActivitiesCoordinator::trackIdentifierChanged(const String& identifier)
{
    if (identifier != String([m_playbackCoordinator currentItemIdentifier]))
        [m_playbackCoordinator transitionToItemWithIdentifier:identifier.createNSString().get() proposingInitialTimingBasedOnTimebase:nil];
}

void GroupActivitiesCoordinator::issuePlayCommand(AVDelegatingPlaybackCoordinatorPlayCommand *playCommand, CommandCompletionHandler&& callback)
{
    RefPtr client = this->client();
    if (!client) {
        callback();
        return;
    }

    std::optional<double> itemTime;
    if (CMTIME_IS_NUMERIC(playCommand.itemTime))
        itemTime = PAL::CMTimeGetSeconds(playCommand.itemTime);
    std::optional<MonotonicTime> hostTime;
    if (CMTIME_IS_NUMERIC(playCommand.hostClockTime))
        hostTime = MonotonicTime::fromMachAbsoluteTime(PAL::CMClockConvertHostTimeToSystemUnits(playCommand.hostClockTime));

    client->playSession(itemTime, hostTime, [callback = WTF::move(callback)] (bool) {
        callback();
    });
}

void GroupActivitiesCoordinator::issuePauseCommand(AVDelegatingPlaybackCoordinatorPauseCommand *pauseCommand, CommandCompletionHandler&& callback)
{
    RefPtr client = this->client();
    if (!client) {
        callback();
        return;
    }

    client->pauseSession([callback = WTF::move(callback)] (bool) {
        callback();
    });
}

void GroupActivitiesCoordinator::issueSeekCommand(AVDelegatingPlaybackCoordinatorSeekCommand *seekCommand, CommandCompletionHandler&& callback)
{
    RefPtr client = this->client();
    if (!client) {
        callback();
        return;
    }

    if (!CMTIME_IS_NUMERIC(seekCommand.itemTime)) {
        ASSERT_NOT_REACHED();
        callback();
        return;
    }

    client->seekSessionToTime(PAL::CMTimeGetSeconds(seekCommand.itemTime), [callback = WTF::move(callback)] (bool) mutable {
        callback();
    });
}

void GroupActivitiesCoordinator::issueBufferingCommand(AVDelegatingPlaybackCoordinatorBufferingCommand *, CommandCompletionHandler&& completionHandler)
{
    completionHandler();
    notImplemented();
}

void GroupActivitiesCoordinator::issuePrepareTransitionCommand(AVDelegatingPlaybackCoordinatorPrepareTransitionCommand *)
{
    notImplemented();
}

}

#endif // ENABLE(MEDIA_SESSION_COORDINATOR) && HAVE(GROUP_ACTIVITIES)
