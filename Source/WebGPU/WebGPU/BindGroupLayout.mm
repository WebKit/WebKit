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
#import "Texture.h"
#import <wtf/TZoneMallocInlines.h>

namespace WebGPU {

static uint64_t makeKey(uint32_t firstInteger, auto secondValue)
{
    return firstInteger | (static_cast<uint64_t>(secondValue) << 32);
}

bool BindGroupLayout::isPresent(const WGPUBufferBindingLayout& buffer)
{
    return buffer.type != WGPUBufferBindingType_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUBufferBindingLayout& buffer, const Device&, const WGPUBindGroupLayoutEntry& entry)
{
    if (buffer.nextInChain)
        return nil;

    auto visibility = entry.visibility;
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

    switch (bufferType) {
    case WGPUBufferBindingType_Uniform:
    case WGPUBufferBindingType_ReadOnlyStorage:
        descriptor.access = BindGroupLayout::BindingAccessReadOnly;
        break;
    case WGPUBufferBindingType_Storage: {
        if (visibility & WGPUShaderStage_Vertex)
            return nil;
        descriptor.access = BindGroupLayout::BindingAccessReadWrite;
        break;
    } case WGPUBufferBindingType_Undefined:
    case WGPUBufferBindingType_Force32:
        ASSERT_NOT_REACHED();
        return nil;
    }
    return descriptor;
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(BindGroupLayout);

bool BindGroupLayout::isPresent(const WGPUSamplerBindingLayout& sampler)
{
    return sampler.type != WGPUSamplerBindingType_Undefined;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUSamplerBindingLayout& sampler, const Device&, const WGPUBindGroupLayoutEntry&)
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

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUTextureBindingLayout& texture, const Device&, const WGPUBindGroupLayoutEntry&)
{
    if (texture.nextInChain)
        return nil;

    if (texture.multisampled) {
        if (texture.viewDimension != WGPUTextureViewDimension_2D || texture.sampleType == WGPUTextureSampleType_Float)
            return nil;
    }

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

static MTLArgumentDescriptor *createArgumentDescriptor(const WGPUStorageTextureBindingLayout& storageTexture, const Device& device, const WGPUBindGroupLayoutEntry& entry)
{
    if (storageTexture.nextInChain)
        return nil;

    auto visibility = entry.visibility;
    if ((visibility & WGPUShaderStage_Vertex) && storageTexture.access != WGPUStorageTextureAccess_ReadOnly)
        return nil;

    if (storageTexture.viewDimension == WGPUTextureViewDimension_Cube || storageTexture.viewDimension == WGPUTextureViewDimension_CubeArray)
        return nil;

    if (!Texture::hasStorageBindingCapability(storageTexture.format, device, storageTexture.access))
        return nil;

    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeTexture;
    descriptor.access = BindGroupLayout::BindingAccessReadWrite;
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

static void reportErrorInCreateBindGroupLayout(NSString* errorMessage, bool isAutoGenerated, Device& device)
{
    if (!isAutoGenerated)
        device.generateAValidationError(errorMessage);
}

Ref<BindGroupLayout> Device::createBindGroupLayout(const WGPUBindGroupLayoutDescriptor& descriptor, bool isAutoGenerated)
{
    if (descriptor.nextInChain || !isValid())
        return BindGroupLayout::createInvalid(*this);

    BindGroupLayout::StageMapTable indicesForBinding;
    constexpr uint32_t stageCount = 3;
    static_assert(stageCount == 3, "vertex, fragment, and compute stages supported");
    std::array<RetainPtr<NSMutableArray<MTLArgumentDescriptor *>>, stageCount> arguments;
    BindGroupLayout::ShaderStageArray<uint32_t> uniformBuffersPerStage;
    BindGroupLayout::ShaderStageArray<uint32_t> storageBuffersPerStage;
    BindGroupLayout::ShaderStageArray<uint32_t> samplersPerStage;
    BindGroupLayout::ShaderStageArray<uint32_t> texturesPerStage;
    BindGroupLayout::ShaderStageArray<uint32_t> storageTexturesPerStage;
    constexpr auto stages = std::to_array<ShaderStage>({ ShaderStage::Vertex, ShaderStage::Fragment, ShaderStage::Compute });
    for (uint32_t i = 0; i < stageCount; ++i) {
        ShaderStage shaderStage = stages[i];
        arguments[i] = [NSMutableArray arrayWithCapacity:descriptor.entryCount];
        uniformBuffersPerStage[shaderStage] = 0;
        storageBuffersPerStage[shaderStage] = 0;
        samplersPerStage[shaderStage] = 0;
        texturesPerStage[shaderStage] = 0;
        storageTexturesPerStage[shaderStage] = 0;
    }

    Vector<WGPUBindGroupLayoutEntry> descriptorEntries(descriptor.entriesSpan());
    if (!isAutoGenerated) {
        std::sort(descriptorEntries.begin(), descriptorEntries.end(), [](const WGPUBindGroupLayoutEntry& a, const WGPUBindGroupLayoutEntry& b) {
            return a.binding < b.binding;
        });
    }
    BindGroupLayout::EntriesContainer bindGroupLayoutEntries;
    std::array<size_t, stageCount> sizeOfDynamicOffsets { };
    std::array<uint32_t, stageCount> bindingOffset { };
    std::array<uint32_t, stageCount> bufferCounts { };
    HashMap<uint32_t, uint64_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> slotForEntry;
    const auto maxBindingIndex = limits().maxBindingsPerBindGroup;
    HashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> usedBindingSlots;
    uint32_t dynamicUniformBuffers = 0;
    uint32_t dynamicStorageBuffers = 0;
    auto& deviceLimits = limits();
    for (auto& entry : descriptorEntries) {
        if (entry.nextInChain) {
            if (entry.nextInChain->sType != static_cast<WGPUSType>(WGPUSTypeExtended_BindGroupLayoutEntryExternalTexture))
                return BindGroupLayout::createInvalid(*this);
            if (entry.nextInChain->next)
                return BindGroupLayout::createInvalid(*this);
        }

        if (entry.binding >= maxBindingIndex || usedBindingSlots.contains(entry.binding)) {
            reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Binding index is invalid: entry.binding(%u) >= maxBindingIndex(%u) || entry binding %s duplicated", entry.binding, maxBindingIndex, usedBindingSlots.contains(entry.binding) ? "is" : "is not"], isAutoGenerated, *this);
            return BindGroupLayout::createInvalid(*this);
        }
        usedBindingSlots.add(entry.binding);

        if (entry.buffer.hasDynamicOffset) {
            ASSERT(entry.buffer.type != WGPUBufferBindingType_Undefined);
            if (entry.buffer.type == WGPUBufferBindingType_Uniform && ++dynamicUniformBuffers > deviceLimits.maxDynamicUniformBuffersPerPipelineLayout) {
                reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Too many dynamic uniform buffers: used(%u), limit(%u)", dynamicUniformBuffers, deviceLimits.maxDynamicUniformBuffersPerPipelineLayout], isAutoGenerated, *this);
                return BindGroupLayout::createInvalid(*this);
            }
            if ((entry.buffer.type == WGPUBufferBindingType_Storage || entry.buffer.type == WGPUBufferBindingType_ReadOnlyStorage) && ++dynamicStorageBuffers > deviceLimits.maxDynamicStorageBuffersPerPipelineLayout) {
                reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Too many dynamic storage buffers: used(%u), limit(%u)", dynamicStorageBuffers, deviceLimits.maxDynamicStorageBuffersPerPipelineLayout], isAutoGenerated, *this);
                return BindGroupLayout::createInvalid(*this);
            }
        }

        bool isExternalTexture = false;
        constexpr int maxGeneratedDescriptors = 4;
        std::array<RetainPtr<MTLArgumentDescriptor>, maxGeneratedDescriptors> descriptors { };
        BindGroupLayout::Entry::BindingLayout bindingLayout;
        auto processBindingLayout = [&](const auto& type) {
            if (!BindGroupLayout::isPresent(type))
                return true;
            if (descriptors[0])
                return false;
            descriptors[0] = createArgumentDescriptor(type, *this, entry);
            if (!descriptors[0])
                return false;
            bindingLayout = type;
            return true;
        };

        if (entry.texture.sampleType == WGPUTextureSampleType_ExternalTexture) {
            isExternalTexture = true;
            descriptors[0] = createArgumentDescriptor(entry.texture, *this, entry);
            descriptors[1] = createArgumentDescriptor(entry.texture, *this, entry);
            WGPUBufferBindingLayout bufferLayout {
                .nextInChain = nullptr,
                .type = static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_Float3x2),
                .hasDynamicOffset = static_cast<WGPUBool>(false),
                .minBindingSize = 0,
                .bufferSizeForBinding = 0
            };
            descriptors[2] = createArgumentDescriptor(bufferLayout, *this, entry);
            bufferLayout.type = static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_Float4x3);
            descriptors[3] = createArgumentDescriptor(bufferLayout, *this, entry);
            bindingLayout = WGPUExternalTextureBindingLayout();
        } else if (entry.buffer.type == static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_ArrayLength)) {
            slotForEntry.set(entry.buffer.bufferSizeForBinding, entry.binding);
            continue;
        } else {
            if (!processBindingLayout(entry.buffer)) {
                reportErrorInCreateBindGroupLayout(@"Buffer layout is not valid", isAutoGenerated, *this);
                return BindGroupLayout::createInvalid(*this);
            }
            if (!processBindingLayout(entry.sampler))
                return BindGroupLayout::createInvalid(*this);
            if (!processBindingLayout(entry.texture)) {
                reportErrorInCreateBindGroupLayout(@"Texture layout not valid", isAutoGenerated, *this);
                return BindGroupLayout::createInvalid(*this);
            }
            if (!processBindingLayout(entry.storageTexture)) {
                reportErrorInCreateBindGroupLayout(@"Storage texture layout not valid", isAutoGenerated, *this);
                return BindGroupLayout::createInvalid(*this);
            }
        }

        if (!descriptors[0])
            return BindGroupLayout::createInvalid(*this);

        std::array<std::optional<uint32_t>, stageCount> dynamicOffsets;
        BindGroupLayout::ArgumentBufferIndices argumentBufferIndices;
        BindGroupLayout::ArgumentBufferIndices bufferSizeArgumentBufferIndices;
        if (!entry.visibility)
            indicesForBinding.add(makeKey(entry.binding, ShaderStage::Undefined), descriptors[0].get().access);
        else {
            for (uint32_t stage = 0; stage < stageCount; ++stage) {
                auto shaderStage = stages[stage];
                if (containsStage(entry.visibility, stage)) {
                    indicesForBinding.add(makeKey(entry.binding, stage), descriptors[0].get().access);
                    auto renderStage = stages[stage];
                    auto argumentBufferBindingIndex = isAutoGenerated ? entry.metalBinding[stage] : entry.binding;
                    argumentBufferIndices[renderStage] = isAutoGenerated ? argumentBufferBindingIndex : (argumentBufferBindingIndex + bindingOffset[stage]);
                    if (entry.buffer.hasDynamicOffset) {
                        dynamicOffsets[stage] = sizeOfDynamicOffsets[stage];
                        sizeOfDynamicOffsets[stage] += sizeof(uint32_t);
                    }
                    if (BindGroupLayout::isPresent(entry.buffer)) {
                        ++bufferCounts[stage];
                        bufferSizeArgumentBufferIndices[renderStage] = bufferCounts[stage];
                        if (entry.buffer.type == WGPUBufferBindingType_Uniform) {
                            if (++uniformBuffersPerStage[shaderStage] > deviceLimits.maxUniformBuffersPerShaderStage) {
                                reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Uniform buffers count(%u) exceeded max count per stage(%u)", uniformBuffersPerStage[shaderStage], deviceLimits.maxUniformBuffersPerShaderStage], isAutoGenerated, *this);
                                return BindGroupLayout::createInvalid(*this);
                            }
                        }
                        if (entry.buffer.type == WGPUBufferBindingType_Storage || entry.buffer.type == WGPUBufferBindingType_ReadOnlyStorage) {
                            if (++storageBuffersPerStage[shaderStage] > deviceLimits.maxStorageBuffersPerShaderStage) {
                                reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Storage buffers count(%u) exceeded max count per stage(%u)", storageBuffersPerStage[shaderStage], deviceLimits.maxStorageBuffersPerShaderStage], isAutoGenerated, *this);
                                return BindGroupLayout::createInvalid(*this);
                            }
                        }
                    }
                    if (BindGroupLayout::isPresent(entry.sampler)) {
                        if (++samplersPerStage[shaderStage] > deviceLimits.maxSamplersPerShaderStage) {
                            reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Sampler count(%u) exceeded max count per stage(%u)", samplersPerStage[shaderStage], deviceLimits.maxSamplersPerShaderStage], isAutoGenerated, *this);
                            return BindGroupLayout::createInvalid(*this);
                        }
                    }
                    if (BindGroupLayout::isPresent(entry.storageTexture)) {
                        if (++storageTexturesPerStage[shaderStage] > deviceLimits.maxStorageTexturesPerShaderStage) {
                            reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Storage texture count(%u) exceeded max count per stage(%u)", storageTexturesPerStage[shaderStage], deviceLimits.maxStorageTexturesPerShaderStage], isAutoGenerated, *this);
                            return BindGroupLayout::createInvalid(*this);
                        }
                    }
                    if (BindGroupLayout::isPresent(entry.texture)) {
                        if (++texturesPerStage[shaderStage] > deviceLimits.maxSampledTexturesPerShaderStage) {
                            reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Texture count(%u) exceeded max count per stage(%u)", texturesPerStage[shaderStage], deviceLimits.maxSampledTexturesPerShaderStage], isAutoGenerated, *this);
                            return BindGroupLayout::createInvalid(*this);
                        }
                    }

                    for (int descriptorIndex = 0; descriptorIndex < maxGeneratedDescriptors; ++descriptorIndex) {
                        if (MTLArgumentDescriptor *descriptor = descriptors[descriptorIndex].get())
                            addDescriptor(arguments[stage].get(), descriptor, *argumentBufferIndices[renderStage] + descriptorIndex);
                        else
                            break;
                    }

                    if (isExternalTexture)
                        bindingOffset[stage] += maxGeneratedDescriptors;
                }
            }
        }

        bindGroupLayoutEntries.add(entry.binding, BindGroupLayout::Entry {
            .binding = entry.binding,
            .visibility = entry.visibility,
            .bindingLayout = WTFMove(bindingLayout),
            .argumentBufferIndices = WTFMove(argumentBufferIndices),
            .bufferSizeArgumentBufferIndices = WTFMove(bufferSizeArgumentBufferIndices),
            .vertexDynamicOffset = WTFMove(dynamicOffsets[0]),
            .fragmentDynamicOffset = WTFMove(dynamicOffsets[1]),
            .computeDynamicOffset = WTFMove(dynamicOffsets[2]),
            .dynamicOffsetsIndex = std::numeric_limits<uint32_t>::max()
        });
    }

