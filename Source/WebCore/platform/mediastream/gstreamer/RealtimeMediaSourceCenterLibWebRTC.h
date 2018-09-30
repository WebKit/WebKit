 /*
 * Copyright (C) 2018 Metrological Group B.V.
 * Author: Thibault Saunier <tsaunier@igalia.com>
 * Author: Alejandro G. Castro <alex@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#if ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)

#include "CaptureDeviceManager.h"
#include "LibWebRTCProvider.h"
#include "RealtimeMediaSource.h"
#include "RealtimeMediaSourceCenter.h"

#include <webrtc/api/peerconnectioninterface.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class RealtimeMediaSourceCenterLibWebRTC final : public RealtimeMediaSourceCenter {
public:
    WEBCORE_EXPORT static RealtimeMediaSourceCenterLibWebRTC& singleton();

    static VideoCaptureFactory& videoCaptureSourceFactory();
    static AudioCaptureFactory& audioCaptureSourceFactory();

private:
    friend class NeverDestroyed<RealtimeMediaSourceCenterLibWebRTC>;
    RealtimeMediaSourceCenterLibWebRTC();
    ~RealtimeMediaSourceCenterLibWebRTC();

    void setAudioFactory(AudioCaptureFactory& factory) final { m_audioFactoryOverride = &factory; }
    void unsetAudioFactory(AudioCaptureFactory&) final { m_audioFactoryOverride = nullptr; }

    AudioCaptureFactory& audioFactory() final;
    VideoCaptureFactory& videoFactory() final;
    DisplayCaptureFactory& displayCaptureFactory() final;

    CaptureDeviceManager& audioCaptureDeviceManager() final;
    CaptureDeviceManager& videoCaptureDeviceManager() final;
    CaptureDeviceManager& displayCaptureDeviceManager() final;

    AudioCaptureFactory* m_audioFactoryOverride { nullptr };
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(LIBWEBRTC)

