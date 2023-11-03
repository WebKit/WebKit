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
#import "RenderBundleEncoder.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "Buffer.h"
#import "Device.h"
#import "RenderBundle.h"
#import "RenderPipeline.h"

#define ENABLE_WEBGPU_USE_ICB_REPLAY 0

@implementation RenderBundleICBWithResources {
    Vector<WebGPU::BindableResources> _resources;
}

- (instancetype)initWithICB:(id<MTLIndirectCommandBuffer>)icb pipelineState:(id<MTLRenderPipelineState>)pipelineState depthStencilState:(id<MTLDepthStencilState>)depthStencilState cullMode:(MTLCullMode)cullMode frontFace:(MTLWinding)frontFace depthClipMode:(MTLDepthClipMode)depthClipMode depthBias:(float)depthBias depthBiasSlopeScale:(float)depthBiasSlopeScale depthBiasClamp:(float)depthBiasClamp
{
    if (!(self = [super init]))
        return nil;

    _indirectCommandBuffer = icb;
    _currentPipelineState = pipelineState;
    _depthStencilState = depthStencilState;
    _cullMode = cullMode;
    _frontFace = frontFace;
    _depthClipMode = depthClipMode;
    _depthBias = depthBias;
    _depthBiasSlopeScale = depthBiasSlopeScale;
    _depthBiasClamp = depthBiasClamp;

    return self;
}

- (Vector<WebGPU::BindableResources>*)resources
{
    return &_resources;
}

@end