    auto hasDynamicOffsets = ^(std::array<size_t, stageCount>& sizeOfDynamicOffsets, size_t stageCount) {
        for (size_t i = 0; i < stageCount; ++i) {
            if (sizeOfDynamicOffsets[i])
                return true;
        }

        return false;
    };

    if (hasDynamicOffsets(sizeOfDynamicOffsets, stageCount)) {
        RELEASE_ASSERT(!isAutoGenerated);
        uint32_t nextDynamicOffsetIndex = 0;
        for (auto& entry : descriptorEntries) {
            if (entry.buffer.hasDynamicOffset) {
                auto it = bindGroupLayoutEntries.find(entry.binding);
                ASSERT(it != bindGroupLayoutEntries.end());
                it->value.dynamicOffsetsIndex = nextDynamicOffsetIndex++;
            }
        }
    }

    auto label = fromAPI(descriptor.label);
    std::array<RetainPtr<NSArray<MTLArgumentDescriptor *>>, stageCount> argumentDescriptors = { nil, nil, nil };
    std::array<id<MTLArgumentEncoder>, stageCount> argumentEncoders;
    for (size_t stage = 0; stage < stageCount; ++stage) {
        auto renderStage = stages[stage];
        if (auto bufferCountPerStage = bufferCounts[stage]) {
            auto descriptor = [MTLArgumentDescriptor new];
            descriptor.dataType = MTLDataTypeInt;
            descriptor.access = BindGroupLayout::BindingAccessReadOnly;
            const auto& addArgument = [&](unsigned index) {
                addDescriptor(arguments[stage].get(), descriptor, index);
            };

            NSUInteger maxIndex = [arguments[stage].get() objectAtIndex:arguments[stage].get().count - 1].index;
            for (auto& entry : bindGroupLayoutEntries) {
                if (entry.value.bufferSizeArgumentBufferIndices[renderStage]) {
                    if (!isAutoGenerated)
                        *entry.value.bufferSizeArgumentBufferIndices[renderStage] += maxIndex;
                    else if (auto it = slotForEntry.find(entry.value.binding); it != slotForEntry.end())
                        entry.value.bufferSizeArgumentBufferIndices[renderStage] = it->value;
                    else {
                        entry.value.bufferSizeArgumentBufferIndices[renderStage] = std::nullopt;
                        continue;
                    }
                    addArgument(*entry.value.bufferSizeArgumentBufferIndices[renderStage]);
                }
            }
        }
        NSArray<MTLArgumentDescriptor *> *sortedArray = [arguments[stage].get() sortedArrayUsingComparator:^NSComparisonResult(MTLArgumentDescriptor *a, MTLArgumentDescriptor *b) {
            if (a.index < b.index)
                return NSOrderedAscending;
            if (a.index == b.index)
                return NSOrderedSame;
            return NSOrderedDescending;
        }];
        argumentDescriptors[stage] = sortedArray;
        argumentEncoders[stage] = arguments[stage].get().count ? [m_device newArgumentEncoderWithArguments:sortedArray] : nil;
        argumentEncoders[stage].label = label;
        if (arguments[stage].get().count && !argumentEncoders[stage])
            return BindGroupLayout::createInvalid(*this);
    }

