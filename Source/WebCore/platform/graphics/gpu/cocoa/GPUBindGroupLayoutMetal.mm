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
static void appendArgumentToArray(ArgumentArray array, RetainPtr<MTLArgumentDescriptor> argument)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if (!array)
        array = [[NSMutableArray alloc] initWithObjects:argument.get(), nil];
    else
        [array addObject:argument.get()];
    END_BLOCK_OBJC_EXCEPTIONS;
}

static RetainPtr<MTLArgumentEncoder> newEncoder(const GPUDevice& device, ArgumentArray array)
{
    RetainPtr<MTLArgumentEncoder> encoder;
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    encoder = adoptNS([device.platformDevice() newArgumentEncoderWithArguments:array.get()]);
    END_BLOCK_OBJC_EXCEPTIONS;
    if (!encoder)
        LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Unable to create MTLArgumentEncoder!");

    return encoder;
};

RefPtr<GPUBindGroupLayout> GPUBindGroupLayout::tryCreate(const GPUDevice& device, GPUBindGroupLayoutDescriptor&& descriptor)
{
    ArgumentArray vertexArguments, fragmentArguments, computeArguments;

    for (const auto& binding : descriptor.bindings) {
        RetainPtr<MTLArgumentDescriptor> mtlArgument;

        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        mtlArgument = adoptNS([MTLArgumentDescriptor new]);
        END_BLOCK_OBJC_EXCEPTIONS;

        if (!mtlArgument) {
            LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Unable to create MTLArgumentDescriptor!");
            return nullptr;
        }

        mtlArgument.get().dataType = MTLDataTypeForBindingType(binding.type);
        mtlArgument.get().index = binding.binding;

        if (binding.visibility & GPUShaderStageBit::VERTEX)
            appendArgumentToArray(vertexArguments, mtlArgument);
        if (binding.visibility & GPUShaderStageBit::FRAGMENT)
            appendArgumentToArray(fragmentArguments, mtlArgument);
        if (binding.visibility & GPUShaderStageBit::COMPUTE)
            appendArgumentToArray(computeArguments, mtlArgument);
    }

    ArgumentEncoders encoders;

    if (vertexArguments) {
        if (!(encoders.vertex = newEncoder(device, vertexArguments)))
            return nullptr;
    }
    if (fragmentArguments) {
        if (!(encoders.fragment = newEncoder(device, fragmentArguments)))
            return nullptr;
    }
    if (computeArguments) {
        if (!(encoders.compute = newEncoder(device, computeArguments)))
            return nullptr;
    }

    return adoptRef(new GPUBindGroupLayout(WTFMove(encoders)));
}

GPUBindGroupLayout::GPUBindGroupLayout(ArgumentEncoders&& encoders)
    : m_argumentEncoders(WTFMove(encoders))
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
