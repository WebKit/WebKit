/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUTextureView.h"
#include <WebGPU/WebGPU.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>

namespace PAL::WebGPU {

// The only purpose of DeviceWrapper is to expose a refcounted wrapper around WGPUDevice.
// We need refcounting in the situation where both the DeviceImpl and the QueueImpl need to keep
// the same WGPUDevice alive - but WGPUDevices aren't refcounted by themselves.
class DeviceWrapper final : public RefCounted<DeviceWrapper> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<DeviceWrapper> create(WGPUDevice device)
    {
        return adoptRef(*new DeviceWrapper(device));
    }

    ~DeviceWrapper();

    WGPUDevice backing() { return m_device; }

private:
    DeviceWrapper(WGPUDevice);

    DeviceWrapper(const DeviceWrapper&) = delete;
    DeviceWrapper(DeviceWrapper&&) = delete;
    DeviceWrapper& operator=(const DeviceWrapper&) = delete;
    DeviceWrapper& operator=(DeviceWrapper&&) = delete;

    WGPUDevice m_device { nullptr };
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