    BindGroupLayout::ShaderStageArray<BindGroupLayout::ArgumentIndices> argumentIndices;
    for (size_t stage = 0; stage < stageCount; ++stage) {
        for (MTLArgumentDescriptor* descriptor : argumentDescriptors[stage].get())
            argumentIndices[stages[stage]].add(static_cast<uint32_t>(descriptor.index));
    }

    return BindGroupLayout::create(WTFMove(indicesForBinding), argumentEncoders[0], argumentEncoders[1], argumentEncoders[2], WTFMove(bindGroupLayoutEntries), sizeOfDynamicOffsets[0], sizeOfDynamicOffsets[1], sizeOfDynamicOffsets[2], isAutoGenerated, WTFMove(uniformBuffersPerStage), WTFMove(storageBuffersPerStage), WTFMove(samplersPerStage), WTFMove(texturesPerStage), WTFMove(storageTexturesPerStage), dynamicUniformBuffers, dynamicStorageBuffers, WTFMove(argumentIndices), *this);
}

uint32_t BindGroupLayout::dynamicBufferCount() const
{
    return dynamicStorageBuffers() + dynamicUniformBuffers();
}

NSString* BindGroupLayout::errorValidatingDynamicOffsets(std::span<const uint32_t> dynamicOffsets, const BindGroup& group) const
{
    auto bufferCount = dynamicBufferCount();
    if (dynamicOffsets.size() != bufferCount)
        return [NSString stringWithFormat:@"dynamicOffsetCount(%zu) in setBindGroupCall does not equal the dynamicBufferCount(%u) in bind group layout", dynamicOffsets.size(), bufferCount];

    auto minUniformBufferOffsetAlignment = m_device->limits().minUniformBufferOffsetAlignment;
    auto minStorageBufferOffsetAlignment = m_device->limits().minStorageBufferOffsetAlignment;
    for (size_t i = 0; i < dynamicOffsets.size(); ++i) {
        uint32_t dynamicOffset = dynamicOffsets[i];
        auto* buffer = group.dynamicBuffer(i);
        if (!buffer)
            return [NSString stringWithFormat:@"dynamicBuffer(%zu) is nil", i];
        auto dynamicOffsetPlusBindingSize = checkedSum<uint64_t>(dynamicOffset, buffer->bindingSize);
        if (dynamicOffsetPlusBindingSize.hasOverflowed() || dynamicOffsetPlusBindingSize.value() > buffer->bufferSize)
            return [NSString stringWithFormat:@"dynamicBuffer(%zu): dynamicOffset(%u) + buffer->bindingSize(%llu) > buffer->bufferSize(%llu)", i, dynamicOffset, buffer->bindingSize, buffer->bufferSize];

        auto alignment = buffer->type == WGPUBufferBindingType_Uniform ? minUniformBufferOffsetAlignment : minStorageBufferOffsetAlignment;
        if (dynamicOffset % alignment)
            return [NSString stringWithFormat:@"dynamicBuffer(%zu): dynamicOffset(%u) is not divisible by the %s buffer alignment(%u)", i, dynamicOffset, buffer->type == WGPUBufferBindingType_Uniform ? "uniform" : "storage", alignment];
    }

    return nil;
}

