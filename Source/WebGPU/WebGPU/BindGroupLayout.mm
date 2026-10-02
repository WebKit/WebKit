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
#import <ranges>
#import <wtf/TZoneMallocInlines.h>

namespace WebGPU::Metal {

static uint64_t NODELETE makeKey(uint32_t firstInteger, auto secondValue)
{
    return firstInteger | (static_cast<uint64_t>(secondValue) << 32);
}

static MTLArgumentDescriptor *createArgumentDescriptor(const BindGroupLayout::BufferBindingLayout& buffer, WGPUShaderStage visibility)
{
    auto descriptor = [MTLArgumentDescriptor new];
    auto bufferType = buffer.type;
    if (bufferType == static_cast<uint32_t>(WGPUBufferBindingType_Float3x2)) {
        descriptor.dataType = MTLDataTypeFloat3x2;
        bufferType = WGPUBufferBindingType_Uniform;
    } else if (bufferType == static_cast<uint32_t>(WGPUBufferBindingType_Float4x3)) {
        descriptor.dataType = MTLDataTypeFloat4x3;
        bufferType = WGPUBufferBindingType_Uniform;
    } else if (bufferType == static_cast<uint32_t>(WGPUBufferBindingType_Float3x3)) {
        descriptor.dataType = MTLDataTypeFloat3x3;
        bufferType = WGPUBufferBindingType_Uniform;
    } else if (bufferType == static_cast<uint32_t>(WGPUBufferBindingType_UInt2)) {
        descriptor.dataType = MTLDataTypeUInt2;
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

static MTLArgumentDescriptor *createArgumentDescriptor(const BindGroupLayout::SamplerBindingLayout&)
{
    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeSampler;
    descriptor.access = BindGroupLayout::BindingAccessReadOnly;
    return descriptor;
}

static MTLArgumentDescriptor *createTextureArgumentDescriptor()
{
    auto descriptor = [MTLArgumentDescriptor new];
    descriptor.dataType = MTLDataTypeTexture;
    descriptor.access = BindGroupLayout::BindingAccessReadOnly;
    return descriptor;
}

static MTLArgumentDescriptor *createArgumentDescriptor(const BindGroupLayout::TextureBindingLayout& texture)
{
    if (texture.multisampled) {
        if (texture.viewDimension != WGPUTextureViewDimension_2D || texture.sampleType == WGPUTextureSampleType_Float)
            return nil;
    }

    return createTextureArgumentDescriptor();
}

static MTLArgumentDescriptor *createArgumentDescriptor(const BindGroupLayout::StorageTextureBindingLayout& storageTexture, const Device& device, WGPUShaderStage visibility)
{
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

static bool NODELETE containsStage(WGPUShaderStage stageBitfield, auto stage)
{
    static_assert(1 == WGPUShaderStage_Vertex && 2 == WGPUShaderStage_Fragment && 4 == WGPUShaderStage_Compute, "Expect WGPUShaderStage to be a bitfield");
    return stageBitfield & (1 << static_cast<uint32_t>(stage));
}

static void reportErrorInCreateBindGroupLayout(NSString* errorMessage, bool isAutoGenerated, Device& device)
{
    if (!isAutoGenerated)
        device.generateAValidationError(errorMessage);
}

static const BindGroupLayout::BufferBindingLayout* bufferBindingLayout(const ResolvedBindGroupLayoutEntry& entry)
{
    return std::get_if<BindGroupLayout::BufferBindingLayout>(&entry.bindingLayout);
}

static bool NODELETE isArrayLength(const ResolvedBindGroupLayoutEntry& entry)
{
    auto* buffer = bufferBindingLayout(entry);
    return buffer && buffer->type == static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_ArrayLength);
}

static bool NODELETE hasDynamicOffset(const ResolvedBindGroupLayoutEntry& entry)
{
    auto* buffer = bufferBindingLayout(entry);
    return buffer && buffer->hasDynamicOffset;
}

// The binding layout of an entry, which has to have exactly one of the binding layout members.
static std::optional<BindGroupLayout::Entry::BindingLayout> bindingLayout(const WebGPU::BindGroupLayoutEntry& entry)
{
    // FIXME: The specification makes an entry with an external texture and another binding layout
    // a validation error. An external texture binding layout has always taken precedence over the
    // other ones instead, and fast/webgpu/nocrash/fuzz-274317.html relies on it.
    if (entry.externalTexture)
        return BindGroupLayout::ExternalTextureBindingLayout { };

    if (entry.buffer.has_value() + entry.sampler.has_value() + entry.texture.has_value() + entry.storageTexture.has_value() != 1)
        return std::nullopt;

    if (auto& buffer = entry.buffer) {
        return BindGroupLayout::BufferBindingLayout {
            .type = toAPI(buffer->type),
            .hasDynamicOffset = buffer->hasDynamicOffset,
            .minBindingSize = buffer->minBindingSize,
        };
    }
    if (auto& sampler = entry.sampler)
        return BindGroupLayout::SamplerBindingLayout { .type = toAPI(sampler->type) };
    if (auto& texture = entry.texture) {
        return BindGroupLayout::TextureBindingLayout {
            .sampleType = toAPI(texture->sampleType),
            .viewDimension = toAPI(texture->viewDimension),
            .multisampled = texture->multisampled,
        };
    }
    auto& storageTexture = *entry.storageTexture;
    return BindGroupLayout::StorageTextureBindingLayout {
        .access = toAPI(storageTexture.access),
        .format = toAPI(storageTexture.format),
        .viewDimension = toAPI(storageTexture.viewDimension),
    };
}

Ref<BindGroupLayout> Device::createBindGroupLayout(const WebGPU::BindGroupLayoutDescriptor& descriptor)
{
    if (!isValid())
        return BindGroupLayout::createInvalid(*this);

    Vector<ResolvedBindGroupLayoutEntry> entries;
    entries.reserveInitialCapacity(descriptor.entries.size());
    for (auto& entry : descriptor.entries) {
        auto entryBindingLayout = bindingLayout(entry);
        if (!entryBindingLayout) {
            generateAValidationError([NSString stringWithFormat:@"Entry for binding %u does not have exactly one of buffer, sampler, texture, storageTexture and externalTexture", entry.binding]);
            return BindGroupLayout::createInvalid(*this);
        }
        entries.append({
            .binding = entry.binding,
            .metalBinding = { entry.binding, entry.binding, entry.binding },
            .visibility = toAPI(entry.visibility),
            .bindingLayout = WTF::move(*entryBindingLayout),
        });
    }

    return createBindGroupLayout(descriptor.label, WTF::move(entries), false);
}

Ref<BindGroupLayout> Device::createBindGroupLayout(const String& label, Vector<ResolvedBindGroupLayoutEntry>&& descriptorEntries, bool isAutoGenerated)
{
    if (!isValid())
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
    BindGroupLayout::ShaderStageArray<uint32_t> maxIndices;
    constexpr auto stages = WTF::toArray<ShaderStage>({ ShaderStage::Vertex, ShaderStage::Fragment, ShaderStage::Compute });
    for (uint32_t i = 0; i < stageCount; ++i) {
        ShaderStage shaderStage = stages[i];
        arguments[i] = [NSMutableArray arrayWithCapacity:descriptorEntries.size()];
        uniformBuffersPerStage[shaderStage] = 0;
        storageBuffersPerStage[shaderStage] = 0;
        samplersPerStage[shaderStage] = 0;
        texturesPerStage[shaderStage] = 0;
        storageTexturesPerStage[shaderStage] = 0;
        maxIndices[shaderStage] = 0;
    }

    if (!isAutoGenerated)
        std::ranges::sort(descriptorEntries, { }, &ResolvedBindGroupLayoutEntry::binding);

    BindGroupLayout::EntriesContainer bindGroupLayoutEntries;
    std::array<size_t, stageCount> sizeOfDynamicOffsets { };
    std::array<uint32_t, stageCount> bindingOffset { };
    std::array<uint32_t, stageCount> bufferCounts { };
    std::array<HashMap<uint32_t, std::pair<std::array<uint32_t, stageCount>, WGPUShaderStage>, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>, stageCount> slotForEntry;
    const auto maxBindingIndex = limits().maxBindingsPerBindGroup;
    HashSet<uint32_t, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> usedBindingSlots;
    uint32_t dynamicUniformBuffers = 0;
    uint32_t dynamicStorageBuffers = 0;
    auto& deviceLimits = limits();
    for (auto& entry : descriptorEntries) {
        if (!isArrayLength(entry)) {
            if (entry.binding >= maxBindingIndex || usedBindingSlots.contains(entry.binding)) {
                reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Binding index is invalid: entry.binding(%u) >= maxBindingIndex(%u) || entry binding %s duplicated", entry.binding, maxBindingIndex, usedBindingSlots.contains(entry.binding) ? "is" : "is not"], isAutoGenerated, *this);
                return BindGroupLayout::createInvalid(*this);
            }
            usedBindingSlots.add(entry.binding);
        }

        auto* entryBufferLayout = bufferBindingLayout(entry);
        if (hasDynamicOffset(entry)) {
            if (entryBufferLayout->type == WGPUBufferBindingType_Uniform && ++dynamicUniformBuffers > deviceLimits.maxDynamicUniformBuffersPerPipelineLayout) {
                reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Too many dynamic uniform buffers: used(%u), limit(%u)", dynamicUniformBuffers, deviceLimits.maxDynamicUniformBuffersPerPipelineLayout], isAutoGenerated, *this);
                return BindGroupLayout::createInvalid(*this);
            }
            if ((entryBufferLayout->type == WGPUBufferBindingType_Storage || entryBufferLayout->type == WGPUBufferBindingType_ReadOnlyStorage) && ++dynamicStorageBuffers > deviceLimits.maxDynamicStorageBuffersPerPipelineLayout) {
                reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Too many dynamic storage buffers: used(%u), limit(%u)", dynamicStorageBuffers, deviceLimits.maxDynamicStorageBuffersPerPipelineLayout], isAutoGenerated, *this);
                return BindGroupLayout::createInvalid(*this);
            }
        }

        if (isArrayLength(entry)) {
            for (uint32_t stage = 0; stage < stageCount; ++stage) {
                if (containsStage(entry.visibility, stage))
                    slotForEntry[stage].set(entryBufferLayout->bufferSizeForBinding, std::make_pair(entry.metalBinding, entry.visibility));
            }
            continue;
        }

        bool isExternalTexture = false;
        constexpr int maxGeneratedDescriptors = 6;
        std::array<RetainPtr<MTLArgumentDescriptor>, maxGeneratedDescriptors> descriptors { };
        NSString *layoutError = nil;
        WTF::switchOn(entry.bindingLayout, [&](const BindGroupLayout::BufferBindingLayout& buffer) {
            descriptors[0] = createArgumentDescriptor(buffer, entry.visibility);
            layoutError = @"Buffer layout is not valid";
        }, [&](const BindGroupLayout::SamplerBindingLayout& sampler) {
            descriptors[0] = createArgumentDescriptor(sampler);
        }, [&](const BindGroupLayout::TextureBindingLayout& texture) {
            descriptors[0] = createArgumentDescriptor(texture);
            layoutError = @"Texture layout not valid";
        }, [&](const BindGroupLayout::StorageTextureBindingLayout& storageTexture) {
            descriptors[0] = createArgumentDescriptor(storageTexture, *this, entry.visibility);
            layoutError = @"Storage texture layout not valid";
        }, [&](const BindGroupLayout::ExternalTextureBindingLayout&) {
            isExternalTexture = true;
            descriptors[0] = createTextureArgumentDescriptor();
            descriptors[1] = createTextureArgumentDescriptor();
            BindGroupLayout::BufferBindingLayout bufferLayout { .type = static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_Float3x2) };
            descriptors[2] = createArgumentDescriptor(bufferLayout, entry.visibility);
            bufferLayout.type = static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_Float4x3);
            descriptors[3] = createArgumentDescriptor(bufferLayout, entry.visibility);
            bufferLayout.type = static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_Float3x3);
            descriptors[4] = createArgumentDescriptor(bufferLayout, entry.visibility);
            bufferLayout.type = static_cast<WGPUBufferBindingType>(WGPUBufferBindingType_UInt2);
            descriptors[5] = createArgumentDescriptor(bufferLayout, entry.visibility);
        });

        if (!descriptors[0]) {
            if (layoutError)
                reportErrorInCreateBindGroupLayout(layoutError, isAutoGenerated, *this);
            return BindGroupLayout::createInvalid(*this);
        }

        std::array<std::optional<uint32_t>, stageCount> dynamicOffsets;
        BindGroupLayout::ArgumentBufferIndices argumentBufferIndices;
        BindGroupLayout::ArgumentBufferIndices bufferSizeArgumentBufferIndices;
        if (!entry.visibility) {
            if (!isArrayLength(entry))
                indicesForBinding.add(makeKey(entry.binding, ShaderStage::Undefined), descriptors[0].get().access);
        } else {
            for (uint32_t stage = 0; stage < stageCount; ++stage) {
                auto shaderStage = stages[stage];
                if (containsStage(entry.visibility, stage)) {
                    if (!isArrayLength(entry))
                        indicesForBinding.add(makeKey(entry.binding, stage), descriptors[0].get().access);
                    auto renderStage = stages[stage];
                    auto argumentBufferBindingIndex = isAutoGenerated ? entry.metalBinding[stage] : entry.binding;
                    argumentBufferIndices[renderStage] = isAutoGenerated ? argumentBufferBindingIndex : (argumentBufferBindingIndex + bindingOffset[stage]);
                    if (hasDynamicOffset(entry)) {
                        dynamicOffsets[stage] = sizeOfDynamicOffsets[stage];
                        sizeOfDynamicOffsets[stage] += sizeof(uint32_t);
                    }
                    if (entryBufferLayout) {
                        ++bufferCounts[stage];
                        bufferSizeArgumentBufferIndices[renderStage] = bufferCounts[stage];
                        if (entryBufferLayout->type == WGPUBufferBindingType_Uniform) {
                            if (++uniformBuffersPerStage[shaderStage] > deviceLimits.maxUniformBuffersPerShaderStage) {
                                reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Uniform buffers count(%u) exceeded max count per stage(%u)", uniformBuffersPerStage[shaderStage], deviceLimits.maxUniformBuffersPerShaderStage], isAutoGenerated, *this);
                                return BindGroupLayout::createInvalid(*this);
                            }
                        }
                        if (entryBufferLayout->type == WGPUBufferBindingType_Storage || entryBufferLayout->type == WGPUBufferBindingType_ReadOnlyStorage) {
                            uint32_t maxStorageBuffers = deviceLimits.maxStorageBuffersPerShaderStage;
                            if (shaderStage == ShaderStage::Vertex)
                                maxStorageBuffers = deviceLimits.maxStorageBuffersInVertexStage;
                            else if (shaderStage == ShaderStage::Fragment)
                                maxStorageBuffers = deviceLimits.maxStorageBuffersInFragmentStage;
                            if (++storageBuffersPerStage[shaderStage] > maxStorageBuffers) {
                                reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Storage buffers count(%u) exceeded max count per stage(%u)", storageBuffersPerStage[shaderStage], maxStorageBuffers], isAutoGenerated, *this);
                                return BindGroupLayout::createInvalid(*this);
                            }
                        }
                    }
                    if (std::holds_alternative<BindGroupLayout::SamplerBindingLayout>(entry.bindingLayout)) {
                        if (++samplersPerStage[shaderStage] > deviceLimits.maxSamplersPerShaderStage) {
                            reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Sampler count(%u) exceeded max count per stage(%u)", samplersPerStage[shaderStage], deviceLimits.maxSamplersPerShaderStage], isAutoGenerated, *this);
                            return BindGroupLayout::createInvalid(*this);
                        }
                    }
                    if (std::holds_alternative<BindGroupLayout::StorageTextureBindingLayout>(entry.bindingLayout)) {
                        uint32_t maxStorageTextures = deviceLimits.maxStorageTexturesPerShaderStage;
                        if (shaderStage == ShaderStage::Vertex)
                            maxStorageTextures = deviceLimits.maxStorageTexturesInVertexStage;
                        else if (shaderStage == ShaderStage::Fragment)
                            maxStorageTextures = deviceLimits.maxStorageTexturesInFragmentStage;
                        if (++storageTexturesPerStage[shaderStage] > maxStorageTextures) {
                            reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Storage texture count(%u) exceeded max count per stage(%u)", storageTexturesPerStage[shaderStage], maxStorageTextures], isAutoGenerated, *this);
                            return BindGroupLayout::createInvalid(*this);
                        }
                    }
                    if (std::holds_alternative<BindGroupLayout::TextureBindingLayout>(entry.bindingLayout)) {
                        if (++texturesPerStage[shaderStage] > deviceLimits.maxSampledTexturesPerShaderStage) {
                            reportErrorInCreateBindGroupLayout([NSString stringWithFormat:@"Texture count(%u) exceeded max count per stage(%u)", texturesPerStage[shaderStage], deviceLimits.maxSampledTexturesPerShaderStage], isAutoGenerated, *this);
                            return BindGroupLayout::createInvalid(*this);
                        }
                    }

                    for (int descriptorIndex = 0; descriptorIndex < maxGeneratedDescriptors; ++descriptorIndex) {
                        if (MTLArgumentDescriptor *descriptor = descriptors[descriptorIndex].get()) {
                            auto newIndex = *argumentBufferIndices[renderStage] + descriptorIndex;
                            maxIndices[renderStage] = std::max(maxIndices[renderStage], newIndex);
                            addDescriptor(arguments[stage].get(), descriptor, newIndex);
                        } else
                            break;
                    }

                    if (isExternalTexture)
                        bindingOffset[stage] += (maxGeneratedDescriptors - 1);
                }
            }
        }

        if (isArrayLength(entry))
            continue;

        bindGroupLayoutEntries.add(entry.binding, BindGroupLayout::Entry {
            .binding = entry.binding,
            .visibility = entry.visibility,
            .bindingLayout = entry.bindingLayout,
            .argumentBufferIndices = WTF::move(argumentBufferIndices),
            .bufferSizeArgumentBufferIndices = WTF::move(bufferSizeArgumentBufferIndices),
            .vertexDynamicOffset = WTF::move(dynamicOffsets[0]),
            .fragmentDynamicOffset = WTF::move(dynamicOffsets[1]),
            .computeDynamicOffset = WTF::move(dynamicOffsets[2]),
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
            if (hasDynamicOffset(entry)) {
                auto it = bindGroupLayoutEntries.find(entry.binding);
                ASSERT(it != bindGroupLayoutEntries.end());
                it->value.dynamicOffsetsIndex = nextDynamicOffsetIndex++;
            }
        }
    }

    std::array<RetainPtr<NSArray<MTLArgumentDescriptor *>>, stageCount> argumentDescriptors = { nil, nil, nil };
    std::array<id<MTLArgumentEncoder>, stageCount> argumentEncoders;
    for (size_t stage = 0; stage < stageCount; ++stage) {
        auto renderStage = stages[stage];
        if (bufferCounts[stage]) {
            auto descriptor = [MTLArgumentDescriptor new];
            descriptor.dataType = MTLDataTypeInt;
            descriptor.access = BindGroupLayout::BindingAccessReadOnly;
            const auto& addArgument = [&](unsigned index) {
                addDescriptor(arguments[stage].get(), descriptor, index);
            };

            NSUInteger maxIndex = maxIndices[renderStage];
            for (auto& entry : bindGroupLayoutEntries) {
                if (entry.value.bufferSizeArgumentBufferIndices[renderStage]) {
                    if (!isAutoGenerated)
                        *entry.value.bufferSizeArgumentBufferIndices[renderStage] += maxIndex;
                    else if (auto it = slotForEntry[stage].find(entry.value.binding); it != slotForEntry[stage].end() && containsStage(it->value.second, stage))
                        entry.value.bufferSizeArgumentBufferIndices[renderStage] = it->value.first[stage];
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
        argumentEncoders[stage].label = label.createNSString().get();
        if (arguments[stage].get().count && !argumentEncoders[stage])
            return BindGroupLayout::createInvalid(*this);
    }

    BindGroupLayout::ShaderStageArray<BindGroupLayout::ArgumentIndices> argumentIndices;
    for (size_t stage = 0; stage < stageCount; ++stage) {
        for (MTLArgumentDescriptor* descriptor : argumentDescriptors[stage].get())
            argumentIndices[stages[stage]].add(static_cast<uint32_t>(descriptor.index));
    }

    if (m_bindGroupLayoutId == std::numeric_limits<decltype(m_bindGroupLayoutId)>::max()) {
        loseTheDevice(WGPUDeviceLostReason_Undefined);
        return BindGroupLayout::createInvalid(*this);
    }

    return BindGroupLayout::create(WTF::move(indicesForBinding), argumentEncoders[0], argumentEncoders[1], argumentEncoders[2], WTF::move(bindGroupLayoutEntries), sizeOfDynamicOffsets[0], sizeOfDynamicOffsets[1], sizeOfDynamicOffsets[2], isAutoGenerated, WTF::move(uniformBuffersPerStage), WTF::move(storageBuffersPerStage), WTF::move(samplersPerStage), WTF::move(texturesPerStage), WTF::move(storageTexturesPerStage), dynamicUniformBuffers, dynamicStorageBuffers, WTF::move(argumentIndices), ++m_bindGroupLayoutId, *this);
}

uint32_t BindGroupLayout::dynamicBufferCount() const
{
    return dynamicStorageBuffers() + dynamicUniformBuffers();
}

NSString* BindGroupLayout::errorValidatingDynamicOffsets(std::span<const uint32_t> dynamicOffsets, const BindGroup& group, uint32_t& maxOffset) const
{
    auto bufferCount = dynamicBufferCount();
    if (dynamicOffsets.size() != bufferCount)
        return [NSString stringWithFormat:@"dynamicOffsetCount(%zu) in setBindGroupCall does not equal the dynamicBufferCount(%u) in bind group layout", dynamicOffsets.size(), bufferCount];

    auto minUniformBufferOffsetAlignment = m_device->limits().minUniformBufferOffsetAlignment;
    auto minStorageBufferOffsetAlignment = m_device->limits().minStorageBufferOffsetAlignment;
    for (size_t i = 0; i < dynamicOffsets.size(); ++i) {
        uint32_t dynamicOffset = dynamicOffsets[i];
        maxOffset = std::max(maxOffset, dynamicOffset);
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

static bool NODELETE isEqual(const BindGroupLayout::BufferBindingLayout& entry, const BindGroupLayout::BufferBindingLayout& otherEntry)
{
    if (entry.type > WGPUBufferBindingType_ReadOnlyStorage || otherEntry.type > WGPUBufferBindingType_ReadOnlyStorage)
        return true;

    return entry.type == otherEntry.type && entry.hasDynamicOffset == otherEntry.hasDynamicOffset && entry.minBindingSize == otherEntry.minBindingSize && entry.bufferSizeForBinding == otherEntry.bufferSizeForBinding;
}
static bool NODELETE isEqual(const BindGroupLayout::SamplerBindingLayout& entry, const BindGroupLayout::SamplerBindingLayout& otherEntry)
{
    return entry.type == otherEntry.type;
}
static bool NODELETE isEqual(const BindGroupLayout::TextureBindingLayout& entry, const BindGroupLayout::TextureBindingLayout& otherEntry)
{
    return entry.multisampled == otherEntry.multisampled && entry.sampleType == otherEntry.sampleType && entry.viewDimension == otherEntry.viewDimension;
}
static bool NODELETE isEqual(const BindGroupLayout::StorageTextureBindingLayout& entry, const BindGroupLayout::StorageTextureBindingLayout& otherEntry)
{
    return entry.format == otherEntry.format && entry.access == otherEntry.access && entry.viewDimension == otherEntry.viewDimension;
}

template<typename T>
static bool NODELETE isEqual(const T* bindingLayoutPtr, const T& bindingLayout)
{
    if (!bindingLayoutPtr)
        return false;

    return isEqual(*bindingLayoutPtr, bindingLayout);
}

bool BindGroupLayout::equalBindingEntries(const BindGroupLayout::Entry::BindingLayout& entry, const BindGroupLayout::Entry::BindingLayout& otherEntry)
{
    return WTF::switchOn(entry, [&](const BindGroupLayout::BufferBindingLayout& bufferEntry) {
        return isEqual(get_if<BindGroupLayout::BufferBindingLayout>(&otherEntry), bufferEntry);
    }, [&](const BindGroupLayout::SamplerBindingLayout& samplerEntry) {
        return isEqual(std::get_if<BindGroupLayout::SamplerBindingLayout>(&otherEntry), samplerEntry);
    }, [&](const BindGroupLayout::TextureBindingLayout& textureEntry) {
        return isEqual(std::get_if<BindGroupLayout::TextureBindingLayout>(&otherEntry), textureEntry);
    }, [&](const BindGroupLayout::StorageTextureBindingLayout& storageEntry) {
        return isEqual(std::get_if<BindGroupLayout::StorageTextureBindingLayout>(&otherEntry), storageEntry);
    }, [&](const BindGroupLayout::ExternalTextureBindingLayout&) {
        return !!std::get_if<BindGroupLayout::ExternalTextureBindingLayout>(&otherEntry);
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

static uint64_t NODELETE makeBindGroupLayoutPairIdentifier(uint32_t bindGroupIdentifier, uint32_t otherBindGroupIdentifier)
{
    return static_cast<uint64_t>(bindGroupIdentifier) | (static_cast<uint64_t>(otherBindGroupIdentifier) << 32);
}

bool Device::isCachedCompatibile(const BindGroupLayout& layoutA, const BindGroupLayout& layoutB) const
{
    auto layoutIdentifierA = layoutA.uniqueId();
    auto layoutIdentifierB = layoutB.uniqueId();
    return layoutIdentifierA && layoutIdentifierB && m_bindGroupCompatibilityCache.contains(makeBindGroupLayoutPairIdentifier(layoutIdentifierA, layoutIdentifierB));
}

void Device::setCachedCompatibile(const BindGroupLayout& layoutA, const BindGroupLayout& layoutB) const
{
    m_bindGroupCompatibilityCache.add(makeBindGroupLayoutPairIdentifier(layoutA.uniqueId(), layoutB.uniqueId()));
}

void Device::removeCachedBindGroupLayout(const BindGroupLayout& bindGroupLayout) const
{
    uint64_t layoutID = bindGroupLayout.uniqueId();
    if (!layoutID)
        return;

    constexpr uint64_t uint32Max = std::numeric_limits<uint32_t>::max();
    m_bindGroupCompatibilityCache.removeIf([layoutID] (uint64_t cachedValue) {
        return layoutID == (cachedValue & uint32Max) || layoutID == ((cachedValue >> 32) & uint32Max);
    });
}

NSString* BindGroupLayout::errorValidatingBindGroupCompatibility(const BindGroupLayout& otherLayout) const
{
    Ref device = m_device;
    if (device->isCachedCompatibile(*this, otherLayout))
        return nil;

    if (isAutoGenerated() != otherLayout.isAutoGenerated() || autogeneratedPipelineLayout() != otherLayout.autogeneratedPipelineLayout())
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

    device->setCachedCompatibile(*this, otherLayout);
    return nil;
}

BindGroupLayout::BindGroupLayout(StageMapTable&& indicesForBinding, id<MTLArgumentEncoder> vertexArgumentEncoder, id<MTLArgumentEncoder> fragmentArgumentEncoder, id<MTLArgumentEncoder> computeArgumentEncoder, BindGroupLayout::EntriesContainer&& bindGroupLayoutEntries, size_t sizeOfVertexDynamicOffsets, size_t sizeOfFragmentDynamicOffsets, size_t sizeOfComputeDynamicOffsets, bool isAutoGenerated, ShaderStageArray<uint32_t>&& uniformBuffersPerStage, ShaderStageArray<uint32_t>&& storageBuffersPerStage, ShaderStageArray<uint32_t>&& samplersPerStage, ShaderStageArray<uint32_t>&& texturesPerStage, ShaderStageArray<uint32_t>&& storageTexturesPerStage, uint32_t dynamicUniformBuffers, uint32_t dynamicStorageBuffers, ShaderStageArray<ArgumentIndices>&& argumentIndices, uint32_t identifier, const Device& device)
    : m_indicesForBinding(WTF::move(indicesForBinding))
    , m_vertexArgumentEncoder(vertexArgumentEncoder)
    , m_fragmentArgumentEncoder(fragmentArgumentEncoder)
    , m_computeArgumentEncoder(computeArgumentEncoder)
    , m_bindGroupLayoutEntries(WTF::move(bindGroupLayoutEntries))
    , m_sizeOfVertexDynamicOffsets(sizeOfVertexDynamicOffsets)
    , m_sizeOfFragmentDynamicOffsets(sizeOfFragmentDynamicOffsets)
    , m_sizeOfComputeDynamicOffsets(sizeOfComputeDynamicOffsets)
    , m_device(device)
    , m_isAutoGenerated(isAutoGenerated)
    , m_uniformBuffersPerStage(WTF::move(uniformBuffersPerStage))
    , m_storageBuffersPerStage(WTF::move(storageBuffersPerStage))
    , m_samplersPerStage(WTF::move(samplersPerStage))
    , m_texturesPerStage(WTF::move(texturesPerStage))
    , m_storageTexturesPerStage(WTF::move(storageTexturesPerStage))
    , m_argumentIndices(WTF::move(argumentIndices))
    , m_dynamicUniformBuffers(dynamicUniformBuffers)
    , m_dynamicStorageBuffers(dynamicStorageBuffers)
    , m_uniqueIdentifier(identifier)
{
    m_sortedEntries.reserveCapacity(m_bindGroupLayoutEntries.size());
    for (auto& kvp : m_bindGroupLayoutEntries)
        m_sortedEntries.append(&kvp.value);
    std::ranges::sort(m_sortedEntries, { }, &Entry::binding);
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

BindGroupLayout::~BindGroupLayout()
{
    protect(m_device)->removeCachedBindGroupLayout(*this);
}

void BindGroupLayout::setLabel(String&& label)
{
    RetainPtr labelString = label.createNSString();
    m_vertexArgumentEncoder.label = labelString.get();
    m_fragmentArgumentEncoder.label = labelString.get();
    m_computeArgumentEncoder.label = labelString.get();
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

RefPtr<const PipelineLayout> BindGroupLayout::autogeneratedPipelineLayout() const
{
    return m_autogeneratedPipelineLayout.get();
}

void BindGroupLayout::setAutogeneratedPipelineLayout(const PipelineLayout* autogeneratedPipelineLayout)
{
    m_autogeneratedPipelineLayout = autogeneratedPipelineLayout;
}

bool BindGroupLayout::isAutoGenerated() const
{
    ASSERT(m_isAutoGenerated || !m_autogeneratedPipelineLayout.get());
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

} // namespace WebGPU::Metal

#pragma mark WGPU Stubs

void NODELETE wgpuBindGroupLayoutAddRef(WGPUBindGroupLayout bindGroupLayout)
{
    WebGPU::Metal::fromAPI(bindGroupLayout).ref();
}

void wgpuBindGroupLayoutRelease(WGPUBindGroupLayout bindGroupLayout)
{
    WebGPU::Metal::fromAPI(bindGroupLayout).deref();
}

void wgpuBindGroupLayoutSetLabel(WGPUBindGroupLayout bindGroupLayout, WGPUStringView label)
{
    protect(WebGPU::Metal::fromAPI(bindGroupLayout))->setLabel(WebGPU::Metal::fromAPI(label));
}
