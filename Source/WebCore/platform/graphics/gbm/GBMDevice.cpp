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

#if USE(ANGLE) && USE(NICOSIA)

#include <fcntl.h>
#include <gbm.h>
#include <mutex>
#include <wtf/ThreadSpecific.h>
#include <xf86drm.h>

namespace WebCore {

static ThreadSpecific<GBMDevice>& threadSpecificDevice()
{
    static ThreadSpecific<GBMDevice>* s_gbmDevice;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag,
        [] {
            s_gbmDevice = new ThreadSpecific<GBMDevice>;
        });
    return *s_gbmDevice;
}

const GBMDevice& GBMDevice::get()
{
    return *threadSpecificDevice();
}

GBMDevice::GBMDevice()
{
    static int s_globalFd { -1 };
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag, [] {
        drmDevicePtr devices[64];
        memset(devices, 0, sizeof(devices));

        int numDevices = drmGetDevices2(0, devices, WTF_ARRAY_LENGTH(devices));
        if (numDevices <= 0)
            return;

        for (int i = 0; i < numDevices; ++i) {
            drmDevice* device = devices[i];
            if (!(device->available_nodes & (1 << DRM_NODE_RENDER)))
                continue;

            s_globalFd = open(device->nodes[DRM_NODE_RENDER], O_RDWR | O_CLOEXEC);
            if (s_globalFd >= 0)
                break;
        }

        drmFreeDevices(devices, numDevices);
    });

    if (s_globalFd >= 0)
        m_device = gbm_create_device(s_globalFd);
}

GBMDevice::~GBMDevice()
{
    if (m_device) {
        gbm_device_destroy(m_device);
        m_device = nullptr;
    }
}

} // namespace WebCore

#endif // USE(ANGLE) && USE(NICOSIA)