static bool isEqual(const WGPUBufferBindingLayout& entry, const WGPUBufferBindingLayout& otherEntry)
{
    if (entry.type > WGPUBufferBindingType_ReadOnlyStorage || otherEntry.type > WGPUBufferBindingType_ReadOnlyStorage)
        return true;

    return entry.type == otherEntry.type && entry.hasDynamicOffset == otherEntry.hasDynamicOffset && entry.minBindingSize == otherEntry.minBindingSize && entry.bufferSizeForBinding == otherEntry.bufferSizeForBinding;
}
static bool isEqual(const WGPUSamplerBindingLayout& entry, const WGPUSamplerBindingLayout& otherEntry)
{
    return entry.type == otherEntry.type;
}
static bool isEqual(const WGPUTextureBindingLayout& entry, const WGPUTextureBindingLayout& otherEntry)
{
    return entry.multisampled == otherEntry.multisampled && entry.sampleType == otherEntry.sampleType && entry.viewDimension == otherEntry.viewDimension;
}
static bool isEqual(const WGPUStorageTextureBindingLayout& entry, const WGPUStorageTextureBindingLayout& otherEntry)
{
    return entry.format == otherEntry.format && entry.access == otherEntry.access && entry.viewDimension == otherEntry.viewDimension;
}

template<typename T>
static bool isEqual(const T* bindingLayoutPtr, const T& bindingLayout)
{
    if (!bindingLayoutPtr)
        return false;

    return isEqual(*bindingLayoutPtr, bindingLayout);
}

