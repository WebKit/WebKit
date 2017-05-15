/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "EventTarget.h"
#include "ExceptionOr.h"
#include "JSDOMPromiseDeferred.h"
#include "MediaTrackConstraints.h"
#include "RealtimeMediaSourceCenter.h"
#include "Timer.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class MediaDeviceInfo;
class MediaStream;

struct MediaTrackSupportedConstraints;

class MediaDevices : public RefCounted<MediaDevices>, public ContextDestructionObserver, public EventTargetWithInlineData {
public:
    static Ref<MediaDevices> create(Document&);

    ~MediaDevices();

    Document* document() const;

    using Promise = DOMPromiseDeferred<IDLInterface<MediaStream>>;
    using EnumerateDevicesPromise = DOMPromiseDeferred<IDLSequence<IDLInterface<MediaDeviceInfo>>>;

    struct StreamConstraints {
        Variant<bool, MediaTrackConstraints> video;
        Variant<bool, MediaTrackConstraints> audio;
    };
    ExceptionOr<void> getUserMedia(const StreamConstraints&, Promise&&) const;
    void enumerateDevices(EnumerateDevicesPromise&&) const;
    MediaTrackSupportedConstraints getSupportedConstraints();

    using RefCounted<MediaDevices>::ref;
    using RefCounted<MediaDevices>::deref;

private:
    explicit MediaDevices(Document&);

    void scheduledEventTimerFired();

    // EventTargetWithInlineData.
    EventTargetInterface eventTargetInterface() const override { return MediaDevicesEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return m_scriptExecutionContext; }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }

    WeakPtr<MediaDevices> createWeakPtr() { return m_weakPtrFactory.createWeakPtr(); }

    Timer m_scheduledEventTimer;
    std::optional<RealtimeMediaSourceCenter::DevicesChangedObserverToken> m_deviceChangedToken;
    WeakPtrFactory<MediaDevices> m_weakPtrFactory;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
