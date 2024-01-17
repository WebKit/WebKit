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
#import "ComputePassEncoder.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "BindableResource.h"
#import "Buffer.h"
#import "CommandEncoder.h"
#import "ComputePipeline.h"
#import "QuerySet.h"

namespace WebGPU {

ComputePassEncoder::ComputePassEncoder(id<MTLComputeCommandEncoder> computeCommandEncoder, const WGPUComputePassDescriptor& descriptor, CommandEncoder& parentEncoder, Device& device)
    : m_computeCommandEncoder(computeCommandEncoder)
    , m_device(device)
    , m_parentEncoder(parentEncoder)
{
    m_parentEncoder->lock(true);

    if (m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::CommandBoundary) {
        if (descriptor.timestampWrites) {
            const auto& timestampWrite = *descriptor.timestampWrites;
            [m_computeCommandEncoder sampleCountersInBuffer:fromAPI(timestampWrite.querySet).counterSampleBuffer() atSampleIndex:timestampWrite.beginningOfPassWriteIndex withBarrier:NO];
            m_pendingTimestampWrites.append({ fromAPI(timestampWrite.querySet), timestampWrite.endOfPassWriteIndex });
        }
    }
}

ComputePassEncoder::ComputePassEncoder(CommandEncoder& parentEncoder, Device& device)
    : m_device(device)
    , m_parentEncoder(parentEncoder)
{
    m_parentEncoder->lock(true);
}

ComputePassEncoder::~ComputePassEncoder()
{
    [m_computeCommandEncoder endEncoding];
}

using EntryUsage = OptionSet<BindGroupEntryUsage>;
struct EntryUsageData {
    EntryUsage usage;
    uint32_t bindGroup;
};
using EntryMap = HashMap<uint64_t, EntryUsageData>;
static bool addResourceToActiveResources(id<MTLResource> mtlResource, OptionSet<BindGroupEntryUsage> initialUsage, HashMap<void*, EntryMap>& usagesForResource, uint32_t bindGroup)
{
    uint32_t baseMipLevel = 0;
    uint32_t baseArrayLayer = 0;
    WGPUTextureAspect aspect = WGPUTextureAspect_All;
    if (isTextureBindGroupEntryUsage(initialUsage)) {
        ASSERT([mtlResource conformsToProtocol:@protocol(MTLTexture)]);
        id<MTLTexture> textureView = (id<MTLTexture>)mtlResource;
        if (id<MTLTexture> parentTexture = textureView.parentTexture) {
            mtlResource = parentTexture;
            ASSERT(textureView.parentRelativeLevel <= std::numeric_limits<uint32_t>::max() && textureView.parentRelativeSlice <= std::numeric_limits<uint32_t>::max());
            if (baseMipLevel || baseArrayLayer) {
                ASSERT(textureView.parentRelativeLevel == baseMipLevel);
                ASSERT(textureView.parentRelativeSlice == baseArrayLayer);
            }
            baseMipLevel = static_cast<uint32_t>(textureView.parentRelativeLevel);
            baseArrayLayer = static_cast<uint32_t>(textureView.parentRelativeSlice);
        }
    }

    auto mapKey = BindGroup::makeEntryMapKey(baseMipLevel, baseArrayLayer, aspect);
    EntryUsage resourceUsage = initialUsage;
    auto* mtlResourceAddr = (__bridge void*)mtlResource;
    EntryMap* entryMap = nullptr;
    if (auto it = usagesForResource.find(mtlResourceAddr); it != usagesForResource.end()) {
        entryMap = &it->value;
        if (auto innerIt = it->value.find(mapKey); innerIt != it->value.end()) {
            auto existingUsage = innerIt->value.usage;
            resourceUsage.add(existingUsage);
            if (innerIt->value.bindGroup != bindGroup) {
                if ((initialUsage == BindGroupEntryUsage::StorageTextureWriteOnly && existingUsage == BindGroupEntryUsage::StorageTextureWriteOnly) || (initialUsage == BindGroupEntryUsage::StorageTextureReadWrite && existingUsage == BindGroupEntryUsage::StorageTextureReadWrite))
                    return false;
            }
        } else
            entryMap->set(mapKey, EntryUsageData { .usage = resourceUsage, .bindGroup = bindGroup });
    }

    if (!BindGroup::allowedUsage(resourceUsage))
        return false;

    if (!entryMap) {
        EntryMap entryMap;
        entryMap.set(mapKey, EntryUsageData { .usage = resourceUsage, .bindGroup = bindGroup });
        usagesForResource.set(mtlResourceAddr, entryMap);
    }

    return true;
}

void ComputePassEncoder::executePreDispatchCommands(id<MTLBuffer> indirectBuffer)
{
    if (!m_pipeline) {
        makeInvalid();
        return;
    }

    HashMap<void*, EntryMap> usagesForResource;
    if (indirectBuffer)
        addResourceToActiveResources(indirectBuffer, BindGroupEntryUsage::Input, usagesForResource, INT32_MAX);

    auto& pipelineLayout = m_pipeline->pipelineLayout();
    auto pipelineLayoutCount = pipelineLayout.numberOfBindGroupLayouts();
    for (auto& kvp : m_bindGroupResources) {
        auto bindGroupIndex = kvp.key;
        if (bindGroupIndex >= pipelineLayoutCount)
            continue;

        auto& bindGroupLayout = pipelineLayout.bindGroupLayout(bindGroupIndex);
        for (auto* bindGroupResources : kvp.value) {
            for (size_t i = 0, sz = bindGroupResources->mtlResources.size(); i < sz; ++i) {
                id<MTLResource> mtlResource = bindGroupResources->mtlResources[i];
                auto& usageData = bindGroupResources->resourceUsages[i];
                constexpr ShaderStage shaderStages[] = { ShaderStage::Vertex, ShaderStage::Fragment, ShaderStage::Compute, ShaderStage::Undefined };
                std::optional<BindGroupLayout::StageMapValue> bindingAccess = std::nullopt;
                for (auto shaderStage : shaderStages) {
                    bindingAccess = bindGroupLayout.bindingAccessForBindingIndex(usageData.binding, shaderStage);
                    if (bindingAccess)
                        break;
                }
                if (!bindingAccess)
                    continue;

                if (!addResourceToActiveResources(mtlResource, usageData.usage, usagesForResource, bindGroupIndex)) {
                    makeInvalid();
                    return;
                }
            }
        }
    }

    m_bindGroupResources.clear();

    if (!m_computeDynamicOffsets.size())
        return;

    for (auto& kvp : m_bindGroupDynamicOffsets) {
        auto& pipelineLayout = m_pipeline->pipelineLayout();
        auto bindGroupIndex = kvp.key;
        auto* pcomputeOffsets = pipelineLayout.computeOffsets(bindGroupIndex, kvp.value);
        if (pcomputeOffsets && pcomputeOffsets->size()) {
            auto& computeOffsets = *pcomputeOffsets;
            auto startIndex = pipelineLayout.computeOffsetForBindGroup(bindGroupIndex);
            RELEASE_ASSERT(computeOffsets.size() <= m_computeDynamicOffsets.size() + startIndex);
            memcpy(&m_computeDynamicOffsets[startIndex], &computeOffsets[0], sizeof(computeOffsets[0]) * computeOffsets.size());
        }
    }

    [m_computeCommandEncoder setBytes:&m_computeDynamicOffsets[0] length:m_computeDynamicOffsets.size() * sizeof(m_computeDynamicOffsets[0]) atIndex:m_device->maxBuffersForComputeStage()];

    m_bindGroupDynamicOffsets.clear();
}

void ComputePassEncoder::dispatch(uint32_t x, uint32_t y, uint32_t z)
{
    if (!(x * y * z))
        return;

    executePreDispatchCommands();
    [m_computeCommandEncoder dispatchThreadgroups:MTLSizeMake(x, y, z) threadsPerThreadgroup:m_threadsPerThreadgroup];
}

void ComputePassEncoder::dispatchIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    if ((indirectOffset % 4) || !(indirectBuffer.usage() & WGPUBufferUsage_Indirect) || (indirectOffset + 3 * sizeof(uint32_t) > indirectBuffer.buffer().length)) {
        makeInvalid();
        return;
    }

