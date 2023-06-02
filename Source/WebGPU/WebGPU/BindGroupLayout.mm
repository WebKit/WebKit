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
#if USE(METAL_ARGUMENT_ACCESS_ENUMS)
        descriptor.access = MTLArgumentAccessReadOnly;
#else
        descriptor.access = MTLBindingAccessReadOnly;
#endif
        break;
    case WGPUBufferBindingType_Storage:
#if USE(METAL_ARGUMENT_ACCESS_ENUMS)
        descriptor.access = MTLArgumentAccessReadWrite;
#else
        descriptor.access = MTLBindingAccessReadWrite;
#endif
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
#if USE(METAL_ARGUMENT_ACCESS_ENUMS)
    descriptor.access = MTLArgumentAccessReadOnly;
#else
    descriptor.access = MTLBindingAccessReadOnly;
#endif
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
#if USE(METAL_ARGUMENT_ACCESS_ENUMS)
    descriptor.access = MTLArgumentAccessReadOnly;
#else
    descriptor.access = MTLBindingAccessReadOnly;
#endif
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
#if USE(METAL_ARGUMENT_ACCESS_ENUMS)
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    descriptor.access = MTLArgumentAccessReadOnly;
ALLOW_DEPRECATED_DECLARATIONS_END
#else
    descriptor.access = MTLBindingAccessReadOnly;
#endif
    return descriptor;
}

static void addDescriptor(NSMutableArray<MTLArgumentDescriptor *> *arguments, MTLArgumentDescriptor *descriptor)
{
    MTLArgumentDescriptor *stageDescriptor = [descriptor copy];
    stageDescriptor.index = arguments.count;
    [arguments addObject:stageDescriptor];
}

static bool containsStage(WGPUShaderStageFlags stageBitfield, auto stage)
{
    static_assert(1 == WGPUShaderStage_Vertex && 2 == WGPUShaderStage_Fragment && 4 == WGPUShaderStage_Compute, "Expect WGPUShaderStage to be a bitfield");
    return stageBitfield & (1 << static_cast<uint32_t>(stage));
}

Ref<BindGroupLayout> Device::createBindGroupLayout(const WGPUBindGroupLayoutDescriptor& descriptor)
{
    if (descriptor.nextInChain || !descriptor.entryCount)
        return BindGroupLayout::createInvalid(*this);

    HashMap<uint32_t, WGPUShaderStageFlags> shaderStageForBinding;
    constexpr size_t stageCount = 3;
    static_assert(stageCount == 3, "vertex, fragment, and compute stages supported");
    NSMutableArray<MTLArgumentDescriptor *> *arguments[stageCount];
    for (size_t i = 0; i < stageCount; ++i)
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

        shaderStageForBinding.add(entry.binding + 1, entry.visibility);
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

        for (size_t stage = 0; stage < stageCount; ++stage) {
            if (containsStage(entry.visibility, stage))
                addDescriptor(arguments[stage], descriptor);
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
        argumentEncoders[stage] = arguments[stage].count ? [m_device newArgumentEncoderWithArguments:arguments[stage]] : nil;
        argumentEncoders[stage].label = label;
        if (arguments[stage].count && !argumentEncoders[stage])
            return BindGroupLayout::createInvalid(*this);
    }

    return BindGroupLayout::create(WTFMove(shaderStageForBinding), argumentEncoders[0], argumentEncoders[1], argumentEncoders[2], WTFMove(bindGroupLayoutEntries));
}

BindGroupLayout::BindGroupLayout(HashMap<uint32_t, WGPUShaderStageFlags>&& shaderStageForBinding, id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder, Vector<Entry>&& bindGroupLayoutEntries)
    : m_shaderStageForBinding(WTFMove(shaderStageForBinding))
    , m_vertexArgumentEncoder(vertexArgumentEncoder)
    , m_fragmentArgumentEncoder(fragmentArgumentEncoder)
    , m_computeArgumentEncoder(computeArgumentEncoder)
    , m_bindGroupLayoutEntries(WTFMove(bindGroupLayoutEntries))
{
}

BindGroupLayout::BindGroupLayout() = default;

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

bool BindGroupLayout::bindingContainsStage(uint32_t bindingIndex, ShaderStage shaderStage) const
{
    ASSERT(m_shaderStageForBinding.contains(bindingIndex + 1));
    return containsStage(m_shaderStageForBinding.find(bindingIndex + 1)->value, shaderStage);
}

#if HAVE(METAL_BUFFER_BINDING_REFLECTION)
WGPUBindGroupLayoutEntry BindGroupLayout::createEntryFromStructMember(MTLStructMember *structMember, uint32_t& currentBindingIndex, WGPUShaderStage shaderStage)
{
    WGPUBindGroupLayoutEntry entry = { };
    entry.binding = currentBindingIndex++;
    entry.visibility = shaderStage;
    switch (structMember.dataType) {
    case MTLDataTypeTexture:
        entry.texture.sampleType = WGPUTextureSampleType_Float;
        entry.texture.viewDimension = WGPUTextureViewDimension_2D;
        break;
    case MTLDataTypeSampler:
        entry.sampler.type = WGPUSamplerBindingType_Filtering;
        break;
    case MTLDataTypePointer:
        entry.buffer.type = WGPUBufferBindingType_Uniform;
        break;
    case MTLDataTypeFloat3x2:
        entry.buffer.type = static_cast<decltype(entry.buffer.type)>(WGPUBufferBindingType_Float3x2);
        break;
    case MTLDataTypeFloat4x3:
        entry.buffer.type = static_cast<decltype(entry.buffer.type)>(WGPUBufferBindingType_Float4x3);
        break;
    default:
        ASSERT_NOT_REACHED();
        break;
    }

    return entry;
}
#endif // HAVE(METAL_BUFFER_BINDING_REFLECTION)

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
