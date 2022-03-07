/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "BindGroupLayout.h"

#import "Device.h"

namespace WebGPU {

static bool isPresent(const WGPUBufferBindingLayout& buffer)
{
    return buffer.type != WGPUBufferBindingType_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUBufferBindingLayout& buffer)
{
    if (buffer.nextInChain)
        return nil;

    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypePointer;
    // FIXME: Handle hasDynamicOffset.
    // FIXME: Handle minBindingSize.
    switch (buffer.type) {
    case WGPUBufferBindingType_Uniform:
    case WGPUBufferBindingType_ReadOnlyStorage:
        descriptor.access = MTLArgumentAccessReadOnly;
        break;
    case WGPUBufferBindingType_Storage:
        descriptor.access = MTLArgumentAccessReadWrite;
        break;
    default:
        return nil;
    }
    return descriptor;
}

static bool isPresent(const WGPUSamplerBindingLayout& sampler)
{
    return sampler.type != WGPUSamplerBindingType_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUSamplerBindingLayout& sampler)
{
    if (sampler.nextInChain)
        return nil;

    UNUSED_PARAM(sampler);
    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeSampler;
    // FIXME: Implement this.
    return descriptor;
}

static bool isPresent(const WGPUTextureBindingLayout& texture)
{
    return texture.sampleType != WGPUTextureSampleType_Undefined
        && texture.viewDimension != WGPUTextureViewDimension_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUTextureBindingLayout& texture)
{
    if (texture.nextInChain)
        return nil;

    UNUSED_PARAM(texture);
    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeTexture;
    // FIXME: Implement this.
    return descriptor;
}

static bool isPresent(const WGPUStorageTextureBindingLayout& storageTexture)
{
    return storageTexture.access != WGPUStorageTextureAccess_Undefined
        && storageTexture.format != WGPUTextureFormat_Undefined
        && storageTexture.viewDimension != WGPUTextureViewDimension_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUStorageTextureBindingLayout& storageTexture)
{
    if (storageTexture.nextInChain)
        return nil;

    UNUSED_PARAM(storageTexture);
    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeTexture;
    // FIXME: Implement this.
    return descriptor;
}

RefPtr<BindGroupLayout> Device::createBindGroupLayout(const WGPUBindGroupLayoutDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return nullptr;

    NSMutableArray<MTLArgumentDescriptor *> *vertexArguments = [NSMutableArray arrayWithCapacity:descriptor.entryCount];
    NSMutableArray<MTLArgumentDescriptor *> *fragmentArguments = [NSMutableArray arrayWithCapacity:descriptor.entryCount];
    NSMutableArray<MTLArgumentDescriptor *> *computeArguments = [NSMutableArray arrayWithCapacity:descriptor.entryCount];
    if (!vertexArguments || !fragmentArguments || !computeArguments)
        return nullptr;

    for (uint32_t i = 0; i < descriptor.entryCount; ++i) {
        const WGPUBindGroupLayoutEntry& entry = descriptor.entries[i];
        if (entry.nextInChain)
            return nullptr;

        // https://gpuweb.github.io/gpuweb/#dictdef-gpubindgrouplayoutentry
        // "Only one [of buffer, sampler, texture, storageTexture, or externalTexture] may be defined for any given GPUBindGroupLayoutEntry."

        ASSERT(i == vertexArguments.count);
        ASSERT(vertexArguments.count == fragmentArguments.count);
        ASSERT(fragmentArguments.count == computeArguments.count);

        const WGPUBufferBindingLayout& buffer = entry.buffer;
        const WGPUSamplerBindingLayout& sampler = entry.sampler;
        const WGPUTextureBindingLayout& texture = entry.texture;
        const WGPUStorageTextureBindingLayout& storageTexture = entry.storageTexture;

        MTLArgumentDescriptor *descriptor = nil;

        if (isPresent(buffer)) {
            if (i != vertexArguments.count)
                return nullptr;

            descriptor = createArgumentDescriptor(buffer);
            if (!descriptor)
                return nullptr;
        }

        if (isPresent(sampler)) {
            if (i != vertexArguments.count)
                return nullptr;

            descriptor = createArgumentDescriptor(sampler);
            if (!descriptor)
                return nullptr;
        }

        if (isPresent(texture)) {
            if (i != vertexArguments.count)
                return nullptr;

            descriptor = createArgumentDescriptor(texture);
            if (!descriptor)
                return nullptr;
        }

        if (isPresent(storageTexture)) {
            if (i != vertexArguments.count)
                return nullptr;

            descriptor = createArgumentDescriptor(storageTexture);
            if (!descriptor)
                return nullptr;
        }

        // FIXME: This needs coordination with the shader compiler.
        descriptor.index = i;

        MTLArgumentDescriptor *vertexDescriptor = descriptor;
        MTLArgumentDescriptor *fragmentDescriptor = [descriptor copy];
        MTLArgumentDescriptor *computeDescriptor = [descriptor copy];
        if (!fragmentDescriptor || !computeDescriptor)
            return nullptr;

        if (!(entry.visibility & WGPUShaderStage_Vertex))
            vertexDescriptor.access = MTLArgumentAccessReadOnly;
        if (!(entry.visibility & WGPUShaderStage_Fragment))
            fragmentDescriptor.access = MTLArgumentAccessReadOnly;
        if (!(entry.visibility & WGPUShaderStage_Compute))
            computeDescriptor.access = MTLArgumentAccessReadOnly;

        [vertexArguments addObject:vertexDescriptor];
        [fragmentArguments addObject:fragmentDescriptor];
        [computeArguments addObject:computeDescriptor];
    }

    id<MTLArgumentEncoder> vertexArgumentEncoder = [m_device newArgumentEncoderWithArguments:vertexArguments];
    id<MTLArgumentEncoder> fragmentArgumentEncoder = [m_device newArgumentEncoderWithArguments:fragmentArguments];
    id<MTLArgumentEncoder> computeArgumentEncoder = [m_device newArgumentEncoderWithArguments:computeArguments];
    if (!vertexArgumentEncoder || !fragmentArgumentEncoder || !computeArgumentEncoder)
        return nullptr;

    auto label = [NSString stringWithCString:descriptor.label encoding:NSUTF8StringEncoding];
    vertexArgumentEncoder.label = label;
    fragmentArgumentEncoder.label = label;
    computeArgumentEncoder.label = label;

    return BindGroupLayout::create(vertexArgumentEncoder, fragmentArgumentEncoder, computeArgumentEncoder);
}

BindGroupLayout::BindGroupLayout(id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder)
    : m_vertexArgumentEncoder(vertexArgumentEncoder)
    , m_fragmentArgumentEncoder(fragmentArgumentEncoder)
    , m_computeArgumentEncoder(computeArgumentEncoder)
{
}

BindGroupLayout::~BindGroupLayout() = default;

void BindGroupLayout::setLabel(const char* label)
{
    auto labelString = [NSString stringWithCString:label encoding:NSUTF8StringEncoding];
    m_vertexArgumentEncoder.label = labelString;
    m_fragmentArgumentEncoder.label = labelString;
    m_computeArgumentEncoder.label = labelString;
}

NSUInteger BindGroupLayout::encodedLength() const
{
    auto result = m_vertexArgumentEncoder.encodedLength;
    ASSERT(result == m_fragmentArgumentEncoder.encodedLength);
    ASSERT(result == m_computeArgumentEncoder.encodedLength);
    return result;
}

}

void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bindGroupLayout)
{
    delete bindGroupLayout;
}

void wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, const char* label)
{
    bindGroupLayout->bindGroupLayout->setLabel(label);
}
