/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2013 Nokia Corporation and/or its subsidiary(-ies).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "MediaStreamTrack.h"

#if ENABLE(MEDIA_STREAM)

#include "CommonAtomStrings.h"
#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FrameLoader.h"
#include "JSDOMPromiseDeferred.h"
#include "JSMeteringMode.h"
#include "JSOverconstrainedError.h"
#include "JSPhotoCapabilities.h"
#include "JSPhotoSettings.h"
#include "LocalFrame.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "MediaDevices.h"
#include "MediaStream.h"
#include "MediaStreamPrivate.h"
#include "NavigatorMediaDevices.h"
#include "NetworkingContext.h"
#include "NotImplemented.h"
#include "OverconstrainedError.h"
#include "Page.h"
#include "PhotoCapabilities.h"
#include "PlatformMediaSessionManager.h"
#include "RealtimeMediaSourceCenter.h"
#include "ScriptExecutionContext.h"
#include "Settings.h"
#include "WebAudioSourceProvider.h"
#include <wtf/CompletionHandler.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/NativePromise.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaStreamTrack);

Ref<MediaStreamTrack> MediaStreamTrack::create(ScriptExecutionContext& context, Ref<MediaStreamTrackPrivate>&& privateTrack, RegisterCaptureTrackToOwner registerCaptureTrackToOwner)
{
    auto track = adoptRef(*new MediaStreamTrack(context, WTFMove(privateTrack)));
    track->suspendIfNeeded();

    if (track->isCaptureTrack() && !track->ended() && registerCaptureTrackToOwner == RegisterCaptureTrackToOwner::Yes)
        downcast<Document>(context).addCaptureSource(track->privateTrack().source());

    return track;
}

MediaStreamTrack::MediaStreamTrack(ScriptExecutionContext& context, Ref<MediaStreamTrackPrivate>&& privateTrack)
    : ActiveDOMObject(&context)
    , m_private(WTFMove(privateTrack))
    , m_muted(m_private->muted())
    , m_isCaptureTrack(is<Document>(context) && m_private->isCaptureTrack())
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_private->addObserver(*this);

    if (!isCaptureTrack())
        return;

    ASSERT(isMainThread());
    ASSERT(is<Document>(context));

    auto& settings = m_private->settings();
    if (settings.supportsGroupId()) {
        RefPtr window = downcast<Document>(context).domWindow();
        if (RefPtr mediaDevices = window ? NavigatorMediaDevices::mediaDevices(window->navigator()) : nullptr)
            m_groupId = mediaDevices->hashedGroupId(settings.groupId());
    }

    m_isInterrupted = m_private->interrupted();

    if (m_private->isAudio())
        PlatformMediaSessionManager::sharedManager().addAudioCaptureSource(*this);
}

MediaStreamTrack::~MediaStreamTrack()
{
    m_private->removeObserver(*this);

    if (!isCaptureTrack())
        return;

    if (m_private->isAudio())
        PlatformMediaSessionManager::sharedManager().removeAudioCaptureSource(*this);
}

const AtomString& MediaStreamTrack::kind() const
{
    if (m_kind.isNull())
        m_kind = m_private->isAudio() ? "audio"_s : "video"_s;
    return m_kind;
}

const String& MediaStreamTrack::id() const
{
    return m_private->id();
}

const String& MediaStreamTrack::label() const
{
    return m_private->label();
}

static AtomString contentHintToAtomString(MediaStreamTrackHintValue hint)
{
    switch (hint) {
    case MediaStreamTrackHintValue::Empty:
        return emptyAtom();
    case MediaStreamTrackHintValue::Speech:
        return "speech"_s;
    case MediaStreamTrackHintValue::Music:
        return "music"_s;
    case MediaStreamTrackHintValue::Motion:
        return "motion"_s;
    case MediaStreamTrackHintValue::Detail:
        return "detail"_s;
    case MediaStreamTrackHintValue::Text:
        return "text"_s;
    default:
        return emptyAtom();
    }
}

const AtomString& MediaStreamTrack::contentHint() const
{
    if (m_contentHint.isNull())
        m_contentHint = contentHintToAtomString(m_private->contentHint());

    return m_contentHint;
}

