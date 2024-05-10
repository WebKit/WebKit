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
#import "BindableResource.h"
#import "Buffer.h"
#import "Device.h"
#import "IsValidToUseWith.h"
#import "RenderBundle.h"
#import "RenderPipeline.h"

#define ENABLE_WEBGPU_ALWAYS_USE_ICB_REPLAY 0

@implementation RenderBundleICBWithResources {
    Vector<WebGPU::BindableResources> _resources;
}

- (instancetype)initWithICB:(id<MTLIndirectCommandBuffer>)icb pipelineState:(id<MTLRenderPipelineState>)pipelineState depthStencilState:(id<MTLDepthStencilState>)depthStencilState cullMode:(MTLCullMode)cullMode frontFace:(MTLWinding)frontFace depthClipMode:(MTLDepthClipMode)depthClipMode depthBias:(float)depthBias depthBiasSlopeScale:(float)depthBiasSlopeScale depthBiasClamp:(float)depthBiasClamp fragmentDynamicOffsetsBuffer:(id<MTLBuffer>)fragmentDynamicOffsetsBuffer pipeline:(const WebGPU::RenderPipeline*)pipeline
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
    _fragmentDynamicOffsetsBuffer = fragmentDynamicOffsetsBuffer;
    _pipeline = pipeline;

    return self;
}

- (Vector<WebGPU::BindableResources>*)resources
{
    return &_resources;
}

@end

