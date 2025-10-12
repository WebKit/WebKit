/*
 * Copyright (C) 2025 Igalia S.L.
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
#include <unistd.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

RefPtr<GBMDevice> GBMDevice::create(const CString& filename)
{
    RELEASE_ASSERT(isMainThread());
    auto fd = UnixFileDescriptor { open(filename.data(), O_RDWR | O_CLOEXEC), UnixFileDescriptor::Adopt };
    if (!fd) {
        WTFLogAlways("Failed to open DRM node %s: %s", filename.data(), safeStrerror(errno).data());
        return nullptr;
    }
    auto* device = gbm_create_device(fd.value());
    if (!device) {
        WTFLogAlways("Failed to create GBM device for DRM node: %s: %s", filename.data(), safeStrerror(errno).data());
        return nullptr;
    }
    return adoptRef(*new GBMDevice(WTFMove(fd), device));
}

GBMDevice::GBMDevice(UnixFileDescriptor&& fd, struct gbm_device* device)
    : m_fd(WTFMove(fd))
    , m_device(device)
{
}

GBMDevice::~GBMDevice()
{
    gbm_device_destroy(m_device);
}

} // namespace WebCore

#endif // USE(GBM)
