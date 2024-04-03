/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022, 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "GBMDevice.h"

#if USE(GBM)

#include <fcntl.h>
#include <gbm.h>
#include <mutex>
#include <unistd.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>
#include <xf86drm.h>

namespace WebCore {

GBMDevice::Device::~Device()
{
    if (m_gbmDevice.has_value() && m_gbmDevice.value())
        gbm_device_destroy(m_gbmDevice.value());
}

struct gbm_device* GBMDevice::Device::device() const
{
    RELEASE_ASSERT(m_filename.has_value());
    if (m_filename->isNull())
        return nullptr;

    if (m_gbmDevice)
        return m_gbmDevice.value();

    m_fd = UnixFileDescriptor { open(m_filename->data(), O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
    if (m_fd) {
        m_gbmDevice = gbm_create_device(m_fd.value());
        if (m_gbmDevice.value())
            return m_gbmDevice.value();

        WTFLogAlways("Failed to create GBM device for render device: %s: %s", m_filename->data(), safeStrerror(errno).data());
        m_fd = { };
    } else {
        WTFLogAlways("Failed to open DRM render device %s: %s", m_filename->data(), safeStrerror(errno).data());
        m_gbmDevice = nullptr;
    }

    return m_gbmDevice.value();
}

GBMDevice& GBMDevice::singleton()
{
    static std::unique_ptr<GBMDevice> s_device;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] {
        s_device = makeUnique<GBMDevice>();
    });
    return *s_device;
}

GBMDevice::~GBMDevice()
{
}

void GBMDevice::initialize(const String& deviceFile)
{
    RELEASE_ASSERT(!m_renderDevice.hasFilename());
    if (deviceFile.isEmpty()) {
        m_renderDevice.setFilename(nullptr);
        return;
    }

    drmDevicePtr devices[64];
    memset(devices, 0, sizeof(devices));

    int numDevices = drmGetDevices2(0, devices, std::size(devices));
    if (numDevices <= 0) {
        WTFLogAlways("No DRM devices found");
        m_renderDevice.setFilename(nullptr);
        return;
    }

    drmDevice* device = nullptr;
    for (int i = 0; i < numDevices && !device; ++i) {
        for (int j = 0; j < DRM_NODE_MAX && !device; ++j) {
            if (!(devices[i]->available_nodes & (1 << j)))
                continue;

            if (String::fromUTF8(devices[i]->nodes[j]) == deviceFile)
                device = devices[i];
        }
    }

    if (device) {
        RELEASE_ASSERT(device->available_nodes & (1 << DRM_NODE_PRIMARY));

        if (device->available_nodes & (1 << DRM_NODE_RENDER)) {
            m_renderDevice.setFilename(device->nodes[DRM_NODE_RENDER]);
            m_displayDevice.setFilename(device->nodes[DRM_NODE_PRIMARY]);
        } else
            m_renderDevice.setFilename(device->nodes[DRM_NODE_PRIMARY]);
    } else {
        WTFLogAlways("Failed to find DRM device for %s", deviceFile.utf8().data());
        m_renderDevice.setFilename(nullptr);
    }

    drmFreeDevices(devices, numDevices);
}

struct gbm_device* GBMDevice::device(GBMDevice::Type type) const
{
    RELEASE_ASSERT(m_renderDevice.hasFilename());

    if (type == Type::Render)
        return m_renderDevice.device();

    return m_displayDevice.hasFilename() ? m_displayDevice.device() : m_renderDevice.device();
}

} // namespace WebCore

#endif // USE(GBM)
