/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "GPUBindGroupLayout.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"

#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

static MTLDataType MTLDataTypeForBindingType(GPUBindGroupLayoutBinding::BindingType type)
{
    switch (type) {
    case GPUBindGroupLayoutBinding::BindingType::Sampler:
        return MTLDataTypeSampler;
    case GPUBindGroupLayoutBinding::BindingType::SampledTexture:
        return MTLDataTypeTexture;
    case GPUBindGroupLayoutBinding::BindingType::UniformBuffer:
    case GPUBindGroupLayoutBinding::BindingType::StorageBuffer:
        return MTLDataTypePointer;
    }
}

using ArgumentArraysMap = HashMap<GPUShaderStageFlags, RetainPtr<NSMutableArray<MTLArgumentDescriptor *>>>;
static void appendArgumentToArrayInMap(ArgumentArraysMap& map, GPUShaderStageFlags stage, RetainPtr<MTLArgumentDescriptor> argument)
{
    auto iterator = map.find(stage);
    if (iterator == map.end()) {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        map.set(stage, [[NSMutableArray alloc] initWithObjects:argument.get(), nil]);
        END_BLOCK_OBJC_EXCEPTIONS;
    } else
        [iterator->value addObject:argument.get()];
}

Ref<GPUBindGroupLayout> GPUBindGroupLayout::create(const GPUDevice& device, GPUBindGroupLayoutDescriptor&& descriptor)
{
    ArgumentArraysMap argumentArraysMap;

    for (const auto& binding : descriptor.bindings) {

        RetainPtr<MTLArgumentDescriptor> mtlArgument;

        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        mtlArgument = adoptNS([MTLArgumentDescriptor new]);
        END_BLOCK_OBJC_EXCEPTIONS;

        mtlArgument.get().dataType = MTLDataTypeForBindingType(binding.type);
        mtlArgument.get().index = binding.binding;

        if (binding.visibility & GPUShaderStageBit::VERTEX)
            appendArgumentToArrayInMap(argumentArraysMap, GPUShaderStageBit::VERTEX, mtlArgument);
        if (binding.visibility & GPUShaderStageBit::FRAGMENT)
            appendArgumentToArrayInMap(argumentArraysMap, GPUShaderStageBit::FRAGMENT, mtlArgument);
        if (binding.visibility & GPUShaderStageBit::COMPUTE)
            appendArgumentToArrayInMap(argumentArraysMap, GPUShaderStageBit::COMPUTE, mtlArgument);
    }

    ArgumentsMap argumentsMap;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    for (const auto& argumentArrayPair : argumentArraysMap) {
        auto mtlArgumentEncoder = adoptNS([device.platformDevice() newArgumentEncoderWithArguments:argumentArrayPair.value.get()]);
        argumentsMap.set(argumentArrayPair.key, mtlArgumentEncoder);
    }

    END_BLOCK_OBJC_EXCEPTIONS;

    return adoptRef(*new GPUBindGroupLayout(WTFMove(argumentsMap)));
}

GPUBindGroupLayout::GPUBindGroupLayout(ArgumentsMap&& map)
    : m_argumentsMap(map)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
