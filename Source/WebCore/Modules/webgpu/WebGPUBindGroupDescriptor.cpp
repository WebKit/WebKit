/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WebGPUBindGroupDescriptor.h"

#if ENABLE(WEBGPU)

#include "GPUBindGroupDescriptor.h"
#include "GPUBuffer.h"
#include "Logging.h"
#include <wtf/Variant.h>

namespace WebCore {

static bool validateBufferBindingType(const GPUBuffer* buffer, const GPUBindGroupLayoutBinding& binding, const char* const functionName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif

    switch (binding.type) {
    case GPUBindingType::UniformBuffer:
        if (!buffer->isUniform()) {
            LOG(WebGPU, "%s: GPUBuffer resource for binding %u does not have UNIFORM usage!", functionName, binding.binding);
            return false;
        }
        return true;
    case GPUBindingType::StorageBuffer:
        if (!buffer->isStorage()) {
            LOG(WebGPU, "%s: GPUBuffer resource for binding %u does not have STORAGE usage!", functionName, binding.binding);
            return false;
        }
        return true;
    default:
        LOG(WebGPU, "%s: Layout binding %u is not a buffer-type resource!", functionName, binding.binding);
        return false;
    }
}

Optional<GPUBindGroupDescriptor> WebGPUBindGroupDescriptor::tryCreateGPUBindGroupDescriptor() const
{
    const char* const functionName = "GPUDevice::createBindGroup()";

    if (!layout || !layout->bindGroupLayout()) {
        LOG(WebGPU, "%s: Invalid GPUBindGroupLayout!", functionName);
        return std::nullopt;
    }

    if (bindings.size() != layout->bindGroupLayout()->bindingsMap().size()) {
        LOG(WebGPU, "%s: Mismatched number of GPUBindGroupLayoutBindings and GPUBindGroupBindings!", functionName);
        return std::nullopt;
    }

    auto layoutMap = layout->bindGroupLayout()->bindingsMap();

    Vector<GPUBindGroupBinding> bindGroupBindings;
    bindGroupBindings.reserveCapacity(bindings.size());

    for (const auto& binding : bindings) {
        auto iterator = layoutMap.find(binding.binding);
        if (iterator == layoutMap.end()) {
            LOG(WebGPU, "%s: GPUBindGroupLayoutBinding %u not found in GPUBindGroupLayout!", functionName, binding.binding);
            return std::nullopt;
        }

        const auto layoutBinding = iterator->value;

        auto bindingResourceVisitor = WTF::makeVisitor([](const RefPtr<WebGPUSampler>& sampler) -> Optional<GPUBindingResource> {
            if (!sampler)
                return std::nullopt;
            auto gpuSampler = sampler->sampler();
            if (!gpuSampler)
                return std::nullopt;

            return static_cast<GPUBindingResource>(makeRef(*gpuSampler));
        }, [](const RefPtr<WebGPUTextureView>& view) -> Optional<GPUBindingResource> {
            if (!view)
                return std::nullopt;
            auto texture = view->texture();
            if (!texture)
                return std::nullopt;

            return static_cast<GPUBindingResource>(makeRef(*texture));
        }, [&layoutBinding, functionName] (WebGPUBufferBinding bufferBinding) -> Optional<GPUBindingResource> {
            if (!bufferBinding.buffer)
                return std::nullopt;
            auto buffer = bufferBinding.buffer->buffer();
            if (!buffer)
                return std::nullopt;

            if (!validateBufferBindingType(buffer, layoutBinding.externalBinding, functionName))
                return std::nullopt;

            return static_cast<GPUBindingResource>(GPUBufferBinding { makeRef(*buffer), bufferBinding.offset, bufferBinding.size });
        });

        auto bindingResource = WTF::visit(bindingResourceVisitor, binding.resource);
        if (!bindingResource) {
            LOG(WebGPU, "%s: Invalid resource for binding %u!", functionName, layoutBinding.externalBinding.binding);
            return std::nullopt;
        }

        bindGroupBindings.uncheckedAppend(GPUBindGroupBinding { binding.binding, WTFMove(bindingResource.value()) });
    }

    return GPUBindGroupDescriptor { makeRef(*layout->bindGroupLayout()), WTFMove(bindGroupBindings) };
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