namespace WebGPU {

bool RenderBundleEncoder::returnIfEncodingIsFinished(NSString* errorString)
{
    if (!validToEncodeCommand()) {
        m_device->generateAValidationError(errorString);
        return true;
    }

    return false;
}

#define RETURN_IF_FINISHED() \
if (returnIfEncodingIsFinished([NSString stringWithFormat:@"%s: failed as encoding has finished", __PRETTY_FUNCTION__]) || !m_icbDescriptor) \
    return;

static RenderBundleICBWithResources* makeRenderBundleICBWithResources(id<MTLIndirectCommandBuffer> icb, RenderBundle::ResourcesContainer* resources, id<MTLRenderPipelineState> renderPipelineState, id<MTLDepthStencilState> depthStencilState, MTLCullMode cullMode, MTLWinding frontFace, MTLDepthClipMode depthClipMode, float depthBias, float depthBiasSlopeScale, float depthBiasClamp, id<MTLBuffer> fragmentDynamicOffsetsBuffer, const RenderPipeline* pipeline)
{
    constexpr auto maxResourceUsageValue = MTLResourceUsageRead | MTLResourceUsageWrite;
    constexpr auto maxStageValue = (MTLRenderStageVertex | MTLRenderStageFragment) + 1;
    static_assert(maxResourceUsageValue == 3 && maxStageValue == 4, "Code path assumes MTLResourceUsageRead | MTLResourceUsageWrite == 3 and MTLRenderStageVertex | MTLRenderStageFragment == 3");
    Vector<id<MTLResource>> stageResources[maxStageValue][maxResourceUsageValue];
    Vector<BindGroupEntryUsageData> stageResourceUsages[maxStageValue][maxResourceUsageValue];

    for (id<MTLResource> r : resources) {
        if (!r)
            continue;

        ResourceUsageAndRenderStage *usageAndStage = [resources objectForKey:r];
        if (!usageAndStage.renderStages || !usageAndStage.usage)
            continue;
        stageResources[usageAndStage.renderStages - 1][usageAndStage.usage - 1].append(r);
        stageResourceUsages[usageAndStage.renderStages - 1][usageAndStage.usage - 1].append(BindGroupEntryUsageData { .usage = usageAndStage.entryUsage, .binding = usageAndStage.binding, .resource = usageAndStage.resource });
    }

    RenderBundleICBWithResources* renderBundle = [[RenderBundleICBWithResources alloc] initWithICB:icb pipelineState:renderPipelineState depthStencilState:depthStencilState cullMode:cullMode frontFace:frontFace depthClipMode:depthClipMode depthBias:depthBias depthBiasSlopeScale:depthBiasSlopeScale depthBiasClamp:depthBiasClamp fragmentDynamicOffsetsBuffer:fragmentDynamicOffsetsBuffer pipeline:pipeline];

    for (size_t stage = 0; stage < maxStageValue; ++stage) {
        for (size_t i = 0; i < maxResourceUsageValue; ++i) {
            auto &v = stageResources[stage][i];
            auto &u = stageResourceUsages[stage][i];
            if (v.size()) {
                renderBundle.resources->append(BindableResources {
                    .mtlResources = WTFMove(v),
                    .resourceUsages = WTFMove(u),
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
    if (descriptor.nextInChain || !isValid())
        return RenderBundleEncoder::createInvalid(*this, @"createRenderBundleEncoder: invalid device");

    MTLIndirectCommandBufferDescriptor *icbDescriptor = [MTLIndirectCommandBufferDescriptor new];
    icbDescriptor.inheritBuffers = NO;
    icbDescriptor.inheritPipelineState = NO;
    icbDescriptor.maxFragmentBufferBindCount = 1;

    uint32_t bytesPerSample = 0;
    const auto maxColorAttachmentBytesPerSample = limits().maxColorAttachmentBytesPerSample;

    if (descriptor.colorFormatCount > limits().maxColorAttachments) {
        NSString* error = [NSString stringWithFormat:@"descriptor.colorFormatCount(%zu) > limits().maxColorAttachments(%u)", descriptor.colorFormatCount, limits().maxColorAttachments];
        generateAValidationError(error);
        return RenderBundleEncoder::createInvalid(*this, error);
    }
    for (size_t i = 0, colorFormatCount = descriptor.colorFormatCount; i < colorFormatCount; ++i) {
        auto textureFormat = descriptor.colorFormats[i];
        if (textureFormat == WGPUTextureFormat_Undefined)
            continue;
        if (!Texture::isColorRenderableFormat(textureFormat, *this)) {
            NSString* error = [NSString stringWithFormat:@"createRenderBundleEncoder - colorAttachment[%zu] with format %d is not renderable", i, textureFormat];
            generateAValidationError(error);
            return RenderBundleEncoder::createInvalid(*this, error);
        }
        bytesPerSample = roundUpToMultipleOfNonPowerOfTwo(Texture::renderTargetPixelByteAlignment(textureFormat), bytesPerSample);
        bytesPerSample += Texture::renderTargetPixelByteCost(textureFormat);
        if (bytesPerSample > maxColorAttachmentBytesPerSample) {
            NSString* error = @"createRenderBundleEncoder - bytesPerSample > maxColorAttachmentBytesPerSample";
            generateAValidationError(error);
            return RenderBundleEncoder::createInvalid(*this, error);
        }
    }

    if (descriptor.depthStencilFormat != WGPUTextureFormat_Undefined) {
        if (!Texture::isDepthOrStencilFormat(descriptor.depthStencilFormat)) {
            NSString* error = [NSString stringWithFormat:@"createRenderBundleEncoder - provided depthStencilFormat %d is not a depth or stencil format", descriptor.depthStencilFormat];
            generateAValidationError(error);
            return RenderBundleEncoder::createInvalid(*this, error);
        }
    } else if (!descriptor.colorFormatCount) {
        NSString* error = @"createRenderBundleEncoder - zero color and depth-stencil formats provided";
        generateAValidationError(error);
        return RenderBundleEncoder::createInvalid(*this, error);
    }

    return RenderBundleEncoder::create(icbDescriptor, descriptor, *this);
}

RenderBundleEncoder::RenderBundleEncoder(MTLIndirectCommandBufferDescriptor *indirectCommandBufferDescriptor, const WGPURenderBundleEncoderDescriptor& descriptor, Device& device)
    : m_device(device)
    , m_icbDescriptor(indirectCommandBufferDescriptor)
    , m_resources([NSMapTable strongToStrongObjectsMapTable])
    , m_descriptor(descriptor)
    , m_descriptorColorFormats(descriptor.colorFormats ? Vector<WGPUTextureFormat>(std::span { descriptor.colorFormats, descriptor.colorFormatCount }) : Vector<WGPUTextureFormat>())
{
    if (m_descriptorColorFormats.size())
        m_descriptor.colorFormats = &m_descriptorColorFormats[0];
    m_icbArray = [NSMutableArray array];
    m_bindGroupDynamicOffsets = BindGroupDynamicOffsetsContainer();
#if ENABLE(WEBGPU_ALWAYS_USE_ICB_REPLAY)
    m_requiresCommandReplay = true;
#endif
}

RenderBundleEncoder::RenderBundleEncoder(Device& device, NSString* errorString)
    : m_device(device)
    , m_lastErrorString(errorString)
{
}

void RenderBundleEncoder::makeInvalid(NSString* errorString)
{
    m_indirectCommandBuffer = nil;
    m_icbDescriptor = nil;
    m_resources = nil;
    m_lastErrorString = errorString;
    m_icbArray = nil;
    if (m_renderPassEncoder)
        m_renderPassEncoder->makeInvalid(errorString);
}

RenderBundleEncoder::~RenderBundleEncoder() = default;

id<MTLIndirectRenderCommand> RenderBundleEncoder::currentRenderCommand()
{
    if (m_renderPassEncoder)
        return (id<MTLIndirectRenderCommand>)m_renderPassEncoder->renderCommandEncoder();

    if (m_currentCommand)
        return m_currentCommand;

    ASSERT(!m_indirectCommandBuffer || m_currentCommandIndex < m_indirectCommandBuffer.size);
    m_currentCommand = m_currentCommandIndex < m_indirectCommandBuffer.size ? [m_indirectCommandBuffer indirectRenderCommandAtIndex:m_currentCommandIndex] : nil;
    return m_currentCommand;
}

void RenderBundleEncoder::addResource(RenderBundle::ResourcesContainer* resources, id<MTLResource> mtlResource, ResourceUsageAndRenderStage *resource)
{
    if (m_renderPassEncoder) {
        if (resource.renderStages && mtlResource)
            [m_renderPassEncoder->renderCommandEncoder() useResource:mtlResource usage:resource.usage stages:resource.renderStages];
        ASSERT(resource.entryUsage.hasExactlyOneBitSet());
        m_renderPassEncoder->addResourceToActiveResources(resource.resource, mtlResource, resource.entryUsage);
        m_renderPassEncoder->setCommandEncoder(resource.resource);
        return;
    }

    if (ResourceUsageAndRenderStage *existingResource = [resources objectForKey:mtlResource]) {
        existingResource.usage |= resource.usage;
        existingResource.renderStages |= resource.renderStages;
        existingResource.entryUsage |= resource.entryUsage;
        existingResource.binding = resource.binding;
    } else
        [resources setObject:resource forKey:mtlResource];
}

void RenderBundleEncoder::addResource(RenderBundle::ResourcesContainer* container, id<MTLResource> mtlResource, MTLRenderStages stage)
{
    RefPtr<const Buffer> placeholderBuffer;
    addResource(container, mtlResource, stage, placeholderBuffer);
}

void RenderBundleEncoder::addResource(RenderBundle::ResourcesContainer* resources, id<MTLResource> mtlResource, MTLRenderStages stage, const BindGroupEntryUsageData::Resource& resource)
{
    if (m_renderPassEncoder && mtlResource) {
        [m_renderPassEncoder->renderCommandEncoder() useResource:mtlResource usage:MTLResourceUsageRead stages:stage];
        return;
    }

    return addResource(resources, mtlResource, [[ResourceUsageAndRenderStage alloc] initWithUsage:MTLResourceUsageRead renderStages:stage entryUsage:BindGroupEntryUsage::Input binding:BindGroupEntryUsageData::invalidBindingIndex resource:resource]);
}

bool RenderBundleEncoder::executePreDrawCommands()
{
    auto vertexDynamicOffset = m_vertexDynamicOffset;
    auto fragmentDynamicOffset = m_fragmentDynamicOffset;
    if (!m_pipeline) {
        makeInvalid(@"Pipeline was not set prior to draw command");
        return false;
    }

    m_vertexDynamicOffset += sizeof(uint32_t) * m_pipeline->pipelineLayout().sizeOfVertexDynamicOffsets();
    m_fragmentDynamicOffset += sizeof(uint32_t) * m_pipeline->pipelineLayout().sizeOfFragmentDynamicOffsets();

    id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand();
    if (!icbCommand)
        return true;

    if (NSString* error = m_pipeline->pipelineLayout().errorValidatingBindGroupCompatibility(m_bindGroups, m_pipeline->vertexStageInBufferCount())) {
        makeInvalid(error);
        return false;
    }

    if (m_currentPipelineState)
        [icbCommand setRenderPipelineState:m_currentPipelineState];

    if (NSString* error = errorValidatingDraw()) {
        makeInvalid(error);
        return false;
    }

    for (size_t i = 0, sz = m_vertexBuffers.size(); i < sz; ++i) {
        if (m_vertexBuffers[i].buffer) {
            if (m_vertexBuffers[i].offset < m_vertexBuffers[i].buffer.length)
                [icbCommand setVertexBuffer:m_vertexBuffers[i].buffer offset:m_vertexBuffers[i].offset atIndex:i];
            else if (m_vertexBuffers[i].size) {
                makeInvalid(@"attempting to set non-zero sized buffer");
                return false;
            } else
                [icbCommand setVertexBuffer:m_device->placeholderBuffer() offset:0 atIndex:i];
        }
    }

    for (size_t i = 0, sz = m_fragmentBuffers.size(); i < sz; ++i) {
        if (m_fragmentBuffers[i].buffer)
            [icbCommand setFragmentBuffer:m_fragmentBuffers[i].buffer offset:m_fragmentBuffers[i].offset atIndex:i];
    }

    if (m_bindGroupDynamicOffsets) {
        for (auto& kvp : *m_bindGroupDynamicOffsets) {
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
                auto maxBufferLength = m_dynamicOffsetsFragmentBuffer.length;
                auto bufferOffset = fragmentDynamicOffset;
                uint8_t* fragmentBufferContents = static_cast<uint8_t*>(m_dynamicOffsetsFragmentBuffer.contents) + bufferOffset + RenderBundleEncoder::startIndexForFragmentDynamicOffsets * sizeof(float);
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
    }

    if (m_dynamicOffsetsVertexBuffer)
        [icbCommand setVertexBuffer:m_dynamicOffsetsVertexBuffer offset:vertexDynamicOffset atIndex:m_device->maxBuffersPlusVertexBuffersForVertexStage()];

    if (m_dynamicOffsetsFragmentBuffer) {
        RELEASE_ASSERT(m_fragmentBuffers.size());
        [icbCommand setFragmentBuffer:m_dynamicOffsetsFragmentBuffer offset:fragmentDynamicOffset atIndex:m_fragmentBuffers.size() - 1];
    }

    if (m_bindGroupDynamicOffsets)
        m_bindGroupDynamicOffsets->clear();

    return true;
}

void RenderBundleEncoder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    RETURN_IF_FINISHED();
    if (!executePreDrawCommands())
        return;
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        if (!runVertexBufferValidation(vertexCount, instanceCount, firstVertex, firstInstance))
            return;
        if (!vertexCount || !instanceCount)
            return;

        [icbCommand drawPrimitives:m_primitiveType vertexStart:firstVertex vertexCount:vertexCount instanceCount:instanceCount baseInstance:firstInstance];
    } else {
        recordCommand([vertexCount, instanceCount, firstVertex, firstInstance, protectedThis = Ref { *this }] {
            protectedThis->draw(vertexCount, instanceCount, firstVertex, firstInstance);
            return true;
        });
    }

    finalizeRenderCommand(MTLIndirectCommandTypeDraw);
}

uint32_t RenderBundleEncoder::maxVertexBufferIndex() const
{
    return m_maxVertexBufferSlot;
}

uint32_t RenderBundleEncoder::maxBindGroupIndex() const
{
    return m_maxBindGroupSlot;
}

NSString* RenderBundleEncoder::errorValidatingDraw() const
{
    if (!m_pipeline)
        return @"pipeline is not set";

    auto& requiredBufferIndices = m_pipeline->requiredBufferIndices();
    for (auto& [bufferIndex, _] : requiredBufferIndices) {
        if (!m_utilizedBufferIndices.contains(bufferIndex))
            return [NSString stringWithFormat:@"Buffer index[%u] is missing", bufferIndex];
    }

    auto bindGroupSpaceUsed = maxBindGroupIndex() + 1;
    auto vertexBufferSpaceUsed = maxVertexBufferIndex() + 1;
    if (bindGroupSpaceUsed + vertexBufferSpaceUsed > m_device->limits().maxBindGroupsPlusVertexBuffers)
        return @"Too many bind groups and vertex buffers used";

    return nil;
}

NSString* RenderBundleEncoder::errorValidatingDrawIndexed() const
{
    if (!m_pipeline->requiredBufferIndices().size())
        return nil;

    if (!m_indexBuffer)
        return @"Index buffer is not set";

    auto topology = m_pipeline->primitiveTopology();
    if (topology == WGPUPrimitiveTopology_LineStrip || topology == WGPUPrimitiveTopology_TriangleStrip) {
        if (m_indexType != m_pipeline->stripIndexFormat())
            return @"Primitive topology mismiatch with render pipeline";
    }

    return nil;
}

void RenderBundleEncoder::finalizeRenderCommand(MTLIndirectCommandType commandTypes)
{
    m_currentCommand = nil;
    ++m_currentCommandIndex;
    m_icbDescriptor.commandTypes |= commandTypes;
}


bool RenderBundleEncoder::runIndexBufferValidation(uint32_t firstInstance, uint32_t instanceCount)
{
    if (!m_pipeline || !m_indexBuffer) {
        makeInvalid(@"Missing pipeline before draw command");
        return false;
    }

    auto strideCount = firstInstance + instanceCount;
    if (!strideCount)
        return true;

    auto& requiredBufferIndices = m_pipeline->requiredBufferIndices();
    bool abortDraw = false;
    for (auto& [bufferIndex, bufferData] : requiredBufferIndices) {
        RELEASE_ASSERT(bufferIndex < m_vertexBuffers.size());
        auto& vertexBuffer = m_vertexBuffers[bufferIndex];
        auto bufferSize = vertexBuffer.size;
        auto stride = bufferData.stride;
        auto lastStride = bufferData.lastStride;
        if (bufferData.stepMode == WGPUVertexStepMode_Instance) {
            if ((strideCount - 1) * stride + lastStride > bufferSize) {
                makeInvalid([NSString stringWithFormat:@"(RenderBundle) Buffer[%d] fails: (strideCount(%d) - 1) * stride(%llu) + lastStride(%llu) > bufferSize(%llu), metalBufferLength(%lu)", bufferIndex, strideCount, stride, lastStride, bufferSize, static_cast<unsigned long>(vertexBuffer.buffer.length)]);
                return false;
            }

            if (stride && m_indexBuffer->maxIndex(m_indexType) >= (bufferSize / stride))
                abortDraw = true;
        }
    }

    return !abortDraw;
}

bool RenderBundleEncoder::runVertexBufferValidation(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    if (!m_pipeline) {
        makeInvalid(@"Missing pipeline before draw command");
        return false;
    }

    auto& requiredBufferIndices = m_pipeline->requiredBufferIndices();
    for (auto& [bufferIndex, bufferData] : requiredBufferIndices) {
        uint64_t strideCount = 0;
        switch (bufferData.stepMode) {
        case WGPUVertexStepMode_Vertex:
            strideCount = firstVertex + vertexCount;
            break;
        case WGPUVertexStepMode_Instance:
            strideCount = firstInstance + instanceCount;
            break;
        default:
            break;
        }
        if (!strideCount)
            continue;

        if (bufferIndex >= m_vertexBuffers.size()) {
            makeInvalid([NSString stringWithFormat:@"vertex buffer validation failed as vertex buffer %d was not set", bufferIndex]);
            return false;
        }
        auto& vertexBuffer = m_vertexBuffers[bufferIndex];
        auto bufferSize = vertexBuffer.size;
        if ((strideCount - 1) * bufferData.stride + bufferData.lastStride > bufferSize) {
            makeInvalid([NSString stringWithFormat:@"Buffer[%d] fails: (strideCount(%llu) - 1) * bufferData.stride(%llu) + bufferData.lastStride(%llu) > bufferSize(%llu)", bufferIndex, strideCount, bufferData.stride,  bufferData.lastStride, bufferSize]);
            return false;
        }
    }

    return true;
}

void RenderBundleEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    RETURN_IF_FINISHED();
    if (!executePreDrawCommands())
        return;

    auto indexSizeInBytes = (m_indexType == MTLIndexTypeUInt16 ? sizeof(uint16_t) : sizeof(uint32_t));
    auto firstIndexOffsetInBytes = firstIndex * indexSizeInBytes;
    id<MTLBuffer> indexBuffer = m_indexBuffer ? m_indexBuffer->buffer() : nil;
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        if (NSString* error = errorValidatingDrawIndexed()) {
            makeInvalid(error);
            return;
        }
        if (firstIndexOffsetInBytes + indexCount * indexSizeInBytes > m_indexBufferSize) {
            makeInvalid(@"firstIndexOffsetInBytes + indexCount * indexSizeInBytes > m_indexBufferSize");
            return;
        }

        if (!runIndexBufferValidation(firstInstance, instanceCount))
            return;

        if (!indexCount || !instanceCount)
            return;

        [icbCommand drawIndexedPrimitives:m_primitiveType indexCount:indexCount indexType:m_indexType indexBuffer:indexBuffer indexBufferOffset:(m_indexBufferOffset + firstIndexOffsetInBytes) instanceCount:instanceCount baseVertex:baseVertex baseInstance:firstInstance];
    } else {
        recordCommand([indexCount, instanceCount, firstIndex, baseVertex, firstInstance, protectedThis = Ref { *this }] {
            protectedThis->drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
            return true;
        });
    }

    finalizeRenderCommand(MTLIndirectCommandTypeDrawIndexed);
}

void RenderBundleEncoder::drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    RETURN_IF_FINISHED();

    m_requiresCommandReplay = true;

    if (!executePreDrawCommands())
        return;
    id<MTLBuffer> indexBuffer = m_indexBuffer ? m_indexBuffer->buffer() : nil;
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        if (!indexBuffer.length)
            return;

        if (m_renderPassEncoder) {
            indirectBuffer.setCommandEncoder(m_renderPassEncoder->parentEncoder());
            if (!indirectBuffer.isDestroyed() && indexBuffer.length)
                [m_renderPassEncoder->renderCommandEncoder() drawIndexedPrimitives:m_primitiveType indexType:m_indexType indexBuffer:indexBuffer indexBufferOffset:m_indexBufferOffset indirectBuffer:indirectBuffer.buffer() indirectBufferOffset:indirectOffset];
        } else {
            auto contents = (MTLDrawIndexedPrimitivesIndirectArguments*)indirectBuffer.buffer().contents;
            if (!contents || !contents->indexCount || !contents->instanceCount)
                return;

            ASSERT(m_indexBufferOffset == contents->indexStart);
            addResource(m_resources, indirectBuffer.buffer(), MTLRenderStageVertex, &indirectBuffer);
            [icbCommand drawIndexedPrimitives:m_primitiveType indexCount:contents->indexCount indexType:m_indexType indexBuffer:indexBuffer indexBufferOffset:m_indexBufferOffset instanceCount:contents->instanceCount baseVertex:contents->baseVertex baseInstance:contents->baseInstance];
        }
    } else {
        if (!isValidToUseWith(indirectBuffer, *this)) {
            makeInvalid(@"drawIndexedIndirect: buffer was invalid");
            return;
        }

        if (!(indirectBuffer.usage() & WGPUBufferUsage_Indirect) || (indirectOffset % 4)) {
            makeInvalid(@"drawIndexedIndirect: validation failed");
            return;
        }

        if (indirectBuffer.initialSize() < indirectOffset + sizeof(MTLDrawIndexedPrimitivesIndirectArguments)) {
            makeInvalid(@"drawIndexedIndirect: validation failed");
            return;
        }

        recordCommand([indirectBuffer = Ref { indirectBuffer }, indirectOffset, protectedThis = Ref { *this }] {
            protectedThis->drawIndexedIndirect(indirectBuffer.get(), indirectOffset);
            return true;
        });
    }

    finalizeRenderCommand(MTLIndirectCommandTypeDrawIndexed);
}

void RenderBundleEncoder::drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    RETURN_IF_FINISHED();