namespace WebGPU {

static RenderBundleICBWithResources* makeRenderBundleICBWithResources(id<MTLIndirectCommandBuffer> icb, RenderBundle::ResourcesContainer* resources, id<MTLRenderPipelineState> renderPipelineState, id<MTLDepthStencilState> depthStencilState, MTLCullMode cullMode, MTLWinding frontFace, MTLDepthClipMode depthClipMode, float depthBias, float depthBiasSlopeScale, float depthBiasClamp)
{
    constexpr auto maxResourceUsageValue = MTLResourceUsageRead | MTLResourceUsageWrite;
    constexpr auto maxStageValue = MTLRenderStageVertex | MTLRenderStageFragment;
    static_assert(maxResourceUsageValue == 3 && maxStageValue == 3, "Code path assumes MTLResourceUsageRead | MTLResourceUsageWrite == 3 and MTLRenderStageVertex | MTLRenderStageFragment == 3");
    Vector<id<MTLResource>> stageResources[maxStageValue][maxResourceUsageValue];

    for (id<MTLResource> r : resources) {
        ResourceUsageAndRenderStage *usageAndStage = [resources objectForKey:r];
        stageResources[usageAndStage.renderStages - 1][usageAndStage.usage - 1].append(r);
    }

    RenderBundleICBWithResources* renderBundle = [[RenderBundleICBWithResources alloc] initWithICB:icb pipelineState:renderPipelineState depthStencilState:depthStencilState cullMode:cullMode frontFace:frontFace depthClipMode:depthClipMode depthBias:depthBias depthBiasSlopeScale:depthBiasSlopeScale depthBiasClamp:depthBiasClamp];

    for (size_t stage = 0; stage < maxStageValue; ++stage) {
        for (size_t i = 0; i < maxResourceUsageValue; ++i) {
            Vector<id<MTLResource>> &v = stageResources[stage][i];
            if (v.size()) {
                renderBundle.resources->append(BindableResources {
                    .mtlResources = WTFMove(v),
                    .usage = static_cast<MTLResourceUsage>(i + 1),
                    .renderStages = static_cast<MTLRenderStages>(stage + 1)
                });
            }
        }
    }

    return renderBundle;
}

Ref<RenderBundleEncoder> Device::createRenderBundleEncoder(const WGPURenderBundleEncoderDescriptor& descriptor)
{
    if (descriptor.nextInChain)
        return RenderBundleEncoder::createInvalid(*this);

    MTLIndirectCommandBufferDescriptor *icbDescriptor = [MTLIndirectCommandBufferDescriptor new];
    icbDescriptor.inheritBuffers = NO;
    icbDescriptor.inheritPipelineState = NO;

    return RenderBundleEncoder::create(icbDescriptor, *this);
}

RenderBundleEncoder::RenderBundleEncoder(MTLIndirectCommandBufferDescriptor *indirectCommandBufferDescriptor, Device& device)
    : m_icbDescriptor(indirectCommandBufferDescriptor)
    , m_resources([NSMapTable strongToStrongObjectsMapTable])
    , m_device(device)
{
    m_icbArray = [NSMutableArray array];
}

RenderBundleEncoder::RenderBundleEncoder(Device& device)
    : m_device(device)
{
}

RenderBundleEncoder::~RenderBundleEncoder() = default;

id<MTLIndirectRenderCommand> RenderBundleEncoder::currentRenderCommand()
{
    if (m_commandEncoder)
        return (id<MTLIndirectRenderCommand>)m_commandEncoder;

    ASSERT(!m_indirectCommandBuffer || m_currentCommandIndex < m_indirectCommandBuffer.size);
    return m_currentCommandIndex < m_indirectCommandBuffer.size ? [m_indirectCommandBuffer indirectRenderCommandAtIndex:m_currentCommandIndex] : nil;
}

void RenderBundleEncoder::addResource(RenderBundle::ResourcesContainer* resources, id<MTLResource> mtlResource, ResourceUsageAndRenderStage *resource)
{
    if (m_commandEncoder) {
        [m_commandEncoder useResource:mtlResource usage:resource.usage stages:resource.renderStages];
        return;
    }

    if (ResourceUsageAndRenderStage *existingResource = [resources objectForKey:mtlResource]) {
        existingResource.usage |= resource.usage;
        existingResource.renderStages |= resource.renderStages;
    } else
        [resources setObject:resource forKey:mtlResource];
}

void RenderBundleEncoder::addResource(RenderBundle::ResourcesContainer* resources, id<MTLResource> mtlResource, MTLRenderStages stage)
{
    if (m_commandEncoder) {
        [m_commandEncoder useResource:mtlResource usage:MTLResourceUsageRead stages:stage];
        return;
    }

    return addResource(resources, mtlResource, [[ResourceUsageAndRenderStage alloc] initWithUsage:MTLResourceUsageRead renderStages:stage]);
}

void RenderBundleEncoder::executePreDrawCommands()
{
    auto vertexDynamicOffset = m_vertexDynamicOffset;
    auto fragmentDynamicOffset = m_fragmentDynamicOffset;
    if (m_pipeline) {
        m_vertexDynamicOffset += sizeof(uint32_t) * m_pipeline->pipelineLayout().sizeOfVertexDynamicOffsets();
        m_fragmentDynamicOffset += sizeof(uint32_t) * m_pipeline->pipelineLayout().sizeOfFragmentDynamicOffsets();
    }

    id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand();
    if (!icbCommand)
        return;

    if (m_currentPipelineState)
        [icbCommand setRenderPipelineState:m_currentPipelineState];

    for (size_t i = 0, sz = m_vertexBuffers.size(); i < sz; ++i) {
        if (m_vertexBuffers[i].buffer)
            [icbCommand setVertexBuffer:m_vertexBuffers[i].buffer offset:m_vertexBuffers[i].offset atIndex:i];
    }

    for (size_t i = 0, sz = m_fragmentBuffers.size(); i < sz; ++i) {
        if (m_fragmentBuffers[i].buffer)
            [icbCommand setFragmentBuffer:m_fragmentBuffers[i].buffer offset:m_fragmentBuffers[i].offset atIndex:i];
    }

    for (auto& kvp : m_bindGroupDynamicOffsets) {
        auto& pipelineLayout = m_pipeline->pipelineLayout();
        auto bindGroupIndex = kvp.key;

        if (m_dynamicOffsetsVertexBuffer) {
            auto maxBufferLength = m_dynamicOffsetsVertexBuffer.length;
            auto bufferOffset = vertexDynamicOffset;
            uint8_t* vertexBufferContents = static_cast<uint8_t*>(m_dynamicOffsetsVertexBuffer.contents) + bufferOffset;
            auto* pvertexOffsets = pipelineLayout.vertexOffsets(bindGroupIndex, kvp.value);
            if (pvertexOffsets && pvertexOffsets->size()) {
                auto& vertexOffsets = *pvertexOffsets;
                auto startIndex = sizeof(uint32_t) * pipelineLayout.vertexOffsetForBindGroup(bindGroupIndex);
                auto bytesToCopy = sizeof(vertexOffsets[0]) * vertexOffsets.size();
                RELEASE_ASSERT(bytesToCopy <= maxBufferLength - (startIndex + bufferOffset));
                memcpy(&vertexBufferContents[startIndex], &vertexOffsets[0], bytesToCopy);
            }
        }

        if (m_dynamicOffsetsFragmentBuffer) {
            auto maxBufferLength = m_dynamicOffsetsVertexBuffer.length;
            auto bufferOffset = fragmentDynamicOffset;
            uint8_t* fragmentBufferContents = static_cast<uint8_t*>(m_dynamicOffsetsFragmentBuffer.contents) + bufferOffset;
            auto* pfragmentOffsets = pipelineLayout.fragmentOffsets(bindGroupIndex, kvp.value);
            if (pfragmentOffsets && pfragmentOffsets->size()) {
                auto& fragmentOffsets = *pfragmentOffsets;
                auto startIndex = sizeof(uint32_t) * pipelineLayout.fragmentOffsetForBindGroup(bindGroupIndex);
                auto bytesToCopy = sizeof(fragmentOffsets[0]) * fragmentOffsets.size();
                RELEASE_ASSERT(bytesToCopy <= maxBufferLength - (startIndex + bufferOffset));
                memcpy(&fragmentBufferContents[startIndex], &fragmentOffsets[0], bytesToCopy);
            }
        }
    }

    if (m_dynamicOffsetsVertexBuffer)
        [icbCommand setVertexBuffer:m_dynamicOffsetsVertexBuffer offset:vertexDynamicOffset atIndex:m_device->maxBuffersPlusVertexBuffersForVertexStage()];

    if (m_dynamicOffsetsFragmentBuffer)
        [icbCommand setFragmentBuffer:m_dynamicOffsetsFragmentBuffer offset:fragmentDynamicOffset atIndex:m_fragmentBuffers.size()];

    m_bindGroupDynamicOffsets.clear();
}

void RenderBundleEncoder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    if (!vertexCount || !instanceCount)
        return;

