/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
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
#include "MediaDevices.h"

#if ENABLE(MEDIA_STREAM)

#include "Document.h"
#include "Event.h"
#include "EventNames.h"
#include "MediaDevicesRequest.h"
#include "MediaTrackSupportedConstraints.h"
#include "UserMediaRequest.h"
#include <wtf/RandomNumber.h>

namespace WebCore {

inline MediaDevices::MediaDevices(Document& document)
    : ContextDestructionObserver(&document)
    , m_scheduledEventTimer(*this, &MediaDevices::scheduledEventTimerFired)
    , m_weakPtrFactory(this)
{
    m_deviceChangedToken = RealtimeMediaSourceCenter::singleton().addDevicesChangedObserver([weakThis = createWeakPtr(), this]() {

        if (!weakThis)
            return;

        // FIXME: We should only dispatch an event if the user has been granted access to the type of
        // device that was added or removed.
        if (!m_scheduledEventTimer.isActive())
            m_scheduledEventTimer.startOneShot(Seconds(randomNumber() / 2));
    });
}

MediaDevices::~MediaDevices()
{
    if (m_deviceChangedToken)
        RealtimeMediaSourceCenter::singleton().removeDevicesChangedObserver(m_deviceChangedToken.value());
}

Ref<MediaDevices> MediaDevices::create(Document& document)
{
    return adoptRef(*new MediaDevices(document));
}

Document* MediaDevices::document() const
{
    return downcast<Document>(scriptExecutionContext());
}

static MediaConstraints createMediaConstraints(const Variant<bool, MediaTrackConstraints>& constraints)
{
    return WTF::switchOn(constraints,
        [&] (bool isValid) {
            MediaConstraints constraints;
            constraints.isValid = isValid;
            return constraints;
        },
        [&] (const MediaTrackConstraints& trackConstraints) {
            return createMediaConstraints(trackConstraints);
        }
    );
}

ExceptionOr<void> MediaDevices::getUserMedia(const StreamConstraints& constraints, Promise&& promise) const
{
    auto* document = this->document();
    if (!document)
        return Exception { InvalidStateError };

    auto audioConstraints = createMediaConstraints(constraints.audio);
    auto videoConstraints = createMediaConstraints(constraints.video);
    if (videoConstraints.isValid)
        videoConstraints.setDefaultVideoConstraints();
    return UserMediaRequest::start(*document, WTFMove(audioConstraints), WTFMove(videoConstraints), WTFMove(promise));
}

void MediaDevices::enumerateDevices(EnumerateDevicesPromise&& promise) const
{
    auto* document = this->document();
    if (!document)
        return;
    MediaDevicesRequest::create(*document, WTFMove(promise))->start();
}

MediaTrackSupportedConstraints MediaDevices::getSupportedConstraints()
{
    auto& supported = RealtimeMediaSourceCenter::singleton().supportedConstraints();
    MediaTrackSupportedConstraints result;
    result.width = supported.supportsWidth();
    result.height = supported.supportsHeight();
    result.aspectRatio = supported.supportsAspectRatio();
    result.frameRate = supported.supportsFrameRate();
    result.facingMode = supported.supportsFacingMode();
    result.volume = supported.supportsVolume();
    result.sampleRate = supported.supportsSampleRate();
    result.sampleSize = supported.supportsSampleSize();
    result.echoCancellation = supported.supportsEchoCancellation();
    result.deviceId = supported.supportsDeviceId();
    result.groupId = supported.supportsGroupId();
    return result;
}

void MediaDevices::scheduledEventTimerFired()
{
    dispatchEvent(Event::create(eventNames().devicechangeEvent, false, false));
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