    m_requiresCommandReplay = true;
    if (!executePreDrawCommands())
        return;
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        if (m_renderPassEncoder) {
            indirectBuffer.setCommandEncoder(m_renderPassEncoder->parentEncoder());
            if (!indirectBuffer.isDestroyed())
                [m_renderPassEncoder->renderCommandEncoder() drawPrimitives:m_primitiveType indirectBuffer:indirectBuffer.buffer() indirectBufferOffset:indirectOffset];
        } else {
            auto contents = (MTLDrawPrimitivesIndirectArguments*)indirectBuffer.buffer().contents;
            if (!contents || !contents->instanceCount || !contents->vertexCount)
                return;

            addResource(m_resources, indirectBuffer.buffer(), MTLRenderStageVertex, &indirectBuffer);
            [icbCommand drawPrimitives:m_primitiveType vertexStart:contents->vertexStart vertexCount:contents->vertexCount instanceCount:contents->instanceCount baseInstance:contents->baseInstance];
        }
    } else {
        if (!isValidToUseWith(indirectBuffer, *this)) {
            makeInvalid(@"drawIndirect: buffer was invalid");
            return;
        }

        if (!(indirectBuffer.usage() & WGPUBufferUsage_Indirect) || (indirectOffset % 4)) {
            makeInvalid(@"drawIndirect: validation failed");
            return;
        }

        if (indirectBuffer.initialSize() < indirectOffset + sizeof(MTLDrawPrimitivesIndirectArguments)) {
            makeInvalid(@"drawIndirect: validation failed");
            return;
        }

        recordCommand([indirectBuffer = Ref { indirectBuffer }, indirectOffset, protectedThis = Ref { *this }] {
            protectedThis->drawIndirect(indirectBuffer.get(), indirectOffset);
            return true;
        });
    }

    finalizeRenderCommand(MTLIndirectCommandTypeDraw);
}

