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

#if ENABLE(GPU_PROCESS)

#include "WebGPUIdentifier.h"
#include <pal/graphics/WebGPU/WebGPUAdapter.h>
#include <wtf/Deque.h>

namespace WebKit::WebGPU {

class ConvertToBackingContext;

class RemoteAdapterProxy final : public PAL::WebGPU::Adapter {
public:
    static Ref<RemoteAdapterProxy> create(String&& name, PAL::WebGPU::SupportedFeatures& features, PAL::WebGPU::SupportedLimits& limits, bool isFallbackAdapter, ConvertToBackingContext& convertToBackingContext)
    {
        return adoptRef(*new RemoteAdapterProxy(WTFMove(name), features, limits, isFallbackAdapter, convertToBackingContext));
    }

    virtual ~RemoteAdapterProxy();

private:
    friend class DowncastConvertToBackingContext;

    RemoteAdapterProxy(String&& name, PAL::WebGPU::SupportedFeatures&, PAL::WebGPU::SupportedLimits&, bool isFallbackAdapter, ConvertToBackingContext&);

    RemoteAdapterProxy(const RemoteAdapterProxy&) = delete;
    RemoteAdapterProxy(RemoteAdapterProxy&&) = delete;
    RemoteAdapterProxy& operator=(const RemoteAdapterProxy&) = delete;
    RemoteAdapterProxy& operator=(RemoteAdapterProxy&&) = delete;

    WebGPUIdentifier backing() const { return m_backing; }

    void requestDevice(const PAL::WebGPU::DeviceDescriptor&, WTF::Function<void(Ref<PAL::WebGPU::Device>&&)>&&) final;

    Deque<WTF::Function<void(Ref<PAL::WebGPU::Device>)>> m_callbacks;

    WebGPUIdentifier m_backing { WebGPUIdentifier::generate() };
    Ref<ConvertToBackingContext> m_convertToBackingContext;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
