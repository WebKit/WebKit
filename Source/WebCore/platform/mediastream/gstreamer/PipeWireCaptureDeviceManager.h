/*
 * Copyright (C) 2025 Igalia S.L.
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

#include "DesktopPortal.h"
#include "MediaConstraints.h"
#include "MediaDeviceHashSalts.h"
#include "PipeWireCaptureDevice.h"
#include "RealtimeMediaSource.h"

#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>

namespace WebCore {

class PipeWireCaptureDeviceManager : public RefCounted<PipeWireCaptureDeviceManager> {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(PipeWireCaptureDeviceManager);
public:
    static RefPtr<PipeWireCaptureDeviceManager> create(OptionSet<CaptureDevice::DeviceType>);
    PipeWireCaptureDeviceManager(OptionSet<CaptureDevice::DeviceType>);

    void computeCaptureDevices(CompletionHandler<void()>&&);
    const Vector<CaptureDevice>& captureDevices() const { return m_devices; }
    CaptureSourceOrError createCaptureSource(const CaptureDevice&, MediaDeviceHashSalts&&, const MediaConstraints*);

private:
    OptionSet<CaptureDevice::DeviceType> m_deviceTypes;
    GRefPtr<GstDeviceProvider> m_pipewireDeviceProvider;
    Vector<CaptureDevice> m_devices;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
