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
#import "Logging.h"

#import <Foundation/Foundation.h>
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

using ArgumentArray = RetainPtr<NSMutableArray<MTLArgumentDescriptor *>>;
static void appendArgumentToArray(ArgumentArray& array, RetainPtr<MTLArgumentDescriptor> argument)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (!array)
        array = [[NSMutableArray alloc] initWithObjects:argument.get(), nil];
    else
        [array addObject:argument.get()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

static GPUBindGroupLayout::ArgumentEncoderBuffer tryCreateArgumentEncoderAndBuffer(const GPUDevice& device, ArgumentArray array)
{
    GPUBindGroupLayout::ArgumentEncoderBuffer args;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    args.encoder = adoptNS([device.platformDevice() newArgumentEncoderWithArguments:array.get()]);
    END_BLOCK_OBJC_EXCEPTIONS;
    if (!args.encoder) {
        LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Unable to create MTLArgumentEncoder!");
        return { };
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    args.buffer = adoptNS([device.platformDevice() newBufferWithLength:args.encoder.get().encodedLength options:0]);
    [args.encoder setArgumentBuffer:args.buffer.get() offset:0];
    END_BLOCK_OBJC_EXCEPTIONS;
    if (!args.buffer) {
        LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Unable to create MTLBuffer from MTLArgumentEncoder!");
        return { };
    }

    return args;
};

RefPtr<GPUBindGroupLayout> GPUBindGroupLayout::tryCreate(const GPUDevice& device, GPUBindGroupLayoutDescriptor&& descriptor)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Invalid MTLDevice!");
        return nullptr;
    }

    ArgumentArray vertexArgsArray, fragmentArgsArray, computeArgsArray;
    BindingsMapType bindingsMap;

    for (const auto& binding : descriptor.bindings) {
        if (!bindingsMap.add(binding.binding, binding)) {
            LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Duplicate binding %lu found in WebGPUBindGroupLayoutDescriptor!", binding.binding);
            return nullptr;
        }

        RetainPtr<MTLArgumentDescriptor> mtlArgument;

        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        mtlArgument = adoptNS([MTLArgumentDescriptor new]);
        END_BLOCK_OBJC_EXCEPTIONS;

        if (!mtlArgument) {
            LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Unable to create MTLArgumentDescriptor for binding %lu!", binding.binding);
            return nullptr;
        }

        mtlArgument.get().dataType = MTLDataTypeForBindingType(binding.type);
        mtlArgument.get().index = binding.binding;

        if (binding.visibility & GPUShaderStageBit::VERTEX)
            appendArgumentToArray(vertexArgsArray, mtlArgument);
        if (binding.visibility & GPUShaderStageBit::FRAGMENT)
            appendArgumentToArray(fragmentArgsArray, mtlArgument);
        if (binding.visibility & GPUShaderStageBit::COMPUTE)
            appendArgumentToArray(computeArgsArray, mtlArgument);
    }

    ArgumentEncoderBuffer vertex, fragment, compute;

    if (vertexArgsArray) {
        if (!(vertex = tryCreateArgumentEncoderAndBuffer(device, vertexArgsArray)).isValid())
            return nullptr;
    }
    if (fragmentArgsArray) {
        if (!(fragment = tryCreateArgumentEncoderAndBuffer(device, fragmentArgsArray)).isValid())
            return nullptr;
    }
    if (computeArgsArray) {
        if (!(compute = tryCreateArgumentEncoderAndBuffer(device, computeArgsArray)).isValid())
            return nullptr;
    }

    return adoptRef(new GPUBindGroupLayout(WTFMove(bindingsMap), WTFMove(vertex), WTFMove(fragment), WTFMove(compute)));
}

GPUBindGroupLayout::GPUBindGroupLayout(BindingsMapType&& bindingsMap, ArgumentEncoderBuffer&& vertex, ArgumentEncoderBuffer&& fragment, ArgumentEncoderBuffer&& compute)
    : m_vertexArguments(WTFMove(vertex))
    , m_fragmentArguments(WTFMove(fragment))
    , m_computeArguments(WTFMove(compute))
    , m_bindingsMap(WTFMove(bindingsMap))
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
