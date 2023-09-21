/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#import "APIConversions.h"
#import "Device.h"

namespace WebGPU {

static uint64_t makeKey(uint32_t firstInteger, auto secondValue)
{
    return firstInteger | (static_cast<uint64_t>(secondValue) << 32);
}

bool BindGroupLayout::isPresent(const WGPUBufferBindingLayout& buffer)
{
    return buffer.type != WGPUBufferBindingType_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUBufferBindingLayout& buffer)
{
    if (buffer.nextInChain)
        return nil;

    auto descriptor = [MTLArgumentDescriptor new];
    auto bufferType = buffer.type;
    if (bufferType == static_cast<uint32_t>(WGPUBufferBindingType_Float3x2)) {
        descriptor.dataType = MTLDataTypeFloat3x2;
        bufferType = WGPUBufferBindingType_Uniform;
    } else if (bufferType == static_cast<uint32_t>(WGPUBufferBindingType_Float4x3)) {
        descriptor.dataType = MTLDataTypeFloat4x3;
        bufferType = WGPUBufferBindingType_Uniform;
    } else
        descriptor.dataType = MTLDataTypePointer;

    // FIXME: Handle hasDynamicOffset.
    // FIXME: Handle minBindingSize.
    switch (bufferType) {
    case WGPUBufferBindingType_Uniform:
    case WGPUBufferBindingType_ReadOnlyStorage:
        descriptor.access = BindGroupLayout::BindingAccessReadOnly;
        break;
    case WGPUBufferBindingType_Storage:
        descriptor.access = BindGroupLayout::BindingAccessReadWrite;
        break;
    case WGPUBufferBindingType_Undefined:
    case WGPUBufferBindingType_Force32:
        ASSERT_NOT_REACHED();
        return nil;
    }
    return descriptor;
}

bool BindGroupLayout::isPresent(const WGPUSamplerBindingLayout& sampler)
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
    descriptor.access = BindGroupLayout::BindingAccessReadOnly;
    return descriptor;
}

bool BindGroupLayout::isPresent(const WGPUTextureBindingLayout& texture)
{
    return texture.sampleType != WGPUTextureSampleType_Undefined
        && texture.viewDimension != WGPUTextureViewDimension_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUTextureBindingLayout& texture)
{
    if (texture.nextInChain)
        return nil;

    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeTexture;
    descriptor.access = BindGroupLayout::BindingAccessReadOnly;
    return descriptor;
}

bool BindGroupLayout::isPresent(const WGPUStorageTextureBindingLayout& storageTexture)
{
    return storageTexture.access != WGPUStorageTextureAccess_Undefined
        && storageTexture.format != WGPUTextureFormat_Undefined
        && storageTexture.viewDimension != WGPUTextureViewDimension_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUStorageTextureBindingLayout& storageTexture)
{
    if (storageTexture.nextInChain)
        return nil;

    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeTexture;
    // FIXME: Implement this.
    return descriptor;
}

bool BindGroupLayout::isPresent(const WGPUExternalTextureBindingLayout&)
{
    return true;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUExternalTextureBindingLayout& externalTexture)
{
    if (externalTexture.nextInChain)
        return nil;

    // FIXME: Implement this properly.
    // External textures have a bunch of information included in them, not just a single texture.
    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeTexture;
    descriptor.access = BindGroupLayout::BindingAccessReadOnly;
    return descriptor;
}

static void addDescriptor(NSMutableArray<MTLArgumentDescriptor *> *arguments, MTLArgumentDescriptor *descriptor, NSUInteger index)
{
    MTLArgumentDescriptor *stageDescriptor = [descriptor copy];
    stageDescriptor.index = index;
    [arguments addObject:stageDescriptor];
}

static bool containsStage(WGPUShaderStageFlags stageBitfield, auto stage)
{
    static_assert(1 == WGPUShaderStage_Vertex && 2 == WGPUShaderStage_Fragment && 4 == WGPUShaderStage_Compute, "Expect WGPUShaderStage to be a bitfield");
    return stageBitfield & (1 << static_cast<uint32_t>(stage));
}

