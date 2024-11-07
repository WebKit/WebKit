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

#include "DocumentInlines.h"
#include "DocumentLoader.h"
#include "EventNames.h"
#include "HTMLMediaElement.h"
#include "JSDOMPromiseDeferred.h"
#include "JSMediaPositionState.h"
#include "JSMediaSessionAction.h"
#include "JSMediaSessionPlaybackState.h"
#include "LocalDOMWindow.h"
#include "Logging.h"
#include "MediaMetadata.h"
#include "MediaSessionCoordinator.h"
#include "Navigator.h"
#include "NowPlayingInfo.h"
#include "Page.h"
#include "PlatformMediaSessionManager.h"
#include "UserMediaController.h"
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/JSONValues.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

static uint64_t nextLogIdentifier()
{
    static uint64_t logIdentifier = cryptographicallyRandomNumber<uint32_t>();
    return ++logIdentifier;
}

#if !RELEASE_LOG_DISABLED
static WTFLogChannel& logChannel()
{
    return LogMedia;
}

static ASCIILiteral logClassName()
{
    return "MediaSession"_s;
}
#endif

static PlatformMediaSession::RemoteControlCommandType platformCommandForMediaSessionAction(MediaSessionAction action)
{
    static constexpr std::pair<MediaSessionAction, PlatformMediaSession::RemoteControlCommandType> mappings[] {
        { MediaSessionAction::Play, PlatformMediaSession::RemoteControlCommandType::PlayCommand },
        { MediaSessionAction::Pause, PlatformMediaSession::RemoteControlCommandType::PauseCommand },
        { MediaSessionAction::Seekbackward, PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand },
        { MediaSessionAction::Seekforward, PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand },
        { MediaSessionAction::Previoustrack, PlatformMediaSession::RemoteControlCommandType::PreviousTrackCommand },
        { MediaSessionAction::Nexttrack, PlatformMediaSession::RemoteControlCommandType::NextTrackCommand },
        { MediaSessionAction::Skipad, PlatformMediaSession::RemoteControlCommandType::NextTrackCommand },
        { MediaSessionAction::Stop, PlatformMediaSession::RemoteControlCommandType::StopCommand },
        { MediaSessionAction::Seekto, PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand },
    };
    static constexpr SortedArrayMap map { mappings };
    return map.get(action, PlatformMediaSession::RemoteControlCommandType::NoCommand);
}

