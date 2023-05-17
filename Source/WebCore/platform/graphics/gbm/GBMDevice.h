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

#pragma once

#if USE(GBM)

#include <optional>
#include <wtf/Noncopyable.h>
#include <wtf/unix/UnixFileDescriptor.h>

struct gbm_device;

namespace WTF {
class String;
}

namespace WebCore {

class GBMDevice {
    WTF_MAKE_NONCOPYABLE(GBMDevice);
    WTF_MAKE_FAST_ALLOCATED();
public:
    static GBMDevice& singleton();

    GBMDevice() = default;
    ~GBMDevice();

    bool isInitialized() const { return m_device.has_value(); }
    void initialize(const WTF::String&);
    struct gbm_device* device() const { RELEASE_ASSERT(m_device.has_value()); return m_device.value(); }

private:
    WTF::UnixFileDescriptor m_fd;
    std::optional<struct gbm_device*> m_device;
};

} // namespace WebCore

#endif // USE(GBM)
