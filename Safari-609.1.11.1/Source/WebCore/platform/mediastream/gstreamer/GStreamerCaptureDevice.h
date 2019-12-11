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

#include "CaptureDevice.h"
#include "GStreamerCommon.h"

#include <gst/gst.h>

namespace WebCore {

class GStreamerCaptureDevice : public CaptureDevice {
public:
    GStreamerCaptureDevice(GRefPtr<GstDevice>&& device, const String& persistentId, DeviceType type, const String& label, const String& groupId = emptyString())
        : CaptureDevice(persistentId, type, label, groupId)
        , m_device(device)
    {
    }

    GstCaps* caps() const { return gst_device_get_caps(m_device.get()); }
    GstDevice* device() { return m_device.get(); }

private:
    GRefPtr<GstDevice> m_device;
};

}

#endif // ENABLE(MEDIA_STREAM)  && USE(GSTREAMER)
