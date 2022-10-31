/*
 * Copyright (C) 2015 Ericsson AB. All rights reserved.
 * Copyright (C) 2016-2022 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "EventNames.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "IDLTypes.h"
#include "MediaTrackConstraints.h"
#include "RealtimeMediaSourceCenter.h"
#include "UserMediaClient.h"
#include <wtf/RobinHoodHashMap.h>
#include <wtf/RunLoop.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class MediaDeviceInfo;
class MediaStream;
class UserGestureToken;

struct MediaTrackSupportedConstraints;

template<typename IDLType> class DOMPromiseDeferred;

class MediaDevices final : public RefCounted<MediaDevices>, public ActiveDOMObject, public EventTarget {
    WTF_MAKE_ISO_ALLOCATED(MediaDevices);
public:
    static Ref<MediaDevices> create(Document&);

    ~MediaDevices();

    Document* document() const;

    using Promise = DOMPromiseDeferred<IDLInterface<MediaStream>>;
    using EnumerateDevicesPromise = DOMPromiseDeferred<IDLSequence<IDLInterface<MediaDeviceInfo>>>;

    enum class DisplayCaptureSurfaceType {
        Monitor,
        Window,
        Application,
        Browser,
    };

    struct StreamConstraints {
        std::variant<bool, MediaTrackConstraints> video;
        std::variant<bool, MediaTrackConstraints> audio;
    };
    void getUserMedia(StreamConstraints&&, Promise&&);

    struct DisplayMediaStreamConstraints {
        std::variant<bool, MediaTrackConstraints> video;
        std::variant<bool, MediaTrackConstraints> audio;
    };
    void getDisplayMedia(DisplayMediaStreamConstraints&&, Promise&&);

    void enumerateDevices(EnumerateDevicesPromise&&);
    MediaTrackSupportedConstraints getSupportedConstraints();

    String deviceIdToPersistentId(const String& deviceId) const { return m_audioOutputDeviceIdToPersistentId.get(deviceId); }

    using RefCounted<MediaDevices>::ref;
    using RefCounted<MediaDevices>::deref;

private:
    explicit MediaDevices(Document&);

    void scheduledEventTimerFired();
    bool addEventListener(const AtomString& eventType, Ref<EventListener>&&, const AddEventListenerOptions&) override;

    void exposeDevices(const Vector<CaptureDevice>&, MediaDeviceHashSalts&&, EnumerateDevicesPromise&&);
    void listenForDeviceChanges();

    friend class JSMediaDevicesOwner;

    // ActiveDOMObject
    const char* activeDOMObjectName() const final;
    void stop() final;
    bool virtualHasPendingActivity() const final;

    // EventTarget.
    EventTargetInterface eventTargetInterface() const final { return MediaDevicesEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const final { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() final { ref(); }
    void derefEventTarget() final { deref(); }

    enum class GestureAllowedRequest {
        Microphone = 1 << 0,
        Camera = 1 << 1,
        Display = 1 << 2,
    };
    bool computeUserGesturePriviledge(GestureAllowedRequest);

    RunLoop::Timer<MediaDevices> m_scheduledEventTimer;
    UserMediaClient::DeviceChangeObserverToken m_deviceChangeToken;
    const EventNames& m_eventNames; // Need to cache this so we can use it from GC threads.
    bool m_listeningForDeviceChanges { false };

    String m_groupIdHashSalt;

    OptionSet<GestureAllowedRequest> m_requestTypesForCurrentGesture;
    WeakPtr<UserGestureToken> m_currentGestureToken;

    MemoryCompactRobinHoodHashMap<String, String> m_audioOutputDeviceIdToPersistentId;
    String m_audioOutputDeviceId;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
