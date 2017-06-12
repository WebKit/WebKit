/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2016 Apple Inc. All rights reserved.
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
#include "RealtimeMediaSource.h"
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
    virtual ~RealtimeMediaSourceCenter();

    WEBCORE_EXPORT static RealtimeMediaSourceCenter& singleton();
    static void setSharedStreamCenterOverride(RealtimeMediaSourceCenter*);

    using ValidConstraintsHandler = WTF::Function<void(Vector<String>&& audioDeviceUIDs, Vector<String>&& videoDeviceUIDs, String&&)>;
    using InvalidConstraintsHandler = WTF::Function<void(const String& invalidConstraint)>;
    virtual void validateRequestConstraints(ValidConstraintsHandler&&, InvalidConstraintsHandler&&, const MediaConstraints& audioConstraints, const MediaConstraints& videoConstraints, String&&);

    using NewMediaStreamHandler = std::function<void(RefPtr<MediaStreamPrivate>&&)>;
    virtual void createMediaStream(NewMediaStreamHandler&&, const String& audioDeviceID, const String& videoDeviceID, const MediaConstraints* audioConstraints, const MediaConstraints* videoConstraints);

    WEBCORE_EXPORT virtual Vector<CaptureDevice> getMediaStreamDevices();
    
    const RealtimeMediaSourceSupportedConstraints& supportedConstraints() { return m_supportedConstraints; }

    virtual RealtimeMediaSource::AudioCaptureFactory& defaultAudioFactory() = 0;
    virtual RealtimeMediaSource::VideoCaptureFactory& defaultVideoFactory() = 0;

    virtual CaptureDeviceManager& defaultAudioCaptureDeviceManager() = 0;
    virtual CaptureDeviceManager& defaultVideoCaptureDeviceManager() = 0;

    WEBCORE_EXPORT void setAudioFactory(RealtimeMediaSource::AudioCaptureFactory&);
    WEBCORE_EXPORT void unsetAudioFactory(RealtimeMediaSource::AudioCaptureFactory&);
    WEBCORE_EXPORT RealtimeMediaSource::AudioCaptureFactory& audioFactory();

    WEBCORE_EXPORT void setVideoFactory(RealtimeMediaSource::VideoCaptureFactory&);
    WEBCORE_EXPORT void unsetVideoFactory(RealtimeMediaSource::VideoCaptureFactory&);
    WEBCORE_EXPORT RealtimeMediaSource::VideoCaptureFactory& videoFactory();

    WEBCORE_EXPORT void setAudioCaptureDeviceManager(CaptureDeviceManager&);
    WEBCORE_EXPORT void unsetAudioCaptureDeviceManager(CaptureDeviceManager&);
    CaptureDeviceManager& audioCaptureDeviceManager();

    WEBCORE_EXPORT void setVideoCaptureDeviceManager(CaptureDeviceManager&);
    WEBCORE_EXPORT void unsetVideoCaptureDeviceManager(CaptureDeviceManager&);
    CaptureDeviceManager& videoCaptureDeviceManager();

    String hashStringWithSalt(const String& id, const String& hashSalt);
    WEBCORE_EXPORT std::optional<CaptureDevice> captureDeviceWithUniqueID(const String& id, const String& hashSalt);
    WEBCORE_EXPORT ExceptionOr<void> setDeviceEnabled(const String&, bool);

    using DevicesChangedObserverToken = unsigned;
    DevicesChangedObserverToken addDevicesChangedObserver(std::function<void()>&&);
    void removeDevicesChangedObserver(DevicesChangedObserverToken);
    void captureDevicesChanged();

    void setVideoCapturePageState(bool, bool);

protected:
    RealtimeMediaSourceCenter();

    static RealtimeMediaSourceCenter& platformCenter();
    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;

    RealtimeMediaSource::AudioCaptureFactory* m_audioFactory { nullptr };
    RealtimeMediaSource::VideoCaptureFactory* m_videoFactory { nullptr };

    CaptureDeviceManager* m_audioCaptureDeviceManager { nullptr };
    CaptureDeviceManager* m_videoCaptureDeviceManager { nullptr };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)

