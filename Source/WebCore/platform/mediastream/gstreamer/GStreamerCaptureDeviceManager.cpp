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

#include "config.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "GStreamerCaptureDeviceManager.h"

#include "GStreamerCommon.h"
#include "GStreamerMockDeviceProvider.h"

namespace WebCore {

GST_DEBUG_CATEGORY(webkitGStreamerCaptureDeviceManagerDebugCategory);
#define GST_CAT_DEFAULT webkitGStreamerCaptureDeviceManagerDebugCategory

static gint sortDevices(gconstpointer a, gconstpointer b)
{
    GstDevice* adev = GST_DEVICE(a), *bdev = GST_DEVICE(b);
    GUniquePtr<GstStructure> aprops(gst_device_get_properties(adev));
    GUniquePtr<GstStructure> bprops(gst_device_get_properties(bdev));
    gboolean aIsDefault = FALSE, bIsDefault = FALSE;

    gst_structure_get_boolean(aprops.get(), "is-default", &aIsDefault);
    gst_structure_get_boolean(bprops.get(), "is-default", &bIsDefault);

    if (aIsDefault == bIsDefault) {
        GUniquePtr<char> aName(gst_device_get_display_name(adev));
        GUniquePtr<char> bName(gst_device_get_display_name(bdev));

        return g_strcmp0(aName.get(), bName.get());
    }

    return aIsDefault > bIsDefault ? -1 : 1;
}

GStreamerAudioCaptureDeviceManager& GStreamerAudioCaptureDeviceManager::singleton()
{
    static NeverDestroyed<GStreamerAudioCaptureDeviceManager> manager;
    return manager;
}

GStreamerVideoCaptureDeviceManager& GStreamerVideoCaptureDeviceManager::singleton()
{
    static NeverDestroyed<GStreamerVideoCaptureDeviceManager> manager;
    return manager;
}

void teardownGStreamerCaptureDeviceManagers()
{
    auto& audioManager = GStreamerAudioCaptureDeviceManager::singleton();
    audioManager.teardown();

    auto& videoManager = GStreamerVideoCaptureDeviceManager::singleton();
    videoManager.teardown();
}

GStreamerCaptureDeviceManager::GStreamerCaptureDeviceManager()
{
    ensureGStreamerInitialized();

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        GST_DEBUG_CATEGORY_INIT(webkitGStreamerCaptureDeviceManagerDebugCategory, "webkitcapturedevicemanager", 0, "WebKit Capture Device Manager");
        gst_device_provider_register(nullptr, "mock-device-provider", GST_RANK_PRIMARY, GST_TYPE_MOCK_DEVICE_PROVIDER);
    });

    RealtimeMediaSourceCenter::singleton().addDevicesChangedObserver(*this);
}

GStreamerCaptureDeviceManager::~GStreamerCaptureDeviceManager()
{
    teardown();
}

void GStreamerCaptureDeviceManager::teardown()
{
    GST_DEBUG_OBJECT(m_deviceMonitor.get(), "Tearing down");
    m_isTearingDown = true;
    stopMonitor();
    RealtimeMediaSourceCenter::singleton().removeDevicesChangedObserver(*this);
    m_devices.clear();
    m_gstreamerDevices.clear();
}

void GStreamerCaptureDeviceManager::stopMonitor()
{
    if (!m_deviceMonitor)
        return;

    auto bus = adoptGRef(gst_device_monitor_get_bus(m_deviceMonitor.get()));
    gst_bus_remove_watch(bus.get());
    gst_device_monitor_stop(m_deviceMonitor.get());
    m_deviceMonitor.clear();
}

void GStreamerCaptureDeviceManager::devicesChanged()
{
    GST_INFO_OBJECT(m_deviceMonitor.get(), "RealtimeMediaSourceCenter notified devices list update, clearing our internal cache");
    stopMonitor();
    m_gstreamerDevices.clear();
    m_devices.clear();
}

void GStreamerCaptureDeviceManager::deviceWillBeRemoved(const String& persistentId)
{
    stopCapturing(persistentId);
}

void GStreamerCaptureDeviceManager::registerCapturer(const RefPtr<GStreamerCapturer>& capturer)
{
    m_capturers.append(capturer);
}

void GStreamerCaptureDeviceManager::unregisterCapturer(const GStreamerCapturer& capturer)
{
    m_capturers.removeAllMatching([&](auto& item) -> bool {
        return item.get() == &capturer;
    });
}

void GStreamerCaptureDeviceManager::stopCapturing(const String& persistentId)
{
    GST_DEBUG("Stopping capturer for device with persistent ID: %s", persistentId.ascii().data());
    m_capturers.removeAllMatching([&persistentId](auto& capturer) -> bool {
        GST_DEBUG("Checking capturer with device persistent ID: %s", capturer->devicePersistentId().ascii().data());
        if (capturer->devicePersistentId() != persistentId)
            return false;

        capturer->stopDevice();
        return true;
    });
}

std::optional<GStreamerCaptureDevice> GStreamerCaptureDeviceManager::gstreamerDeviceWithUID(const String& deviceID)
{
    captureDevices();

    GST_DEBUG("Looking for device with UID %s", deviceID.ascii().data());
    for (auto& device : m_gstreamerDevices) {
        GST_LOG("Checking device with persistent ID: %s", device.persistentId().ascii().data());
        if (device.persistentId() == deviceID)
            return device;
    }
    GST_WARNING("Device not found");
    return std::nullopt;
}

const Vector<CaptureDevice>& GStreamerCaptureDeviceManager::captureDevices()
{
    if (m_devices.isEmpty() && !m_isTearingDown)
        refreshCaptureDevices();

    return m_devices;
}

