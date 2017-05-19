/*
 * Copyright (C) 2011 Ericsson AB. All rights reserved.
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2015 Igalia S.L. All rights reserved.
 * Copyright (C) 2015 Metrological. All rights reserved.
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

#if ENABLE(MEDIA_STREAM) && USE(OPENWEBRTC)

#include "CaptureDeviceManager.h"
#include "RealtimeMediaSourceCenter.h"
#include "RealtimeMediaSourceOwr.h"
#include <wtf/RefPtr.h>

namespace WebCore {

class CaptureDevice;
class MediaStreamPrivate;
class RealtimeMediaSource;
class MediaStreamSourcesQueryClient;
class TrackSourceInfo;

class RealtimeMediaSourceCenterOwr final : public RealtimeMediaSourceCenter {
public:
    RealtimeMediaSourceCenterOwr();
    ~RealtimeMediaSourceCenterOwr();

    void validateRequestConstraints(ValidConstraintsHandler&& validHandler, InvalidConstraintsHandler&& invalidHandler, const MediaConstraints& audioConstraints, const MediaConstraints& videoConstraints, String&&) final;

    void createMediaStream(NewMediaStreamHandler&&, const String& audioDeviceID, const String& videoDeviceID, const MediaConstraints* videoConstraints, const MediaConstraints* audioConstraints) final;

    Vector<CaptureDevice> getMediaStreamDevices() final;

    void mediaSourcesAvailable(GList* sources);

private:
    RealtimeMediaSource::AudioCaptureFactory& defaultAudioFactory() final { return m_defaultAudioFactory; }
    RealtimeMediaSource::VideoCaptureFactory& defaultVideoFactory() final { return m_defaultVideoFactory; }
    CaptureDeviceManager& defaultAudioCaptureDeviceManager() final { return m_defaultAudioCaptureDeviceManager; }
    CaptureDeviceManager& defaultVideoCaptureDeviceManager() final { return m_defaultVideoCaptureDeviceManager; }

    RealtimeMediaSource* firstSource(RealtimeMediaSource::Type);
    RealtimeMediaSourceOwrMap m_sourceMap;
    ValidConstraintsHandler m_validConstraintsHandler;
    InvalidConstraintsHandler m_invalidConstraintsHandler;
    NewMediaStreamHandler m_completionHandler;

    class AudioCaptureFactoryOwr : public RealtimeMediaSource::AudioCaptureFactory {
    private:
        CaptureSourceOrError createAudioCaptureSource(const String&, const MediaConstraints*) final { return { }; }
    };

    class VideoCaptureFactoryOwr : public RealtimeMediaSource::VideoCaptureFactory {
    private:
        CaptureSourceOrError createVideoCaptureSource(const String&, const MediaConstraints*) final { return { }; }
    };

    class CaptureDeviceManagerOwr : public CaptureDeviceManager {
    private:
        Vector<CaptureDevice>& captureDevices() final { return m_devices; }

        Vector<CaptureDevice> m_devices;
    };

    AudioCaptureFactoryOwr m_defaultAudioFactory;
    VideoCaptureFactoryOwr m_defaultVideoFactory;
    CaptureDeviceManagerOwr m_defaultAudioCaptureDeviceManager;
    CaptureDeviceManagerOwr m_defaultVideoCaptureDeviceManager;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(OPENWEBRTC)