    executePreDrawCommands();
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand())
        [icbCommand drawPrimitives:m_primitiveType vertexStart:firstVertex vertexCount:vertexCount instanceCount:instanceCount baseInstance:firstInstance];
    else {
        m_icbDescriptor.commandTypes |= MTLIndirectCommandTypeDraw;

        m_recordedCommands.append([vertexCount, instanceCount, firstVertex, firstInstance, protectedThis = Ref { *this }] {
            protectedThis->draw(vertexCount, instanceCount, firstVertex, firstInstance);
            return true;
        });
    }

    ++m_currentCommandIndex;
}

void RenderBundleEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    if (!indexCount || !instanceCount)
        return;

    executePreDrawCommands();
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        auto firstIndexOffsetInBytes = firstIndex * (m_indexType == MTLIndexTypeUInt16 ? sizeof(uint16_t) : sizeof(uint32_t));
        [icbCommand drawIndexedPrimitives:m_primitiveType indexCount:indexCount indexType:m_indexType indexBuffer:m_indexBuffer indexBufferOffset:(m_indexBufferOffset + firstIndexOffsetInBytes) instanceCount:instanceCount baseVertex:baseVertex baseInstance:firstInstance];
    } else {
        m_icbDescriptor.commandTypes |= MTLIndirectCommandTypeDrawIndexed;

        m_recordedCommands.append([indexCount, instanceCount, firstIndex, baseVertex, firstInstance, protectedThis = Ref { *this }] {
            protectedThis->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
            return true;
        });
    }

    ++m_currentCommandIndex;
}

