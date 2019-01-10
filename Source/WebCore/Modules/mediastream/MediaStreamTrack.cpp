/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2011, 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "JSOverconstrainedError.h"
#include "MediaConstraints.h"
#include "MediaStream.h"
#include "MediaStreamPrivate.h"
#include "NotImplemented.h"
#include "OverconstrainedError.h"
#include "Page.h"
#include "RealtimeMediaSourceCenter.h"
#include "ScriptExecutionContext.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Ref<MediaStreamTrack> MediaStreamTrack::create(ScriptExecutionContext& context, Ref<MediaStreamTrackPrivate>&& privateTrack)
{
    return adoptRef(*new MediaStreamTrack(context, WTFMove(privateTrack)));
}

MediaStreamTrack::MediaStreamTrack(ScriptExecutionContext& context, Ref<MediaStreamTrackPrivate>&& privateTrack)
    : ActiveDOMObject(&context)
    , m_private(WTFMove(privateTrack))
    , m_taskQueue(context)
{
    suspendIfNeeded();

    m_private->addObserver(*this);

    if (auto document = this->document())
        document->addAudioProducer(this);
}

MediaStreamTrack::~MediaStreamTrack()
{
    m_private->removeObserver(*this);

    if (auto document = this->document())
        document->removeAudioProducer(this);
}

const AtomicString& MediaStreamTrack::kind() const
{
    static NeverDestroyed<AtomicString> audioKind("audio", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<AtomicString> videoKind("video", AtomicString::ConstructFromLiteral);

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

const AtomicString& MediaStreamTrack::contentHint() const
{
    static NeverDestroyed<const AtomicString> speechHint("speech", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> musicHint("music", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> detailHint("detail", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> textHint("text", AtomicString::ConstructFromLiteral);
    static NeverDestroyed<const AtomicString> motionHint("motion", AtomicString::ConstructFromLiteral);

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
    result.reserveCapacity(modes.size());
    for (auto& mode : modes)
        result.uncheckedAppend(RealtimeMediaSourceSettings::facingMode(mode));
    return result;
}

static Vector<bool> capabilityBooleanVector(RealtimeMediaSourceCapabilities::EchoCancellation cancellation)
{
    Vector<bool> result;
    result.reserveCapacity(2);
    result.uncheckedAppend(true);
    result.uncheckedAppend(cancellation == RealtimeMediaSourceCapabilities::EchoCancellation::ReadWrite);
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
    m_promise = WTFMove(promise);

    auto weakThis = makeWeakPtr(*this);
    auto failureHandler = [weakThis] (const String& failedConstraint, const String& message) {
        if (!weakThis || !weakThis->m_promise)
            return;
        weakThis->m_promise->rejectType<IDLInterface<OverconstrainedError>>(OverconstrainedError::create(failedConstraint, message).get());
    };
    auto successHandler = [weakThis, constraints] () {
        if (!weakThis || !weakThis->m_promise)
            return;
        weakThis->m_promise->resolve();
        weakThis->m_constraints = constraints.valueOr(MediaTrackConstraints { });
    };
    m_private->applyConstraints(createMediaConstraints(constraints), WTFMove(successHandler), WTFMove(failureHandler));
}

void MediaStreamTrack::addObserver(Observer& observer)
{
    m_observers.append(&observer);
}

void MediaStreamTrack::removeObserver(Observer& observer)
{
    m_observers.removeFirst(&observer);
}

void MediaStreamTrack::pageMutedStateDidChange()
{
    if (m_ended || !isCaptureTrack())
        return;

    Document* document = this->document();
    if (!document || !document->page())
        return;

    m_private->setMuted(document->page()->isMediaCaptureMuted());
}

MediaProducer::MediaStateFlags MediaStreamTrack::mediaState() const
{
    if (m_ended || !isCaptureTrack())
        return IsNotPlaying;

    Document* document = this->document();
    if (!document || !document->page())
        return IsNotPlaying;

    bool pageCaptureMuted = document->page()->isMediaCaptureMuted();

    if (source().type() == RealtimeMediaSource::Type::Audio) {
        if (source().interrupted() && !pageCaptureMuted)
            return HasInterruptedAudioCaptureDevice;
        if (muted())
            return HasMutedAudioCaptureDevice;
        if (m_private->isProducingData())
            return HasActiveAudioCaptureDevice;
    } else {
        auto deviceType = source().deviceType();
        ASSERT(deviceType == CaptureDevice::DeviceType::Camera || deviceType == CaptureDevice::DeviceType::Screen || deviceType == CaptureDevice::DeviceType::Window);
        if (source().interrupted() && !pageCaptureMuted)
            return deviceType == CaptureDevice::DeviceType::Camera ? HasInterruptedVideoCaptureDevice : HasInterruptedDisplayCaptureDevice;
        if (muted())
            return deviceType == CaptureDevice::DeviceType::Camera ? HasMutedVideoCaptureDevice : HasMutedDisplayCaptureDevice;
        if (m_private->isProducingData())
            return deviceType == CaptureDevice::DeviceType::Camera ? HasActiveVideoCaptureDevice : HasActiveDisplayCaptureDevice;
    }

    return IsNotPlaying;
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
    if (scriptExecutionContext()->activeDOMObjectsAreSuspended() || scriptExecutionContext()->activeDOMObjectsAreStopped() || m_ended)
        return;

    AtomicString eventType = muted() ? eventNames().muteEvent : eventNames().unmuteEvent;
    dispatchEvent(Event::create(eventType, Event::CanBubble::No, Event::IsCancelable::No));

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
    m_taskQueue.enqueueTask([this] {
        if (auto document = this->document())
            document->updateIsPlayingMedia();
    });

    // 4.3.1
    // ... media from the source only flows when a MediaStreamTrack object is both unmuted and enabled
}

void MediaStreamTrack::stop()
{
    stopTrack();
    m_taskQueue.close();
}

const char* MediaStreamTrack::activeDOMObjectName() const
{
    return "MediaStreamTrack";
}

bool MediaStreamTrack::canSuspendForDocumentSuspension() const
{
    return !hasPendingActivity();
}

bool MediaStreamTrack::hasPendingActivity() const
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

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
