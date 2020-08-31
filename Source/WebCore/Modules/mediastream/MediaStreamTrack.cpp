/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "JSDOMPromiseDeferred.h"
#include "JSOverconstrainedError.h"
#include "Logging.h"
#include "MediaConstraints.h"
#include "MediaStream.h"
#include "MediaStreamPrivate.h"
#include "NetworkingContext.h"
#include "NotImplemented.h"
#include "OverconstrainedError.h"
#include "Page.h"
#include "PlatformMediaSessionManager.h"
#include "RealtimeMediaSourceCenter.h"
#include "RuntimeEnabledFeatures.h"
#include "ScriptExecutionContext.h"
#include <wtf/CompletionHandler.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

static HashSet<MediaStreamTrack*>& allCaptureTracks()
{
    static NeverDestroyed<HashSet<MediaStreamTrack*>> captureTracks;
    return captureTracks;
}

static MediaProducer::MediaStateFlags sourceCaptureState(RealtimeMediaSource&);

WTF_MAKE_ISO_ALLOCATED_IMPL(MediaStreamTrack);

Ref<MediaStreamTrack> MediaStreamTrack::create(ScriptExecutionContext& context, Ref<MediaStreamTrackPrivate>&& privateTrack)
{
    auto track = adoptRef(*new MediaStreamTrack(context, WTFMove(privateTrack)));
    track->suspendIfNeeded();

    if (track->isCaptureTrack())
        track->updateToPageMutedState();

    return track;
}

MediaStreamTrack::MediaStreamTrack(ScriptExecutionContext& context, Ref<MediaStreamTrackPrivate>&& privateTrack)
    : ActiveDOMObject(&context)
    , m_private(WTFMove(privateTrack))
    , m_isCaptureTrack(m_private->isCaptureTrack())
{
    ALWAYS_LOG(LOGIDENTIFIER);

    m_private->addObserver(*this);

    if (!isCaptureTrack())
        return;

    allCaptureTracks().add(this);

    if (m_private->type() == RealtimeMediaSource::Type::Audio)
        PlatformMediaSessionManager::sharedManager().addAudioCaptureSource(*this);
}

MediaStreamTrack::~MediaStreamTrack()
{
    m_private->removeObserver(*this);

    if (!isCaptureTrack())
        return;

    allCaptureTracks().remove(this);

    if (m_private->type() == RealtimeMediaSource::Type::Audio)
        PlatformMediaSessionManager::sharedManager().removeAudioCaptureSource(*this);
}