bool BindGroupLayout::equalBindingEntries(const BindGroupLayout::Entry::BindingLayout& entry, const BindGroupLayout::Entry::BindingLayout& otherEntry)
{
    return WTF::switchOn(entry, [&](const WGPUBufferBindingLayout& bufferEntry) {
        return isEqual(get_if<WGPUBufferBindingLayout>(&otherEntry), bufferEntry);
    }, [&](const WGPUSamplerBindingLayout& samplerEntry) {
        return isEqual(std::get_if<WGPUSamplerBindingLayout>(&otherEntry), samplerEntry);
    }, [&](const WGPUTextureBindingLayout& textureEntry) {
        return isEqual(std::get_if<WGPUTextureBindingLayout>(&otherEntry), textureEntry);
    }, [&](const WGPUStorageTextureBindingLayout& storageEntry) {
        return isEqual(std::get_if<WGPUStorageTextureBindingLayout>(&otherEntry), storageEntry);
    }, [&](const WGPUExternalTextureBindingLayout&) {
        return !!std::get_if<WGPUExternalTextureBindingLayout>(&otherEntry);
    });
}

static bool equalEntries(const BindGroupLayout::Entry& entry, const BindGroupLayout::Entry& otherEntry)
{
    if (entry.binding != otherEntry.binding)
        return false;

    if (entry.visibility != otherEntry.visibility)
        return false;

    return BindGroupLayout::equalBindingEntries(entry.bindingLayout, otherEntry.bindingLayout);
}