void RenderBundleEncoder::endCurrentICB()
{
    auto commandCount = m_currentCommandIndex;
    m_currentCommandIndex = 0;
    RELEASE_ASSERT(!commandCount || !!m_icbDescriptor.commandTypes);

    m_icbDescriptor.maxVertexBufferBindCount = m_device->maxBuffersPlusVertexBuffersForVertexStage() + 1;
    m_vertexBuffers.grow(m_icbDescriptor.maxVertexBufferBindCount);
    if (m_fragmentBuffers.size() < m_icbDescriptor.maxFragmentBufferBindCount)
        m_fragmentBuffers.grow(m_icbDescriptor.maxFragmentBufferBindCount);
    if (m_vertexDynamicOffset && !m_dynamicOffsetsVertexBuffer) {
        m_dynamicOffsetsVertexBuffer = [m_device->device() newBufferWithLength:m_vertexDynamicOffset options:MTLResourceStorageModeShared];
        addResource(m_resources, m_dynamicOffsetsVertexBuffer, MTLRenderStageVertex);
    }
    m_vertexDynamicOffset = 0;

    if (!m_dynamicOffsetsFragmentBuffer) {
        m_dynamicOffsetsFragmentBuffer = [m_device->device() newBufferWithLength:m_fragmentDynamicOffset + RenderBundleEncoder::startIndexForFragmentDynamicOffsets * sizeof(float) options:MTLResourceStorageModeShared];
        auto* fragmentBufferPtr = m_dynamicOffsetsFragmentBuffer.contents;
        RELEASE_ASSERT(fragmentBufferPtr);
        static_assert(sizeof(float) == sizeof(uint32_t));
        static_cast<float*>(fragmentBufferPtr)[0] = 0.f;
        static_cast<float*>(fragmentBufferPtr)[1] = 1.f;
        static_cast<uint32_t*>(fragmentBufferPtr)[2] = m_sampleMask;
        addResource(m_resources, m_dynamicOffsetsFragmentBuffer, MTLRenderStageFragment);
    }
    m_fragmentDynamicOffset = 0;

    if (!m_renderPassEncoder)
        m_indirectCommandBuffer = [m_device->device() newIndirectCommandBufferWithDescriptor:m_icbDescriptor maxCommandCount:commandCount options:0];

    uint64_t completedDraws = 0, lastIndexOfRecordedCommand = 0;
    for (auto& command : m_recordedCommands) {
        completedDraws += command() ? 1 : 0;
        ++lastIndexOfRecordedCommand;

        if (completedDraws == commandCount)
            break;
    }

    if (m_renderPassEncoder)
        return;

    if (lastIndexOfRecordedCommand == m_recordedCommands.size()) {
        m_recordedCommands.clear();
        m_icbDescriptor.commandTypes = 0;
    } else
        m_recordedCommands.remove(0, lastIndexOfRecordedCommand);

    m_currentCommandIndex = commandCount - completedDraws;
    [m_icbArray addObject:makeRenderBundleICBWithResources(m_indirectCommandBuffer, m_resources, m_currentPipelineState, m_depthStencilState, m_cullMode, m_frontFace, m_depthClipMode, m_depthBias, m_depthBiasSlopeScale, m_depthBiasClamp, m_dynamicOffsetsFragmentBuffer, m_pipeline.get())];
    m_indirectCommandBuffer = nil;
    m_currentCommand = nil;
    m_currentPipelineState = nil;
    m_dynamicOffsetsFragmentBuffer = nil;
    m_dynamicOffsetsVertexBuffer = nil;
    m_resources = [NSMapTable strongToStrongObjectsMapTable];
}

