/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "WebGPUAdapter.h"
#include <WebGPU/WebGPU.h>
#include <wtf/Deque.h>

namespace PAL::WebGPU {

class ConvertToBackingContext;

class AdapterImpl final : public Adapter {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<AdapterImpl> create(WGPUAdapter adapter, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new AdapterImpl(adapter, convertToBackingContext));
    }

    virtual ~AdapterImpl();

private:
    friend class DowncastConvertToBackingContext;

    AdapterImpl(WGPUAdapter, ConvertToBackingContext&);

    AdapterImpl(const AdapterImpl&) = delete;
    AdapterImpl(AdapterImpl&&) = delete;
    AdapterImpl& operator=(const AdapterImpl&) = delete;
    AdapterImpl& operator=(AdapterImpl&&) = delete;

    WGPUAdapter backing() const { return m_backing; }

    void requestDevice(const DeviceDescriptor&, CompletionHandler<void(Ref<Device>&&)>&&) final;

    WGPUAdapter m_backing { nullptr };
    Ref<ConvertToBackingContext> m_convertToBackingContext;
};

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