void MediaStreamTrack::setContentHint(const String& hintValue)
{
    MediaStreamTrackHintValue value;
    if (m_private->isAudio()) {
        if (hintValue.isEmpty())
            value = MediaStreamTrackHintValue::Empty;
        else if (hintValue == "speech"_s)
            value = MediaStreamTrackHintValue::Speech;
        else if (hintValue == "music"_s)
            value = MediaStreamTrackHintValue::Music;
        else
            return;
    } else {
        if (hintValue.isEmpty())
            value = MediaStreamTrackHintValue::Empty;
        else if (hintValue == "detail"_s)
            value = MediaStreamTrackHintValue::Detail;
        else if (hintValue == "motion"_s)
            value = MediaStreamTrackHintValue::Motion;
        else if (hintValue == "text"_s)
            value = MediaStreamTrackHintValue::Text;
        else
            return;
    }
    m_contentHint = { };
    m_private->setContentHint(value);
}

bool MediaStreamTrack::enabled() const
{
    return m_private->enabled();
}

void MediaStreamTrack::setEnabled(bool enabled)
{
    m_private->setEnabled(enabled);
}

bool MediaStreamTrack::muted() const
{
    return m_private->muted();
}

bool MediaStreamTrack::mutedForBindings() const
{
    return m_muted;
}

bool MediaStreamTrack::ended() const
{
    return m_ended || m_private->ended();
}

RefPtr<MediaStreamTrack> MediaStreamTrack::clone()
{
    if (!scriptExecutionContext())
        return nullptr;

    ALWAYS_LOG(LOGIDENTIFIER);

    auto clone = MediaStreamTrack::create(*scriptExecutionContext(), m_private->clone(), RegisterCaptureTrackToOwner::No);

    clone->m_readyState = m_readyState;
    if (clone->ended() && clone->m_readyState == State::Live)
        trackEnded(clone->m_private);

    return clone;
}

void MediaStreamTrack::stopTrack(StopMode mode)
{
    // NOTE: this method is called when the "stop" method is called from JS, using the "ImplementedAs" IDL attribute.
    // This is done because ActiveDOMObject requires a "stop" method.

    if (ended())
        return;

    ALWAYS_LOG(LOGIDENTIFIER, static_cast<int>(mode));

    // An 'ended' event is not posted if m_ended is true when trackEnded is called, so set it now if we are
    // not supposed to post the event.
    if (mode == StopMode::Silently) {
        m_ended = true;
        m_readyState = State::Ended;
    }

    m_private->endTrack();
    m_ended = true;

    configureTrackRendering();
}

MediaStreamTrack::TrackSettings MediaStreamTrack::getSettings() const
{
    auto& settings = m_private->settings();
    TrackSettings result;
    if (settings.supportsWidth())
        result.width = settings.width();
    if (settings.supportsHeight())
        result.height = settings.height();
    if (settings.supportsAspectRatio() && result.height && result.width)
        result.aspectRatio = *result.width / static_cast<double>(*result.height);
    if (settings.supportsFrameRate())
        result.frameRate = settings.frameRate();
    if (settings.supportsFacingMode())
        result.facingMode = convertEnumerationToString(settings.facingMode());
    if (settings.supportsVolume())
        result.volume = settings.volume();
    if (settings.supportsSampleRate())
        result.sampleRate = settings.sampleRate();
    if (settings.supportsSampleSize())
        result.sampleSize = settings.sampleSize();
    if (settings.supportsEchoCancellation())
        result.echoCancellation = settings.echoCancellation();
    if (settings.supportsDeviceId())
        result.deviceId = settings.deviceId();
    if (settings.supportsGroupId())
        result.groupId = m_groupId;
    if (settings.supportsDisplaySurface() && settings.displaySurface() != DisplaySurfaceType::Invalid)
        result.displaySurface = RealtimeMediaSourceSettings::displaySurface(settings.displaySurface());

    if (settings.supportsWhiteBalanceMode())
        result.whiteBalanceMode = convertEnumerationToString(settings.whiteBalanceMode());
    if (settings.supportsZoom())
        result.zoom = settings.zoom();
    if (settings.supportsTorch())
        result.torch = settings.torch();

    return result;
}

MediaStreamTrack::TrackCapabilities MediaStreamTrack::getCapabilities() const
{
    auto result = toMediaTrackCapabilities(m_private->capabilities(), m_groupId);

    auto settings = m_private->settings();
    if (settings.supportsDisplaySurface() && settings.displaySurface() != DisplaySurfaceType::Invalid)
        result.displaySurface = RealtimeMediaSourceSettings::displaySurface(settings.displaySurface());

    return result;
}

