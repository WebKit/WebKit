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
#import "PipelineLayout.h"

#import "APIConversions.h"
#import "BindGroupLayout.h"
#import "Device.h"

namespace WebGPU {

Ref<PipelineLayout> Device::createPipelineLayout(const WGPUPipelineLayoutDescriptor& descriptor)
{
    if (descriptor.nextInChain || !isValid())
        return PipelineLayout::createInvalid(*this);

    std::optional<Vector<Ref<BindGroupLayout>>> optionalBindGroupLayouts = std::nullopt;
    if (descriptor.bindGroupLayouts) {
        Vector<Ref<BindGroupLayout>> bindGroupLayouts(descriptor.bindGroupLayoutCount, [&](size_t i) {
            auto* bindGroupLayout = descriptor.bindGroupLayouts[i];
            return Ref<BindGroupLayout> { WebGPU::fromAPI(bindGroupLayout) };
        });
        optionalBindGroupLayouts = WTFMove(bindGroupLayouts);
    }

    return PipelineLayout::create(WTFMove(optionalBindGroupLayouts), *this);
}

static void addInitialOffset(uint32_t initialOffset, uint32_t offset, uint32_t groupIndex, auto& offsets, auto& dynamicOffets)
{
    if (initialOffset != offset) {
        offsets.add(groupIndex, Vector<uint32_t>((offset - initialOffset) / sizeof(uint32_t)));
        dynamicOffets.add(groupIndex, initialOffset);
    }
}

PipelineLayout::PipelineLayout(std::optional<Vector<Ref<BindGroupLayout>>>&& optionalBindGroupLayouts, Device& device)
    : m_bindGroupLayouts(WTFMove(optionalBindGroupLayouts))
    , m_device(device)
{
    if (!m_bindGroupLayouts)
        return;

    auto& bindGroupLayouts = *m_bindGroupLayouts;
    auto bindGroupLayoutsSize = bindGroupLayouts.size();
    uint32_t vertexOffset = 0, fragmentOffset = 0, computeOffset = 0;
    for (size_t groupIndex = 0; groupIndex < bindGroupLayoutsSize; ++groupIndex) {
        auto& bindGroupLayout = bindGroupLayouts[groupIndex];
        uint32_t initialVertexOffset = vertexOffset, initialFragmentOffset = fragmentOffset, initialComputeOffset = computeOffset;
        vertexOffset += bindGroupLayout->sizeOfVertexDynamicOffsets();
        fragmentOffset += bindGroupLayout->sizeOfFragmentDynamicOffsets();
        computeOffset += bindGroupLayout->sizeOfComputeDynamicOffsets();

        addInitialOffset(initialVertexOffset, vertexOffset, groupIndex, m_vertexOffsets, m_vertexDynamicOffsets);
        addInitialOffset(initialFragmentOffset, fragmentOffset, groupIndex, m_fragmentOffsets, m_fragmentDynamicOffsets);
        addInitialOffset(initialComputeOffset, computeOffset, groupIndex, m_computeOffsets, m_computeDynamicOffsets);
    }
}

PipelineLayout::PipelineLayout(Device& device)
    : m_device(device)
    , m_isValid(false)
{
}

PipelineLayout::~PipelineLayout() = default;

void PipelineLayout::setLabel(String&&)
{
    // There is no Metal object that represents a pipeline layout.
}

bool PipelineLayout::operator==(const PipelineLayout& other) const
{
    UNUSED_PARAM(other);
    // FIXME: Implement this
    return false;
}

BindGroupLayout& PipelineLayout::bindGroupLayout(size_t i) const
{
    RELEASE_ASSERT(m_bindGroupLayouts.has_value());
    return (*m_bindGroupLayouts)[i];
}

void PipelineLayout::makeInvalid()
{
    m_isValid = false;
    if (m_bindGroupLayouts)
        m_bindGroupLayouts->clear();
}

static size_t returnTotalSize(auto& container)
{
    size_t totalSize = 0;
    for (auto& v : container) {
        ASSERT(v.value.size() < std::numeric_limits<uint32_t>::max());
        totalSize += v.value.size();
    }

    return totalSize;
}

size_t PipelineLayout::sizeOfVertexDynamicOffsets() const
{
    return returnTotalSize(m_vertexOffsets);
}

size_t PipelineLayout::sizeOfFragmentDynamicOffsets() const
{
    return returnTotalSize(m_fragmentOffsets);
}

size_t PipelineLayout::sizeOfComputeDynamicOffsets() const
{
    return returnTotalSize(m_computeOffsets);
}

static size_t returnOffsetOfGroup0(auto& v, auto groupIndex)
{
    if (auto it = v.find(groupIndex); it != v.end())
        return it->value;

    return 0;
}

size_t PipelineLayout::vertexOffsetForBindGroup(uint32_t groupIndex) const
{
    return returnOffsetOfGroup0(m_vertexDynamicOffsets, groupIndex);
}

size_t PipelineLayout::fragmentOffsetForBindGroup(uint32_t groupIndex) const
{
    return returnOffsetOfGroup0(m_fragmentDynamicOffsets, groupIndex);
}

size_t PipelineLayout::computeOffsetForBindGroup(uint32_t groupIndex) const
{
    return returnOffsetOfGroup0(m_computeDynamicOffsets, groupIndex);
}

const Vector<uint32_t>* PipelineLayout::offsetVectorForBindGroup(uint32_t bindGroupIndex, PipelineLayout::DynamicOffsetBufferMap& stageOffsets, const Vector<uint32_t>& dynamicOffsets, WGPUShaderStage stage)
{
    if (!m_bindGroupLayouts)
        return nullptr;

    auto& bindGroupLayouts = *m_bindGroupLayouts;
    if (auto it = stageOffsets.find(bindGroupIndex); it != stageOffsets.end()) {
        auto& container = it->value;
        uint32_t dynamicOffsetIndex = 0, stageOffsetIndex = 0;
        auto& bindGroupLayout = bindGroupLayouts[bindGroupIndex];
        for (auto& entryKvp : bindGroupLayout->entries()) {
            auto& entry = entryKvp.value;
            bool hasDynamicOffset = entry.vertexDynamicOffset || entry.fragmentDynamicOffset || entry.computeDynamicOffset;
            if (!hasDynamicOffset)
                continue;

            if (entry.visibility & stage) {
                ASSERT(container.size() > stageOffsetIndex && dynamicOffsets.size() > dynamicOffsetIndex);
                container[stageOffsetIndex] = dynamicOffsets[dynamicOffsetIndex];
                ++stageOffsetIndex;
            }

            ++dynamicOffsetIndex;
        }

        return &container;
    }

    return nullptr;
}

const Vector<uint32_t>* PipelineLayout::vertexOffsets(uint32_t bindGroupIndex, const Vector<uint32_t>& dynamicOffsets)
{
    return offsetVectorForBindGroup(bindGroupIndex, m_vertexOffsets, dynamicOffsets, WGPUShaderStage_Vertex);
}

const Vector<uint32_t>* PipelineLayout::fragmentOffsets(uint32_t bindGroupIndex, const Vector<uint32_t>& dynamicOffsets)
{
    return offsetVectorForBindGroup(bindGroupIndex, m_fragmentOffsets, dynamicOffsets, WGPUShaderStage_Fragment);
}

const Vector<uint32_t>* PipelineLayout::computeOffsets(uint32_t bindGroupIndex, const Vector<uint32_t>& dynamicOffsets)
{
    return offsetVectorForBindGroup(bindGroupIndex, m_computeOffsets, dynamicOffsets, WGPUShaderStage_Compute);
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuPipelineLayoutReference(WGPUPipelineLayout pipelineLayout)
{
    WebGPU::fromAPI(pipelineLayout).ref();
}

void wgpuPipelineLayoutRelease(WGPUPipelineLayout pipelineLayout)
{
    WebGPU::fromAPI(pipelineLayout).deref();
}

void wgpuPipelineLayoutSetLabel(WGPUPipelineLayout pipelineLayout, const char* label)
{
    WebGPU::fromAPI(pipelineLayout).setLabel(WebGPU::fromAPI(label));
}