NSString* BindGroupLayout::errorValidatingBindGroupCompatibility(const BindGroupLayout& otherLayout) const
{
    if (autogeneratedPipelineLayout() != otherLayout.autogeneratedPipelineLayout())
        return @"Auto-generated layouts mismatch";

    auto& entries = m_sortedEntries;
    auto& otherEntries = otherLayout.sortedEntries();
    if (entries.size() != otherEntries.size())
        return [NSString stringWithFormat:@"entries.size()(%zu) > otherEntries.size()(%zu)", entries.size(), otherEntries.size()];

    auto entryCount = entries.size();
    for (size_t i = 0; i < entryCount; ++i) {
        const auto* entry = entries[i];
        const auto* otherEntry = otherEntries[i];
        RELEASE_ASSERT(entry && otherEntry);
        if (!equalEntries(*entry, *otherEntry))
            return @"entries are not equal";
    }

    return nil;
}

BindGroupLayout::BindGroupLayout(StageMapTable&& indicesForBinding, id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder, BindGroupLayout::EntriesContainer&& bindGroupLayoutEntries, size_t sizeOfVertexDynamicOffsets, size_t sizeOfFragmentDynamicOffsets, size_t sizeOfComputeDynamicOffsets, bool isAutoGenerated, ShaderStageArray<uint32_t>&& uniformBuffersPerStage, ShaderStageArray<uint32_t>&& storageBuffersPerStage, ShaderStageArray<uint32_t>&& samplersPerStage, ShaderStageArray<uint32_t>&& texturesPerStage, ShaderStageArray<uint32_t>&& storageTexturesPerStage, uint32_t dynamicUniformBuffers, uint32_t dynamicStorageBuffers, ShaderStageArray<ArgumentIndices>&& argumentIndices, const Device& device)
    : m_indicesForBinding(WTFMove(indicesForBinding))
    , m_vertexArgumentEncoder(vertexArgumentEncoder)
    , m_fragmentArgumentEncoder(fragmentArgumentEncoder)
    , m_computeArgumentEncoder(computeArgumentEncoder)
    , m_bindGroupLayoutEntries(WTFMove(bindGroupLayoutEntries))
    , m_sizeOfVertexDynamicOffsets(sizeOfVertexDynamicOffsets)
    , m_sizeOfFragmentDynamicOffsets(sizeOfFragmentDynamicOffsets)
    , m_sizeOfComputeDynamicOffsets(sizeOfComputeDynamicOffsets)
    , m_device(device)
    , m_isAutoGenerated(isAutoGenerated)
    , m_uniformBuffersPerStage(WTFMove(uniformBuffersPerStage))
    , m_storageBuffersPerStage(WTFMove(storageBuffersPerStage))
    , m_samplersPerStage(WTFMove(samplersPerStage))
    , m_texturesPerStage(WTFMove(texturesPerStage))
    , m_storageTexturesPerStage(WTFMove(storageTexturesPerStage))
    , m_argumentIndices(WTFMove(argumentIndices))
    , m_dynamicUniformBuffers(dynamicUniformBuffers)
    , m_dynamicStorageBuffers(dynamicStorageBuffers)
{
    m_sortedEntries.reserveCapacity(m_bindGroupLayoutEntries.size());
    for (auto& kvp : m_bindGroupLayoutEntries)
        m_sortedEntries.append(&kvp.value);
    std::sort(m_sortedEntries.begin(), m_sortedEntries.end(), [](const Entry* a, const Entry* b) {
        return a->binding < b->binding;
    });
}