auto MediaStreamTrack::takePhoto(PhotoSettings&& settings) -> Ref<TakePhotoPromise>
{
    // https://w3c.github.io/mediacapture-image/#dom-imagecapture-takephoto
    // If the readyState of track provided in the constructor is not live, return
    // a promise rejected with a new DOMException whose name is InvalidStateError,
    // and abort these steps.
    if (m_ended)
        return TakePhotoPromise::createAndReject(Exception { ExceptionCode::InvalidStateError, "Track has ended"_s });

    return m_private->takePhoto(WTFMove(settings))->whenSettled(RunLoop::main(), [protectedThis = Ref { *this }] (auto&& result) mutable {

        // https://w3c.github.io/mediacapture-image/#dom-imagecapture-takephoto
        // If the operation cannot be completed for any reason (for example, upon
        // invocation of multiple takePhoto() method calls in rapid succession),
        // then reject p with a new DOMException whose name is UnknownError, and
        // abort these steps.
        if (!result)
            return TakePhotoPromise::createAndReject(Exception { ExceptionCode::UnknownError, WTFMove(result.error()) });

        RefPtr context = protectedThis->scriptExecutionContext();
        if (!context || context->activeDOMObjectsAreStopped() || protectedThis->m_ended)
            return TakePhotoPromise::createAndReject(Exception { ExceptionCode::OperationError, "Track has ended"_s });

        return TakePhotoPromise::createAndResolve(WTFMove(result.value()));
    });
}

auto MediaStreamTrack::getPhotoCapabilities() -> Ref<PhotoCapabilitiesPromise>
{
    // https://w3c.github.io/mediacapture-image/#dom-imagecapture-getphotocapabilities
    // If the readyState of track provided in the constructor is not live, return
    // a promise rejected with a new DOMException whose name is InvalidStateError,
    // and abort these steps.
    if (m_ended)
        return PhotoCapabilitiesPromise::createAndReject(Exception { ExceptionCode::InvalidStateError, "Track has ended"_s });

    return m_private->getPhotoCapabilities()->whenSettled(RunLoop::main(), [protectedThis = Ref { *this }] (auto&& result) mutable {

        // https://w3c.github.io/mediacapture-image/#ref-for-dom-imagecapture-getphotocapabilities②
        // If the data cannot be gathered for any reason (for example, the MediaStreamTrack being ended
        // asynchronously), then reject p with a new DOMException whose name is OperationError, and
        // abort these steps.
        if (!result)
            return PhotoCapabilitiesPromise::createAndReject(Exception { ExceptionCode::UnknownError, WTFMove(result.error()) });

        RefPtr context = protectedThis->scriptExecutionContext();
        if (!context || context->activeDOMObjectsAreStopped() || protectedThis->m_ended)
            return PhotoCapabilitiesPromise::createAndReject(Exception { ExceptionCode::OperationError, "Track has ended"_s });

        return PhotoCapabilitiesPromise::createAndResolve(WTFMove(result.value()));
    });
}

auto MediaStreamTrack::getPhotoSettings() -> Ref<PhotoSettingsPromise>
{
    if (m_ended)
        return PhotoSettingsPromise::createAndReject(Exception { ExceptionCode::InvalidStateError, "Track has ended"_s });

    return m_private->getPhotoSettings()->whenSettled(RunLoop::main(), [protectedThis = Ref { *this }] (auto&& result) mutable {

        // https://w3c.github.io/mediacapture-image/#ref-for-dom-imagecapture-getphotosettings②
        // If the data cannot be gathered for any reason (for example, the MediaStreamTrack being ended
        // asynchronously), then reject p with a new DOMException whose name is OperationError, and
        // abort these steps.
        if (!result)
            return PhotoSettingsPromise::createAndReject(Exception { ExceptionCode::UnknownError, WTFMove(result.error()) });

        RefPtr context = protectedThis->scriptExecutionContext();
        if (!context || context->activeDOMObjectsAreStopped() || protectedThis->m_ended)
            return PhotoSettingsPromise::createAndReject(Exception { ExceptionCode::OperationError, "Track has ended"_s });

        return PhotoSettingsPromise::createAndResolve(WTFMove(result.value()));
    });
}

static MediaConstraints createMediaConstraints(const std::optional<MediaTrackConstraints>& constraints)
{
    if (!constraints) {
        MediaConstraints validConstraints;
        validConstraints.isValid = true;
        return validConstraints;
    }
    return createMediaConstraints(constraints.value());
}