void RenderBundleEncoder::drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    UNUSED_PARAM(indirectOffset);

    auto contents = (MTLDrawIndexedPrimitivesIndirectArguments*)indirectBuffer.buffer().contents;
    if (!contents || !contents->indexCount || !contents->instanceCount)
        return;

    executePreDrawCommands();
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        if (!m_indexBuffer.length)
            return;

        ASSERT(m_indexBufferOffset == contents->indexStart);
        [icbCommand drawIndexedPrimitives:m_primitiveType indexCount:contents->indexCount indexType:m_indexType indexBuffer:m_indexBuffer indexBufferOffset:m_indexBufferOffset instanceCount:contents->instanceCount baseVertex:contents->baseVertex baseInstance:contents->baseInstance];
    } else {
        m_icbDescriptor.commandTypes |= MTLIndirectCommandTypeDrawIndexed;

        m_recordedCommands.append([&indirectBuffer, indirectOffset, protectedThis = Ref { *this }] {
            protectedThis->drawIndexedIndirect(indirectBuffer, indirectOffset);
            return true;
        });
    }

    ++m_currentCommandIndex;
}

void RenderBundleEncoder::drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    UNUSED_PARAM(indirectOffset);

    auto contents = (MTLDrawPrimitivesIndirectArguments*)indirectBuffer.buffer().contents;
    if (!contents || !contents->instanceCount || !contents->vertexCount)
        return;

    executePreDrawCommands();
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand())
        [icbCommand drawPrimitives:m_primitiveType vertexStart:contents->vertexStart vertexCount:contents->vertexCount instanceCount:contents->instanceCount baseInstance:contents->baseInstance];
    else {
        m_icbDescriptor.commandTypes |= MTLIndirectCommandTypeDraw;

        m_recordedCommands.append([&indirectBuffer, indirectOffset, protectedThis = Ref { *this }] {
            protectedThis->drawIndirect(indirectBuffer, indirectOffset);
            return true;
        });
    }

    ++m_currentCommandIndex;
}

void RenderBundleEncoder::endCurrentICB()
{
    auto commandCount = m_currentCommandIndex;
    m_currentCommandIndex = 0;

    ASSERT(!m_indirectCommandBuffer);
    m_icbDescriptor.maxVertexBufferBindCount = m_device->maxBuffersPlusVertexBuffersForVertexStage() + 1;
    m_vertexBuffers.grow(m_icbDescriptor.maxVertexBufferBindCount);
    if (m_fragmentBuffers.size() < m_icbDescriptor.maxFragmentBufferBindCount + 1)
        m_fragmentBuffers.grow(m_icbDescriptor.maxFragmentBufferBindCount + 1);
    if (m_vertexDynamicOffset) {
        m_dynamicOffsetsVertexBuffer = [m_device->device() newBufferWithLength:m_vertexDynamicOffset options:MTLResourceStorageModeShared];
        addResource(m_resources, m_dynamicOffsetsVertexBuffer, MTLRenderStageVertex);
        m_vertexDynamicOffset = 0;
    }

    if (m_fragmentDynamicOffset) {
        m_dynamicOffsetsFragmentBuffer = [m_device->device() newBufferWithLength:m_fragmentDynamicOffset options:MTLResourceStorageModeShared];
        addResource(m_resources, m_dynamicOffsetsFragmentBuffer, MTLRenderStageFragment);
        m_fragmentDynamicOffset = 0;
    }

    m_indirectCommandBuffer = [m_device->device() newIndirectCommandBufferWithDescriptor:m_icbDescriptor maxCommandCount:commandCount options:0];

    uint64_t completedDraws = 0, lastIndexOfRecordedCommand = 0;
    for (auto& command : m_recordedCommands) {
        completedDraws += command() ? 1 : 0;
        ++lastIndexOfRecordedCommand;

        if (completedDraws == commandCount)
            break;
    }

    if (m_commandEncoder)
        return;

    if (lastIndexOfRecordedCommand == m_recordedCommands.size())
        m_recordedCommands.clear();
    else
        m_recordedCommands.remove(0, lastIndexOfRecordedCommand);

    [m_icbArray addObject:makeRenderBundleICBWithResources(m_indirectCommandBuffer, m_resources, m_currentPipelineState, m_depthStencilState, m_cullMode, m_frontFace, m_depthClipMode, m_depthBias, m_depthBiasSlopeScale, m_depthBiasClamp)];
    m_indirectCommandBuffer = nil;

    m_currentPipelineState = nil;
    m_resources = [NSMapTable strongToStrongObjectsMapTable];

    m_icbDescriptor.commandTypes = 0;
}

