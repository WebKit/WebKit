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

#include "config.h"
#include "PipeWireCaptureDeviceManager.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "GStreamerCaptureDeviceManager.h"
#include "GStreamerVideoCaptureSource.h"
#include "MockRealtimeMediaSourceCenter.h"
#include <wtf/Scope.h>

namespace WebCore {

GST_DEBUG_CATEGORY(webkit_pipewire_capture_device_manager_debug);
#define GST_CAT_DEFAULT webkit_pipewire_capture_device_manager_debug

RefPtr<PipeWireCaptureDeviceManager> PipeWireCaptureDeviceManager::create(OptionSet<CaptureDevice::DeviceType> deviceTypes)
{
    return adoptRef(*new PipeWireCaptureDeviceManager(deviceTypes));
}

PipeWireCaptureDeviceManager::PipeWireCaptureDeviceManager(OptionSet<CaptureDevice::DeviceType> deviceTypes)
    : m_deviceTypes(deviceTypes)
    , m_pipewireDeviceProvider(adoptGRef(gst_device_provider_factory_get_by_name("pipewiredeviceprovider")))
{
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkit_pipewire_capture_device_manager_debug, "webkitpipewirecapturedevicemanager", 0, "WebKit PipeWire Capture Device Manager");
    });
}

void PipeWireCaptureDeviceManager::computeCaptureDevices(CompletionHandler<void()>&& callback)
{
    if (MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled()) {
        callback();
        return;
    }

#if PLATFORM(WPE)
    callback();
    return;
#endif
    if (!m_pipewireDeviceProvider || !gstObjectHasProperty(GST_OBJECT_CAST(m_pipewireDeviceProvider.get()), "fd"_s)) {
        GST_WARNING("PipeWire Device Provider is missing or too old. Please install PipeWire >= 0.3.64.");
        callback();
        return;
    }

    auto portal = DesktopPortalCamera::create();

    GST_DEBUG("Checking with Camera portal");
    if (!portal || !portal->isCameraPresent()) {
        GST_DEBUG("Portal not present or has no camera");
        callback();
        return;
    }

    GST_DEBUG("Attempting to access the camera");
    portal->accessCamera([this, callback = WTFMove(callback)](auto fd) mutable {
        if (!fd) {
            GST_DEBUG("Camera access unavailable");
            callback();
            return;
        }

        GST_DEBUG("FD from portal: %d", *fd);
        g_object_set(m_pipewireDeviceProvider.get(), "fd", *fd, nullptr);
        gst_device_provider_start(m_pipewireDeviceProvider.get());

        GList* devices = gst_device_provider_get_devices(m_pipewireDeviceProvider.get());
        GST_DEBUG("Provisioning VideoCaptureDeviceManager with %u device(s).", g_list_length(devices));
        auto& manager = GStreamerVideoCaptureDeviceManager::singleton();
        while (devices) {
            manager.addDevice(adoptGRef(GST_DEVICE_CAST(devices->data)));
            devices = g_list_delete_link(devices, devices);
        }

        gst_device_provider_stop(m_pipewireDeviceProvider.get());
        callback();
    });
}

CaptureSourceOrError PipeWireCaptureDeviceManager::createCaptureSource(const CaptureDevice& device, MediaDeviceHashSalts&& hashSalts, const MediaConstraints* constraints)
{
    GST_DEBUG("Creating capture source for device %s", device.persistentId().ascii().data());
    if (MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled())
        return GStreamerVideoCaptureSource::create(String { device.persistentId() }, WTFMove(hashSalts), constraints);

    // We don't support audio capture yet.
    if (!m_deviceTypes.contains(CaptureDevice::DeviceType::Camera))
        return CaptureSourceOrError({ { }, MediaAccessDenialReason::PermissionDenied });

    return GStreamerVideoCaptureSource::create(String { device.persistentId() }, WTFMove(hashSalts), constraints);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