const AtomString& MediaStreamTrack::kind() const
{
    static MainThreadNeverDestroyed<const AtomString> audioKind("audio", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> videoKind("video", AtomString::ConstructFromLiteral);

    if (m_private->type() == RealtimeMediaSource::Type::Audio)
        return audioKind;
    return videoKind;
}

const String& MediaStreamTrack::id() const
{
    return m_private->id();
}

const String& MediaStreamTrack::label() const
{
    return m_private->label();
}

const AtomString& MediaStreamTrack::contentHint() const
{
    static MainThreadNeverDestroyed<const AtomString> speechHint("speech", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> musicHint("music", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> detailHint("detail", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> textHint("text", AtomString::ConstructFromLiteral);
    static MainThreadNeverDestroyed<const AtomString> motionHint("motion", AtomString::ConstructFromLiteral);

    switch (m_private->contentHint()) {
    case MediaStreamTrackPrivate::HintValue::Empty:
        return emptyAtom();
    case MediaStreamTrackPrivate::HintValue::Speech:
        return speechHint;
    case MediaStreamTrackPrivate::HintValue::Music:
        return musicHint;
    case MediaStreamTrackPrivate::HintValue::Motion:
        return motionHint;
    case MediaStreamTrackPrivate::HintValue::Detail:
        return detailHint;
    case MediaStreamTrackPrivate::HintValue::Text:
        return textHint;
    default:
        return emptyAtom();
    }
}

void MediaStreamTrack::setContentHint(const String& hintValue)
{
    MediaStreamTrackPrivate::HintValue value;
    if (m_private->type() == RealtimeMediaSource::Type::Audio) {
        if (hintValue == "")
            value = MediaStreamTrackPrivate::HintValue::Empty;
        else if (hintValue == "speech")
            value = MediaStreamTrackPrivate::HintValue::Speech;
        else if (hintValue == "music")
            value = MediaStreamTrackPrivate::HintValue::Music;
        else
            return;
    } else {
        if (hintValue == "")
            value = MediaStreamTrackPrivate::HintValue::Empty;
        else if (hintValue == "detail")
            value = MediaStreamTrackPrivate::HintValue::Detail;
        else if (hintValue == "motion")
            value = MediaStreamTrackPrivate::HintValue::Motion;
        else if (hintValue == "text")
            value = MediaStreamTrackPrivate::HintValue::Text;
        else
            return;
    }
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

auto MediaStreamTrack::readyState() const -> State
{
    return ended() ? State::Ended : State::Live;
}

bool MediaStreamTrack::ended() const
{
    return m_ended || m_private->ended();
}

RefPtr<MediaStreamTrack> MediaStreamTrack::clone()
{
    if (!scriptExecutionContext())
        return nullptr;

    return MediaStreamTrack::create(*scriptExecutionContext(), m_private->clone());
}

void MediaStreamTrack::stopTrack(StopMode mode)
{
    // NOTE: this method is called when the "stop" method is called from JS, using the "ImplementedAs" IDL attribute.
    // This is done because ActiveDOMObject requires a "stop" method.

    if (ended())
        return;

    // An 'ended' event is not posted if m_ended is true when trackEnded is called, so set it now if we are
    // not supposed to post the event.
    if (mode == StopMode::Silently)
        m_ended = true;

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
    if (settings.supportsAspectRatio() && settings.aspectRatio()) // FIXME: Why the check for zero here?
        result.aspectRatio = settings.aspectRatio();
    if (settings.supportsFrameRate())
        result.frameRate = settings.frameRate();
    if (settings.supportsFacingMode())
        result.facingMode = RealtimeMediaSourceSettings::facingMode(settings.facingMode());
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
        result.groupId = settings.groupId();

    // FIXME: shouldn't this include displaySurface and logicalSurface?

    return result;
}

static DoubleRange capabilityDoubleRange(const CapabilityValueOrRange& value)
{
    DoubleRange range;
    switch (value.type()) {
    case CapabilityValueOrRange::Double:
        range.min = value.value().asDouble;
        range.max = range.min;
        break;
    case CapabilityValueOrRange::DoubleRange:
        range.min = value.rangeMin().asDouble;
        range.max = value.rangeMax().asDouble;
        break;
    case CapabilityValueOrRange::Undefined:
    case CapabilityValueOrRange::ULong:
    case CapabilityValueOrRange::ULongRange:
        ASSERT_NOT_REACHED();
    }
    return range;
}

static LongRange capabilityIntRange(const CapabilityValueOrRange& value)
{
    LongRange range;
    switch (value.type()) {
    case CapabilityValueOrRange::ULong:
        range.min = value.value().asInt;
        range.max = range.min;
        break;
    case CapabilityValueOrRange::ULongRange:
        range.min = value.rangeMin().asInt;
        range.max = value.rangeMax().asInt;
        break;
    case CapabilityValueOrRange::Undefined:
    case CapabilityValueOrRange::Double:
    case CapabilityValueOrRange::DoubleRange:
        ASSERT_NOT_REACHED();
    }
    return range;
}

static Vector<String> capabilityStringVector(const Vector<RealtimeMediaSourceSettings::VideoFacingMode>& modes)
{
    Vector<String> result;
    result.reserveInitialCapacity(modes.size());
    for (auto& mode : modes)
        result.uncheckedAppend(RealtimeMediaSourceSettings::facingMode(mode));
    return result;
}

static Vector<bool> capabilityBooleanVector(RealtimeMediaSourceCapabilities::EchoCancellation cancellation)
{
    Vector<bool> result;
    result.reserveInitialCapacity(2);
    result.uncheckedAppend(true);
    if (cancellation == RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite)
        result.uncheckedAppend(false);
    return result;
}

MediaStreamTrack::TrackCapabilities MediaStreamTrack::getCapabilities() const
{
    auto capabilities = m_private->capabilities();
    TrackCapabilities result;
    if (capabilities.supportsWidth())
        result.width = capabilityIntRange(capabilities.width());
    if (capabilities.supportsHeight())
        result.height = capabilityIntRange(capabilities.height());
    if (capabilities.supportsAspectRatio())
        result.aspectRatio = capabilityDoubleRange(capabilities.aspectRatio());
    if (capabilities.supportsFrameRate())
        result.frameRate = capabilityDoubleRange(capabilities.frameRate());
    if (capabilities.supportsFacingMode())
        result.facingMode = capabilityStringVector(capabilities.facingMode());
    if (capabilities.supportsVolume())
        result.volume = capabilityDoubleRange(capabilities.volume());
    if (capabilities.supportsSampleRate())
        result.sampleRate = capabilityIntRange(capabilities.sampleRate());
    if (capabilities.supportsSampleSize())
        result.sampleSize = capabilityIntRange(capabilities.sampleSize());
    if (capabilities.supportsEchoCancellation())
        result.echoCancellation = capabilityBooleanVector(capabilities.echoCancellation());
    if (capabilities.supportsDeviceId())
        result.deviceId = capabilities.deviceId();
    if (capabilities.supportsGroupId())
        result.groupId = capabilities.groupId();
    return result;
}

static MediaConstraints createMediaConstraints(const Optional<MediaTrackConstraints>& constraints)
{
    if (!constraints) {
        MediaConstraints validConstraints;
        validConstraints.isValid = true;
        return validConstraints;
    }
    return createMediaConstraints(constraints.value());
}

void MediaStreamTrack::applyConstraints(const Optional<MediaTrackConstraints>& constraints, DOMPromiseDeferred<void>&& promise)
{
    m_promise = WTF::makeUnique<DOMPromiseDeferred<void>>(WTFMove(promise));

    auto completionHandler = [this, protectedThis = makeRef(*this), constraints](auto&& error) mutable {
        if (!m_promise)
            return;
        if (error) {
            m_promise->rejectType<IDLInterface<OverconstrainedError>>(OverconstrainedError::create(WTFMove(error->badConstraint), WTFMove(error->message)));
            return;
        }
        m_promise->resolve();
        m_constraints = constraints.valueOr(MediaTrackConstraints { });
    };
    m_private->applyConstraints(createMediaConstraints(constraints), WTFMove(completionHandler));
}

void MediaStreamTrack::addObserver(Observer& observer)
{
    m_observers.append(&observer);
}

void MediaStreamTrack::removeObserver(Observer& observer)
{
    m_observers.removeFirst(&observer);
}

MediaProducer::MediaStateFlags MediaStreamTrack::mediaState() const
{
    if (m_ended || !isCaptureTrack())
        return MediaProducer::IsNotPlaying;

    auto* document = this->document();
    if (!document || !document->page())
        return MediaProducer::IsNotPlaying;

    return sourceCaptureState(source());
}

MediaProducer::MediaStateFlags sourceCaptureState(RealtimeMediaSource& source)
{
    switch (source.deviceType()) {
    case CaptureDevice::DeviceType::Microphone:
        if (source.muted())
            return MediaProducer::HasMutedAudioCaptureDevice;
        if (source.interrupted())
            return MediaProducer::HasInterruptedAudioCaptureDevice;
        if (source.isProducingData())
            return MediaProducer::HasActiveAudioCaptureDevice;
        break;
    case CaptureDevice::DeviceType::Camera:
        if (source.muted())
            return MediaProducer::HasMutedVideoCaptureDevice;
        if (source.interrupted())
            return MediaProducer::HasInterruptedVideoCaptureDevice;
        if (source.isProducingData())
            return MediaProducer::HasActiveVideoCaptureDevice;
        break;
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Window:
        if (source.muted())
            return MediaProducer::HasMutedDisplayCaptureDevice;
        if (source.interrupted())
            return MediaProducer::HasInterruptedDisplayCaptureDevice;
        if (source.isProducingData())
            return MediaProducer::HasActiveDisplayCaptureDevice;
        break;
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
    }

    return MediaProducer::IsNotPlaying;
}

MediaProducer::MediaStateFlags MediaStreamTrack::captureState(Document& document)
{
    MediaProducer::MediaStateFlags state = MediaProducer::IsNotPlaying;
    for (auto* captureTrack : allCaptureTracks()) {
        if (captureTrack->document() != &document || captureTrack->ended())
            continue;
        state |= sourceCaptureState(captureTrack->source());
    }
    return state;
}

#if PLATFORM(IOS_FAMILY)
static bool isSourceCapturingForTrackInDocument(RealtimeMediaSource& source, Document& document)
{
    for (auto* track : allCaptureTracks()) {
        if (track->document() != &document || track->ended())
            continue;

        if (track->source().isSameAs(source))
            return true;
    }
    return false;
}
#endif

void MediaStreamTrack::updateCaptureAccordingToMutedState(Document& document)
{
#if PLATFORM(IOS_FAMILY)
    auto* page = document.page();
    if (!page)
        return;

    auto* activeAudioSource = RealtimeMediaSourceCenter::singleton().audioCaptureFactory().activeSource();
    if (activeAudioSource && isSourceCapturingForTrackInDocument(*activeAudioSource, document)) {
        bool pageMuted = page->mutedState() & MediaProducer::AudioAndVideoCaptureIsMuted;
        activeAudioSource->setMuted(pageMuted || (document.hidden() && document.settings().interruptAudioOnPageVisibilityChangeEnabled()));
    }

    auto* activeVideoSource = RealtimeMediaSourceCenter::singleton().videoCaptureFactory().activeSource();
    if (activeVideoSource && isSourceCapturingForTrackInDocument(*activeVideoSource, document)) {
        bool pageMuted = page->mutedState() & MediaProducer::AudioAndVideoCaptureIsMuted;
        activeVideoSource->setMuted(pageMuted || document.hidden());
    }
#else
    for (auto* captureTrack : allCaptureTracks()) {
        if (captureTrack->document() == &document && !captureTrack->ended())
            captureTrack->updateToPageMutedState();
    }
#endif
}

void MediaStreamTrack::updateToPageMutedState()
{
    ASSERT(isCaptureTrack());
    auto* page = document()->page();
    if (!page)
        return;

    switch (source().deviceType()) {
    case CaptureDevice::DeviceType::Microphone:
    case CaptureDevice::DeviceType::Camera:
        m_private->setMuted(page->mutedState() & MediaProducer::AudioAndVideoCaptureIsMuted);
        break;
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Window:
        m_private->setMuted(page->mutedState() & MediaProducer::ScreenCaptureIsMuted);
        break;
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }
}

void MediaStreamTrack::endCapture(Document& document)
{
    bool didEndCapture = false;
    for (auto* captureTrack : allCaptureTracks()) {
        if (captureTrack->document() != &document)
            continue;
        captureTrack->stopTrack(MediaStreamTrack::StopMode::PostEvent);
        didEndCapture = true;
    }
    if (didEndCapture)
        document.updateIsPlayingMedia();
}

void MediaStreamTrack::trackStarted(MediaStreamTrackPrivate&)
{
    configureTrackRendering();
}

void MediaStreamTrack::trackEnded(MediaStreamTrackPrivate&)
{
    // http://w3c.github.io/mediacapture-main/#life-cycle
    // When a MediaStreamTrack track ends for any reason other than the stop() method being invoked, the User Agent must queue a task that runs the following steps:
    // 1. If the track's readyState attribute has the value ended already, then abort these steps.
    if (m_ended)
        return;

    if (m_isCaptureTrack && m_private->source().captureDidFail())
        scriptExecutionContext()->addConsoleMessage(MessageSource::JS, MessageLevel::Error, "A MediaStreamTrack ended due to a capture failure"_s);

    // 2. Set track's readyState attribute to ended.
    m_ended = true;

    if (scriptExecutionContext()->activeDOMObjectsAreSuspended() || scriptExecutionContext()->activeDOMObjectsAreStopped())
        return;

    // 3. Notify track's source that track is ended so that the source may be stopped, unless other MediaStreamTrack objects depend on it.
    // 4. Fire a simple event named ended at the object.
    dispatchEvent(Event::create(eventNames().endedEvent, Event::CanBubble::No, Event::IsCancelable::No));

    for (auto& observer : m_observers)
        observer->trackDidEnd();

    configureTrackRendering();
}
    
void MediaStreamTrack::trackMutedChanged(MediaStreamTrackPrivate&)
{
    if (scriptExecutionContext()->activeDOMObjectsAreStopped() || m_ended)
        return;

    AtomString eventType = muted() ? eventNames().muteEvent : eventNames().unmuteEvent;
    queueTaskToDispatchEvent(*this, TaskSource::Networking, Event::create(eventType, Event::CanBubble::No, Event::IsCancelable::No));

    configureTrackRendering();
}

void MediaStreamTrack::trackSettingsChanged(MediaStreamTrackPrivate&)
{
    configureTrackRendering();
}

void MediaStreamTrack::trackEnabledChanged(MediaStreamTrackPrivate&)
{
    configureTrackRendering();
}

void MediaStreamTrack::configureTrackRendering()
{
    if (auto document = this->document())
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

AudioSourceProvider* MediaStreamTrack::audioSourceProvider()
{
    return m_private->audioSourceProvider();
}

Document* MediaStreamTrack::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

bool MediaStreamTrack::isCapturingAudio() const
{
    ASSERT(isCaptureTrack() && m_private->type() == RealtimeMediaSource::Type::Audio);
    return !ended() && !muted();
}

#if !RELEASE_LOG_DISABLED
WTFLogChannel& MediaStreamTrack::logChannel() const
{
    return LogWebRTC;
}
#endif

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