BindGroupLayout::BindGroupLayout(const Device& device)
    : m_valid(false)
    , m_device(device)
{
}

uint32_t BindGroupLayout::uniformBuffersPerStage(ShaderStage shaderStage) const
{
    return m_uniformBuffersPerStage[shaderStage];
}
uint32_t BindGroupLayout::storageBuffersPerStage(ShaderStage shaderStage) const
{
    return m_storageBuffersPerStage[shaderStage];
}
uint32_t BindGroupLayout::samplersPerStage(ShaderStage shaderStage) const
{
    return m_samplersPerStage[shaderStage];
}
uint32_t BindGroupLayout::texturesPerStage(ShaderStage shaderStage) const
{
    return m_texturesPerStage[shaderStage];
}
uint32_t BindGroupLayout::storageTexturesPerStage(ShaderStage shaderStage) const
{
    return m_storageTexturesPerStage[shaderStage];
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
    case ShaderStage::Undefined:
        return 0;
    }
}

std::optional<BindGroupLayout::StageMapValue> BindGroupLayout::bindingAccessForBindingIndex(uint32_t bindingIndex, ShaderStage shaderStage) const
{
    auto it = m_indicesForBinding.find(makeKey(bindingIndex, shaderStage));
    if (it == m_indicesForBinding.end())
        return std::nullopt;

    return it->value;
}

