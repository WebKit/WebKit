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
#include "RemoteGPUProxy.h"

#if ENABLE(GPU_PROCESS)

#include "GPUProcessConnection.h"
#include "RemoteAdapterProxy.h"
#include "RemoteGPUMessages.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUSupportedFeatures.h>
#include <pal/graphics/WebGPU/WebGPUSupportedLimits.h>

namespace WebKit::WebGPU {

static constexpr size_t defaultStreamSize = 1 << 21;

RemoteGPUProxy::RemoteGPUProxy(GPUProcessConnection& gpuProcessConnection, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_streamConnection(gpuProcessConnection.connection(), defaultStreamSize)
{
}

RemoteGPUProxy::~RemoteGPUProxy()
{
}

void RemoteGPUProxy::requestAdapter(const PAL::WebGPU::RequestAdapterOptions& options, WTF::Function<void(RefPtr<PAL::WebGPU::Adapter>&&)>&& callback)
{
    auto convertedOptions = m_convertToBackingContext->convertToBacking(options);
    ASSERT(convertedOptions);
    if (!convertedOptions)
        return;

    auto identifier = WebGPUIdentifier::generate();
    String name;
    SupportedFeatures supportedFeatures;
    SupportedLimits supportedLimits;
    bool isFallbackAdapter;
    auto sendResult = sendSync(Messages::RemoteGPU::RequestAdapter(*convertedOptions, identifier), { name, supportedFeatures, supportedLimits, isFallbackAdapter });
    if (!sendResult)
        return;

    auto resultSupportedFeatures = PAL::WebGPU::SupportedFeatures::create(WTFMove(supportedFeatures.features));
    auto resultSupportedLimits = PAL::WebGPU::SupportedLimits::create(
        supportedLimits.maxTextureDimension1D,
        supportedLimits.maxTextureDimension2D,
        supportedLimits.maxTextureDimension3D,
        supportedLimits.maxTextureArrayLayers,
        supportedLimits.maxBindGroups,
        supportedLimits.maxDynamicUniformBuffersPerPipelineLayout,
        supportedLimits.maxDynamicStorageBuffersPerPipelineLayout,
        supportedLimits.maxSampledTexturesPerShaderStage,
        supportedLimits.maxSamplersPerShaderStage,
        supportedLimits.maxStorageBuffersPerShaderStage,
        supportedLimits.maxStorageTexturesPerShaderStage,
        supportedLimits.maxUniformBuffersPerShaderStage,
        supportedLimits.maxUniformBufferBindingSize,
        supportedLimits.maxStorageBufferBindingSize,
        supportedLimits.minUniformBufferOffsetAlignment,
        supportedLimits.minStorageBufferOffsetAlignment,
        supportedLimits.maxVertexBuffers,
        supportedLimits.maxVertexAttributes,
        supportedLimits.maxVertexBufferArrayStride,
        supportedLimits.maxInterStageShaderComponents,
        supportedLimits.maxComputeWorkgroupStorageSize,
        supportedLimits.maxComputeInvocationsPerWorkgroup,
        supportedLimits.maxComputeWorkgroupSizeX,
        supportedLimits.maxComputeWorkgroupSizeY,
        supportedLimits.maxComputeWorkgroupSizeZ,
        supportedLimits.maxComputeWorkgroupsPerDimension
    );
    callback(RemoteAdapterProxy::create(WTFMove(name), WTFMove(resultSupportedFeatures), WTFMove(resultSupportedLimits), isFallbackAdapter, *this, m_convertToBackingContext, identifier));
}

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
