/*
 * Copyright (C) 2023 Metrological Group B.V.
 * Author: Philippe Normand <philn@igalia.com>
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
#include "GStreamerMockDeviceProvider.h"

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include "ContextDestructionObserverInlines.h"
#include "GStreamerMockDevice.h"
#include "GUniquePtrGStreamer.h"
#include "MockRealtimeMediaSourceCenter.h"
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

struct _GStreamerMockDeviceProviderPrivate {
    ~_GStreamerMockDeviceProviderPrivate();
};

GST_DEBUG_CATEGORY_STATIC(webkitGstMockDeviceProviderDebug);
#define GST_CAT_DEFAULT webkitGstMockDeviceProviderDebug

WEBKIT_DEFINE_TYPE_WITH_CODE(GStreamerMockDeviceProvider, webkit_mock_device_provider, GST_TYPE_DEVICE_PROVIDER, GST_DEBUG_CATEGORY_INIT(webkitGstMockDeviceProviderDebug, "webkitmockdeviceprovider", 0, "Mock Device Provider"))

static GStreamerMockDeviceProvider* s_provider = nullptr;

_GStreamerMockDeviceProviderPrivate::~_GStreamerMockDeviceProviderPrivate()
{
    s_provider = nullptr;
}

static GList* webkitMockDeviceProviderProbe(GstDeviceProvider* provider)
{
    if (!MockRealtimeMediaSourceCenter::mockRealtimeMediaSourceCenterEnabled()) {
        GST_INFO_OBJECT(provider, "Mock capture sources are disabled, returning empty device list");
        return nullptr;
    }

    GST_INFO_OBJECT(provider, "Probing");
    GList* devices = nullptr;
    auto& sourceCenter = MockRealtimeMediaSourceCenter::singleton();
    for (auto& device : sourceCenter.videoDevices())
        devices = g_list_prepend(devices, webkitMockDeviceCreate(device));
    for (auto& device : sourceCenter.microphoneDevices())
        devices = g_list_prepend(devices, webkitMockDeviceCreate(device));
    for (auto& device : sourceCenter.displayDevices())
        devices = g_list_prepend(devices, webkitMockDeviceCreate(device));

    devices = g_list_reverse(devices);
    return devices;
}

void webkitGstMockDeviceProviderSwitchDefaultDevice(const CaptureDevice& oldDevice, const CaptureDevice& newDevice)
{
    if (!s_provider) {
        GST_ERROR("Mock device provider not found");
        return;
    }

    GRefPtr<GstDevice> oldGstDevice, newGstDevice;
    GList* devices = gst_device_provider_get_devices(GST_DEVICE_PROVIDER_CAST(s_provider));
    for (GList* it = devices; it; it = it->next) {
        auto device = GST_DEVICE_CAST(it->data);
        GUniquePtr<GstStructure> properties(gst_device_get_properties(device));
        auto persistentId = gstStructureGetString(properties.get(), "persistent-id"_s);
        if (equal(oldDevice.persistentId(), persistentId.span()))
            oldGstDevice = device;
        else if (equal(newDevice.persistentId(), persistentId.span()))
            newGstDevice = device;
        if (oldGstDevice && newGstDevice)
            break;
    }
    g_list_free_full(devices, gst_object_unref);

    if (!oldGstDevice) {
        GST_ERROR_OBJECT(s_provider, "Unable to find GStreamer mock device corresponding to old capture device with ID %s", oldDevice.persistentId().utf8().data());
        return;
    }

    CaptureDevice previousDefaultDevice = oldDevice;
    previousDefaultDevice.setIsDefault(false);
    auto previousDefaultGstDevice = adoptGRef(webkitMockDeviceCreate(previousDefaultDevice));
    gst_device_provider_device_changed(GST_DEVICE_PROVIDER_CAST(s_provider), previousDefaultGstDevice.get(), oldGstDevice.get());

    if (!newGstDevice) {
        GST_ERROR_OBJECT(s_provider, "Unable to find GStreamer mock device corresponding to new capture device with ID %s", oldDevice.persistentId().utf8().data());
        return;
    }

    CaptureDevice nextDefaultDevice = newDevice;
    nextDefaultDevice.setIsDefault(true);
    auto nextDefaultGstDevice = adoptGRef(webkitMockDeviceCreate(nextDefaultDevice));
    GST_DEBUG_OBJECT(s_provider, "Emitting device-changed notification");
    gst_device_provider_device_changed(GST_DEVICE_PROVIDER_CAST(s_provider), nextDefaultGstDevice.get(), newGstDevice.get());
}

static void webkitMockDeviceProviderConstructed(GObject* object)
{
    G_OBJECT_CLASS(webkit_mock_device_provider_parent_class)->constructed(object);
    s_provider = WEBKIT_MOCK_DEVICE_PROVIDER(object);
}

static void webkit_mock_device_provider_class_init(GStreamerMockDeviceProviderClass* klass)
{
    auto gobjectClass = G_OBJECT_CLASS(klass);
    gobjectClass->constructed = webkitMockDeviceProviderConstructed;

    auto providerClass = GST_DEVICE_PROVIDER_CLASS(klass);
    providerClass->probe = GST_DEBUG_FUNCPTR(webkitMockDeviceProviderProbe);

    gst_device_provider_class_set_static_metadata(providerClass, "WebKit Mock Device Provider", "Source/Audio/Video",
        "List and provide WebKit mock source devices", "Philippe Normand <philn@igalia.com>");
}

#undef GST_CAT_DEFAULT

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