void MediaStreamTrack::applyConstraints(const std::optional<MediaTrackConstraints>& constraints, DOMPromiseDeferred<void>&& promise)
{
    if (m_ended) {
        promise.reject(Exception { ExceptionCode::InvalidAccessError, "Track has ended"_s });
        return;
    }

    m_private->applyConstraints(createMediaConstraints(constraints), [this, protectedThis = Ref { *this }, constraints, promise = WTFMove(promise)](auto&& error) mutable {
        queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [protectedThis = WTFMove(protectedThis), error = WTFMove(error), constraints, promise = WTFMove(promise)]() mutable {
            if (error) {
                promise.rejectType<IDLInterface<OverconstrainedError>>(OverconstrainedError::create(error->invalidConstraint, WTFMove(error->message)));
                return;
            }

            protectedThis->m_constraints = valueOrDefault(constraints);
            promise.resolve();
        });
    });
}

void MediaStreamTrack::addObserver(Observer& observer)
{
    m_observers.append(&observer);
}

void MediaStreamTrack::removeObserver(Observer& observer)
{
    m_observers.removeFirst(&observer);
}

MediaProducerMediaStateFlags MediaStreamTrack::captureState(const RealtimeMediaSource& source)
{
    switch (source.deviceType()) {
    case CaptureDevice::DeviceType::Microphone:
        if (source.muted())
            return MediaProducerMediaState::HasMutedAudioCaptureDevice;
        if (source.interrupted())
            return MediaProducerMediaState::HasInterruptedAudioCaptureDevice;
        if (source.isProducingData())
            return MediaProducerMediaState::HasActiveAudioCaptureDevice;
        break;
    case CaptureDevice::DeviceType::Camera:
        if (source.muted())
            return MediaProducerMediaState::HasMutedVideoCaptureDevice;
        if (source.interrupted())
            return MediaProducerMediaState::HasInterruptedVideoCaptureDevice;
        if (source.isProducingData())
            return MediaProducerMediaState::HasActiveVideoCaptureDevice;
        break;
    case CaptureDevice::DeviceType::Screen:
        if (source.muted())
            return MediaProducerMediaState::HasMutedScreenCaptureDevice;
        if (source.interrupted())
            return MediaProducerMediaState::HasInterruptedScreenCaptureDevice;
        if (source.isProducingData())
            return MediaProducerMediaState::HasActiveScreenCaptureDevice;
        break;
    case CaptureDevice::DeviceType::Window:
        if (source.muted())
            return MediaProducerMediaState::HasMutedWindowCaptureDevice;
        if (source.interrupted())
            return MediaProducerMediaState::HasInterruptedWindowCaptureDevice;
        if (source.isProducingData())
            return MediaProducerMediaState::HasActiveWindowCaptureDevice;
        break;
    case CaptureDevice::DeviceType::SystemAudio:
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
    }

    return MediaProducer::IsNotPlaying;
}

MediaProducerMediaStateFlags MediaStreamTrack::mediaState() const
{
    if (m_ended || !isCaptureTrack())
        return MediaProducer::IsNotPlaying;

    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document || !document->page())
        return MediaProducer::IsNotPlaying;

    return captureState(privateTrack().source());
}

void MediaStreamTrack::trackStarted(MediaStreamTrackPrivate&)
{
    ALWAYS_LOG(LOGIDENTIFIER);
    configureTrackRendering();
}

void MediaStreamTrack::trackEnded(MediaStreamTrackPrivate&)
{
    if (m_isCaptureTrack && m_private->isAudio())
        PlatformMediaSessionManager::sharedManager().removeAudioCaptureSource(*this);

    ALWAYS_LOG(LOGIDENTIFIER);

    if (m_isCaptureTrack && m_private->captureDidFail() && m_readyState != State::Ended)
        scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "A MediaStreamTrack ended due to a capture failure"_s);

    // http://w3c.github.io/mediacapture-main/#life-cycle
    // When a MediaStreamTrack track ends for any reason other than the stop() method being invoked, the User Agent must
    // queue a task that runs the following steps:
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this, muted = m_private->muted()] {
        // 1. If the track's readyState attribute has the value ended already, then abort these steps.
        if (!isAllowedToRunScript() || m_readyState == State::Ended)
            return;

        // 2. Set track's readyState attribute to ended.
        m_readyState = State::Ended;

        ALWAYS_LOG(LOGIDENTIFIER, "firing 'ended' event");

        // 3. Notify track's source that track is ended so that the source may be stopped, unless other MediaStreamTrack objects depend on it.
        // 4. Fire a simple event named ended at the object.
        dispatchEvent(Event::create(eventNames().endedEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });

    if (m_ended)
        return;

    for (auto& observer : m_observers)
        observer->trackDidEnd();

    configureTrackRendering();
}
    
