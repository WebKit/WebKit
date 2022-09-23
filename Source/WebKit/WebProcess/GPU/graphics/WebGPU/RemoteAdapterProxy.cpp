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
#include "RemoteAdapterProxy.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapterMessages.h"
#include "RemoteDeviceProxy.h"
#include "WebGPUConvertToBackingContext.h"

namespace WebKit::WebGPU {

RemoteAdapterProxy::RemoteAdapterProxy(String&& name, PAL::WebGPU::SupportedFeatures& features, PAL::WebGPU::SupportedLimits& limits, bool isFallbackAdapter, RemoteGPUProxy& parent, ConvertToBackingContext& convertToBackingContext, WebGPUIdentifier identifier)
    : Adapter(WTFMove(name), features, limits, isFallbackAdapter)
    , m_backing(identifier)
    , m_convertToBackingContext(convertToBackingContext)
    , m_parent(parent)
{
}

RemoteAdapterProxy::~RemoteAdapterProxy()
{
}

void RemoteAdapterProxy::requestDevice(const PAL::WebGPU::DeviceDescriptor& descriptor, CompletionHandler<void(Ref<PAL::WebGPU::Device>&&)>&& callback)
{
    auto convertedDescriptor = m_convertToBackingContext->convertToBacking(descriptor);
    ASSERT(convertedDescriptor);
    if (!convertedDescriptor)
        return;

    auto identifier = WebGPUIdentifier::generate();
    auto queueIdentifier = WebGPUIdentifier::generate();
    auto sendResult = sendSync(Messages::RemoteAdapter::RequestDevice(*convertedDescriptor, identifier, queueIdentifier));
    if (!sendResult)
        return;

    auto [supportedFeatures, supportedLimits] = sendResult.takeReply();
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
    callback(RemoteDeviceProxy::create(WTFMove(resultSupportedFeatures), WTFMove(resultSupportedLimits), *this, m_convertToBackingContext, identifier, queueIdentifier));
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