bool RenderBundleEncoder::validToEncodeCommand() const
{
    return !m_finished || m_renderPassEncoder;
}

Ref<RenderBundle> RenderBundleEncoder::finish(const WGPURenderBundleDescriptor& descriptor)
{
    if (!m_icbDescriptor || m_debugGroupStackSize) {
        m_device->generateAValidationError(m_lastErrorString);
        return RenderBundle::createInvalid(m_device, m_lastErrorString);
    }

    m_requiresCommandReplay = m_requiresCommandReplay ?: (m_requiresMetalWorkaround || !m_currentCommandIndex);

    auto createRenderBundle = ^{
        if (m_requiresCommandReplay)
            return RenderBundle::create(m_icbArray, this, m_descriptor, m_currentCommandIndex, m_device);

        endCurrentICB();
        m_vertexBuffers.clear();
        m_fragmentBuffers.clear();
        m_icbDescriptor.maxVertexBufferBindCount = 0;
        m_icbDescriptor.maxFragmentBufferBindCount = 0;

        RELEASE_ASSERT(m_icbArray || m_lastErrorString);
        if (m_icbArray)
            return RenderBundle::create(m_icbArray, nullptr, m_descriptor, m_currentCommandIndex, m_device);

        m_device->generateAValidationError(m_lastErrorString);
        return RenderBundle::createInvalid(m_device, m_lastErrorString);
    };

    auto renderBundle = createRenderBundle();
    renderBundle->setLabel(String::fromUTF8(descriptor.label));
    m_finished = true;

    return renderBundle;
}

