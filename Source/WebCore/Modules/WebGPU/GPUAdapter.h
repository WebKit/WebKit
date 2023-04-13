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

#include "GPUDevice.h"
#include "GPUDeviceDescriptor.h"
#include "GPUSupportedFeatures.h"
#include "GPUSupportedLimits.h"
#include "JSDOMPromiseDeferredForward.h"
#include "ScriptExecutionContext.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUAdapter.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUAdapter : public RefCounted<GPUAdapter> {
public:
    static Ref<GPUAdapter> create(Ref<PAL::WebGPU::Adapter>&& backing)
    {
        return adoptRef(*new GPUAdapter(WTFMove(backing)));
    }

    String name() const;
    Ref<GPUSupportedFeatures> features() const;
    Ref<GPUSupportedLimits> limits() const;
    bool isFallbackAdapter() const;

    using RequestDevicePromise = DOMPromiseDeferred<IDLInterface<GPUDevice>>;
    void requestDevice(ScriptExecutionContext&, const std::optional<GPUDeviceDescriptor>&, RequestDevicePromise&&);

    using RequestAdapterInfoPromise = DOMPromiseDeferred<IDLNull>;
    void requestAdapterInfo(const std::optional<Vector<String>>&, RequestAdapterInfoPromise&&);

    PAL::WebGPU::Adapter& backing() { return m_backing; }
    const PAL::WebGPU::Adapter& backing() const { return m_backing; }

private:
    GPUAdapter(Ref<PAL::WebGPU::Adapter>&& backing)
        : m_backing(WTFMove(backing))
    {
    }

    Ref<PAL::WebGPU::Adapter> m_backing;
};

}
