/*
 * Copyright (C) 2013-2016 Apple, Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)
#include "RealtimeMediaSourceCenterMac.h"

#include "AVAudioSessionCaptureDeviceManager.h"
#include "AVCaptureDeviceManager.h"
#include "AVVideoCaptureSource.h"
#include "CoreAudioCaptureDeviceManager.h"
#include "CoreAudioCaptureSource.h"
#include "DisplayCaptureManagerCocoa.h"
#include "Logging.h"
#include "MediaStreamPrivate.h"
#include "ScreenDisplayCaptureSourceMac.h"
#include "WindowDisplayCaptureSourceMac.h"
#include <wtf/MainThread.h>

namespace WebCore {

class VideoCaptureSourceFactoryMac final : public VideoCaptureFactory {
public:
    CaptureSourceOrError createVideoCaptureSource(const CaptureDevice& device, String&& hashSalt, const MediaConstraints* constraints) final
    {
        ASSERT(device.type() == CaptureDevice::DeviceType::Camera);
        return AVVideoCaptureSource::create(String { device.persistentId() }, WTFMove(hashSalt), constraints);
    }

#if PLATFORM(IOS)
private:
    void setVideoCapturePageState(bool interrupted, bool pageMuted)
    {
        if (activeSource())
            activeSource()->setInterrupted(interrupted, pageMuted);
    }
#endif
};

class DisplayCaptureSourceFactoryMac final : public DisplayCaptureFactory {
public:
    CaptureSourceOrError createDisplayCaptureSource(const CaptureDevice& device, const MediaConstraints* constraints) final
    {
#if PLATFORM(IOS)
        UNUSED_PARAM(device);
        UNUSED_PARAM(constraints);
#endif
        switch (device.type()) {
        case CaptureDevice::DeviceType::Screen:
#if PLATFORM(MAC)
            return ScreenDisplayCaptureSourceMac::create(String { device.persistentId() }, constraints);
#endif
        case CaptureDevice::DeviceType::Window:
#if PLATFORM(MAC)
            return WindowDisplayCaptureSourceMac::create(String { device.persistentId() }, constraints);
#endif
        case CaptureDevice::DeviceType::Application:
        case CaptureDevice::DeviceType::Browser:
        case CaptureDevice::DeviceType::Microphone:
        case CaptureDevice::DeviceType::Camera:
        case CaptureDevice::DeviceType::Unknown:
            ASSERT_NOT_REACHED();
            break;
        }

        return { };
    }
};

RealtimeMediaSourceCenterMac& RealtimeMediaSourceCenterMac::singleton()
{
    ASSERT(isMainThread());
    static NeverDestroyed<RealtimeMediaSourceCenterMac> center;
    return center;
}

RealtimeMediaSourceCenter& RealtimeMediaSourceCenter::platformCenter()
{
    return RealtimeMediaSourceCenterMac::singleton();
}

RealtimeMediaSourceCenterMac::RealtimeMediaSourceCenterMac() = default;

RealtimeMediaSourceCenterMac::~RealtimeMediaSourceCenterMac() = default;


AudioCaptureFactory& RealtimeMediaSourceCenterMac::audioFactoryPrivate()
{
    return CoreAudioCaptureSource::factory();
}

VideoCaptureFactory& RealtimeMediaSourceCenterMac::videoFactoryPrivate()
{
    static NeverDestroyed<VideoCaptureSourceFactoryMac> factory;
    return factory.get();
}

DisplayCaptureFactory& RealtimeMediaSourceCenterMac::displayCaptureFactoryPrivate()
{
    static NeverDestroyed<DisplayCaptureSourceFactoryMac> factory;
    return factory.get();
}

CaptureDeviceManager& RealtimeMediaSourceCenterMac::audioCaptureDeviceManager()
{
#if PLATFORM(MAC)
    return CoreAudioCaptureDeviceManager::singleton();
#else
    return AVAudioSessionCaptureDeviceManager::singleton();
#endif
}

CaptureDeviceManager& RealtimeMediaSourceCenterMac::videoCaptureDeviceManager()
{
    return AVCaptureDeviceManager::singleton();
}

CaptureDeviceManager& RealtimeMediaSourceCenterMac::displayCaptureDeviceManager()
{
    return DisplayCaptureManagerCocoa::singleton();
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
