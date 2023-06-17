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

typedef struct _GStreamerMockDeviceProvider GStreamerMockDeviceProvider;
typedef struct _GStreamerMockDeviceProviderClass GStreamerMockDeviceProviderClass;
typedef struct _GStreamerMockDeviceProviderPrivate GStreamerMockDeviceProviderPrivate;

#define GST_TYPE_MOCK_DEVICE_PROVIDER                 (webkit_mock_device_provider_get_type())
#define GST_IS_MOCK_DEVICE_PROVIDER(obj)              (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_MOCK_DEVICE_PROVIDER))
#define GST_IS_MOCK_DEVICE_PROVIDER_CLASS(klass)      (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_MOCK_DEVICE_PROVIDER))
#define WEBKIT_MOCK_DEVICE_PROVIDER_GET_CLASS(obj)       (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_MOCK_DEVICE_PROVIDER, GStreamerMockDeviceProviderClass))
#define WEBKIT_MOCK_DEVICE_PROVIDER(obj)                 (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_MOCK_DEVICE_PROVIDER, GStreamerMockDeviceProvider))
#define WEBKIT_MOCK_DEVICE_PROVIDER_CLASS(klass)         (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_DEVICE_PROVIDER, GStreamerMockDeviceProviderClass))
#define WEBKIT_MOCK_DEVICE_PROVIDER_CAST(obj)            ((GStreamerMockDeviceProvider *)(obj))

struct _GStreamerMockDeviceProvider {
    GstDeviceProvider parent;
    GStreamerMockDeviceProviderPrivate* priv;
};

struct _GStreamerMockDeviceProviderClass {
    GstDeviceProviderClass parentClass;
};

GType webkit_mock_device_provider_get_type(void);

#endif // ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