Ref<RenderBundle> RenderBundleEncoder::finish(const WGPURenderBundleDescriptor& descriptor)
{
    if (!m_currentCommandIndex)
        return RenderBundle::createInvalid(m_device);

#if ENABLE(WEBGPU_USE_ICB_REPLAY)
    auto renderBundle = RenderBundle::create(m_icbArray, this, m_device);
#else
    endCurrentICB();
    auto renderBundle = RenderBundle::create(m_icbArray, nullptr, m_device);
    m_vertexBuffers.clear();
    m_fragmentBuffers.clear();
    m_icbDescriptor.maxVertexBufferBindCount = 0;
    m_icbDescriptor.maxFragmentBufferBindCount = 0;
#endif
    renderBundle->setLabel(String::fromUTF8(descriptor.label));

    return renderBundle;
}

bool RenderBundleEncoder::replayCommands(id<MTLRenderCommandEncoder> commandEncoder)
{
    m_commandEncoder = commandEncoder;
    endCurrentICB();
    m_commandEncoder = nil;
    m_currentPipelineState = nil;
    m_depthStencilState = nil;

    return true;
}

void RenderBundleEncoder::insertDebugMarker(String&&)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker

    if (!prepareTheEncoderState())
        return;

    // MTLIndirectCommandBuffers don't support debug commands.
}

bool RenderBundleEncoder::validatePopDebugGroup() const
{
    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void RenderBundleEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid();
        return;
    }

    --m_debugGroupStackSize;
    // MTLIndirectCommandBuffers don't support debug commands.
}

void RenderBundleEncoder::pushDebugGroup(String&&)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    // MTLIndirectCommandBuffers don't support debug commands.
}

void RenderBundleEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, std::optional<Vector<uint32_t>>&& dynamicOffsets)
{
    id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand();
    if (!icbCommand) {
        if (group.fragmentArgumentBuffer())
            m_icbDescriptor.maxFragmentBufferBindCount = std::max<NSUInteger>(m_icbDescriptor.maxFragmentBufferBindCount, 1 + groupIndex);

        m_recordedCommands.append([groupIndex, &group, protectedThis = Ref { *this }, dynamicOffsets = WTFMove(dynamicOffsets)]() mutable {
            protectedThis->setBindGroup(groupIndex, group, WTFMove(dynamicOffsets));
            return false;
        });
        return;
    }

    uint32_t dynamicOffsetCount = dynamicOffsets ? dynamicOffsets->size() : 0;
    if (dynamicOffsetCount)
        m_bindGroupDynamicOffsets.set(groupIndex, WTFMove(*dynamicOffsets));

    for (const auto& resource : group.resources()) {
        ResourceUsageAndRenderStage* usageAndRenderStage = [[ResourceUsageAndRenderStage alloc] initWithUsage:resource.usage renderStages:resource.renderStages];
        for (id<MTLResource> mtlResource : resource.mtlResources)
            addResource(m_resources, mtlResource, usageAndRenderStage);
    }

    if (group.vertexArgumentBuffer()) {
        addResource(m_resources, group.vertexArgumentBuffer(), MTLRenderStageVertex);
        m_vertexBuffers[m_device->vertexBufferIndexForBindGroup(groupIndex)] = { group.vertexArgumentBuffer(), 0, dynamicOffsetCount, dynamicOffsets->data() };
    }
    if (group.fragmentArgumentBuffer()) {
        addResource(m_resources, group.fragmentArgumentBuffer(), MTLRenderStageFragment);
        m_fragmentBuffers[groupIndex] = { group.fragmentArgumentBuffer(), 0, dynamicOffsetCount, dynamicOffsets->data() };
    }
}

