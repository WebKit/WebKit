/*
 * Copyright (C) 2022 Metrological Group B.V.
 * Copyright (C) 2022 Igalia S.L.
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

#if USE(LIBGBM)

#include <fcntl.h>
#include <gbm.h>
#include <mutex>
#include <unistd.h>
#include <wtf/StdLibExtras.h>
#include <xf86drm.h>

namespace WebCore {

const GBMDevice& GBMDevice::singleton()
{
    static std::unique_ptr<GBMDevice> s_device;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag,
        [] {
            s_device = makeUnique<GBMDevice>();
        });
    return *s_device;
}

GBMDevice::GBMDevice()
{
    [&] {
        drmDevicePtr devices[64];
        memset(devices, 0, sizeof(devices));

        int numDevices = drmGetDevices2(0, devices, std::size(devices));
        if (numDevices <= 0)
            return;

        for (int i = 0; i < numDevices; ++i) {
            drmDevice* device = devices[i];
            if (!(device->available_nodes & (1 << DRM_NODE_RENDER)))
                continue;

            m_fd = open(device->nodes[DRM_NODE_RENDER], O_RDWR | O_CLOEXEC);
            if (m_fd >= 0)
                break;
        }

        drmFreeDevices(devices, numDevices);
    }();

    if (m_fd >= 0)
        m_device = gbm_create_device(m_fd);
}

GBMDevice::~GBMDevice()
{
    if (m_device)
        gbm_device_destroy(m_device);
    if (m_fd >= 0)
        close(m_fd);
}

} // namespace WebCore

#endif // USE(LIBGBM)
