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

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "CaptureDeviceManager.h"
#include "GRefPtrGStreamer.h"
#include "GStreamerCaptureDevice.h"
#include "RealtimeMediaSourceFactory.h"

namespace WebCore {

class GStreamerCaptureDeviceManager : public CaptureDeviceManager {
public:
    Optional<GStreamerCaptureDevice> gstreamerDeviceWithUID(const String&);

    const Vector<CaptureDevice>& captureDevices() final;
    virtual CaptureDevice::DeviceType deviceType() = 0;

private:
    void addDevice(GRefPtr<GstDevice>&&);
    void refreshCaptureDevices();

    GRefPtr<GstDeviceMonitor> m_deviceMonitor;
    Vector<GStreamerCaptureDevice> m_gstreamerDevices;
    Vector<CaptureDevice> m_devices;
};

class GStreamerAudioCaptureDeviceManager final : public GStreamerCaptureDeviceManager {
    friend class NeverDestroyed<GStreamerAudioCaptureDeviceManager>;
public:
    static GStreamerAudioCaptureDeviceManager& singleton();
    CaptureDevice::DeviceType deviceType() final { return CaptureDevice::DeviceType::Microphone; }
private:
    GStreamerAudioCaptureDeviceManager() = default;
    ~GStreamerAudioCaptureDeviceManager() = default;
};

class GStreamerVideoCaptureDeviceManager final : public GStreamerCaptureDeviceManager {
    friend class NeverDestroyed<GStreamerVideoCaptureDeviceManager>;
public:
    static GStreamerVideoCaptureDeviceManager& singleton();
    static VideoCaptureFactory& videoFactory();
    CaptureDevice::DeviceType deviceType() final { return CaptureDevice::DeviceType::Camera; }
private:
    GStreamerVideoCaptureDeviceManager() = default;
    ~GStreamerVideoCaptureDeviceManager() = default;
};

class GStreamerDisplayCaptureDeviceManager final : public GStreamerCaptureDeviceManager {
    friend class NeverDestroyed<GStreamerDisplayCaptureDeviceManager>;
public:
    static GStreamerDisplayCaptureDeviceManager& singleton();
    CaptureDevice::DeviceType deviceType() final { return CaptureDevice::DeviceType::Screen; }
private:
    GStreamerDisplayCaptureDeviceManager() = default;
    ~GStreamerDisplayCaptureDeviceManager() = default;
};

}

#endif // ENABLE(MEDIA_STREAM)  && USE(GSTREAMER)