void RenderBundleEncoder::setIndexBuffer(const Buffer& buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    if (!currentRenderCommand()) {
        m_recordedCommands.append([&buffer, format, offset, size, protectedThis = Ref { *this }] {
            protectedThis->setIndexBuffer(buffer, format, offset, size);
            return false;
        });
        return;
    }

    m_indexBuffer = buffer.buffer();
    RELEASE_ASSERT(m_indexBuffer);
    m_indexType = format == WGPUIndexFormat_Uint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    m_indexBufferOffset = offset;
    if (m_indexBuffer)
        addResource(m_resources, m_indexBuffer, MTLRenderStageVertex);
}

static bool icbNeedsToBeSplit(const RenderPipeline& a, const RenderPipeline& b)
{
#if ENABLE(WEBGPU_USE_ICB_REPLAY)
    UNUSED_PARAM(a);
    UNUSED_PARAM(b);
    return false;
#else
    if (&a == &b)
        return false;

    if (a.cullMode() != b.cullMode())
        return true;

    if (a.frontFace() != b.frontFace())
        return true;

    if (a.depthClipMode() != b.depthClipMode())
        return true;

    if (![a.depthStencilDescriptor() isEqual:b.depthStencilDescriptor()])
        return true;

    if (a.depthBias() != b.depthBias())
        return true;

    if (a.depthBiasSlopeScale() != b.depthBiasSlopeScale())
        return true;

    if (a.depthBiasClamp() != b.depthBiasClamp())
        return true;

    return false;
#endif
}

void RenderBundleEncoder::setPipeline(const RenderPipeline& pipeline)
{
    if (!pipeline.renderPipelineState())
        return;

    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        id<MTLRenderPipelineState> previousRenderPipelineState = m_currentPipelineState;
        id<MTLDepthStencilState> previousDepthStencilState = m_depthStencilState;
        auto previous_cullMode = m_cullMode;
        auto previous_frontFace = m_frontFace;
        auto previous_depthClipmpde = m_depthClipMode;
        auto previous_depthBias = m_depthBias;
        auto previous_depthBiasSlopeScale = m_depthBiasSlopeScale;
        auto previous_depthBiasClamp = m_depthBiasClamp;

        m_currentPipelineState = pipeline.renderPipelineState();
        m_depthStencilState = pipeline.depthStencilState();
        m_cullMode = pipeline.cullMode();
        m_frontFace = pipeline.frontFace();
        m_depthClipMode = pipeline.depthClipMode();
        m_primitiveType = pipeline.primitiveType();
        m_depthBias = pipeline.depthBias();
        m_depthBiasSlopeScale = pipeline.depthBiasSlopeScale();
        m_depthBiasClamp = pipeline.depthBiasClamp();

        if (m_commandEncoder) {
            id<MTLRenderPipelineState> renderPipeline = m_currentPipelineState;
            if (renderPipeline && previousRenderPipelineState != m_currentPipelineState)
                [m_commandEncoder setRenderPipelineState:renderPipeline];
            id<MTLDepthStencilState> depthStencilState = m_depthStencilState;
            if (depthStencilState && previousDepthStencilState != depthStencilState)
                [m_commandEncoder setDepthStencilState:depthStencilState];

            if (previous_cullMode != m_cullMode)
                [m_commandEncoder setCullMode:m_cullMode];
            if (previous_frontFace != m_frontFace)
                [m_commandEncoder setFrontFacingWinding:m_frontFace];
            if (previous_depthClipmpde != m_depthClipMode)
                [m_commandEncoder setDepthClipMode:m_depthClipMode];
            if (previous_depthBias != m_depthBias || previous_depthBiasSlopeScale != m_depthBiasSlopeScale || previous_depthBiasClamp != m_depthBiasClamp)
                [m_commandEncoder setDepthBias:m_depthBias slopeScale:m_depthBiasSlopeScale clamp:m_depthBiasClamp];
        }
    } else {
        if (m_pipeline && icbNeedsToBeSplit(*m_pipeline, pipeline))
            endCurrentICB();

        m_recordedCommands.append([&pipeline, protectedThis = Ref { *this }] {
            protectedThis->setPipeline(pipeline);
            return false;
        });
    }

    m_pipeline = &pipeline;
}

