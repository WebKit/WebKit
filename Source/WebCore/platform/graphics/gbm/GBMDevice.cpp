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

#if USE(GBM)

#include <fcntl.h>
#include <gbm.h>
#include <mutex>
#include <unistd.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

GBMDevice& GBMDevice::singleton()
{
    static std::unique_ptr<GBMDevice> s_device;
    static std::once_flag s_onceFlag;
    std::call_once(s_onceFlag,
        [] {
            s_device = makeUnique<GBMDevice>();
        });
    return *s_device;
}

GBMDevice::~GBMDevice()
{
    if (m_device.has_value() && m_device.value())
        gbm_device_destroy(m_device.value());
}

void GBMDevice::initialize(const String& deviceFile)
{
    RELEASE_ASSERT(!m_device.has_value());
    if (!deviceFile.isEmpty()) {
        m_fd = UnixFileDescriptor { open(deviceFile.utf8().data(), O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
        if (m_fd) {
            m_device = gbm_create_device(m_fd.value());
            if (m_device.value())
                return;

            WTFLogAlways("Failed to create GBM device for render device: %s: %s", deviceFile.utf8().data(), safeStrerror(errno).data());
            m_fd = { };
        } else
            WTFLogAlways("Failed to open DRM render device %s: %s", deviceFile.utf8().data(), safeStrerror(errno).data());
    }
    m_device = nullptr;
}

} // namespace WebCore

#endif // USE(GBM)
