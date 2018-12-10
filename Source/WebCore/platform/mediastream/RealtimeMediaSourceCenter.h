/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include "ExceptionOr.h"
#include "MediaStreamRequest.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceFactory.h"
#include "RealtimeMediaSourceSupportedConstraints.h"
#include <wtf/Function.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CaptureDevice;
class CaptureDeviceManager;
class RealtimeMediaSourceSettings;
class RealtimeMediaSourceSupportedConstraints;
class TrackSourceInfo;

struct MediaConstraints;
    
class RealtimeMediaSourceCenter {
public:
    static RealtimeMediaSourceCenter& platformCenter();
    virtual ~RealtimeMediaSourceCenter();

    WEBCORE_EXPORT static RealtimeMediaSourceCenter& singleton();

    using ValidConstraintsHandler = WTF::Function<void(Vector<CaptureDevice>&& audioDeviceUIDs, Vector<CaptureDevice>&& videoDeviceUIDs, String&&)>;
    using InvalidConstraintsHandler = WTF::Function<void(const String& invalidConstraint)>;
    virtual void validateRequestConstraints(ValidConstraintsHandler&&, InvalidConstraintsHandler&&, const MediaStreamRequest&, String&&);

    using NewMediaStreamHandler = WTF::Function<void(RefPtr<MediaStreamPrivate>&&)>;
    virtual void createMediaStream(NewMediaStreamHandler&&, String&&, CaptureDevice&& audioDevice, CaptureDevice&& videoDevice, const MediaStreamRequest&);

    WEBCORE_EXPORT Vector<CaptureDevice> getMediaStreamDevices();
    WEBCORE_EXPORT std::optional<CaptureDevice> captureDeviceWithPersistentID(CaptureDevice::DeviceType, const String&);
    
    const RealtimeMediaSourceSupportedConstraints& supportedConstraints() { return m_supportedConstraints; }

    WEBCORE_EXPORT static AudioCaptureFactory& audioFactory();
    WEBCORE_EXPORT static void setAudioFactory(AudioCaptureFactory&);
    WEBCORE_EXPORT static void unsetAudioFactory(AudioCaptureFactory&);

    WEBCORE_EXPORT static VideoCaptureFactory& videoFactory();
    WEBCORE_EXPORT static void setVideoFactory(VideoCaptureFactory&);
    WEBCORE_EXPORT static void unsetVideoFactory(VideoCaptureFactory&);

    WEBCORE_EXPORT static DisplayCaptureFactory& displayCaptureFactory();
    WEBCORE_EXPORT static void setDisplayCaptureFactory(DisplayCaptureFactory&);
    WEBCORE_EXPORT static void unsetDisplayCaptureFactory(DisplayCaptureFactory&);

    WEBCORE_EXPORT String hashStringWithSalt(const String& id, const String& hashSalt);

    WEBCORE_EXPORT void setDevicesChangedObserver(std::function<void()>&&);

    void setVideoCapturePageState(bool, bool);

    void captureDevicesChanged();

protected:
    RealtimeMediaSourceCenter();

    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;

    WEBCORE_EXPORT virtual AudioCaptureFactory& audioFactoryPrivate() = 0;
    WEBCORE_EXPORT virtual VideoCaptureFactory& videoFactoryPrivate() = 0;
    WEBCORE_EXPORT virtual DisplayCaptureFactory& displayCaptureFactoryPrivate() = 0;

private:
    struct DeviceInfo {
        unsigned fitnessScore;
        CaptureDevice device;
    };

    void getDisplayMediaDevices(const MediaStreamRequest&, Vector<DeviceInfo>&, String&);
    void getUserMediaDevices(const MediaStreamRequest&, String&&, Vector<DeviceInfo>& audioDevices, Vector<DeviceInfo>& videoDevices, String&);

    WTF::Function<void()> m_deviceChangedObserver;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

