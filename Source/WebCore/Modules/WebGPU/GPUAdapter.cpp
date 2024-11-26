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

#include <wtf/HashSet.h>
#include <wtf/HashTraits.h>
#include <wtf/SortedArrayMap.h>

namespace WebCore {

String GPUAdapter::name() const
{
    return m_backing->name();
}

Ref<GPUSupportedFeatures> GPUAdapter::features() const
{
    return GPUSupportedFeatures::create(WebGPU::SupportedFeatures::clone(m_backing->features()));
}

Ref<GPUSupportedLimits> GPUAdapter::limits() const
{
    return GPUSupportedLimits::create(WebGPU::SupportedLimits::clone(m_backing->limits()));
}

bool GPUAdapter::isFallbackAdapter() const
{
    return m_backing->isFallbackAdapter();
}

static WebGPU::DeviceDescriptor convertToBacking(const std::optional<GPUDeviceDescriptor>& options)
{
    if (!options)
        return { };

    return options->convertToBacking();
}

static GPUFeatureName convertFeatureNameToEnum(const String& stringValue)
{
    static constexpr std::pair<ComparableASCIILiteral, GPUFeatureName> mappings[] = {
        { "bgra8unorm-storage"_s, GPUFeatureName::Bgra8unormStorage },
        { "depth-clip-control"_s, GPUFeatureName::DepthClipControl },
        { "depth32float-stencil8"_s, GPUFeatureName::Depth32floatStencil8 },
        { "float32-filterable"_s, GPUFeatureName::Float32Filterable },
        { "indirect-first-instance"_s, GPUFeatureName::IndirectFirstInstance },
        { "rg11b10ufloat-renderable"_s, GPUFeatureName::Rg11b10ufloatRenderable },
        { "shader-f16"_s, GPUFeatureName::ShaderF16 },
        { "texture-compression-astc"_s, GPUFeatureName::TextureCompressionAstc },
        { "texture-compression-bc"_s, GPUFeatureName::TextureCompressionBc },
        { "texture-compression-etc2"_s, GPUFeatureName::TextureCompressionEtc2 },
        { "timestamp-query"_s, GPUFeatureName::TimestampQuery },
    };
    static constexpr SortedArrayMap enumerationMapping { mappings };
    if (auto* enumerationValue = enumerationMapping.tryGet(stringValue); LIKELY(enumerationValue))
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

void GPUAdapter::requestDevice(ScriptExecutionContext& scriptExecutionContext, const std::optional<GPUDeviceDescriptor>& deviceDescriptor, RequestDevicePromise&& promise)
{
    auto& existingFeatures = m_backing->features().features();
    if (deviceDescriptor && !isSubset(deviceDescriptor->requiredFeatures, existingFeatures)) {
        promise.reject(Exception(ExceptionCode::TypeError));
        return;
    }

    m_backing->requestDevice(convertToBacking(deviceDescriptor), [deviceDescriptor, promise = WTFMove(promise), scriptExecutionContextRef = Ref { scriptExecutionContext }](RefPtr<WebGPU::Device>&& device) mutable {
        if (!device.get())
            promise.reject(Exception(ExceptionCode::OperationError));
        else {
            auto queueLabel = deviceDescriptor->defaultQueue.label;
            Ref<GPUDevice> gpuDevice = GPUDevice::create(scriptExecutionContextRef.ptr(), device.releaseNonNull(), deviceDescriptor ? WTFMove(queueLabel) : ""_s);
            gpuDevice->suspendIfNeeded();
            promise.resolve(WTFMove(gpuDevice));
        }
    });
}

Ref<GPUAdapterInfo> GPUAdapter::info()
{
    return GPUAdapterInfo::create(name());
}

}