    if (&indirectBuffer.device() != m_device.ptr()) {
        makeInvalid();
        return;
    }

    executePreDispatchCommands(indirectBuffer.buffer());
    [m_computeCommandEncoder dispatchThreadgroupsWithIndirectBuffer:indirectBuffer.buffer() indirectBufferOffset:indirectOffset threadsPerThreadgroup:m_threadsPerThreadgroup];
}

void ComputePassEncoder::endPass()
{
    if (m_debugGroupStackSize || !isValid()) {
        m_parentEncoder->makeInvalid();
        return;
    }

    ASSERT(m_pendingTimestampWrites.isEmpty() || m_device->baseCapabilities().counterSamplingAPI == HardwareCapabilities::BaseCapabilities::CounterSamplingAPI::CommandBoundary);
    for (const auto& pendingTimestampWrite : m_pendingTimestampWrites)
        [m_computeCommandEncoder sampleCountersInBuffer:pendingTimestampWrite.querySet->counterSampleBuffer() atSampleIndex:pendingTimestampWrite.queryIndex withBarrier:NO];
    m_pendingTimestampWrites.clear();
    [m_computeCommandEncoder endEncoding];
    m_computeCommandEncoder = nil;
    m_parentEncoder->lock(false);
}

void ComputePassEncoder::insertDebugMarker(String&& markerLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState())
        return;

    [m_computeCommandEncoder insertDebugSignpost:markerLabel];
}