void RenderBundleEncoder::replayCommands(RenderPassEncoder& renderPassEncoder)
{
    if (!renderPassEncoder.renderCommandEncoder())
        return;

    m_renderPassEncoder = &renderPassEncoder;
    endCurrentICB();
    m_renderPassEncoder = nullptr;
    m_currentPipelineState = nil;
    m_depthStencilState = nil;
    m_bindGroupDynamicOffsets = std::nullopt;
}

void RenderBundleEncoder::insertDebugMarker(String&&)
{
    RETURN_IF_FINISHED();
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
    RETURN_IF_FINISHED();
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid(@"validatePopDebugGroup failed");
        return;
    }

    --m_debugGroupStackSize;
    // MTLIndirectCommandBuffers don't support debug commands.
}

void RenderBundleEncoder::pushDebugGroup(String&&)
{
    RETURN_IF_FINISHED();
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    // MTLIndirectCommandBuffers don't support debug commands.
}

void RenderBundleEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, std::optional<Vector<uint32_t>>&& dynamicOffsets)
{
    RETURN_IF_FINISHED();
    if (!isValidToUseWith(group, *this)) {
        makeInvalid(@"setBindGroup: invalid bind group");
        return;
    }

    id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand();
    m_maxBindGroupSlot = std::max(groupIndex, m_maxBindGroupSlot);
    if (!icbCommand) {
        if (!isValidToUseWith(group, *this)) {
            makeInvalid(@"setBindGroup: invalid bind group passed");
            return;
        }

        if (groupIndex >= m_device->limits().maxBindGroups) {
            makeInvalid(@"setBindGroup: groupIndex >= limits.maxBindGroups");
            return;
        }

        if (dynamicOffsets) {
            auto* bindGroupLayout = group.bindGroupLayout();
            if (!bindGroupLayout) {
                makeInvalid(@"GPURenderBundleEncoder.setBindGroup: bind group is nil");
                return;
            }
            if (NSString* error = bindGroupLayout->errorValidatingDynamicOffsets(dynamicOffsets ? dynamicOffsets->data() : nullptr, dynamicOffsets ? dynamicOffsets->size() : 0, group)) {
                makeInvalid([NSString stringWithFormat:@"GPURenderBundleEncoder.setBindGroup: %@", error]);
                return;
            }
        }

        if (group.fragmentArgumentBuffer())
            m_icbDescriptor.maxFragmentBufferBindCount = std::max<NSUInteger>(m_icbDescriptor.maxFragmentBufferBindCount, 2 + groupIndex);

        if (group.vertexArgumentBuffer())
            m_requiresMetalWorkaround = false;

        recordCommand([groupIndex, group = Ref { group }, protectedThis = Ref { *this }, dynamicOffsets = WTFMove(dynamicOffsets)]() mutable {
            protectedThis->setBindGroup(groupIndex, group.get(), WTFMove(dynamicOffsets));
            return false;
        });
        return;
    }

    uint32_t dynamicOffsetCount = dynamicOffsets ? dynamicOffsets->size() : 0;
    if (dynamicOffsetCount && m_bindGroupDynamicOffsets)
        m_bindGroupDynamicOffsets->set(groupIndex, WTFMove(*dynamicOffsets));

    for (const auto& resource : group.resources()) {
        ASSERT(resource.mtlResources.size() == resource.resourceUsages.size());
        for (size_t i = 0, resourceCount = resource.resourceUsages.size(); i < resourceCount; ++i) {
            auto& resourceUsage = resource.resourceUsages[i];
            ResourceUsageAndRenderStage* usageAndRenderStage = [[ResourceUsageAndRenderStage alloc] initWithUsage:resource.usage renderStages:resource.renderStages entryUsage:resourceUsage.usage binding:resourceUsage.binding resource:resourceUsage.resource];
            addResource(m_resources, resource.mtlResources[i], usageAndRenderStage);
        }
        if (m_renderPassEncoder) {
            for (size_t i = 0, resourceCount = resource.resourceUsages.size(); i < resourceCount; ++i) {
                auto& resourceUsage = resource.resourceUsages[i];
                m_renderPassEncoder->setCommandEncoder(resourceUsage.resource);
            }
        }
    }

    m_bindGroups.set(groupIndex, group);
    if (group.vertexArgumentBuffer()) {
        addResource(m_resources, group.vertexArgumentBuffer(), MTLRenderStageVertex);
        m_vertexBuffers[m_device->vertexBufferIndexForBindGroup(groupIndex)] = { .buffer = group.vertexArgumentBuffer(), .offset = 0, .dynamicOffsetCount = dynamicOffsetCount, .dynamicOffsets = dynamicOffsets->data(), .size = group.vertexArgumentBuffer().length };
    }
    if (group.fragmentArgumentBuffer()) {
        addResource(m_resources, group.fragmentArgumentBuffer(), MTLRenderStageFragment);
        m_fragmentBuffers[groupIndex] = { .buffer = group.fragmentArgumentBuffer(), .offset = 0, .dynamicOffsetCount = dynamicOffsetCount, .dynamicOffsets = dynamicOffsets->data(), .size = group.fragmentArgumentBuffer().length };
    }
}