uint32_t BindGroupLayout::sizeOfVertexDynamicOffsets() const
{
    return m_sizeOfVertexDynamicOffsets;
}

uint32_t BindGroupLayout::sizeOfFragmentDynamicOffsets() const
{
    return m_sizeOfFragmentDynamicOffsets;
}

uint32_t BindGroupLayout::sizeOfComputeDynamicOffsets() const
{
    return m_sizeOfComputeDynamicOffsets;
}

NSUInteger BindGroupLayout::argumentBufferIndexForEntryIndex(uint32_t bindingIndex, ShaderStage renderStage) const
{
    if (renderStage == ShaderStage::Undefined)
        return bindingAccessForBindingIndex(bindingIndex, renderStage) ? 0 : NSNotFound;

    if (auto it = m_bindGroupLayoutEntries.find(bindingIndex); it != m_bindGroupLayoutEntries.end()) {
        auto result = it->value.argumentBufferIndices[renderStage];
        return result ? *result : NSNotFound;
    }

    return NSNotFound;
}

std::optional<uint32_t> BindGroupLayout::bufferSizeIndexForEntryIndex(uint32_t bindingIndex, ShaderStage renderStage) const
{
    if (renderStage == ShaderStage::Undefined)
        return std::nullopt;

    if (auto it = m_bindGroupLayoutEntries.find(bindingIndex); it != m_bindGroupLayoutEntries.end())
        return it->value.bufferSizeArgumentBufferIndices[renderStage];

    return std::nullopt;
}

const Device& BindGroupLayout::device() const
{
    return m_device;
}

const PipelineLayout* BindGroupLayout::autogeneratedPipelineLayout() const
{
    return m_autogeneratedPipelineLayout.get();
}

void BindGroupLayout::setAutogeneratedPipelineLayout(const PipelineLayout* autogeneratedPipelineLayout)
{
    m_autogeneratedPipelineLayout = autogeneratedPipelineLayout;
}

bool BindGroupLayout::isAutoGenerated() const
{
    ASSERT(m_isAutoGenerated || !m_autogeneratedPipelineLayout);
    return m_isAutoGenerated;
}

uint32_t BindGroupLayout::dynamicUniformBuffers() const
{
    return m_dynamicUniformBuffers;
}

uint32_t BindGroupLayout::dynamicStorageBuffers() const
{
    return m_dynamicStorageBuffers;
}

const Vector<const BindGroupLayout::Entry*> BindGroupLayout::sortedEntries() const
{
    return m_sortedEntries;
}

const BindGroupLayout::ArgumentIndices& BindGroupLayout::argumentIndices(ShaderStage stage) const
{
    return m_argumentIndices[stage];
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
    WebGPU::protectedFromAPI(bindGroupLayout)->setLabel(WebGPU::fromAPI(label));
}
