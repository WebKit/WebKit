/*
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaStreamTrackPrivate.h"

#if ENABLE(MEDIA_STREAM)

#include "GraphicsContext.h"
#include "IntRect.h"
#include "Logging.h"
#include "PlatformMediaSessionManager.h"
#include <wtf/UUID.h>

#if PLATFORM(COCOA)
#include "MediaStreamTrackAudioSourceProviderCocoa.h"
#elif ENABLE(WEB_AUDIO) && ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC) && USE(GSTREAMER)
#include "AudioSourceProviderGStreamer.h"
#else
#include "WebAudioSourceProvider.h"
#endif

namespace WebCore {

Ref<MediaStreamTrackPrivate> MediaStreamTrackPrivate::create(Ref<const Logger>&& logger, Ref<RealtimeMediaSource>&& source)
{
    return create(WTFMove(logger), WTFMove(source), createCanonicalUUIDString());
}

Ref<MediaStreamTrackPrivate> MediaStreamTrackPrivate::create(Ref<const Logger>&& logger, Ref<RealtimeMediaSource>&& source, String&& id)
{
    return adoptRef(*new MediaStreamTrackPrivate(WTFMove(logger), WTFMove(source), WTFMove(id)));
}

MediaStreamTrackPrivate::MediaStreamTrackPrivate(Ref<const Logger>&& trackLogger, Ref<RealtimeMediaSource>&& source, String&& id)
    : m_source(WTFMove(source))
    , m_id(WTFMove(id))
    , m_logger(WTFMove(trackLogger))
#if !RELEASE_LOG_DISABLED
    , m_logIdentifier(uniqueLogIdentifier())
#endif
{
    ASSERT(isMainThread());
    UNUSED_PARAM(trackLogger);
    ALWAYS_LOG(LOGIDENTIFIER);
#if !RELEASE_LOG_DISABLED
    m_source->setLogger(m_logger.copyRef(), m_logIdentifier);
#endif
    m_source->addObserver(*this);
}

MediaStreamTrackPrivate::~MediaStreamTrackPrivate()
{
    ASSERT(isMainThread());

    ALWAYS_LOG(LOGIDENTIFIER);
    m_source->removeObserver(*this);
}

void MediaStreamTrackPrivate::forEachObserver(const Function<void(Observer&)>& apply)
{
    ASSERT(isMainThread());
    ASSERT(!m_observers.hasNullReferences());
    auto protectedThis = makeRef(*this);
    m_observers.forEach(apply);
}

void MediaStreamTrackPrivate::addObserver(MediaStreamTrackPrivate::Observer& observer)
{
    ASSERT(isMainThread());
    m_observers.add(observer);
}

void MediaStreamTrackPrivate::removeObserver(MediaStreamTrackPrivate::Observer& observer)
{
    ASSERT(isMainThread());
    m_observers.remove(observer);
}

const String& MediaStreamTrackPrivate::label() const
{
    return m_source->name();
}

void MediaStreamTrackPrivate::setContentHint(HintValue hintValue)
{
    m_contentHint = hintValue;
}
    
bool MediaStreamTrackPrivate::muted() const
{
    return m_source->muted();
}

bool MediaStreamTrackPrivate::isCaptureTrack() const
{
    return m_source->isCaptureSource();
}

void MediaStreamTrackPrivate::setEnabled(bool enabled)
{
    if (m_isEnabled == enabled)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, enabled);

    // Always update the enabled state regardless of the track being ended.
    m_isEnabled = enabled;

    forEachObserver([this](auto& observer) {
        observer.trackEnabledChanged(*this);
    });
}

void MediaStreamTrackPrivate::endTrack()
{
    if (m_isEnded)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    // Set m_isEnded to true before telling the source it can stop, so if this is the
    // only track using the source and it does stop, we will only call each observer's
    // trackEnded method once.
    m_isEnded = true;
    updateReadyState();

    m_source->requestToEnd(*this);

    forEachObserver([this](auto& observer) {
        observer.trackEnded(*this);
    });
}

Ref<MediaStreamTrackPrivate> MediaStreamTrackPrivate::clone()
{
    auto clonedMediaStreamTrackPrivate = create(m_logger.copyRef(), m_source->clone());

    ALWAYS_LOG(LOGIDENTIFIER, clonedMediaStreamTrackPrivate->logIdentifier());

    clonedMediaStreamTrackPrivate->m_isEnabled = this->m_isEnabled;
    clonedMediaStreamTrackPrivate->m_isEnded = this->m_isEnded;
    clonedMediaStreamTrackPrivate->m_contentHint = this->m_contentHint;
    clonedMediaStreamTrackPrivate->updateReadyState();

    if (isProducingData())
        clonedMediaStreamTrackPrivate->startProducingData();

    return clonedMediaStreamTrackPrivate;
}

RealtimeMediaSource::Type MediaStreamTrackPrivate::type() const
{
    return m_source->type();
}

const RealtimeMediaSourceSettings& MediaStreamTrackPrivate::settings() const
{
    return m_source->settings();
}

const RealtimeMediaSourceCapabilities& MediaStreamTrackPrivate::capabilities() const
{
    return m_source->capabilities();
}

void MediaStreamTrackPrivate::applyConstraints(const MediaConstraints& constraints, RealtimeMediaSource::ApplyConstraintsHandler&& completionHandler)
{
    m_source->applyConstraints(constraints, WTFMove(completionHandler));
}

RefPtr<WebAudioSourceProvider> MediaStreamTrackPrivate::createAudioSourceProvider()
{
    ALWAYS_LOG(LOGIDENTIFIER);

#if PLATFORM(COCOA)
    return MediaStreamTrackAudioSourceProviderCocoa::create(*this);
#elif USE(LIBWEBRTC) && USE(GSTREAMER)
    return AudioSourceProviderGStreamer::create(*this);
#else
    return nullptr;
#endif
}

void MediaStreamTrackPrivate::sourceStarted()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    forEachObserver([this](auto& observer) {
        observer.trackStarted(*this);
    });
}

void MediaStreamTrackPrivate::sourceStopped()
{
    if (m_isEnded)
        return;

    ALWAYS_LOG(LOGIDENTIFIER);

    m_isEnded = true;
    updateReadyState();

    forEachObserver([this](auto& observer) {
        observer.trackEnded(*this);
    });
}

void MediaStreamTrackPrivate::sourceMutedChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    forEachObserver([this](auto& observer) {
        observer.trackMutedChanged(*this);
    });
}

void MediaStreamTrackPrivate::sourceSettingsChanged()
{
    ALWAYS_LOG(LOGIDENTIFIER);

    forEachObserver([this](auto& observer) {
        observer.trackSettingsChanged(*this);
    });
}

bool MediaStreamTrackPrivate::preventSourceFromStopping()
{
    ALWAYS_LOG(LOGIDENTIFIER, m_isEnded);

    // Do not allow the source to stop if we are still using it.
    return !m_isEnded;
}

void MediaStreamTrackPrivate::hasStartedProducingData()
{
    ASSERT(isMainThread());
    if (m_hasStartedProducingData)
        return;
    ALWAYS_LOG(LOGIDENTIFIER);
    m_hasStartedProducingData = true;
    updateReadyState();
}

void MediaStreamTrackPrivate::updateReadyState()
{
    ReadyState state = ReadyState::None;

    if (m_isEnded)
        state = ReadyState::Ended;
    else if (m_hasStartedProducingData)
        state = ReadyState::Live;

    if (state == m_readyState)
        return;

    ALWAYS_LOG(LOGIDENTIFIER, state == ReadyState::Ended ? "Ended" : "Live");

    m_readyState = state;
    forEachObserver([this](auto& observer) {
        observer.readyStateChanged(*this);
    });
}

void MediaStreamTrackPrivate::audioUnitWillStart()
{
    if (!m_isEnded)
        PlatformMediaSessionManager::sharedManager().sessionCanProduceAudioChanged();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaStreamTrackPrivate::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
