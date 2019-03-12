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
    case GPUBindGroupLayoutBinding::BindingType::DynamicUniformBuffer:
    case GPUBindGroupLayoutBinding::BindingType::StorageBuffer:
    case GPUBindGroupLayoutBinding::BindingType::DynamicStorageBuffer:
        return MTLDataTypePointer;
    }
}

using ArgumentArray = RetainPtr<NSMutableArray<MTLArgumentDescriptor *>>;
static void appendArgumentToArray(ArgumentArray& array, RetainPtr<MTLArgumentDescriptor> argument)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (!array)
        array = adoptNS([[NSMutableArray alloc] initWithObjects:argument.get(), nil]);
    else
        [array addObject:argument.get()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

static RetainPtr<MTLArgumentEncoder> tryCreateMtlArgumentEncoder(const GPUDevice& device, ArgumentArray array)
{
    RetainPtr<MTLArgumentEncoder> encoder;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    encoder = adoptNS([device.platformDevice() newArgumentEncoderWithArguments:array.get()]);
    END_BLOCK_OBJC_EXCEPTIONS;
    if (!encoder) {
        LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Unable to create MTLArgumentEncoder!");
        return nullptr;
    }
    return encoder;
};

RefPtr<GPUBindGroupLayout> GPUBindGroupLayout::tryCreate(const GPUDevice& device, const GPUBindGroupLayoutDescriptor& descriptor)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Invalid MTLDevice!");
        return nullptr;
    }

    ArgumentArray vertexArgsArray, fragmentArgsArray, computeArgsArray;
    BindingsMapType bindingsMap;

    for (const auto& binding : descriptor.bindings) {
        if (!bindingsMap.add(binding.binding, binding)) {
            LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Duplicate binding %lu found in GPUBindGroupLayoutDescriptor!", binding.binding);
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

        [mtlArgument setDataType:MTLDataTypeForBindingType(binding.type)];
        [mtlArgument setIndex:binding.binding];

        if (binding.visibility & GPUShaderStageBit::Flags::Vertex)
            appendArgumentToArray(vertexArgsArray, mtlArgument);
        if (binding.visibility & GPUShaderStageBit::Flags::Fragment)
            appendArgumentToArray(fragmentArgsArray, mtlArgument);
        if (binding.visibility & GPUShaderStageBit::Flags::Compute)
            appendArgumentToArray(computeArgsArray, mtlArgument);
    }

    RetainPtr<MTLArgumentEncoder> vertex, fragment, compute;

    if (vertexArgsArray) {
        if (!(vertex = tryCreateMtlArgumentEncoder(device, vertexArgsArray)))
            return nullptr;
    }
    if (fragmentArgsArray) {
        if (!(fragment = tryCreateMtlArgumentEncoder(device, fragmentArgsArray)))
            return nullptr;
    }
    if (computeArgsArray) {
        if (!(compute = tryCreateMtlArgumentEncoder(device, computeArgsArray)))
            return nullptr;
    }

    return adoptRef(new GPUBindGroupLayout(WTFMove(bindingsMap), WTFMove(vertex), WTFMove(fragment), WTFMove(compute)));
}

GPUBindGroupLayout::GPUBindGroupLayout(BindingsMapType&& bindingsMap, RetainPtr<MTLArgumentEncoder>&& vertex, RetainPtr<MTLArgumentEncoder>&& fragment, RetainPtr<MTLArgumentEncoder>&& compute)
    : m_vertexEncoder(WTFMove(vertex))
    , m_fragmentEncoder(WTFMove(fragment))
    , m_computeEncoder(WTFMove(compute))
    , m_bindingsMap(WTFMove(bindingsMap))
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