void GStreamerCaptureDeviceManager::addDevice(GRefPtr<GstDevice>&& device)
{
    GUniquePtr<GstStructure> properties(gst_device_get_properties(device.get()));
    const char* klass = gst_structure_get_string(properties.get(), "device.class");

    if (klass && !g_strcmp0(klass, "monitor"))
        return;

    CaptureDevice::DeviceType type = deviceType();
    GUniquePtr<char> deviceClassChar(gst_device_get_device_class(device.get()));
    auto deviceClass = String::fromLatin1(deviceClassChar.get());
    if (type == CaptureDevice::DeviceType::Microphone && !deviceClass.startsWith("Audio"_s))
        return;
    if (type == CaptureDevice::DeviceType::Camera && !deviceClass.startsWith("Video"_s))
        return;

    // This isn't really a UID but should be good enough (libwebrtc
    // itself does that at least for pulseaudio devices).
    GUniquePtr<char> deviceName(gst_device_get_display_name(device.get()));
    GST_INFO("Registering device %s", deviceName.get());
    gboolean isDefault = FALSE;
    gst_structure_get_boolean(properties.get(), "is-default", &isDefault);
    auto label = makeString(isDefault ? "default: "_s : ""_s, deviceName.get());

    auto identifier = label;
    bool isMock = false;
    if (const char* persistentId = gst_structure_get_string(properties.get(), "persistent-id")) {
        identifier = String::fromLatin1(persistentId);
        isMock = true;
    }

    auto gstCaptureDevice = GStreamerCaptureDevice(WTFMove(device), identifier, type, label);
    gstCaptureDevice.setEnabled(true);
    gstCaptureDevice.setIsMockDevice(isMock);

    m_gstreamerDevices.append(WTFMove(gstCaptureDevice));
    m_devices.append(m_gstreamerDevices.last());
}

void GStreamerCaptureDeviceManager::removeDevice(GRefPtr<GstDevice>&& gstDevice)
{
    m_gstreamerDevices.removeFirstMatching([&](auto& captureDevice) -> bool {
        if (captureDevice.device() != gstDevice.get())
            return false;

        m_devices.removeFirstMatching([&](auto& device) -> bool {
            return device.persistentId() == captureDevice.persistentId();
        });
        return true;
    });
}

void GStreamerCaptureDeviceManager::refreshCaptureDevices()
{
    GST_DEBUG_OBJECT(m_deviceMonitor.get(), "Refreshing capture devices");
    m_devices.clear();
    m_gstreamerDevices.clear();
    if (m_isTearingDown)
        return;

    if (!m_deviceMonitor) {
        m_deviceMonitor = adoptGRef(gst_device_monitor_new());

        switch (deviceType()) {
        case CaptureDevice::DeviceType::Camera: {
            gst_device_monitor_add_filter(m_deviceMonitor.get(), "Video/Source", nullptr);
            break;
        }
        case CaptureDevice::DeviceType::Microphone: {
            auto caps = adoptGRef(gst_caps_new_empty_simple("audio/x-raw"));
            gst_device_monitor_add_filter(m_deviceMonitor.get(), "Audio/Source", caps.get());
            break;
        }
        case CaptureDevice::DeviceType::SystemAudio:
        case CaptureDevice::DeviceType::Speaker:
            // FIXME: Add Audio/Sink filter. See https://bugs.webkit.org/show_bug.cgi?id=216880
        case CaptureDevice::DeviceType::Screen:
        case CaptureDevice::DeviceType::Window:
            break;
        case CaptureDevice::DeviceType::Unknown:
            return;
        }

        if (!gst_device_monitor_start(m_deviceMonitor.get())) {
            GST_WARNING_OBJECT(m_deviceMonitor.get(), "Could not start device monitor");
            m_deviceMonitor = nullptr;
            return;
        }
    }

    GList* devices = g_list_sort(gst_device_monitor_get_devices(m_deviceMonitor.get()), sortDevices);
    while (devices) {
        addDevice(adoptGRef(GST_DEVICE_CAST(devices->data)));
        devices = g_list_delete_link(devices, devices);
    }

    auto bus = adoptGRef(gst_device_monitor_get_bus(m_deviceMonitor.get()));

    // Flush out device-added messages queued during initial probe of the device providers.
    gst_bus_set_flushing(bus.get(), TRUE);
    gst_bus_set_flushing(bus.get(), FALSE);

    // Monitor the bus for future device-added and device-removed messages.
    gst_bus_add_watch(bus.get(), reinterpret_cast<GstBusFunc>(+[](GstBus*, GstMessage* message, GStreamerCaptureDeviceManager* manager) -> gboolean {
        GRefPtr<GstDevice> device;
#ifndef GST_DISABLE_GST_DEBUG
        GUniquePtr<char> name;
#endif
        switch (GST_MESSAGE_TYPE(message)) {
        case GST_MESSAGE_DEVICE_ADDED:
            gst_message_parse_device_added(message, &device.outPtr());
#ifndef GST_DISABLE_GST_DEBUG
            name.reset(gst_device_get_display_name(device.get()));
            GST_INFO("Device added: %s", name.get());
#endif
            manager->addDevice(WTFMove(device));
            break;
        case GST_MESSAGE_DEVICE_REMOVED:
            gst_message_parse_device_removed(message, &device.outPtr());
#ifndef GST_DISABLE_GST_DEBUG
            name.reset(gst_device_get_display_name(device.get()));
            GST_INFO("Device removed: %s", name.get());
#endif
            manager->removeDevice(WTFMove(device));
            break;
        default:
            break;
        }
        return G_SOURCE_CONTINUE;
    }),  this);
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
