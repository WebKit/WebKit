/*
 * Copyright (C) 2020-2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "MediaSession.h"

#if ENABLE(MEDIA_SESSION)

#include "DOMWindow.h"
#include "EventNames.h"
#include "HTMLMediaElement.h"
#include "JSMediaPositionState.h"
#include "JSMediaSessionAction.h"
#include "JSMediaSessionPlaybackState.h"
#include "Logging.h"
#include "MediaMetadata.h"
#include "MediaSessionCoordinator.h"
#include "Navigator.h"
#include "PlatformMediaSessionManager.h"
#include <wtf/JSONValues.h>

namespace WebCore {

static const void* nextLogIdentifier()
{
    static uint64_t logIdentifier = cryptographicallyRandomNumber();
    return reinterpret_cast<const void*>(++logIdentifier);
}

static WTFLogChannel& logChannel() { return LogMedia; }
static const char* logClassName() { return "MediaSession"; }

static PlatformMediaSession::RemoteControlCommandType platformCommandForMediaSessionAction(MediaSessionAction action)
{
    static const auto commandMap = makeNeverDestroyed([] {
        using ActionToCommandMap = HashMap<MediaSessionAction, PlatformMediaSession::RemoteControlCommandType, WTF::IntHash<MediaSessionAction>, WTF::StrongEnumHashTraits<MediaSessionAction>>;

        return ActionToCommandMap {
            { MediaSessionAction::Play, PlatformMediaSession::PlayCommand },
            { MediaSessionAction::Pause, PlatformMediaSession::PauseCommand },
            { MediaSessionAction::Seekforward, PlatformMediaSession::SkipForwardCommand },
            { MediaSessionAction::Seekbackward, PlatformMediaSession::SkipBackwardCommand },
            { MediaSessionAction::Previoustrack, PlatformMediaSession::NextTrackCommand },
            { MediaSessionAction::Nexttrack, PlatformMediaSession::PreviousTrackCommand },
            { MediaSessionAction::Stop, PlatformMediaSession::StopCommand },
            { MediaSessionAction::Seekto, PlatformMediaSession::SeekToPlaybackPositionCommand },
            { MediaSessionAction::Skipad, PlatformMediaSession::NextTrackCommand },
        };
    }());

    auto it = commandMap.get().find(action);
    if (it != commandMap.get().end())
        return it->value;

    return PlatformMediaSession::NoCommand;
}

static Optional<std::pair<PlatformMediaSession::RemoteControlCommandType, PlatformMediaSession::RemoteCommandArgument>> platformCommandForMediaSessionAction(const MediaSessionActionDetails& actionDetails)
{
    PlatformMediaSession::RemoteControlCommandType command = PlatformMediaSession::NoCommand;
    PlatformMediaSession::RemoteCommandArgument argument;

    switch (actionDetails.action) {
    case MediaSessionAction::Play:
        command = PlatformMediaSession::PlayCommand;
        break;
    case MediaSessionAction::Pause:
        command = PlatformMediaSession::PauseCommand;
        break;
    case MediaSessionAction::Seekbackward:
        command = PlatformMediaSession::SkipBackwardCommand;
        argument.time = actionDetails.seekOffset;
        break;
    case MediaSessionAction::Seekforward:
        command = PlatformMediaSession::SkipForwardCommand;
        argument.time = actionDetails.seekOffset;
        break;
    case MediaSessionAction::Previoustrack:
        command = PlatformMediaSession::PreviousTrackCommand;
        break;
    case MediaSessionAction::Nexttrack:
        command = PlatformMediaSession::NextTrackCommand;
        break;
    case MediaSessionAction::Skipad:
        // Not supported at present.
        break;
    case MediaSessionAction::Stop:
        command = PlatformMediaSession::StopCommand;
        break;
    case MediaSessionAction::Seekto:
        command = PlatformMediaSession::SeekToPlaybackPositionCommand;
        argument.time = actionDetails.seekTime;
        argument.fastSeek = actionDetails.fastSeek;
        break;
    case MediaSessionAction::Settrack:
        // Not supported at present.
        break;
    }
    if (command == PlatformMediaSession::NoCommand)
        return { };

    return std::make_pair(command, argument);
}

Ref<MediaSession> MediaSession::create(Navigator& navigator)
{
    return adoptRef(*new MediaSession(navigator));
}

MediaSession::MediaSession(Navigator& navigator)
    : ActiveDOMObject(navigator.scriptExecutionContext())
    , m_navigator(makeWeakPtr(navigator))
    , m_asyncEventQueue(MainThreadGenericEventQueue::create(*this))
{
    m_logger = makeRefPtr(Document::sharedLogger());
    m_logIdentifier = nextLogIdentifier();
    suspendIfNeeded();

    ALWAYS_LOG(LOGIDENTIFIER);
}

MediaSession::~MediaSession() = default;

bool MediaSession::virtualHasPendingActivity() const
{
    return m_asyncEventQueue->hasPendingActivity();
}

void MediaSession::setMetadata(RefPtr<MediaMetadata>&& metadata)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_metadata)
        m_metadata->resetMediaSession();
    m_metadata = WTFMove(metadata);
    if (m_metadata)
        m_metadata->setMediaSession(*this);
    notifyMetadataObservers();
}

#if ENABLE(MEDIA_SESSION_COORDINATOR)
void MediaSession::setReadyState(MediaSessionReadyState state)
{
    if (m_readyState == state)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);

    m_readyState = state;
    notifyReadyStateObservers();
}

void MediaSession::setCoordinator(MediaSessionCoordinator* coordinator)
{
    if (m_coordinator == coordinator)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_coordinator)
        m_coordinator->setMediaSession(nullptr);

    m_coordinator = coordinator;

    if (m_coordinator)
        m_coordinator->setMediaSession(this);

    m_asyncEventQueue->enqueueEvent(Event::create(eventNames().coordinatorchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
}
#endif

#if ENABLE(MEDIA_SESSION_PLAYLIST)
ExceptionOr<void> MediaSession::setPlaylist(ScriptExecutionContext& context, Vector<RefPtr<MediaMetadata>>&& playlist)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    Vector<Ref<MediaMetadata>> resolvedPlaylist;
    resolvedPlaylist.reserveInitialCapacity(playlist.size());

    for (auto& entry : playlist) {
        auto resolvedEntry = MediaMetadata::create(context, { entry->metadata() });
        if (resolvedEntry.hasException())
            return resolvedEntry.releaseException();
        
        resolvedPlaylist.uncheckedAppend(resolvedEntry.releaseReturnValue());
    }

    m_playlist = WTFMove(resolvedPlaylist);

    return { };
}
#endif

void MediaSession::setPlaybackState(MediaSessionPlaybackState state)
{
    if (m_playbackState == state)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state);

    auto currentPosition = this->currentPosition();
    if (m_positionState && currentPosition) {
        m_positionState->position = *currentPosition;
        m_timeAtLastPositionUpdate = MonotonicTime::now();
    }
    m_playbackState = state;
    notifyPlaybackStateObservers();
}

void MediaSession::setActionHandler(MediaSessionAction action, RefPtr<MediaSessionActionHandler>&& handler)
{
    if (handler) {
        ALWAYS_LOG(LOGIDENTIFIER, "adding ", action);
        m_actionHandlers.set(action, handler);
        auto platformCommand = platformCommandForMediaSessionAction(action);
        if (platformCommand != PlatformMediaSession::NoCommand)
            PlatformMediaSessionManager::sharedManager().addSupportedCommand(platformCommand);
    } else {
        if (m_actionHandlers.contains(action)) {
            ALWAYS_LOG(LOGIDENTIFIER, "removing ", action);
            m_actionHandlers.remove(action);
        }
        PlatformMediaSessionManager::sharedManager().removeSupportedCommand(platformCommandForMediaSessionAction(action));
    }

    notifyActionHandlerObservers();
}

bool MediaSession::callActionHandler(const MediaSessionActionDetails& actionDetails)
{
    if (auto handler = m_actionHandlers.get(actionDetails.action)) {
        handler->handleEvent(actionDetails);
        return true;
    }
    auto element = activeMediaElement();
    if (!element)
        return false;

    auto platformCommand = platformCommandForMediaSessionAction(actionDetails);
    if (!platformCommand)
        return false;
    element->didReceiveRemoteControlCommand(platformCommand->first, platformCommand->second);
    return true;
}

ExceptionOr<void> MediaSession::setPositionState(Optional<MediaPositionState>&& state)
{
    if (state)
        ALWAYS_LOG(LOGIDENTIFIER, state.value());
    else
        ALWAYS_LOG(LOGIDENTIFIER, "{ }");

    if (!state) {
        m_positionState = WTF::nullopt;
        notifyPositionStateObservers();
        return { };
    }

    if (!(state->duration >= 0
        && state->position >= 0
        && state->position <= state->duration
        && std::isfinite(state->playbackRate)
        && state->playbackRate))
        return Exception { TypeError };

    m_positionState = WTFMove(state);
    m_lastReportedPosition = m_positionState->position;
    m_timeAtLastPositionUpdate = MonotonicTime::now();
    notifyPositionStateObservers();

    return { };
}

Optional<double> MediaSession::currentPosition() const
{
    if (!m_positionState || !m_lastReportedPosition)
        return WTF::nullopt;

    auto actualPlaybackRate = m_playbackState == MediaSessionPlaybackState::Playing ? m_positionState->playbackRate : 0;

    auto elapsedTime = (MonotonicTime::now() - m_timeAtLastPositionUpdate) * actualPlaybackRate;

    return std::max(0., std::min(*m_lastReportedPosition + elapsedTime.value(), m_positionState->duration));
}

Document* MediaSession::document() const
{
    if (!m_navigator || !m_navigator->window())
        return nullptr;
    return m_navigator->window()->document();
}

void MediaSession::metadataUpdated()
{
    notifyMetadataObservers();
}

void MediaSession::addObserver(Observer& observer)
{
    ASSERT(isMainThread());
    m_observers.add(observer);
}

void MediaSession::removeObserver(Observer& observer)
{
    ASSERT(isMainThread());
    m_observers.remove(observer);
}

void MediaSession::forEachObserver(const Function<void(Observer&)>& apply)
{
    ASSERT(isMainThread());
    auto protectedThis = makeRef(*this);
    m_observers.forEach(apply);
}

void MediaSession::notifyMetadataObservers()
{
    forEachObserver([this](auto& observer) {
        observer.metadataChanged(m_metadata);
    });
}

void MediaSession::notifyPositionStateObservers()
{
    forEachObserver([this](auto& observer) {
        observer.positionStateChanged(m_positionState);
    });
}

void MediaSession::notifyPlaybackStateObservers()
{
    forEachObserver([this](auto& observer) {
        observer.playbackStateChanged(m_playbackState);
    });
}

void MediaSession::notifyActionHandlerObservers()
{
    forEachObserver([](auto& observer) {
        observer.actionHandlersChanged();
    });
}

RefPtr<HTMLMediaElement> MediaSession::activeMediaElement() const
{
    auto* doc = document();
    if (!doc)
        return nullptr;

    return HTMLMediaElement::bestMediaElementForRemoteControls(MediaElementSession::PlaybackControlsPurpose::MediaSession, doc);
}

#if ENABLE(MEDIA_SESSION_COORDINATOR)
void MediaSession::notifyReadyStateObservers()
{
    forEachObserver([this](auto& observer) {
        observer.readyStateChanged(m_readyState);
    });
}
#endif

String MediaPositionState::toJSONString() const
{
    auto object = JSON::Object::create();

    object->setDouble("duration"_s, duration);
    object->setDouble("playbackRate"_s, playbackRate);
    object->setDouble("position"_s, position);

    return object->toJSONString();
}

}

#endif // ENABLE(MEDIA_SESSION)