void RenderBundleEncoder::setIndexBuffer(const Buffer& buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    RETURN_IF_FINISHED();
    m_indexBuffer = buffer;
    RELEASE_ASSERT(m_indexBuffer);
    m_indexType = format == WGPUIndexFormat_Uint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    m_indexBufferOffset = offset;
    m_indexBufferSize = size;
    if (m_renderPassEncoder)
        buffer.setCommandEncoder(m_renderPassEncoder->parentEncoder());
    id<MTLBuffer> indexBuffer = m_indexBuffer->buffer();

    if (!currentRenderCommand()) {
        if (computedSizeOverflows(buffer, offset, size))  {
            makeInvalid(@"setIndexBuffer: computed size overflows");
            return;
        }

        auto indexSizeInBytes = (format == WGPUIndexFormat_Uint16 ? sizeof(uint16_t) : sizeof(uint32_t));
        if (!(buffer.usage() & WGPUBufferUsage_Index) || (offset % indexSizeInBytes)) {
            makeInvalid(@"setIndexBuffer: validation failed");
            return;
        }

        if (offset + size > buffer.initialSize()) {
            makeInvalid(@"setIndexBuffer: offset + size > buffer.size()");
            return;
        }

        if (!isValidToUseWith(buffer, *this)) {
            makeInvalid(@"setIndexBuffer: buffer is not valid");
            return;
        }

        recordCommand([buffer = Ref { buffer }, format, offset, size, protectedThis = Ref { *this }] {
            protectedThis->setIndexBuffer(buffer.get(), format, offset, size);
            return false;
        });
        return;
    }

    addResource(m_resources, indexBuffer, MTLRenderStageVertex, &buffer);
}

bool RenderBundleEncoder::icbNeedsToBeSplit(const RenderPipeline& a, const RenderPipeline& b)
{
    if (m_requiresCommandReplay || m_renderPassEncoder)
        return false;

    if (&a == &b)
        return false;

    if (a.sampleMask() != b.sampleMask())
        return true;

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
}

void RenderBundleEncoder::recordCommand(WTF::Function<bool(void)>&& function)
{
    ASSERT(!m_renderPassEncoder || m_renderPassEncoder->renderCommandEncoder());
    m_recordedCommands.append(WTFMove(function));
}

