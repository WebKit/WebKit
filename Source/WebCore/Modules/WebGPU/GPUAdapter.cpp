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
#include "GPUAdapter.h"

#include "Exception.h"
#include "JSDOMPromiseDeferred.h"
#include "JSGPUAdapterInfo.h"
#include "JSGPUDevice.h"
#include <wtf/CheckedArithmetic.h>

#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/SortedArrayMap.h>

namespace WebCore {

String GPUAdapter::name() const
{
    return m_adapterInfo.name;
}

GPUAdapter::GPUAdapter(Ref<WebGPU::Adapter>&& backing, Ref<WebGPUIntegration>&& gpu)
    : m_backing(WTF::move(backing))
    , m_gpu(WTF::move(gpu))
    , m_adapterInfo(m_backing->info())
    , m_features(GPUSupportedFeatures::create(m_backing->features()))
    , m_limits(GPUSupportedLimits::create(WebGPUSupportedLimits::create(m_backing->limits())))
    , m_info(GPUAdapterInfo::create(name(), m_adapterInfo.subgroupMinSize, m_adapterInfo.subgroupMaxSize))
{
}

Ref<GPUSupportedFeatures> GPUAdapter::features() const
{
    return m_features;
}

Ref<GPUSupportedLimits> GPUAdapter::limits() const
{
    return m_limits;
}

bool GPUAdapter::isFallbackAdapter() const
{
    return m_adapterInfo.isFallbackAdapter;
}

static WebGPUDeviceDescriptor convertToBacking(const std::optional<GPUDeviceDescriptor>& options)
{
    if (!options)
        return { };

    return options->convertToBacking();
}

static GPUFeatureName convertFeatureNameToEnum(const String& stringValue)
{
    static constexpr SortedArrayMap enumerationMapping { WTF::toArray<std::pair<ComparableASCIILiteral, GPUFeatureName>>({
        { "bgra8unorm-storage"_s, GPUFeatureName::Bgra8unormStorage },
        { "clip-distances"_s, GPUFeatureName::ClipDistances },
        { "core-features-and-limits"_s, GPUFeatureName::CoreFeaturesAndLimits },
        { "depth-clip-control"_s, GPUFeatureName::DepthClipControl },
        { "depth32float-stencil8"_s, GPUFeatureName::Depth32floatStencil8 },
        { "dual-source-blending"_s, GPUFeatureName::DualSourceBlending },
        { "float16-renderable"_s, GPUFeatureName::Float16Renderable },
        { "float32-blendable"_s, GPUFeatureName::Float32Blendable },
        { "float32-filterable"_s, GPUFeatureName::Float32Filterable },
        { "float32-renderable"_s, GPUFeatureName::Float32Renderable },
        { "indirect-first-instance"_s, GPUFeatureName::IndirectFirstInstance },
        { "primitive-index"_s, GPUFeatureName::PrimitiveIndex },
        { "rg11b10ufloat-renderable"_s, GPUFeatureName::Rg11b10ufloatRenderable },
        { "shader-f16"_s, GPUFeatureName::ShaderF16 },
        { "subgroups"_s, GPUFeatureName::Subgroups },
        { "texture-compression-astc"_s, GPUFeatureName::TextureCompressionAstc },
        { "texture-compression-astc-sliced-3d"_s, GPUFeatureName::TextureCompressionAstcSliced3d },
        { "texture-compression-bc"_s, GPUFeatureName::TextureCompressionBc },
        { "texture-compression-bc-sliced-3d"_s, GPUFeatureName::TextureCompressionBcSliced3d },
        { "texture-compression-etc2"_s, GPUFeatureName::TextureCompressionEtc2 },
        { "texture-formats-tier1"_s, GPUFeatureName::TextureFormatsTier1 },
        { "texture-formats-tier2"_s, GPUFeatureName::TextureFormatsTier2 },
        { "timestamp-query"_s, GPUFeatureName::TimestampQuery },
    }) };
    if (auto* enumerationValue = enumerationMapping.tryGet(stringValue); enumerationValue) [[likely]]
        return *enumerationValue;

    RELEASE_ASSERT_NOT_REACHED();
}

static bool isSubset(const Vector<GPUFeatureName>& expectedSubset, const Vector<String>& expectedSuperset)
{
    HashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> expectedSupersetHashSet;
    for (auto& featureName : expectedSuperset)
        expectedSupersetHashSet.add(static_cast<uint32_t>(convertFeatureNameToEnum(featureName)));

    for (auto& featureName : expectedSubset) {
        if (!expectedSupersetHashSet.contains(static_cast<uint32_t>(featureName)))
            return false;
    }

    return true;
}

static bool NODELETE setMaxIntegerValue(uint32_t& limitValue, uint64_t i)
{
    CheckedUint32 narrowed = i;
    if (narrowed.hasOverflowed())
        return false;

    if (uint32_t narrowedValue = narrowed.value(); narrowedValue > limitValue)
        limitValue = narrowedValue;

    return true;
}

static bool NODELETE setMaxIntegerValue(uint64_t& limitValue, uint64_t i)
{
    if (i > limitValue)
        limitValue = i;

    return true;
}

static bool NODELETE setAlignmentIntegerValue(uint32_t& limitValue, uint64_t i, uint32_t supportedAlignment)
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

// The limits of the device that the descriptor asks for: the default limits, raised to the ones it
// names. std::nullopt when it names one that is unknown, or better than the adapter supports.
static std::optional<::WebGPU::Limits> requiredLimits(const WebGPUDeviceDescriptor& descriptor, const ::WebGPU::Limits& supportedLimits)
{
    auto limits = ::WebGPU::defaultLimits();

    for (const auto& pair : descriptor.requiredLimits) {
#define SET_MAX_VALUE(LIMIT) \
        else if (pair.key == #LIMIT ""_s) { \
            if (pair.value > supportedLimits.LIMIT || !setMaxIntegerValue(limits.LIMIT, pair.value)) { \
                return std::nullopt; \
            } \
        }

#define SET_ALIGNMENT_VALUE(LIMIT) \
        else if (pair.key == #LIMIT ""_s) { \
            if (!setAlignmentIntegerValue(limits.LIMIT, pair.value, supportedLimits.LIMIT)) { \
                return std::nullopt; \
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
        SET_MAX_VALUE(maxInterStageShaderVariables)
        SET_MAX_VALUE(maxColorAttachments)
        SET_MAX_VALUE(maxColorAttachmentBytesPerSample)
        SET_MAX_VALUE(maxComputeWorkgroupStorageSize)
        SET_MAX_VALUE(maxComputeInvocationsPerWorkgroup)
        SET_MAX_VALUE(maxComputeWorkgroupSizeX)
        SET_MAX_VALUE(maxComputeWorkgroupSizeY)
        SET_MAX_VALUE(maxComputeWorkgroupSizeZ)
        SET_MAX_VALUE(maxComputeWorkgroupsPerDimension)
        SET_MAX_VALUE(maxStorageBuffersInFragmentStage)
        SET_MAX_VALUE(maxStorageTexturesInFragmentStage)
        SET_MAX_VALUE(maxStorageBuffersInVertexStage)
        SET_MAX_VALUE(maxStorageTexturesInVertexStage)
        else
            return std::nullopt;

#undef SET_ALIGNMENT_VALUE
#undef SET_MAX_VALUE
    }

    // https://gpuweb.github.io/gpuweb/#limits
    // The combined maxStorage{Buffers,Textures}PerShaderStage limits and their per-stage counterparts
    // auto-upgrade each other, so a page that only knows one spelling still gets the capacity it asked
    // for: naming a per-stage limit raises the combined limit to match, and naming only the combined
    // limit fills in the per-stage limits it implies.
    const auto& wasRequested = [&](ASCIILiteral name) {
        return descriptor.requiredLimits.containsIf([&](auto& pair) {
            return pair.key == name;
        });
    };

    if (wasRequested("maxStorageBuffersInVertexStage"_s) || wasRequested("maxStorageBuffersInFragmentStage"_s))
        limits.maxStorageBuffersPerShaderStage = std::max({ limits.maxStorageBuffersPerShaderStage, limits.maxStorageBuffersInVertexStage, limits.maxStorageBuffersInFragmentStage });
    else if (wasRequested("maxStorageBuffersPerShaderStage"_s)) {
        limits.maxStorageBuffersInVertexStage = std::min(limits.maxStorageBuffersPerShaderStage, supportedLimits.maxStorageBuffersInVertexStage);
        limits.maxStorageBuffersInFragmentStage = std::min(limits.maxStorageBuffersPerShaderStage, supportedLimits.maxStorageBuffersInFragmentStage);
    }

    if (wasRequested("maxStorageTexturesInVertexStage"_s) || wasRequested("maxStorageTexturesInFragmentStage"_s))
        limits.maxStorageTexturesPerShaderStage = std::max({ limits.maxStorageTexturesPerShaderStage, limits.maxStorageTexturesInVertexStage, limits.maxStorageTexturesInFragmentStage });
    else if (wasRequested("maxStorageTexturesPerShaderStage"_s)) {
        limits.maxStorageTexturesInVertexStage = std::min(limits.maxStorageTexturesPerShaderStage, supportedLimits.maxStorageTexturesInVertexStage);
        limits.maxStorageTexturesInFragmentStage = std::min(limits.maxStorageTexturesPerShaderStage, supportedLimits.maxStorageTexturesInFragmentStage);
    }

    return limits;
}

// The features that the descriptor asks for, with the ones that they imply.
static Vector<WebGPU::FeatureName> requiredFeatures(const WebGPUDeviceDescriptor& descriptor)
{
    auto features = descriptor.requiredFeatures;

    if (features.contains(WebGPU::FeatureName::TextureFormatsTier2) && !features.contains(WebGPU::FeatureName::TextureFormatsTier1))
        features.append(WebGPU::FeatureName::TextureFormatsTier1);

    if (features.contains(WebGPU::FeatureName::TextureFormatsTier1) && !features.contains(WebGPU::FeatureName::Rg11b10ufloatRenderable))
        features.append(WebGPU::FeatureName::Rg11b10ufloatRenderable);

    if (!features.contains(WebGPU::FeatureName::CoreFeaturesAndLimits))
        features.append(WebGPU::FeatureName::CoreFeaturesAndLimits);

    return features;
}

void GPUAdapter::requestDevice(ScriptExecutionContext& scriptExecutionContext, const std::optional<GPUDeviceDescriptor>& deviceDescriptor, RequestDevicePromise&& promise)
{
    auto& existingFeatures = m_features->backing().features();
    if (deviceDescriptor && !isSubset(deviceDescriptor->requiredFeatures, existingFeatures)) {
        promise.reject(Exception(ExceptionCode::TypeError));
        return;
    }

    auto descriptor = convertToBacking(deviceDescriptor);
    auto limits = requiredLimits(descriptor, m_backing->limits());
    if (!limits) {
        promise.reject(Exception(ExceptionCode::OperationError));
        return;
    }

    auto features = requiredFeatures(descriptor);
    m_backing->requestDevice({ .label = descriptor.label, .requiredFeatures = features.span(), .requiredLimits = *limits }, [protectedThis = protect(*this), label = descriptor.label, deviceDescriptor, promise = WTF::move(promise), scriptExecutionContextRef = protect(scriptExecutionContext)](RefPtr<WebGPU::Device>&& device) mutable {
        if (!device)
            promise.reject(Exception(ExceptionCode::OperationError));
        else {
            auto queueLabel = deviceDescriptor->defaultQueue.label;
            Ref<GPUDevice> gpuDevice = GPUDevice::create(scriptExecutionContextRef.ptr(), device.releaseNonNull(), protectedThis->m_gpu.copyRef(), WTF::move(label), deviceDescriptor ? WTF::move(queueLabel) : ""_s, GPUAdapterInfo::create(protectedThis->name(), protectedThis->m_info->subgroupMinSize(), protectedThis->m_info->subgroupMaxSize()));
            gpuDevice->suspendIfNeeded();
            promise.resolve(WTF::move(gpuDevice));
        }
    });
}

Ref<GPUAdapterInfo> GPUAdapter::info()
{
    return m_info;
}

}