bool ComputePassEncoder::validatePopDebugGroup() const
{
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void ComputePassEncoder::makeInvalid()
{
    [m_computeCommandEncoder endEncoding];
    m_computeCommandEncoder = nil;
}

void ComputePassEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid();
        return;
    }

    --m_debugGroupStackSize;
    [m_computeCommandEncoder popDebugGroup];
}

void ComputePassEncoder::pushDebugGroup(String&& groupLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    [m_computeCommandEncoder pushDebugGroup:groupLabel];
}

void ComputePassEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    if (dynamicOffsetCount)
        m_bindGroupDynamicOffsets.add(groupIndex, Vector<uint32_t>(dynamicOffsets, dynamicOffsetCount));

    Vector<const BindableResources*> resourceList;
    for (const auto& resource : group.resources()) {
        if (resource.renderStages == BindGroup::MTLRenderStageCompute)
            [m_computeCommandEncoder useResources:&resource.mtlResources[0] count:resource.mtlResources.size() usage:resource.usage];

        ASSERT(resource.mtlResources.size() == resource.resourceUsages.size());
        resourceList.append(&resource);
    }

    m_bindGroupResources.set(groupIndex, resourceList);
    [m_computeCommandEncoder setBuffer:group.computeArgumentBuffer() offset:0 atIndex:groupIndex];
}

void ComputePassEncoder::setPipeline(const ComputePipeline& pipeline)
{
    if (!pipeline.isValid()) {
        m_device->generateAValidationError("invalid ComputePipeline in ComputePassEncoder.setPipeline"_s);
        makeInvalid();
        return;
    }

    m_pipeline = &pipeline;
    m_computeDynamicOffsets.resize(m_pipeline->pipelineLayout().sizeOfComputeDynamicOffsets());

    ASSERT(pipeline.computePipelineState());
    [m_computeCommandEncoder setComputePipelineState:pipeline.computePipelineState()];
    m_threadsPerThreadgroup = pipeline.threadsPerThreadgroup();
}

void ComputePassEncoder::setLabel(String&& label)
{
    m_computeCommandEncoder.label = label;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuComputePassEncoderReference(WGPUComputePassEncoder computePassEncoder)
{
    WebGPU::fromAPI(computePassEncoder).ref();
}

void wgpuComputePassEncoderRelease(WGPUComputePassEncoder computePassEncoder)
{
    WebGPU::fromAPI(computePassEncoder).deref();
}

void wgpuComputePassEncoderDispatchWorkgroups(WGPUComputePassEncoder computePassEncoder, uint32_t x, uint32_t y, uint32_t z)
{
    WebGPU::fromAPI(computePassEncoder).dispatch(x, y, z);
}

void wgpuComputePassEncoderDispatchWorkgroupsIndirect(WGPUComputePassEncoder computePassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(computePassEncoder).dispatchIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuComputePassEncoderEnd(WGPUComputePassEncoder computePassEncoder)
{
    WebGPU::fromAPI(computePassEncoder).endPass();
}

void wgpuComputePassEncoderInsertDebugMarker(WGPUComputePassEncoder computePassEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(computePassEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuComputePassEncoderPopDebugGroup(WGPUComputePassEncoder computePassEncoder)
{
    WebGPU::fromAPI(computePassEncoder).popDebugGroup();
}

void wgpuComputePassEncoderPushDebugGroup(WGPUComputePassEncoder computePassEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(computePassEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuComputePassEncoderSetBindGroup(WGPUComputePassEncoder computePassEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    WebGPU::fromAPI(computePassEncoder).setBindGroup(groupIndex, WebGPU::fromAPI(group), dynamicOffsetCount, dynamicOffsets);
}

void wgpuComputePassEncoderSetPipeline(WGPUComputePassEncoder computePassEncoder, WGPUComputePipeline pipeline)
{
    WebGPU::fromAPI(computePassEncoder).setPipeline(WebGPU::fromAPI(pipeline));
}

void wgpuComputePassEncoderSetLabel(WGPUComputePassEncoder computePassEncoder, const char* label)
{
    WebGPU::fromAPI(computePassEncoder).setLabel(WebGPU::fromAPI(label));
}
