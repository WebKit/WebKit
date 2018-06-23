/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(MEDIA_STREAM)

#include "Logging.h"
#include <wtf/NeverDestroyed.h>

#if PLATFORM(MAC)
#include "ScreenDisplayCaptureSourceMac.h"
#include <CoreGraphics/CGDirectDisplay.h>
#endif

namespace WebCore {

#if PLATFORM(MAC)
static void displayReconfigurationCallBack(CGDirectDisplayID, CGDisplayChangeSummaryFlags, void* userInfo)
{
    if (userInfo)
        reinterpret_cast<DisplayCaptureManagerCocoa*>(userInfo)->refreshCaptureDevices();
}
#endif

DisplayCaptureManagerCocoa& DisplayCaptureManagerCocoa::singleton()
{
    static NeverDestroyed<DisplayCaptureManagerCocoa> manager;
    return manager.get();
}

DisplayCaptureManagerCocoa::~DisplayCaptureManagerCocoa()
{
#if PLATFORM(MAC)
    if (m_observingDisplayChanges)
        CGDisplayRemoveReconfigurationCallback(displayReconfigurationCallBack, this);
#endif
}

const Vector<CaptureDevice>& DisplayCaptureManagerCocoa::captureDevices()
{
    static bool initialized;
    if (!initialized) {
        refreshCaptureDevices();

#if PLATFORM(MAC)
        CGDisplayRegisterReconfigurationCallback(displayReconfigurationCallBack, this);
#endif

        m_observingDisplayChanges = true;
        initialized = true;
    };
    
    return m_displays;
}

void DisplayCaptureManagerCocoa::refreshCaptureDevices()
{
#if PLATFORM(MAC)
    uint32_t displayCount = 0;
    auto err = CGGetActiveDisplayList(0, nullptr, &displayCount);
    if (err) {
        RELEASE_LOG(Media, "CGGetActiveDisplayList() returned error %d when trying to get display count", (int)err);
        return;
    }

    if (!displayCount) {
        RELEASE_LOG(Media, "CGGetActiveDisplayList() returned a display count of 0");
        return;
    }

    CGDirectDisplayID activeDisplays[displayCount];
    err = CGGetActiveDisplayList(displayCount, &(activeDisplays[0]), &displayCount);
    if (err) {
        RELEASE_LOG(Media, "CGGetActiveDisplayList() returned error %d when trying to get the active display list", (int)err);
        return;
    }

    bool haveDeviceChanges = false;
    for (auto displayID : activeDisplays) {
        if (std::any_of(m_displaysInternal.begin(), m_displaysInternal.end(), [displayID](auto& device) { return device.cgDirectDisplayID == displayID; }))
            continue;
        haveDeviceChanges = true;
        m_displaysInternal.append({ displayID, CGDisplayIDToOpenGLDisplayMask(displayID) });
    }

    for (auto& display : m_displaysInternal) {
        auto displayMask = CGDisplayIDToOpenGLDisplayMask(display.cgDirectDisplayID);
        if (display.cgOpenGLDisplayMask != displayMask) {
            display.cgOpenGLDisplayMask = displayMask;
            haveDeviceChanges = true;
        }
    }

    if (!haveDeviceChanges)
        return;

    int count = 0;
    m_displays = Vector<CaptureDevice>();
    for (auto& device : m_displaysInternal) {
        CaptureDevice displayDevice(String::number(device.cgDirectDisplayID), CaptureDevice::DeviceType::Screen, makeString("Screen ", String::number(count++)));
        displayDevice.setEnabled(device.cgOpenGLDisplayMask);
        m_displays.append(WTFMove(displayDevice));
    }
#endif
}

std::optional<CaptureDevice> DisplayCaptureManagerCocoa::screenCaptureDeviceWithPersistentID(const String& deviceID)
{
#if PLATFORM(MAC)
    bool ok;
    auto displayID = deviceID.toUIntStrict(&ok);
    if (!ok) {
        RELEASE_LOG(Media, "Display ID does not convert to 32-bit integer");
        return std::nullopt;
    }

    auto actualDisplayID = ScreenDisplayCaptureSourceMac::updateDisplayID(displayID);
    if (!actualDisplayID)
        return std::nullopt;

    auto device = CaptureDevice(String::number(actualDisplayID.value()), CaptureDevice::DeviceType::Screen, "ScreenCaptureDevice"_s);
    device.setEnabled(true);

    return device;
#else
    UNUSED_PARAM(deviceID);
    return std::nullopt;
#endif
}

std::optional<CaptureDevice> DisplayCaptureManagerCocoa::captureDeviceWithPersistentID(CaptureDevice::DeviceType type, const String& id)
{
    switch (type) {
    case CaptureDevice::DeviceType::Screen:
        return screenCaptureDeviceWithPersistentID(id);
        break;
            
    case CaptureDevice::DeviceType::Application:
    case CaptureDevice::DeviceType::Window:
    case CaptureDevice::DeviceType::Browser:
        break;

    case CaptureDevice::DeviceType::Camera:
    case CaptureDevice::DeviceType::Microphone:
    case CaptureDevice::DeviceType::Unknown:
        ASSERT_NOT_REACHED();
        break;
    }

    return std::nullopt;
}

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