static std::optional<std::pair<PlatformMediaSession::RemoteControlCommandType, PlatformMediaSession::RemoteCommandArgument>> platformCommandForMediaSessionAction(const MediaSessionActionDetails& actionDetails)
{
    PlatformMediaSession::RemoteControlCommandType command = PlatformMediaSession::RemoteControlCommandType::NoCommand;
    PlatformMediaSession::RemoteCommandArgument argument;

    switch (actionDetails.action) {
    case MediaSessionAction::Play:
        command = PlatformMediaSession::RemoteControlCommandType::PlayCommand;
        break;
    case MediaSessionAction::Pause:
        command = PlatformMediaSession::RemoteControlCommandType::PauseCommand;
        break;
    case MediaSessionAction::Seekbackward:
        command = PlatformMediaSession::RemoteControlCommandType::SkipBackwardCommand;
        argument.time = actionDetails.seekOffset;
        break;
    case MediaSessionAction::Seekforward:
        command = PlatformMediaSession::RemoteControlCommandType::SkipForwardCommand;
        argument.time = actionDetails.seekOffset;
        break;
    case MediaSessionAction::Previoustrack:
        command = PlatformMediaSession::RemoteControlCommandType::PreviousTrackCommand;
        break;
    case MediaSessionAction::Nexttrack:
        command = PlatformMediaSession::RemoteControlCommandType::NextTrackCommand;
        break;
    case MediaSessionAction::Skipad:
        // Not supported at present.
        break;
    case MediaSessionAction::Stop:
        command = PlatformMediaSession::RemoteControlCommandType::StopCommand;
        break;
    case MediaSessionAction::Seekto:
        command = PlatformMediaSession::RemoteControlCommandType::SeekToPlaybackPositionCommand;
        argument.time = actionDetails.seekTime;
        argument.fastSeek = actionDetails.fastSeek;
        break;
    case MediaSessionAction::Settrack:
        // Not supported at present.
        break;
    case MediaSessionAction::Togglecamera:
    case MediaSessionAction::Togglemicrophone:
    case MediaSessionAction::Togglescreenshare:
    case MediaSessionAction::Voiceactivity:
        break;
    }
    if (command == PlatformMediaSession::RemoteControlCommandType::NoCommand)
        return { };

    return std::make_pair(command, argument);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(MediaSession);

Ref<MediaSession> MediaSession::create(Navigator& navigator)
{
    auto session = adoptRef(*new MediaSession(navigator));
    session->suspendIfNeeded();
    return session;
}

MediaSession::MediaSession(Navigator& navigator)
    : ActiveDOMObject(navigator.scriptExecutionContext())
    , m_navigator(navigator)
#if ENABLE(MEDIA_SESSION_COORDINATOR)
    , m_coordinator(MediaSessionCoordinator::create(navigator.scriptExecutionContext()))
#endif
{
    m_logger = &Document::sharedLogger();
    m_logIdentifier = nextLogIdentifier();

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    RefPtr frame = navigator.frame();
    auto* page = frame ? frame->page() : nullptr;
    if (page && page->mediaSessionCoordinator())
        m_coordinator->setMediaSessionCoordinatorPrivate(*page->mediaSessionCoordinator());
    m_coordinator->setMediaSession(this);
#endif

    ALWAYS_LOG(LOGIDENTIFIER);
}

MediaSession::~MediaSession()
{
    if (m_metadata)
        m_metadata->resetMediaSession();
    if (m_defaultMetadata)
        m_defaultMetadata->resetMediaSession();
}

void MediaSession::suspend(ReasonForSuspension reason)
{
#if ENABLE(MEDIA_SESSION_COORDINATOR)
    if (reason == ReasonForSuspension::BackForwardCache)
        m_coordinator->leave();
#else
    UNUSED_PARAM(reason);
#endif
}

void MediaSession::stop()
{
#if ENABLE(MEDIA_SESSION_COORDINATOR)
    m_coordinator->close();
#endif
}

bool MediaSession::virtualHasPendingActivity() const
{
    return hasActiveActionHandlers();
}

void MediaSession::setMetadata(RefPtr<MediaMetadata>&& metadata)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    if (m_metadata)
        m_metadata->resetMediaSession();
    m_metadata = WTFMove(metadata);
    if (m_metadata)
        m_metadata->setMediaSession(*this);
    notifyMetadataObservers(m_metadata);
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
#endif

#if ENABLE(MEDIA_SESSION_PLAYLIST)
ExceptionOr<void> MediaSession::setPlaylist(ScriptExecutionContext& context, Vector<Ref<MediaMetadata>>&& playlist)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    Vector<Ref<MediaMetadata>> resolvedPlaylist;
    resolvedPlaylist.reserveInitialCapacity(playlist.size());

    for (auto& entry : playlist) {
        auto resolvedEntry = MediaMetadata::create(context, { entry->metadata() });
        if (resolvedEntry.hasException())
            return resolvedEntry.releaseException();
        
        resolvedPlaylist.append(resolvedEntry.releaseReturnValue());
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

    updateReportedPosition();

    m_playbackState = state;
    notifyPlaybackStateObservers();
}

ExceptionOr<void> MediaSession::setActionHandler(MediaSessionAction action, RefPtr<MediaSessionActionHandler>&& handler)
{
#if ENABLE(MEDIA_STREAM)
    RefPtr document = this->document();
    if (document && !document->settings().mediaSessionCaptureToggleAPIEnabled() && (action == MediaSessionAction::Togglecamera || action == MediaSessionAction::Togglemicrophone || action == MediaSessionAction::Togglescreenshare || action == MediaSessionAction::Voiceactivity))
        return Exception { ExceptionCode::TypeError, makeString("Argument 1 ('action') to MediaSession.setActionHandler must be a value other than '"_s, convertEnumerationToString(action), "'"_s) };

    if (action == MediaSessionAction::Voiceactivity) {
        if (RefPtr document = this->document())
            document->setShouldListenToVoiceActivity(!!handler);
    }
#endif

    if (handler) {
        ALWAYS_LOG(LOGIDENTIFIER, "adding ", action);
        {
            Locker lock { m_actionHandlersLock };
            m_actionHandlers.set(action, handler);
        }
        auto platformCommand = platformCommandForMediaSessionAction(action);
        if (platformCommand != PlatformMediaSession::RemoteControlCommandType::NoCommand)
            PlatformMediaSessionManager::singleton().addSupportedCommand(platformCommand);
    } else {
        bool containedAction;
        {
            Locker lock { m_actionHandlersLock };
            containedAction = m_actionHandlers.remove(action);
        }

        if (containedAction)
            ALWAYS_LOG(LOGIDENTIFIER, "removing ", action);
        PlatformMediaSessionManager::singleton().removeSupportedCommand(platformCommandForMediaSessionAction(action));
    }

    notifyActionHandlerObservers();
    return { };
}

void MediaSession::callActionHandler(const MediaSessionActionDetails& actionDetails, DOMPromiseDeferred<void>&& promise)
{
    ALWAYS_LOG(LOGIDENTIFIER);

    if (!callActionHandler(actionDetails, TriggerGestureIndicator::No)) {
        promise.reject(ExceptionCode::InvalidStateError);
        return;
    }

    promise.resolve();
}

bool MediaSession::callActionHandler(const MediaSessionActionDetails& actionDetails, TriggerGestureIndicator triggerGestureIndicator)
{
    RefPtr<MediaSessionActionHandler> handler;
    {
        Locker lock { m_actionHandlersLock };
        handler = m_actionHandlers.get(actionDetails.action);
    }

    if (handler) {
        std::optional<UserGestureIndicator> maybeGestureIndicator;
        if (triggerGestureIndicator == TriggerGestureIndicator::Yes)
            maybeGestureIndicator.emplace(IsProcessingUserGesture::Yes, document());
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

ExceptionOr<void> MediaSession::setPositionState(std::optional<MediaPositionState>&& state)
{
    if (state)
        ALWAYS_LOG(LOGIDENTIFIER, state.value());
    else
        ALWAYS_LOG(LOGIDENTIFIER, "{ }");

    if (!state) {
        m_positionState = std::nullopt;
        notifyPositionStateObservers();
        return { };
    }

    if (!(state->duration >= 0
        && state->position >= 0
        && state->position <= state->duration
        && std::isfinite(state->playbackRate)
        && state->playbackRate))
        return Exception { ExceptionCode::TypeError };

    m_positionState = WTFMove(state);
    m_lastReportedPosition = m_positionState->position;
    m_timeAtLastPositionUpdate = MonotonicTime::now();
    notifyPositionStateObservers();

    return { };
}

std::optional<double> MediaSession::currentPosition() const
{
    if (!m_positionState || !m_lastReportedPosition)
        return std::nullopt;

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

void MediaSession::metadataUpdated(const MediaMetadata& metadata)
{
    notifyMetadataObservers(const_cast<MediaMetadata*>(&metadata));
}

bool MediaSession::hasObserver(MediaSessionObserver& observer) const
{
    ASSERT(isMainThread());
    return m_observers.contains(observer);
}

void MediaSession::addObserver(MediaSessionObserver& observer)
{
    ASSERT(isMainThread());
    m_observers.add(observer);
}

void MediaSession::removeObserver(MediaSessionObserver& observer)
{
    ASSERT(isMainThread());
    m_observers.remove(observer);
}

void MediaSession::forEachObserver(const Function<void(MediaSessionObserver&)>& apply)
{
    ASSERT(isMainThread());
    Ref protectedThis { *this };
    m_observers.forEach(apply);
}

void MediaSession::notifyMetadataObservers(const RefPtr<MediaMetadata>& metadata)
{
    forEachObserver([&](auto& observer) {
        observer.metadataChanged(metadata);
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
    RefPtr document = this->document();
    if (!document)
        return nullptr;

    return HTMLMediaElement::bestMediaElementForRemoteControls(MediaElementSession::PlaybackControlsPurpose::MediaSession, document.get());
}

void MediaSession::updateReportedPosition()
{
    auto currentPosition = this->currentPosition();
    if (m_positionState && currentPosition) {
        m_lastReportedPosition = m_positionState->position = *currentPosition;
        m_timeAtLastPositionUpdate = MonotonicTime::now();
    }
}

void MediaSession::willBeginPlayback()
{
    updateReportedPosition();
    m_playbackState = MediaSessionPlaybackState::Playing;
    notifyPositionStateObservers();
}

void MediaSession::willPausePlayback()
{
    updateReportedPosition();
    m_playbackState = MediaSessionPlaybackState::Paused;
    notifyPositionStateObservers();
}

static Vector<URL> fallbackArtwork(DocumentLoader* loader)
{
    if (!loader)
        return { };
    size_t size = 0;
    for (const auto& icon : loader->linkIcons()) {
        if (icon.url.protocolIsInHTTPFamily())
            size++;
    }
    if (!size)
        return { };

    Vector<URL> images;
    images.reserveInitialCapacity(size);
    for (const auto& icon : loader->linkIcons()) {
        if (icon.url.protocolIsInHTTPFamily())
            images.append(icon.url);
    };
    return images;
}

void MediaSession::updateNowPlayingInfo(NowPlayingInfo& info)
{
    if (auto positionState = this->positionState()) {
        info.duration = positionState->duration;
        info.rate = positionState->playbackRate;
    }
    if (auto currentPosition = this->currentPosition())
        info.currentTime = *currentPosition;

    if (!m_defaultArtworkAttempted && (!m_metadata || m_metadata->artwork().isEmpty())) {
        m_defaultArtworkAttempted = true;
        if (auto images = fallbackArtwork(document() ? document()->loader() : nullptr); images.size())
            m_defaultMetadata = MediaMetadata::create(*this, WTFMove(images));
    }

    if (RefPtr metadataWithImage = m_metadata && m_metadata->artworkImage() ? m_metadata : (m_defaultMetadata && m_defaultMetadata->artworkImage() ? m_defaultMetadata : nullptr)) {
        ASSERT(metadataWithImage->artworkImage()->data(), "An image must always have associated data");
        info.metadata.artwork = { { metadataWithImage->artworkSrc(), metadataWithImage->artworkImage()->mimeType(), metadataWithImage->artworkImage() } };
    }
    if (m_metadata) {
        info.metadata.title = m_metadata->title();
        info.metadata.artist = m_metadata->artist();
        info.metadata.album = m_metadata->album();
    }
}

#if ENABLE(MEDIA_STREAM)
void MediaSession::updateCaptureState(bool isActive, DOMPromiseDeferred<void>&& promise, MediaProducerMediaCaptureKind kind)
{
    RefPtr document = this->document();
    if (!document->isFullyActive()) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Document is not fully active or does not have focus"_s });
        return;
    }

    if (isActive && (document->hidden() || !UserGestureIndicator::currentUserGesture())) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Activating capture must be called from a user gesture handler."_s });
        return;
    }

    auto* controller = UserMediaController::from(document->page());
    if (!controller) {
        promise.reject(Exception { ExceptionCode::InvalidStateError, "Unable to proceed with the request."_s });
        return;
    }

    if (!document->isCapturing()) {
        promise.resolve();
        return;
    }

    controller->updateCaptureState(*document, isActive, kind, [weakDocument = WeakPtr { document.get() }, promise = WTFMove(promise)] (auto&& exception) mutable {
        RefPtr protectedDocument = weakDocument.get();
        if (!protectedDocument)
            return;
        protectedDocument->eventLoop().queueTask(TaskSource::MediaElement, [promise = WTFMove(promise), exception = WTFMove(exception)] () mutable {
            if (exception) {
                promise.reject(WTFMove(*exception));
                return;
            }
            promise.resolve();
        });
    });
}
#endif

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
