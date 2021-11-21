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

#include "config.h"
#include "RemoteGPU.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapter.h"
#include "WebGPUObjectHeap.h"
#include "WebGPUObjectRegistry.h"
#include <pal/graphics/WebGPU/WebGPU.h>
#include <pal/graphics/WebGPU/WebGPUAdapter.h>

namespace WebKit {

RemoteGPU::RemoteGPU(PAL::WebGPU::GPU& gpu, WebGPU::ObjectRegistry& objectRegistry, WebGPU::ObjectHeap& objectHeap, WebGPUIdentifier identifier)
    : m_backing(gpu)
    , m_objectRegistry(objectRegistry)
    , m_objectHeap(objectHeap)
    , m_identifier(identifier)
{
    m_objectRegistry.addObject(m_identifier, m_backing);
}

RemoteGPU::~RemoteGPU()
{
    m_objectRegistry.removeObject(m_identifier);
}

void RemoteGPU::requestAdapter(const WebGPU::RequestAdapterOptions& options, WebGPUIdentifier identifier, WTF::CompletionHandler<void(String&&, WebGPU::SupportedFeatures&&, WebGPU::SupportedLimits&&, bool)>&& callback)
{
    auto convertedOptions = m_objectRegistry.convertFromBacking(options);
    ASSERT(convertedOptions);
    if (!convertedOptions) {
        callback(StringImpl::empty(), { { } }, { }, false);
        return;
    }

    m_backing->requestAdapter(*convertedOptions, [callback = WTFMove(callback), weakObjectHeap = WeakPtr<WebGPU::ObjectHeap>(m_objectHeap), objectRegistry = m_objectRegistry, identifier] (RefPtr<PAL::WebGPU::Adapter>&& adapter) mutable {
        if (!weakObjectHeap || !adapter) {
            // FIXME: Deal with failure here.
            return;
        }
        auto remoteAdapter = RemoteAdapter::create(*adapter, objectRegistry, *weakObjectHeap, identifier);
        weakObjectHeap->addObject(remoteAdapter);
        auto name = adapter->name();
        const auto& features = adapter->features();
        const auto& limits = adapter->limits();
        callback(WTFMove(name), WebGPU::SupportedFeatures { features.features() }, WebGPU::SupportedLimits {
            limits.maxTextureDimension1D(),
            limits.maxTextureDimension2D(),
            limits.maxTextureDimension3D(),
            limits.maxTextureArrayLayers(),
            limits.maxBindGroups(),
            limits.maxDynamicUniformBuffersPerPipelineLayout(),
            limits.maxDynamicStorageBuffersPerPipelineLayout(),
            limits.maxSampledTexturesPerShaderStage(),
            limits.maxSamplersPerShaderStage(),
            limits.maxStorageBuffersPerShaderStage(),
            limits.maxStorageTexturesPerShaderStage(),
            limits.maxUniformBuffersPerShaderStage(),
            limits.maxUniformBufferBindingSize(),
            limits.maxStorageBufferBindingSize(),
            limits.minUniformBufferOffsetAlignment(),
            limits.minStorageBufferOffsetAlignment(),
            limits.maxVertexBuffers(),
            limits.maxVertexAttributes(),
            limits.maxVertexBufferArrayStride(),
            limits.maxInterStageShaderComponents(),
            limits.maxComputeWorkgroupStorageSize(),
            limits.maxComputeInvocationsPerWorkgroup(),
            limits.maxComputeWorkgroupSizeX(),
            limits.maxComputeWorkgroupSizeY(),
            limits.maxComputeWorkgroupSizeZ(),
            limits.maxComputeWorkgroupsPerDimension(),
        }, adapter->isFallbackAdapter());
    });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
