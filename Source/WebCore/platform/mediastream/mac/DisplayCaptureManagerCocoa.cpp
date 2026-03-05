/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DisplayCaptureManagerCocoa.h"
#include "RealtimeMediaSourceCenter.h"

#if ENABLE(MEDIA_STREAM)

#include "Logging.h"
#include <wtf/NeverDestroyed.h>

#if HAVE(SCREEN_CAPTURE_KIT)
#include "ScreenCaptureKitCaptureSource.h"
#endif

namespace WebCore {

DisplayCaptureManagerCocoa& DisplayCaptureManagerCocoa::singleton()
{
    static NeverDestroyed<DisplayCaptureManagerCocoa> manager;
    return manager.get();
}

const Vector<CaptureDevice>& DisplayCaptureManagerCocoa::captureDevices()
{
    return m_devices;
}

std::optional<CaptureDevice> DisplayCaptureManagerCocoa::screenCaptureDeviceWithPersistentID(const String& deviceID)
{
#if HAVE(SCREEN_CAPTURE_KIT)
    if (ScreenCaptureKitCaptureSource::isAvailable())
        return ScreenCaptureKitCaptureSource::screenCaptureDeviceWithPersistentID(deviceID);
    ASSERT_NOT_REACHED();
    return std::nullopt;
#else
    UNUSED_PARAM(deviceID);
    return std::nullopt;
#endif
}

std::optional<CaptureDevice> DisplayCaptureManagerCocoa::windowCaptureDeviceWithPersistentID(const String& deviceID)
{
    UNUSED_PARAM(deviceID);

#if HAVE(SCREEN_CAPTURE_KIT)
    if (ScreenCaptureKitCaptureSource::isAvailable())
        return ScreenCaptureKitCaptureSource::windowCaptureDeviceWithPersistentID(deviceID);
#endif

    return std::nullopt;
}

std::optional<CaptureDevice> DisplayCaptureManagerCocoa::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& id)
{
    switch (type) {
    case CaptureDevice::DeviceType::Screen:
        return screenCaptureDeviceWithPersistentID(id);
        break;

    case CaptureDevice::DeviceType::Window:
        return windowCaptureDeviceWithPersistentID(id);
        break;

    case CaptureDevice::DeviceType::SystemAudio:
    case CaptureDevice::DeviceType::Camera:
    case CaptureDevice::DeviceType::Microphone:
    case CaptureDevice::DeviceType::Speaker:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    return std::nullopt;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
