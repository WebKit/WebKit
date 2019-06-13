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
#include "GPUPipelineMetalConvertLayout.h"

#include "GPUPipelineLayout.h"

#if ENABLE(WEBGPU)

namespace WebCore {

static OptionSet<WHLSL::ShaderStage> convertShaderStageFlags(GPUShaderStageFlags flags)
{
    OptionSet<WHLSL::ShaderStage> result;
    if (flags & GPUShaderStageBit::Flags::Vertex)
        result.add(WHLSL::ShaderStage::Vertex);
    if (flags & GPUShaderStageBit::Flags::Fragment)
        result.add(WHLSL::ShaderStage::Fragment);
    if (flags & GPUShaderStageBit::Flags::Compute)
        result.add(WHLSL::ShaderStage::Compute);
    return result;
}

static Optional<WHLSL::Binding::BindingDetails> convertBindingType(GPUBindGroupLayout::InternalBindingDetails internalBindingDetails)
{
    return WTF::visit(WTF::makeVisitor([&](GPUBindGroupLayout::UniformBuffer uniformBuffer) -> Optional<WHLSL::Binding::BindingDetails> {
        return { WHLSL::UniformBufferBinding { uniformBuffer.internalLengthName } };
    }, [&](GPUBindGroupLayout::DynamicUniformBuffer) -> Optional<WHLSL::Binding::BindingDetails> {
        return WTF::nullopt;
    }, [&](GPUBindGroupLayout::Sampler) -> Optional<WHLSL::Binding::BindingDetails> {
        return { WHLSL::SamplerBinding { } };
    }, [&](GPUBindGroupLayout::SampledTexture) -> Optional<WHLSL::Binding::BindingDetails> {
        return { WHLSL::TextureBinding { } };
    }, [&](GPUBindGroupLayout::StorageBuffer storageBuffer) -> Optional<WHLSL::Binding::BindingDetails> {
        return { WHLSL::StorageBufferBinding { storageBuffer.internalLengthName } };
    }, [&](GPUBindGroupLayout::DynamicStorageBuffer) -> Optional<WHLSL::Binding::BindingDetails> {
        return WTF::nullopt;
    }), internalBindingDetails);
}

Optional<WHLSL::Layout> convertLayout(const GPUPipelineLayout& layout)
{
    WHLSL::Layout result;
    if (layout.bindGroupLayouts().size() > std::numeric_limits<unsigned>::max())
        return WTF::nullopt;
    for (size_t i = 0; i < layout.bindGroupLayouts().size(); ++i) {
        const auto& bindGroupLayout = layout.bindGroupLayouts()[i];
        WHLSL::BindGroup bindGroup;
        bindGroup.name = static_cast<unsigned>(i);
        for (const auto& keyValuePair : bindGroupLayout->bindingsMap()) {
            const auto& bindingDetails = keyValuePair.value;
            WHLSL::Binding binding;
            binding.visibility = convertShaderStageFlags(bindingDetails.externalBinding.visibility);
            if (auto bindingType = convertBindingType(bindingDetails.internalBindingDetails))
                binding.binding = *bindingType;
            else
                return WTF::nullopt;
            if (bindingDetails.externalBinding.binding > std::numeric_limits<unsigned>::max())
                return WTF::nullopt;
            binding.externalName = bindingDetails.externalBinding.binding;
            binding.internalName = bindingDetails.internalName;
            bindGroup.bindings.append(WTFMove(binding));
        }
        result.append(WTFMove(bindGroup));
    }
    return result;
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
