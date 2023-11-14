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

#include "config.h"
#include "WebGPUAdapterImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUConvertToBackingContext.h"
#include "WebGPUDeviceImpl.h"
#include <WebGPU/WebGPUExt.h>
#include <wtf/BlockPtr.h>

namespace WebCore::WebGPU {

static String adapterName(WGPUAdapter adapter)
{
    WGPUAdapterProperties properties;
    wgpuAdapterGetProperties(adapter, &properties);
    return String::fromLatin1(properties.name);
}

static Ref<SupportedFeatures> supportedFeatures(const Vector<WGPUFeatureName>& features)
{
    Vector<String> result;
    for (auto feature : features) {
        switch (feature) {
        case WGPUFeatureName_Undefined:
            continue;
        case WGPUFeatureName_DepthClipControl:
            result.append("depth-clip-control"_s);
            break;
        case WGPUFeatureName_Depth32FloatStencil8:
            result.append("depth32float-stencil8"_s);
            break;
        case WGPUFeatureName_TimestampQuery:
            result.append("timestamp-query"_s);
            break;
        case WGPUFeatureName_TextureCompressionBC:
            result.append("texture-compression-bc"_s);
            break;
        case WGPUFeatureName_TextureCompressionETC2:
            result.append("texture-compression-etc2"_s);
            break;
        case WGPUFeatureName_TextureCompressionASTC:
            result.append("texture-compression-astc"_s);
            break;
        case WGPUFeatureName_IndirectFirstInstance:
            result.append("indirect-first-instance"_s);
            break;
        case WGPUFeatureName_ShaderF16:
            result.append("shader-f16"_s);
            break;
        case WGPUFeatureName_RG11B10UfloatRenderable:
            result.append("rg11b10ufloat-renderable"_s);
            break;
        case WGPUFeatureName_BGRA8UnormStorage:
            result.append("bgra8unorm-storage"_s);
            break;
        case WGPUFeatureName_Float32Filterable:
            result.append("float32-filterable"_s);
            break;
        case WGPUFeatureName_Force32:
            ASSERT_NOT_REACHED();
            continue;
        }
    }
    return SupportedFeatures::create(WTFMove(result));
}

static Ref<SupportedFeatures> supportedFeatures(WGPUAdapter adapter)
{
    auto featureCount = wgpuAdapterEnumerateFeatures(adapter, nullptr);
    Vector<WGPUFeatureName> features(featureCount);
    wgpuAdapterEnumerateFeatures(adapter, features.data());

    return supportedFeatures(features);
}

static Ref<SupportedLimits> supportedLimits(WGPUAdapter adapter)
{
    WGPUSupportedLimits limits;
    limits.nextInChain = nullptr;
    auto result = wgpuAdapterGetLimits(adapter, &limits);
    ASSERT_UNUSED(result, result);
    return SupportedLimits::create(
        limits.limits.maxTextureDimension1D,
        limits.limits.maxTextureDimension2D,
        limits.limits.maxTextureDimension3D,
        limits.limits.maxTextureArrayLayers,
        limits.limits.maxBindGroups,
        limits.limits.maxBindGroupsPlusVertexBuffers,
        limits.limits.maxBindingsPerBindGroup,
        limits.limits.maxDynamicUniformBuffersPerPipelineLayout,
        limits.limits.maxDynamicStorageBuffersPerPipelineLayout,
        limits.limits.maxSampledTexturesPerShaderStage,
        limits.limits.maxSamplersPerShaderStage,
        limits.limits.maxStorageBuffersPerShaderStage,
        limits.limits.maxStorageTexturesPerShaderStage,
        limits.limits.maxUniformBuffersPerShaderStage,
        limits.limits.maxUniformBufferBindingSize,
        limits.limits.maxStorageBufferBindingSize,
        limits.limits.minUniformBufferOffsetAlignment,
        limits.limits.minStorageBufferOffsetAlignment,
        limits.limits.maxVertexBuffers,
        limits.limits.maxBufferSize,
        limits.limits.maxVertexAttributes,
        limits.limits.maxVertexBufferArrayStride,
        limits.limits.maxInterStageShaderComponents,
        limits.limits.maxInterStageShaderVariables,
        limits.limits.maxColorAttachments,
        limits.limits.maxColorAttachmentBytesPerSample,
        limits.limits.maxComputeWorkgroupStorageSize,
        limits.limits.maxComputeInvocationsPerWorkgroup,
        limits.limits.maxComputeWorkgroupSizeX,
        limits.limits.maxComputeWorkgroupSizeY,
        limits.limits.maxComputeWorkgroupSizeZ,
        limits.limits.maxComputeWorkgroupsPerDimension);
}

static bool isFallbackAdapter(WGPUAdapter adapter)
{
    WGPUAdapterProperties properties;
    wgpuAdapterGetProperties(adapter, &properties);
    return properties.adapterType == WGPUAdapterType_CPU;
}

AdapterImpl::AdapterImpl(WebGPUPtr<WGPUAdapter>&& adapter, ConvertToBackingContext& convertToBackingContext)
    : Adapter(adapterName(adapter.get()), supportedFeatures(adapter.get()), supportedLimits(adapter.get()), WebGPU::isFallbackAdapter(adapter.get()))
    , m_backing(WTFMove(adapter))
    , m_convertToBackingContext(convertToBackingContext)
{
}

AdapterImpl::~AdapterImpl() = default;

static bool setMaxIntegerValue(uint32_t& limitValue, uint64_t i)
{
    CheckedUint32 narrowed = i;
    if (narrowed.hasOverflowed())
        return false;

    if (uint32_t narrowedValue = narrowed.value(); narrowedValue > limitValue)
        limitValue = narrowedValue;

    return true;
}

static bool setMaxIntegerValue(uint64_t& limitValue, uint64_t i)
{
    if (i > limitValue)
        limitValue = i;

    return true;
}

static bool setAlignmentIntegerValue(uint32_t& limitValue, uint64_t i, uint32_t supportedAlignment)
{
    CheckedUint32 narrowed = i;
    if (narrowed.hasOverflowed())
        return false;

    uint32_t narrowedValue = narrowed.value();
    if (narrowedValue < supportedAlignment || (narrowedValue % supportedAlignment))
        return false;

    if (narrowedValue < limitValue)
        limitValue = narrowedValue;

    return true;
}

static void requestDeviceCallback(WGPURequestDeviceStatus status, WGPUDevice device, const char* message, void* userdata)
{
    auto block = reinterpret_cast<void(^)(WGPURequestDeviceStatus, WGPUDevice, const char*)>(userdata);
    block(status, device, message);
    Block_release(block); // Block_release is matched with Block_copy below in AdapterImpl::requestDevice().
}

void AdapterImpl::requestDevice(const DeviceDescriptor& descriptor, CompletionHandler<void(RefPtr<Device>&&)>&& callback)
{
    auto label = descriptor.label.utf8();

    auto features = descriptor.requiredFeatures.map([&convertToBackingContext = m_convertToBackingContext.get()](auto featureName) {
        return convertToBackingContext.convertToBacking(featureName);
    });

    auto limits = WGPULimits {
        .maxTextureDimension1D =    8192,
        .maxTextureDimension2D =    8192,
        .maxTextureDimension3D =    2048,
        .maxTextureArrayLayers =    256,
        .maxBindGroups =    4,
        .maxBindGroupsPlusVertexBuffers = 24,
        .maxBindingsPerBindGroup =    1000,
        .maxDynamicUniformBuffersPerPipelineLayout =    8,
        .maxDynamicStorageBuffersPerPipelineLayout =    4,
        .maxSampledTexturesPerShaderStage =    16,
        .maxSamplersPerShaderStage =    16,
        .maxStorageBuffersPerShaderStage =    8,
        .maxStorageTexturesPerShaderStage =    4,
        .maxUniformBuffersPerShaderStage =    12,
        .maxUniformBufferBindingSize =    65536,
        .maxStorageBufferBindingSize =    134217728,
        .minUniformBufferOffsetAlignment =    256,
        .minStorageBufferOffsetAlignment =    256,
        .maxVertexBuffers =    8,
        .maxBufferSize =    268435456,
        .maxVertexAttributes =    16,
        .maxVertexBufferArrayStride =    2048,
        .maxInterStageShaderComponents =    60,
        .maxInterStageShaderVariables =    16,
        .maxColorAttachments =    8,
        .maxColorAttachmentBytesPerSample = 32,
        .maxComputeWorkgroupStorageSize =    16384,
        .maxComputeInvocationsPerWorkgroup =    256,
        .maxComputeWorkgroupSizeX =    256,
        .maxComputeWorkgroupSizeY =    256,
        .maxComputeWorkgroupSizeZ =    64,
        .maxComputeWorkgroupsPerDimension =    65535,
    };

    auto& supportedLimits = this->limits();

    for (const auto& pair : descriptor.requiredLimits) {
#define SET_MAX_VALUE(LIMIT) \
        else if (pair.key == #LIMIT ""_s) { \
            if (pair.value > supportedLimits.LIMIT() || !setMaxIntegerValue(limits.LIMIT, pair.value)) { \
                callback(nullptr); \
                return; \
            } \
        }

#define SET_ALIGNMENT_VALUE(LIMIT) \
        else if (pair.key == #LIMIT ""_s) { \
            if (!setAlignmentIntegerValue(limits.LIMIT, pair.value, supportedLimits.LIMIT())) { \
                callback(nullptr); \
                return; \
            } \
        }

        if (false) { }
        SET_MAX_VALUE(maxTextureDimension1D)
        SET_MAX_VALUE(maxTextureDimension2D)
        SET_MAX_VALUE(maxTextureDimension3D)
        SET_MAX_VALUE(maxTextureArrayLayers)
        SET_MAX_VALUE(maxBindGroups)
        SET_MAX_VALUE(maxBindGroupsPlusVertexBuffers)
        SET_MAX_VALUE(maxBindingsPerBindGroup)
        SET_MAX_VALUE(maxDynamicUniformBuffersPerPipelineLayout)
        SET_MAX_VALUE(maxDynamicStorageBuffersPerPipelineLayout)
        SET_MAX_VALUE(maxSampledTexturesPerShaderStage)
        SET_MAX_VALUE(maxSamplersPerShaderStage)
        SET_MAX_VALUE(maxStorageBuffersPerShaderStage)
        SET_MAX_VALUE(maxStorageTexturesPerShaderStage)
        SET_MAX_VALUE(maxUniformBuffersPerShaderStage)
        SET_MAX_VALUE(maxUniformBufferBindingSize)
        SET_MAX_VALUE(maxStorageBufferBindingSize)
        SET_ALIGNMENT_VALUE(minUniformBufferOffsetAlignment)
        SET_ALIGNMENT_VALUE(minStorageBufferOffsetAlignment)
        SET_MAX_VALUE(maxVertexBuffers)
        SET_MAX_VALUE(maxBufferSize)
        SET_MAX_VALUE(maxVertexAttributes)
        SET_MAX_VALUE(maxVertexBufferArrayStride)
        SET_MAX_VALUE(maxInterStageShaderComponents)
        SET_MAX_VALUE(maxInterStageShaderVariables)
        SET_MAX_VALUE(maxColorAttachments)
        SET_MAX_VALUE(maxColorAttachmentBytesPerSample)
        SET_MAX_VALUE(maxComputeWorkgroupStorageSize)
        SET_MAX_VALUE(maxComputeInvocationsPerWorkgroup)
        SET_MAX_VALUE(maxComputeWorkgroupSizeX)
        SET_MAX_VALUE(maxComputeWorkgroupSizeY)
        SET_MAX_VALUE(maxComputeWorkgroupSizeZ)
        SET_MAX_VALUE(maxComputeWorkgroupsPerDimension)
        else {
            callback(nullptr);
            return;
        }

#undef SET_ALIGNMENT_VALUE
#undef SET_MAX_VALUE
    }

    WGPURequiredLimits requiredLimits { nullptr, WTFMove(limits) };

    WGPUDeviceDescriptor backingDescriptor {
        .nextInChain = nullptr,
        .label = label.data(),
        .requiredFeatureCount = static_cast<uint32_t>(features.size()),
        .requiredFeatures = features.data(),
        .requiredLimits = &requiredLimits,
        .defaultQueue = {
            { },
            "queue"
        },
        .deviceLostCallback = nullptr,
        .deviceLostUserdata = nullptr,
    };

    auto requestedLimits = SupportedLimits::create(limits.maxTextureDimension1D,
        limits.maxTextureDimension2D,
        limits.maxTextureDimension3D,
        limits.maxTextureArrayLayers,
        limits.maxBindGroups,
        limits.maxBindGroupsPlusVertexBuffers,
        limits.maxBindingsPerBindGroup,
        limits.maxDynamicUniformBuffersPerPipelineLayout,
        limits.maxDynamicStorageBuffersPerPipelineLayout,
        limits.maxSampledTexturesPerShaderStage,
        limits.maxSamplersPerShaderStage,
        limits.maxStorageBuffersPerShaderStage,
        limits.maxStorageTexturesPerShaderStage,
        limits.maxUniformBuffersPerShaderStage,
        limits.maxUniformBufferBindingSize,
        limits.maxStorageBufferBindingSize,
        limits.minUniformBufferOffsetAlignment,
        limits.minStorageBufferOffsetAlignment,
        limits.maxVertexBuffers,
        limits.maxBufferSize,
        limits.maxVertexAttributes,
        limits.maxVertexBufferArrayStride,
        limits.maxInterStageShaderComponents,
        limits.maxInterStageShaderVariables,
        limits.maxColorAttachments,
        limits.maxColorAttachmentBytesPerSample,
        limits.maxComputeWorkgroupStorageSize,
        limits.maxComputeInvocationsPerWorkgroup,
        limits.maxComputeWorkgroupSizeX,
        limits.maxComputeWorkgroupSizeY,
        limits.maxComputeWorkgroupSizeZ,
        limits.maxComputeWorkgroupsPerDimension);

    auto requestedFeatures = supportedFeatures(features);
    auto blockPtr = makeBlockPtr([protectedThis = Ref { *this }, convertToBackingContext = m_convertToBackingContext.copyRef(), callback = WTFMove(callback), requestedLimits, requestedFeatures](WGPURequestDeviceStatus status, WGPUDevice device, const char*) mutable {
        callback(DeviceImpl::create(adoptWebGPU(device), status == WGPURequestDeviceStatus_Success ? WTFMove(requestedFeatures) : SupportedFeatures::create({ }), WTFMove(requestedLimits), convertToBackingContext));
    });
    wgpuAdapterRequestDevice(m_backing.get(), &backingDescriptor, &requestDeviceCallback, Block_copy(blockPtr.get())); // Block_copy is matched with Block_release above in requestDeviceCallback().
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
