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

#include "ExceptionOr.h"
#include "GPUIntegralTypes.h"
#include "WebGPUComputePassEncoder.h"
#include <JavaScriptCore/Uint32Array.h>
#include <optional>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUBindGroup;
class GPUBuffer;
class GPUComputePipeline;
class GPUQuerySet;

class GPUComputePassEncoder : public RefCounted<GPUComputePassEncoder> {
public:
    static Ref<GPUComputePassEncoder> create(Ref<WebGPU::ComputePassEncoder>&& backing)
    {
        return adoptRef(*new GPUComputePassEncoder(WTFMove(backing)));
    }

    String label() const;
    void setLabel(String&&);

    void setPipeline(const GPUComputePipeline&);
    void dispatchWorkgroups(GPUSize32 workgroupCountX, std::optional<GPUSize32> workgroupCountY, std::optional<GPUSize32> workgroupCountZ);
    void dispatchWorkgroupsIndirect(const GPUBuffer& indirectBuffer, GPUSize64 indirectOffset);

    void end();

    void setBindGroup(GPUIndex32, const GPUBindGroup&,
        std::optional<Vector<GPUBufferDynamicOffset>>&&);

    ExceptionOr<void> setBindGroup(GPUIndex32, const GPUBindGroup&,
        const JSC::Uint32Array& dynamicOffsetsData,
        GPUSize64 dynamicOffsetsDataStart,
        GPUSize32 dynamicOffsetsDataLength);

    void pushDebugGroup(String&& groupLabel);
    void popDebugGroup();
    void insertDebugMarker(String&& markerLabel);

    WebGPU::ComputePassEncoder& backing() { return m_backing; }
    const WebGPU::ComputePassEncoder& backing() const { return m_backing; }

private:
    GPUComputePassEncoder(Ref<WebGPU::ComputePassEncoder>&& backing)
        : m_backing(WTFMove(backing))
    {
    }

    Ref<WebGPU::ComputePassEncoder> m_backing;
};

}