void RenderBundleEncoder::setVertexBuffer(uint32_t slot, const Buffer& buffer, uint64_t offset, uint64_t size)
{
    UNUSED_PARAM(size);
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        addResource(m_resources, buffer.buffer(), MTLRenderStageVertex);
        m_vertexBuffers[slot] = { buffer.buffer(), offset };
    } else {
        m_recordedCommands.append([slot, &buffer, offset, size, protectedThis = Ref { *this }] {
            protectedThis->setVertexBuffer(slot, buffer, offset, size);
            return false;
        });
    }
}

void RenderBundleEncoder::setLabel(String&& label)
{
    m_indirectCommandBuffer.label = label;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuRenderBundleEncoderReference(WGPURenderBundleEncoder renderBundleEncoder)
{
    WebGPU::fromAPI(renderBundleEncoder).ref();
}

void wgpuRenderBundleEncoderRelease(WGPURenderBundleEncoder renderBundleEncoder)
{
    WebGPU::fromAPI(renderBundleEncoder).deref();
}

void wgpuRenderBundleEncoderDraw(WGPURenderBundleEncoder renderBundleEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderBundleEncoder).draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void wgpuRenderBundleEncoderDrawIndexed(WGPURenderBundleEncoder renderBundleEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderBundleEncoder).drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void wgpuRenderBundleEncoderDrawIndexedIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderBundleEncoder).drawIndexedIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderBundleEncoder).drawIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

WGPURenderBundle wgpuRenderBundleEncoderFinish(WGPURenderBundleEncoder renderBundleEncoder, const WGPURenderBundleDescriptor* descriptor)
{
    return WebGPU::releaseToAPI(WebGPU::fromAPI(renderBundleEncoder).finish(*descriptor));
}

void wgpuRenderBundleEncoderInsertDebugMarker(WGPURenderBundleEncoder renderBundleEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(renderBundleEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuRenderBundleEncoderPopDebugGroup(WGPURenderBundleEncoder renderBundleEncoder)
{
    WebGPU::fromAPI(renderBundleEncoder).popDebugGroup();
}

void wgpuRenderBundleEncoderPushDebugGroup(WGPURenderBundleEncoder renderBundleEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(renderBundleEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuRenderBundleEncoderSetBindGroup(WGPURenderBundleEncoder, uint32_t, WGPUBindGroup, size_t, const uint32_t*)
{
}

void wgpuRenderBundleEncoderSetBindGroupWithDynamicOffsets(WGPURenderBundleEncoder renderBundleEncoder, uint32_t groupIndex, WGPUBindGroup group, std::optional<Vector<uint32_t>>&& dynamicOffsets)
{
    WebGPU::fromAPI(renderBundleEncoder).setBindGroup(groupIndex, WebGPU::fromAPI(group), WTFMove(dynamicOffsets));
}

void wgpuRenderBundleEncoderSetIndexBuffer(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderBundleEncoder).setIndexBuffer(WebGPU::fromAPI(buffer), format, offset, size);
}

void wgpuRenderBundleEncoderSetPipeline(WGPURenderBundleEncoder renderBundleEncoder, WGPURenderPipeline pipeline)
{
    WebGPU::fromAPI(renderBundleEncoder).setPipeline(WebGPU::fromAPI(pipeline));
}

void wgpuRenderBundleEncoderSetVertexBuffer(WGPURenderBundleEncoder renderBundleEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderBundleEncoder).setVertexBuffer(slot, WebGPU::fromAPI(buffer), offset, size);
}

void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, const char* label)
{
    WebGPU::fromAPI(renderBundleEncoder).setLabel(WebGPU::fromAPI(label));
}
