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

#pragma once

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)

#include <gst/gst.h>
#include <wtf/Forward.h>

namespace WebCore {
class CaptureDevice;
};

typedef struct _GStreamerMockDevice GStreamerMockDevice;
typedef struct _GStreamerMockDeviceClass GStreamerMockDeviceClass;
typedef struct _GStreamerMockDevicePrivate GStreamerMockDevicePrivate;

#define GST_TYPE_MOCK_DEVICE                 (webkit_mock_device_get_type())
#define GST_IS_MOCK_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_TYPE((obj), GST_TYPE_MOCK_DEVICE))
#define GST_IS_MOCK_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE((klass), GST_TYPE_MOCK_DEVICE))
#define WEBKIT_MOCK_DEVICE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS((obj), GST_TYPE_MOCK_DEVICE, GStreamerMockDeviceClass))
#define WEBKIT_MOCK_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_CAST((obj), GST_TYPE_MOCK_DEVICE, GStreamerMockDevice))
#define WEBKIT_MOCK_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST((klass), GST_TYPE_DEVICE, GStreamerMockDeviceClass))
#define WEBKIT_MOCK_DEVICE_CAST(obj)         ((GStreamerMockDevice*)(obj))

struct _GStreamerMockDevice {
    GstDevice parent;
    GStreamerMockDevicePrivate* priv;
};

struct _GStreamerMockDeviceClass {
    GstDeviceClass parentClass;
};

GType webkit_mock_device_get_type(void);

GstDevice* webkitMockDeviceCreate(const WebCore::CaptureDevice&);

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
