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

static MTLDataType MTLDataTypeForBindingType(GPUBindingType type)
{
    switch (type) {
    case GPUBindingType::Sampler:
        return MTLDataTypeSampler;
    case GPUBindingType::SampledTexture:
        return MTLDataTypeTexture;
    case GPUBindingType::UniformBuffer:
    case GPUBindingType::DynamicUniformBuffer:
    case GPUBindingType::StorageBuffer:
    case GPUBindingType::DynamicStorageBuffer:
        return MTLDataTypePointer;
    }
}

using ArgumentArray = RetainPtr<NSMutableArray<MTLArgumentDescriptor *>>;
static void appendArgumentToArray(ArgumentArray& array, RetainPtr<MTLArgumentDescriptor> argument)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    if (!array)
        array = adoptNS([[NSMutableArray alloc] initWithObjects:argument.get(), nil]);
    else
        [array addObject:argument.get()];
    END_BLOCK_OBJC_EXCEPTIONS
}

static RetainPtr<MTLArgumentEncoder> tryCreateMtlArgumentEncoder(const GPUDevice& device, ArgumentArray array)
{
    RetainPtr<MTLArgumentEncoder> encoder;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    encoder = adoptNS([device.platformDevice() newArgumentEncoderWithArguments:array.get()]);
    END_BLOCK_OBJC_EXCEPTIONS
    if (!encoder) {
        LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Unable to create MTLArgumentEncoder!");
        return nullptr;
    }
    return encoder;
};

static RetainPtr<MTLArgumentDescriptor> argumentDescriptor(MTLDataType dataType, NSUInteger index)
{
    RetainPtr<MTLArgumentDescriptor> mtlArgument;
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    mtlArgument = adoptNS([MTLArgumentDescriptor new]);
    END_BLOCK_OBJC_EXCEPTIONS

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=201384 This needs to set the "textureType" field
    [mtlArgument setDataType:dataType];
    [mtlArgument setIndex:index];
    return mtlArgument;
}

RefPtr<GPUBindGroupLayout> GPUBindGroupLayout::tryCreate(const GPUDevice& device, const GPUBindGroupLayoutDescriptor& descriptor)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Invalid MTLDevice!");
        return nullptr;
    }

    ArgumentArray vertexArgs, fragmentArgs, computeArgs, vertexLengths, fragmentLengths, computeLengths;
    BindingsMapType bindingsMap;

    unsigned internalName = 0;
    unsigned internalLengthBase = descriptor.bindings.size();
    for (const auto& binding : descriptor.bindings) {
        Optional<unsigned> extraIndex;
        auto internalDetails = ([&]() -> GPUBindGroupLayout::InternalBindingDetails {
            switch (binding.type) {
            case GPUBindingType::UniformBuffer:
                extraIndex = internalLengthBase++;
                return GPUBindGroupLayout::UniformBuffer { *extraIndex };
            case GPUBindingType::DynamicUniformBuffer:
                extraIndex = internalLengthBase++;
                return GPUBindGroupLayout::DynamicUniformBuffer { *extraIndex };
            case GPUBindingType::Sampler:
                return GPUBindGroupLayout::Sampler { };
            case GPUBindingType::SampledTexture:
                return GPUBindGroupLayout::SampledTexture { };
            case GPUBindingType::StorageBuffer:
                extraIndex = internalLengthBase++;
                return GPUBindGroupLayout::StorageBuffer { *extraIndex };
            default:
                ASSERT(binding.type == GPUBindingType::DynamicStorageBuffer);
                extraIndex = internalLengthBase++;
                return GPUBindGroupLayout::DynamicStorageBuffer { *extraIndex };
            }
        })();
        Binding bindingDetails = { binding, internalName++, WTFMove(internalDetails) };
        if (!bindingsMap.add(binding.binding, bindingDetails)) {
            LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Duplicate binding %u found in GPUBindGroupLayoutDescriptor!", binding.binding);
            return nullptr;
        }

        RetainPtr<MTLArgumentDescriptor> mtlArgument = argumentDescriptor(MTLDataTypeForBindingType(binding.type), bindingDetails.internalName);

        if (!mtlArgument) {
            LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Unable to create MTLArgumentDescriptor for binding %u!", binding.binding);
            return nullptr;
        }

        auto addIndices = [&](ArgumentArray& args, ArgumentArray& lengths) -> bool {
            appendArgumentToArray(args, mtlArgument);
            if (extraIndex) {
                RetainPtr<MTLArgumentDescriptor> mtlArgument = argumentDescriptor(MTLDataTypeUInt2, *extraIndex);
                if (!mtlArgument) {
                    LOG(WebGPU, "GPUBindGroupLayout::tryCreate(): Unable to create MTLArgumentDescriptor for binding %u!", binding.binding);
                    return false;
                }
                appendArgumentToArray(lengths, mtlArgument);
            }
            return true;
        };
        if ((binding.visibility & GPUShaderStage::Flags::Vertex) && !addIndices(vertexArgs, vertexLengths))
            return nullptr;
        if ((binding.visibility & GPUShaderStage::Flags::Fragment) && !addIndices(fragmentArgs, fragmentLengths))
            return nullptr;
        if ((binding.visibility & GPUShaderStage::Flags::Compute) && !addIndices(computeArgs, computeLengths))
            return nullptr;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    [vertexArgs addObjectsFromArray:vertexLengths.get()];
    [fragmentArgs addObjectsFromArray:fragmentLengths.get()];
    [computeArgs addObjectsFromArray:computeLengths.get()];
    END_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<MTLArgumentEncoder> vertex, fragment, compute;

    if (vertexArgs && !(vertex = tryCreateMtlArgumentEncoder(device, vertexArgs)))
        return nullptr;
    if (fragmentArgs && !(fragment = tryCreateMtlArgumentEncoder(device, fragmentArgs)))
        return nullptr;
    if (computeArgs && !(compute = tryCreateMtlArgumentEncoder(device, computeArgs)))
        return nullptr;

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