void RenderBundleEncoder::setPipeline(const RenderPipeline& pipeline)
{
    RETURN_IF_FINISHED();
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
        m_sampleMask = pipeline.sampleMask();

        if (m_renderPassEncoder) {
            if (NSString* error = m_renderPassEncoder->errorValidatingPipeline(pipeline)) {
                makeInvalid(error);
                return;
            }

            id<MTLRenderCommandEncoder> commandEncoder = m_renderPassEncoder->renderCommandEncoder();
            id<MTLRenderPipelineState> renderPipeline = m_currentPipelineState;
            if (renderPipeline && previousRenderPipelineState != m_currentPipelineState)
                [commandEncoder setRenderPipelineState:renderPipeline];
            id<MTLDepthStencilState> depthStencilState = m_depthStencilState;
            if (depthStencilState && previousDepthStencilState != depthStencilState)
                [commandEncoder setDepthStencilState:depthStencilState];

            if (previous_cullMode != m_cullMode)
                [commandEncoder setCullMode:m_cullMode];
            if (previous_frontFace != m_frontFace)
                [commandEncoder setFrontFacingWinding:m_frontFace];
            if (previous_depthClipmpde != m_depthClipMode)
                [commandEncoder setDepthClipMode:m_depthClipMode];
            if (previous_depthBias != m_depthBias || previous_depthBiasSlopeScale != m_depthBiasSlopeScale || previous_depthBiasClamp != m_depthBiasClamp)
                [commandEncoder setDepthBias:m_depthBias slopeScale:m_depthBiasSlopeScale clamp:m_depthBiasClamp];
        }
    } else {
        if (!isValidToUseWith(pipeline, *this)) {
            makeInvalid(@"setPipeline: invalid pipeline passed");
            return;
        }

        if (!pipeline.renderPipelineState())
            return;

        if (!pipeline.validateRenderBundle(m_descriptor)) {
            makeInvalid(@"setPipeline: validation failed");
            return;
        }

        // FIXME: Remove after https://bugs.webkit.org/show_bug.cgi?id=26499 is implemented
        if (pipeline.sampleMask() != defaultSampleMask)
            m_requiresCommandReplay = true;

        if (m_pipeline && m_currentCommandIndex && icbNeedsToBeSplit(*m_pipeline, pipeline) && !m_requiresMetalWorkaround)
            endCurrentICB();

        recordCommand([pipeline = Ref { pipeline }, protectedThis = Ref { *this }] {
            protectedThis->setPipeline(pipeline);
            return false;
        });
    }

    m_pipeline = &pipeline;
}

void RenderBundleEncoder::setVertexBuffer(uint32_t slot, const Buffer* optionalBuffer, uint64_t offset, uint64_t size)
{
    RETURN_IF_FINISHED();
    m_maxVertexBufferSlot = std::max(m_maxVertexBufferSlot, slot);
    if (id<MTLIndirectRenderCommand> icbCommand = currentRenderCommand()) {
        if (!optionalBuffer) {
            m_utilizedBufferIndices.remove(slot);
            if (slot < m_vertexBuffers.size())
                m_vertexBuffers[slot] = BufferAndOffset();

            return;
        }

        auto& buffer = *optionalBuffer;
        m_utilizedBufferIndices.add(slot, size);
        addResource(m_resources, buffer.buffer(), MTLRenderStageVertex, &buffer);
        m_vertexBuffers[slot] = { .buffer = buffer.buffer(), .offset = offset, .dynamicOffsetCount = 0, .dynamicOffsets = nullptr, .size = size };
        if (m_renderPassEncoder)
            buffer.setCommandEncoder(m_renderPassEncoder->parentEncoder());
    } else {
        if (optionalBuffer) {
            auto& buffer = *optionalBuffer;
            if (!isValidToUseWith(buffer, *this)) {
                makeInvalid(@"setVertexBuffer: buffer is not valid");
                return;
            }
            if (computedSizeOverflows(buffer, offset, size)) {
                makeInvalid(@"setVertexBuffer: size overflowed");
                return;
            }
            if (slot >= m_device->limits().maxVertexBuffers || !(buffer.usage() & WGPUBufferUsage_Vertex) || (offset % 4)) {
                makeInvalid(@"setVertexBuffer: validation failed");
                return;
            }
            if (offset + size > buffer.initialSize()) {
                makeInvalid(@"offset + size > buffer.size()");
                return;
            }
        }

        m_requiresMetalWorkaround = false;
        recordCommand([slot, optionalBuffer, offset, size, protectedThis = Ref { *this }] {
            protectedThis->setVertexBuffer(slot, optionalBuffer, offset, size);
            return false;
        });
    }
}

void RenderBundleEncoder::setLabel(String&& label)
{
    m_indirectCommandBuffer.label = label;
}

#undef RETURN_IF_FINISHED

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
    RELEASE_ASSERT(indirectBuffer);
    WebGPU::fromAPI(renderBundleEncoder).drawIndexedIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderBundleEncoderDrawIndirect(WGPURenderBundleEncoder renderBundleEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    RELEASE_ASSERT(indirectBuffer);
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
    WebGPU::Buffer* optionalBuffer = nullptr;
    if (buffer)
        optionalBuffer = &WebGPU::fromAPI(buffer);
    WebGPU::fromAPI(renderBundleEncoder).setVertexBuffer(slot, optionalBuffer, offset, size);
}

void wgpuRenderBundleEncoderSetLabel(WGPURenderBundleEncoder renderBundleEncoder, const char* label)
{
    WebGPU::fromAPI(renderBundleEncoder).setLabel(WebGPU::fromAPI(label));
}