Ref<BindGroupLayout> Device::createBindGroupLayout(const WGPUBindGroupLayoutDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return BindGroupLayout::createInvalid(*this);

    BindGroupLayout::StageMapTable indicesForBinding;
    constexpr uint32_t stageCount = 3;
    static_assert(stageCount == 3, "vertex, fragment, and compute stages supported");
    NSMutableArray<MTLArgumentDescriptor *> *arguments[stageCount];
    for (uint32_t i = 0; i < stageCount; ++i)
        arguments[i] = [NSMutableArray arrayWithCapacity:descriptor.entryCount];

    Vector<BindGroupLayout::Entry> bindGroupLayoutEntries;
    bindGroupLayoutEntries.reserveInitialCapacity(descriptor.entryCount);
    for (uint32_t i = 0; i < descriptor.entryCount; ++i) {
        const WGPUBindGroupLayoutEntry& entry = descriptor.entries[i];
        if (entry.nextInChain) {
            if (entry.nextInChain->sType != static_cast<WGPUSType>(WGPUSTypeExtended_BindGroupLayoutEntryExternalTexture))
                return BindGroupLayout::createInvalid(*this);
            if (entry.nextInChain->next)
                return BindGroupLayout::createInvalid(*this);
        }

        MTLArgumentDescriptor *descriptor = nil;

        BindGroupLayout::Entry::BindingLayout bindingLayout;
        auto processBindingLayout = [&](const auto& type) {
            if (!BindGroupLayout::isPresent(type))
                return true;
            if (descriptor)
                return false;
            descriptor = createArgumentDescriptor(type);
            if (!descriptor)
                return false;
            bindingLayout = type;
            return true;
        };
        if (!processBindingLayout(entry.buffer))
            return BindGroupLayout::createInvalid(*this);
        if (!processBindingLayout(entry.sampler))
            return BindGroupLayout::createInvalid(*this);
        if (!processBindingLayout(entry.texture))
            return BindGroupLayout::createInvalid(*this);
        if (!processBindingLayout(entry.storageTexture))
            return BindGroupLayout::createInvalid(*this);
        if (entry.nextInChain) {
            const WGPUExternalTextureBindGroupLayoutEntry& externalTextureEntry = *reinterpret_cast<const WGPUExternalTextureBindGroupLayoutEntry*>(entry.nextInChain);
            ASSERT(externalTextureEntry.chain.sType == static_cast<WGPUSType>(WGPUSTypeExtended_BindGroupLayoutEntryExternalTexture));
            processBindingLayout(entry.storageTexture);
            if (!processBindingLayout(externalTextureEntry.externalTexture))
                return BindGroupLayout::createInvalid(*this);
        }

        if (!descriptor)
            return BindGroupLayout::createInvalid(*this);

        for (uint32_t stage = 0; stage < stageCount; ++stage) {
            if (containsStage(entry.visibility, stage)) {
                indicesForBinding.add(makeKey(entry.binding, stage), descriptor.access);
                addDescriptor(arguments[stage], descriptor, entry.binding);
            }
        }

        bindGroupLayoutEntries.uncheckedAppend({
            entry.binding,
            entry.visibility,
            WTFMove(bindingLayout),
        });
    }

    auto label = fromAPI(descriptor.label);
    id<MTLArgumentEncoder> argumentEncoders[stageCount];
    for (size_t stage = 0; stage < stageCount; ++stage) {
        NSArray<MTLArgumentDescriptor *> *sortedArray = [arguments[stage] sortedArrayUsingComparator:^NSComparisonResult(MTLArgumentDescriptor *a, MTLArgumentDescriptor *b) {
            if (a.index < b.index)
                return NSOrderedAscending;
            if (a.index == b.index)
                return NSOrderedSame;

            return NSOrderedDescending;
        }];
        argumentEncoders[stage] = arguments[stage].count ? [m_device newArgumentEncoderWithArguments:sortedArray] : nil;
        argumentEncoders[stage].label = label;
        if (arguments[stage].count && !argumentEncoders[stage])
            return BindGroupLayout::createInvalid(*this);
    }

    return BindGroupLayout::create(WTFMove(indicesForBinding), argumentEncoders[0], argumentEncoders[1], argumentEncoders[2], WTFMove(bindGroupLayoutEntries));
}

BindGroupLayout::BindGroupLayout(StageMapTable&& indicesForBinding, id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder, Vector<Entry>&& bindGroupLayoutEntries)
    : m_indicesForBinding(WTFMove(indicesForBinding))
    , m_vertexArgumentEncoder(vertexArgumentEncoder)
    , m_fragmentArgumentEncoder(fragmentArgumentEncoder)
    , m_computeArgumentEncoder(computeArgumentEncoder)
    , m_bindGroupLayoutEntries(WTFMove(bindGroupLayoutEntries))
{
}

BindGroupLayout::BindGroupLayout()
    : m_valid(false)
{
}

BindGroupLayout::~BindGroupLayout() = default;

void BindGroupLayout::setLabel(String&& label)
{
    auto labelString = label;
    m_vertexArgumentEncoder.label = labelString;
    m_fragmentArgumentEncoder.label = labelString;
    m_computeArgumentEncoder.label = labelString;
}

NSUInteger BindGroupLayout::encodedLength(ShaderStage shaderStage) const
{
    switch (shaderStage) {
    case ShaderStage::Vertex:
        return m_vertexArgumentEncoder.encodedLength;
    case ShaderStage::Fragment:
        return m_fragmentArgumentEncoder.encodedLength;
    case ShaderStage::Compute:
        return m_computeArgumentEncoder.encodedLength;
    }
}

std::optional<BindGroupLayout::StageMapValue> BindGroupLayout::bindingAccessForBindingIndex(uint32_t bindingIndex, ShaderStage shaderStage) const
{
    auto it = m_indicesForBinding.find(makeKey(bindingIndex, shaderStage));
    if (it == m_indicesForBinding.end())
        return std::nullopt;

    return it->value;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuBindGroupLayoutReference(WGPUBindGroupLayout bindGroupLayout)
{
    WebGPU::fromAPI(bindGroupLayout).ref();
}

void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bindGroupLayout)
{
    WebGPU::fromAPI(bindGroupLayout).deref();
}

void wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, const char* label)
{
    WebGPU::fromAPI(bindGroupLayout).setLabel(WebGPU::fromAPI(label));
}