void MediaStreamTrack::trackMutedChanged(MediaStreamTrackPrivate&)
{
    RefPtr context = scriptExecutionContext();
    if (scriptExecutionContext()->activeDOMObjectsAreStopped() || m_ended)
        return;

    Function<void()> updateMuted = [this, muted = m_private->muted()] {
        RefPtr context = scriptExecutionContext();
        if (!context || context->activeDOMObjectsAreStopped())
            return;

        if (m_muted == muted)
            return;

        m_muted = muted;
        dispatchEvent(Event::create(muted ? eventNames().muteEvent : eventNames().unmuteEvent, Event::CanBubble::No, Event::IsCancelable::No));
    };
    if (m_shouldFireMuteEventImmediately)
        updateMuted();
    else
        queueTaskKeepingObjectAlive(*this, TaskSource::Networking, WTFMove(updateMuted));

    configureTrackRendering();

    bool wasInterrupted = m_isInterrupted;
    m_isInterrupted = m_private->interrupted();
    if (isCaptureTrack() && wasInterrupted != m_isInterrupted && m_private->type() == RealtimeMediaSource::Type::Audio && context->settingsValues().muteCameraOnMicrophoneInterruptionEnabled)
        downcast<Document>(context)->updateVideoCaptureStateForMicrophoneInterruption(m_isInterrupted);
}

void MediaStreamTrack::trackSettingsChanged(MediaStreamTrackPrivate&)
{
    configureTrackRendering();
}

void MediaStreamTrack::trackConfigurationChanged(MediaStreamTrackPrivate&)
{
    queueTaskKeepingObjectAlive(*this, TaskSource::Networking, [this] {
        if (!scriptExecutionContext() || scriptExecutionContext()->activeDOMObjectsAreStopped() || m_private->muted() || ended())
            return;

        dispatchEvent(Event::create(eventNames().configurationchangeEvent, Event::CanBubble::No, Event::IsCancelable::No));
    });
}

void MediaStreamTrack::trackEnabledChanged(MediaStreamTrackPrivate&)
{
    configureTrackRendering();
}

void MediaStreamTrack::configureTrackRendering()
{
    RefPtr document = dynamicDowncast<Document>(scriptExecutionContext());
    if (!document)
        return;

    document->updateIsPlayingMedia();

    // 4.3.1
    // ... media from the source only flows when a MediaStreamTrack object is both unmuted and enabled
}

const char* MediaStreamTrack::activeDOMObjectName() const
{
    return "MediaStreamTrack";
}

void MediaStreamTrack::suspend(ReasonForSuspension reason)
{
    if (reason != ReasonForSuspension::BackForwardCache)
        return;

    // We only end capture tracks, other tracks (capture canvas, remote tracks) can still continue working.
    if (m_ended || !isCaptureTrack())
        return;

    stopTrack();

    queueTaskToDispatchEvent(*this, TaskSource::Networking, Event::create(eventNames().endedEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

bool MediaStreamTrack::virtualHasPendingActivity() const
{
    return !m_ended;
}

#if ENABLE(WEB_AUDIO)
RefPtr<WebAudioSourceProvider> MediaStreamTrack::createAudioSourceProvider()
{
#if ENABLE(WEB_AUDIO)
    return m_private->createAudioSourceProvider();
#else
    return nullptr;
#endif
}
#endif

bool MediaStreamTrack::isCapturingAudio() const
{
    ASSERT(isCaptureTrack() && m_private->isAudio());
    return !ended() && !muted();
}

bool MediaStreamTrack::wantsToCaptureAudio() const
{
    ASSERT(isCaptureTrack() && m_private->isAudio());
    return !ended() && (!muted() || m_private->interrupted());
}

UniqueRef<MediaStreamTrackDataHolder> MediaStreamTrack::detach()
{
    m_isDetached = true;
    return m_private->toDataHolder();
}

Ref<MediaStreamTrack> MediaStreamTrack::create(ScriptExecutionContext& context, UniqueRef<MediaStreamTrackDataHolder>&& dataHolder)
{
    auto privateTrack = MediaStreamTrackPrivate::create(Logger::create(&context), WTFMove(dataHolder), [identifier = context.identifier()](Function<void()>&& task) {
        ScriptExecutionContext::postTaskTo(identifier, [task = WTFMove(task)] (auto&) mutable {
            task();
        });
    });

    return MediaStreamTrack::create(context, WTFMove(privateTrack), RegisterCaptureTrackToOwner::No);
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaStreamTrack::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
