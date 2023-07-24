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

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
#include "GStreamerMockDevice.h"

#include "CaptureDevice.h"
#include "GStreamerCommon.h"
#include <wtf/glib/WTFGType.h>

using namespace WebCore;

struct _GStreamerMockDevicePrivate {
};

GST_DEBUG_CATEGORY_STATIC(webkitGstMockDeviceDebug);
#define GST_CAT_DEFAULT webkitGstMockDeviceDebug

#define webkit_mock_device_provider_parent_class parent_class
WEBKIT_DEFINE_TYPE_WITH_CODE(GStreamerMockDevice, webkit_mock_device, GST_TYPE_DEVICE, GST_DEBUG_CATEGORY_INIT(webkitGstMockDeviceDebug, "webkitmockdevice", 0, "Mock Device"))

static GstElement* webkitMockDeviceCreateElement(GstDevice* device, const char* name)
{
    GST_INFO_OBJECT(device, "Creating source element for device %s", name);
    auto* element = makeGStreamerElement("appsrc", name);
    g_object_set(element, "format", GST_FORMAT_TIME, "is-live", TRUE, nullptr);
    return element;
}

static void webkit_mock_device_class_init(GStreamerMockDeviceClass* klass)
{
    auto* deviceClass = GST_DEVICE_CLASS(klass);
    deviceClass->create_element = GST_DEBUG_FUNCPTR(webkitMockDeviceCreateElement);
}

GstDevice* webkitMockDeviceCreate(const CaptureDevice& captureDevice)
{
    const char* deviceClass;
    GRefPtr<GstCaps> caps;

    switch (captureDevice.type()) {
    case CaptureDevice::DeviceType::Camera:
    case CaptureDevice::DeviceType::Screen:
    case CaptureDevice::DeviceType::Window:
        deviceClass = "Video/Source";
        caps = adoptGRef(gst_caps_new_empty_simple("video/x-raw"));
        break;
    case CaptureDevice::DeviceType::Microphone:
        deviceClass = "Audio/Source";
        caps = adoptGRef(gst_caps_new_empty_simple("audio/x-raw"));
        break;
    default:
        deviceClass = "unknown/unknown";
        caps = adoptGRef(gst_caps_new_any());
        break;
    }

    auto displayName = captureDevice.label();
    GUniquePtr<GstStructure> properties(gst_structure_new("webkit-mock-device", "persistent-id", G_TYPE_STRING, captureDevice.persistentId().ascii().data(), "is-default", G_TYPE_BOOLEAN, captureDevice.isDefault(), nullptr));
    auto* device = WEBKIT_MOCK_DEVICE_CAST(g_object_new(GST_TYPE_MOCK_DEVICE, "display-name", displayName.ascii().data(), "device-class", deviceClass, "caps", caps.get(), "properties", properties.get(), nullptr));
    gst_object_ref_sink(device);
    return GST_DEVICE_CAST(device);
}

#undef GST_CAT_DEFAULT

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
