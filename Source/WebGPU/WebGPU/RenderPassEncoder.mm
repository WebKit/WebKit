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
#import "RenderPassEncoder.h"

#import "APIConversions.h"
#import "BindGroup.h"
#import "BindableResource.h"
#import "Buffer.h"
#import "CommandEncoder.h"
#import "ExternalTexture.h"
#import "IsValidToUseWith.h"
#import "Pipeline.h"
#import "QuerySet.h"
#import "RenderBundle.h"
#import "RenderPipeline.h"
#import "TextureView.h"
#import <wtf/StdLibExtras.h>

namespace WebGPU {

#define RETURN_IF_FINISHED() \
if (!m_parentEncoder->isLocked() || m_parentEncoder->isFinished()) { \
    m_device->generateAValidationError([NSString stringWithFormat:@"%s: failed as encoding has finished", __PRETTY_FUNCTION__]); \
    return; \
} \
if (!m_renderCommandEncoder || !m_parentEncoder->isValid() || !m_parentEncoder->encoderIsCurrent(m_renderCommandEncoder)) { \
    m_renderCommandEncoder = nil; \
    return; \
}


RenderPassEncoder::RenderPassEncoder(id<MTLRenderCommandEncoder> renderCommandEncoder, const WGPURenderPassDescriptor& descriptor, NSUInteger visibilityResultBufferSize, bool depthReadOnly, bool stencilReadOnly, CommandEncoder& parentEncoder, id<MTLBuffer> visibilityResultBuffer, uint64_t maxDrawCount, Device& device)
    : m_renderCommandEncoder(renderCommandEncoder)
    , m_device(device)
    , m_visibilityResultBufferSize(visibilityResultBufferSize)
    , m_depthReadOnly(depthReadOnly)
    , m_stencilReadOnly(stencilReadOnly)
    , m_parentEncoder(parentEncoder)
    , m_visibilityResultBuffer(visibilityResultBuffer)
    , m_descriptor(descriptor)
    , m_descriptorColorAttachments(descriptor.colorAttachmentCount ? Vector<WGPURenderPassColorAttachment>(std::span { descriptor.colorAttachments, descriptor.colorAttachmentCount }) : Vector<WGPURenderPassColorAttachment>())
    , m_descriptorDepthStencilAttachment(descriptor.depthStencilAttachment ? *descriptor.depthStencilAttachment : WGPURenderPassDepthStencilAttachment())
    , m_descriptorTimestampWrites(descriptor.timestampWrites ? *descriptor.timestampWrites : WGPURenderPassTimestampWrites())
    , m_maxDrawCount(maxDrawCount)
{
    if (m_descriptorColorAttachments.size())
        m_descriptor.colorAttachments = &m_descriptorColorAttachments[0];
    if (descriptor.depthStencilAttachment)
        m_descriptor.depthStencilAttachment = &m_descriptorDepthStencilAttachment;
    if (descriptor.timestampWrites)
        m_descriptor.timestampWrites = &m_descriptorTimestampWrites;
    for (size_t i = 0; i < descriptor.colorAttachmentCount; ++i)
        m_colorAttachmentViews.append(WeakPtr { static_cast<TextureView*>(descriptor.colorAttachments[i].view) });
    if (descriptor.depthStencilAttachment)
        m_depthStencilView = WeakPtr { static_cast<TextureView*>(descriptor.depthStencilAttachment->view) };

    m_parentEncoder->lock(true);

    m_attachmentsToClear = [NSMutableDictionary dictionary];
    m_allColorAttachments = [NSMutableDictionary dictionary];
    for (uint32_t i = 0; i < descriptor.colorAttachmentCount; ++i) {
        const auto& attachment = descriptor.colorAttachments[i];
        if (!attachment.view)
            continue;

        auto& texture = fromAPI(attachment.view);
        texture.setPreviouslyCleared();
        addResourceToActiveResources(texture, BindGroupEntryUsage::Attachment);

        if (attachment.resolveTarget) {
            auto& texture = fromAPI(attachment.resolveTarget);
            texture.setCommandEncoder(parentEncoder);
            texture.setPreviouslyCleared();
            addResourceToActiveResources(texture, BindGroupEntryUsage::Attachment);
        }

        texture.setCommandEncoder(parentEncoder);
        id<MTLTexture> textureToClear = texture.texture();
        m_renderTargetWidth = textureToClear.width;
        m_renderTargetHeight = textureToClear.height;
        if (!textureToClear)
            continue;
        TextureAndClearColor *textureWithClearColor = [[TextureAndClearColor alloc] initWithTexture:textureToClear];
        if (attachment.storeOp != WGPUStoreOp_Discard) {
            auto& c = attachment.clearValue;
            textureWithClearColor.clearColor = MTLClearColorMake(c.r, c.g, c.b, c.a);
        } else if (attachment.loadOp == WGPULoadOp_Load) {
            textureWithClearColor.clearColor = MTLClearColorMake(0, 0, 0, 0);
            [m_attachmentsToClear setObject:textureWithClearColor forKey:@(i)];
        }

        textureWithClearColor.depthPlane = attachment.depthSlice.value_or(0);
        [m_allColorAttachments setObject:textureWithClearColor forKey:@(i)];
    }

    if (const auto* attachment = descriptor.depthStencilAttachment) {
        auto& textureView = fromAPI(attachment->view);
        textureView.setPreviouslyCleared();
        textureView.setCommandEncoder(parentEncoder);
        id<MTLTexture> depthTexture = textureView.isDestroyed() ? nil : textureView.texture();
        if (textureView.width() && !m_renderTargetWidth) {
            m_renderTargetWidth = depthTexture.width;
            m_renderTargetHeight = depthTexture.height;
        }

        m_depthClearValue = attachment->depthStoreOp == WGPUStoreOp_Discard ? 0 : quantizedDepthValue(attachment->depthClearValue, textureView.format());
        if (!Device::isStencilOnlyFormat(depthTexture.pixelFormat)) {
            m_clearDepthAttachment = depthTexture && attachment->depthStoreOp == WGPUStoreOp_Discard && attachment->depthLoadOp == WGPULoadOp_Load;
            m_depthStencilAttachmentToClear = depthTexture;
            addResourceToActiveResources(textureView, attachment->depthReadOnly ? BindGroupEntryUsage::AttachmentRead : BindGroupEntryUsage::Attachment, WGPUTextureAspect_DepthOnly);
        }

        m_stencilClearValue = attachment->stencilStoreOp == WGPUStoreOp_Discard ? 0 : attachment->stencilClearValue;
        if (Texture::stencilOnlyAspectMetalFormat(textureView.descriptor().format)) {
            m_clearStencilAttachment = depthTexture && attachment->stencilStoreOp == WGPUStoreOp_Discard && attachment->stencilLoadOp == WGPULoadOp_Load;
            m_depthStencilAttachmentToClear = depthTexture;
            addResourceToActiveResources(textureView, attachment->stencilReadOnly ? BindGroupEntryUsage::AttachmentRead : BindGroupEntryUsage::Attachment, WGPUTextureAspect_StencilOnly);
        }
    }

    m_viewportWidth = static_cast<float>(m_renderTargetWidth);
    m_viewportHeight = static_cast<float>(m_renderTargetHeight);
}

double RenderPassEncoder::quantizedDepthValue(double depthClearValue, WGPUTextureFormat pixelFormat)
{
    if (depthClearValue < 0 || depthClearValue > 1)
        return depthClearValue;
    switch (pixelFormat) {
    case WGPUTextureFormat_Depth16Unorm:
        return std::nextafterf(depthClearValue + 0.5 / USHRT_MAX, 1.f);
    default:
        return depthClearValue;
    }
}

RenderPassEncoder::RenderPassEncoder(CommandEncoder& parentEncoder, Device& device, NSString* errorString)
    : m_device(device)
    , m_parentEncoder(parentEncoder)
    , m_lastErrorString(errorString)
{
    m_parentEncoder->lock(true);
}

RenderPassEncoder::~RenderPassEncoder()
{
    if (m_renderCommandEncoder)
        m_parentEncoder->endEncoding(m_renderCommandEncoder);
    m_renderCommandEncoder = nil;
}

bool RenderPassEncoder::occlusionQueryIsDestroyed() const
{
    return m_visibilityResultBufferSize == NSUIntegerMax;
}

void RenderPassEncoder::beginOcclusionQuery(uint32_t queryIndex)
{
    RETURN_IF_FINISHED();

    queryIndex *= sizeof(uint64_t);
    if (m_occlusionQueryActive || m_queryBufferIndicesToClear.contains(queryIndex)) {
        makeInvalid(@"beginOcclusionQuery validation failure");
        return;
    }
    m_occlusionQueryActive = true;
    m_visibilityResultBufferOffset = queryIndex;
    m_queryBufferIndicesToClear.add(m_visibilityResultBufferOffset);


    if (occlusionQueryIsDestroyed())
        return;
    if (!m_visibilityResultBufferSize || queryIndex >= m_visibilityResultBufferSize) {
        makeInvalid(@"beginOcclusionQuery validation failure");
        return;
    }

    if (m_queryBufferUtilizedIndices.contains(queryIndex))
        return;
    [m_renderCommandEncoder setVisibilityResultMode:MTLVisibilityResultModeCounting offset:queryIndex];
    m_queryBufferUtilizedIndices.add(queryIndex);
}

void RenderPassEncoder::endOcclusionQuery()
{
    RETURN_IF_FINISHED();
    if (!m_occlusionQueryActive) {
        makeInvalid(@"endOcclusionQuery - occlusion query was not active");
        return;
    }
    m_occlusionQueryActive = false;

    if (occlusionQueryIsDestroyed())
        return;
    [m_renderCommandEncoder setVisibilityResultMode:MTLVisibilityResultModeDisabled offset:m_visibilityResultBufferOffset];
}

static void setViewportMinMaxDepthIntoBuffer(auto& fragmentDynamicOffsets, float minDepth, float maxDepth)
{
    static_assert(sizeof(fragmentDynamicOffsets[0]) == sizeof(minDepth), "expect dynamic offsets container to have matching size to depth values");
    static_assert(RenderBundleEncoder::startIndexForFragmentDynamicOffsets > 1, "code path assumes value is 2 or more");
    if (fragmentDynamicOffsets.size() < RenderBundleEncoder::startIndexForFragmentDynamicOffsets)
        fragmentDynamicOffsets.grow(RenderBundleEncoder::startIndexForFragmentDynamicOffsets);

    using destType = typename std::remove_reference<decltype(fragmentDynamicOffsets[0])>::type;
    fragmentDynamicOffsets[0] = bitwise_cast<destType>(minDepth);
    fragmentDynamicOffsets[1] = bitwise_cast<destType>(maxDepth);
}

void RenderPassEncoder::addResourceToActiveResources(const void* resourceAddress, id<MTLResource> mtlResource, OptionSet<BindGroupEntryUsage> initialUsage, uint32_t baseMipLevel, uint32_t baseArrayLayer, WGPUTextureAspect aspect)
{
    if (!mtlResource)
        return;

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
    EntryMap* entryMap = nullptr;
    if (auto it = m_usagesForResource.find(resourceAddress); it != m_usagesForResource.end()) {
        entryMap = &it->value;
        if (auto innerIt = it->value.find(mapKey); innerIt != it->value.end())
            resourceUsage.add(innerIt->value);
    }

    if (!BindGroup::allowedUsage(resourceUsage)) {
        makeInvalid([NSString stringWithFormat:@"Bind group has incompatible usage list: %@", BindGroup::usageName(resourceUsage)]);
        return;
    }
    if (!entryMap) {
        EntryMap entryMap;
        entryMap.set(mapKey, resourceUsage);
        m_usagesForResource.set(resourceAddress, entryMap);
    } else
        entryMap->set(mapKey, resourceUsage);
}

void RenderPassEncoder::addResourceToActiveResources(const TextureView& texture, OptionSet<BindGroupEntryUsage> resourceUsage, WGPUTextureAspect textureAspect)
{
    addResourceToActiveResources(&texture.apiParentTexture(), texture.parentTexture(), resourceUsage, texture.baseMipLevel(), texture.baseArrayLayer(), textureAspect);
}

void RenderPassEncoder::addResourceToActiveResources(const TextureView& texture, OptionSet<BindGroupEntryUsage> resourceUsage)
{
    WGPUTextureAspect textureAspect = texture.aspect();
    if (textureAspect != WGPUTextureAspect_All) {
        addResourceToActiveResources(&texture.apiParentTexture(), texture.parentTexture(), resourceUsage, texture.baseMipLevel(), texture.baseArrayLayer(), textureAspect);
        return;
    }

    addResourceToActiveResources(&texture.apiParentTexture(), texture.parentTexture(), resourceUsage, texture.baseMipLevel(), texture.baseArrayLayer(), WGPUTextureAspect_DepthOnly);
    addResourceToActiveResources(&texture.apiParentTexture(), texture.parentTexture(), resourceUsage, texture.baseMipLevel(), texture.baseArrayLayer(), WGPUTextureAspect_StencilOnly);
}

void RenderPassEncoder::addResourceToActiveResources(const BindGroupEntryUsageData::Resource& resource, id<MTLResource> mtlResource, OptionSet<BindGroupEntryUsage> resourceUsage)
{
    WTF::switchOn(resource, [&](const RefPtr<const Buffer>& buffer) {
        if (buffer.get())
            addResourceToActiveResources(buffer.get(), buffer->buffer(), resourceUsage);
        }, [&](const RefPtr<const TextureView>& textureView) {
            if (textureView.get())
                addResourceToActiveResources(*textureView.get(), resourceUsage);
        }, [&](const RefPtr<const ExternalTexture>& externalTexture) {
            addResourceToActiveResources(externalTexture.get(), mtlResource, resourceUsage);
    });
}

bool RenderPassEncoder::runIndexBufferValidation(uint32_t firstInstance, uint32_t instanceCount)
{
    if (!m_pipeline || !m_indexBuffer) {
        makeInvalid(@"Missing pipeline before draw command");
        return false;
    }

    auto strideCount = firstInstance + instanceCount;
    if (!strideCount)
        return true;

    auto& requiredBufferIndices = m_pipeline->requiredBufferIndices();
    for (auto& [bufferIndex, bufferData] : requiredBufferIndices) {
        auto it = m_vertexBuffers.find(bufferIndex);
        RELEASE_ASSERT(it != m_vertexBuffers.end());
        auto bufferSize = it->value.size;
        auto stride = bufferData.stride;
        auto lastStride = bufferData.lastStride;
        if (bufferData.stepMode == WGPUVertexStepMode_Instance) {
            if ((strideCount - 1) * stride + lastStride > bufferSize) {
                makeInvalid([NSString stringWithFormat:@"Buffer[%d] fails: (strideCount(%d) - 1) * stride(%llu) + lastStride(%llu) > bufferSize(%llu) / mtlBufferSize(%lu)", bufferIndex, strideCount, stride, lastStride, bufferSize, (unsigned long)it->value.buffer.length]);
                return false;
            }
        }
    }

    return true;
}

void RenderPassEncoder::runVertexBufferValidation(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    if (!m_pipeline) {
        makeInvalid(@"Missing pipeline before draw command");
        return;
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

        auto it = m_vertexBuffers.find(bufferIndex);
        RELEASE_ASSERT(it != m_vertexBuffers.end());
        auto bufferSize = it->value.size;
        if ((strideCount - 1) * bufferData.stride + bufferData.lastStride > bufferSize) {
            makeInvalid([NSString stringWithFormat:@"Buffer[%d] fails: (strideCount(%llu) - 1) * bufferData.stride(%llu) + bufferData.lastStride(%llu) > bufferSize(%llu)", bufferIndex, strideCount, bufferData.stride,  bufferData.lastStride, bufferSize]);
            return;
        }
    }
}

uint32_t RenderPassEncoder::maxVertexBufferIndex() const
{
    return m_maxVertexBufferSlot;
}

uint32_t RenderPassEncoder::maxBindGroupIndex() const
{
    return m_maxBindGroupSlot;
}

NSString* RenderPassEncoder::errorValidatingAndBindingBuffers()
{
    if (!m_pipeline)
        return @"pipeline is not set";

    auto& requiredBufferIndices = m_pipeline->requiredBufferIndices();
    for (auto& [bufferIndex, _] : requiredBufferIndices) {
        auto it = m_vertexBuffers.find(bufferIndex);
        if (it == m_vertexBuffers.end())
            return [NSString stringWithFormat:@"Buffer1 index[%u] is missing", bufferIndex];

        if (it->value.offset < it->value.buffer.length)
            [m_renderCommandEncoder setVertexBuffer:it->value.buffer offset:it->value.offset atIndex:bufferIndex];
        else if (it->value.size) {
            makeInvalid(@"offset >= buffer.length && buffer.size");
            return @"offset >= buffer.length && buffer.size";
        } else
            [m_renderCommandEncoder setVertexBuffer:m_device->placeholderBuffer() offset:0 atIndex:bufferIndex];
    }

    auto bindGroupSpaceUsed = maxBindGroupIndex() + 1;
    auto vertexBufferSpaceUsed = maxVertexBufferIndex() + 1;
    if (bindGroupSpaceUsed + vertexBufferSpaceUsed > m_device->limits().maxBindGroupsPlusVertexBuffers)
        return @"Too many bind groups and vertex buffers used";

    return nil;
}

NSString* RenderPassEncoder::errorValidatingDrawIndexed() const
{
    if (!m_indexBuffer)
        return @"Index buffer is not set";

    auto topology = m_pipeline->primitiveTopology();
    if (topology == WGPUPrimitiveTopology_LineStrip || topology == WGPUPrimitiveTopology_TriangleStrip) {
        if (m_indexType != m_pipeline->stripIndexFormat())
            return @"Primitive topology mismiatch with render pipeline";
    }

    return nil;
}

void RenderPassEncoder::incrementDrawCount(uint32_t drawCalls)
{
    m_drawCount += drawCalls;
    if (m_drawCount > m_maxDrawCount)
        makeInvalid(@"m_drawCount > m_maxDrawCount");
}

bool RenderPassEncoder::issuedDrawCall() const
{
    return m_drawCount;
}

bool RenderPassEncoder::executePreDrawCommands(const Buffer* indirectBuffer)
{
    if (!m_pipeline) {
        makeInvalid(@"Missing pipeline before draw command");
        return false;
    }
    auto& pipeline = *m_pipeline.get();
    if (NSString* error = pipeline.pipelineLayout().errorValidatingBindGroupCompatibility(m_bindGroups, pipeline.vertexStageInBufferCount())) {
        makeInvalid(error);
        return false;
    }

    if (indirectBuffer)
        addResourceToActiveResources(indirectBuffer, indirectBuffer->buffer(), BindGroupEntryUsage::Input);
    if (NSString* error = errorValidatingAndBindingBuffers()) {
        makeInvalid(error);
        return false;
    }

    for (auto& [groupIndex, weakBindGroup] : m_bindGroups) {
        if (!weakBindGroup.get()) {
            makeInvalid(@"Bind group is missing");
            return false;
        }
        auto& group = *weakBindGroup.get();
        if (!validateBindGroup(group)) {
            makeInvalid(@"buffer is too small");
            return false;
        }
        [m_renderCommandEncoder setVertexBuffer:group.vertexArgumentBuffer() offset:0 atIndex:m_device->vertexBufferIndexForBindGroup(groupIndex)];
        [m_renderCommandEncoder setFragmentBuffer:group.fragmentArgumentBuffer() offset:0 atIndex:groupIndex];
    }

    if (pipeline.renderPipelineState())
        [m_renderCommandEncoder setRenderPipelineState:pipeline.renderPipelineState()];
    if (pipeline.depthStencilState())
        [m_renderCommandEncoder setDepthStencilState:pipeline.depthStencilState()];
    [m_renderCommandEncoder setCullMode:pipeline.cullMode()];
    [m_renderCommandEncoder setFrontFacingWinding:pipeline.frontFace()];
    [m_renderCommandEncoder setDepthClipMode:pipeline.depthClipMode()];
    [m_renderCommandEncoder setDepthBias:pipeline.depthBias() slopeScale:pipeline.depthBiasSlopeScale() clamp:pipeline.depthBiasClamp()];
    [m_renderCommandEncoder setViewport: { m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_minDepth, m_maxDepth } ];

    m_queryBufferIndicesToClear.remove(m_visibilityResultBufferOffset);

    for (auto& kvp : m_bindGroupDynamicOffsets) {
        auto& pipelineLayout = m_pipeline->pipelineLayout();
        auto bindGroupIndex = kvp.key;

        auto* pvertexOffsets = pipelineLayout.vertexOffsets(bindGroupIndex, kvp.value);
        if (pvertexOffsets && pvertexOffsets->size()) {
            auto& vertexOffsets = *pvertexOffsets;
            auto startIndex = pipelineLayout.vertexOffsetForBindGroup(bindGroupIndex);
            RELEASE_ASSERT(vertexOffsets.size() <= m_vertexDynamicOffsets.size() + startIndex);
            memcpy(&m_vertexDynamicOffsets[startIndex], &vertexOffsets[0], sizeof(vertexOffsets[0]) * vertexOffsets.size());
        }

        auto* pfragmentOffsets = pipelineLayout.fragmentOffsets(bindGroupIndex, kvp.value);
        if (pfragmentOffsets && pfragmentOffsets->size()) {
            auto& fragmentOffsets = *pfragmentOffsets;
            auto startIndex = pipelineLayout.fragmentOffsetForBindGroup(bindGroupIndex);
            RELEASE_ASSERT(fragmentOffsets.size() <= m_fragmentDynamicOffsets.size() + startIndex);
            memcpy(&m_fragmentDynamicOffsets[startIndex + RenderBundleEncoder::startIndexForFragmentDynamicOffsets], &fragmentOffsets[0], sizeof(fragmentOffsets[0]) * fragmentOffsets.size());
        }
    }

    if (m_vertexDynamicOffsets.size())
        [m_renderCommandEncoder setVertexBytes:&m_vertexDynamicOffsets[0] length:m_vertexDynamicOffsets.size() * sizeof(m_vertexDynamicOffsets[0]) atIndex:m_device->maxBuffersPlusVertexBuffersForVertexStage()];

    setViewportMinMaxDepthIntoBuffer(m_fragmentDynamicOffsets, m_minDepth, m_maxDepth);
    ASSERT(m_fragmentDynamicOffsets.size());
    [m_renderCommandEncoder setFragmentBytes:&m_fragmentDynamicOffsets[0] length:m_fragmentDynamicOffsets.size() * sizeof(m_fragmentDynamicOffsets[0]) atIndex:m_device->maxBuffersForFragmentStage()];

    m_bindGroupDynamicOffsets.clear();
    incrementDrawCount();

    return true;
}

void RenderPassEncoder::draw(uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    RETURN_IF_FINISHED();

    // FIXME: validation according to
    // https://gpuweb.github.io/gpuweb/#dom-gpurendercommandsmixin-draw
    if (!executePreDrawCommands())
        return;
    runVertexBufferValidation(vertexCount, instanceCount, firstVertex, firstInstance);
    if (!instanceCount || !vertexCount)
        return;

    [m_renderCommandEncoder
        drawPrimitives:m_primitiveType
        vertexStart:firstVertex
        vertexCount:vertexCount
        instanceCount:instanceCount
        baseInstance:firstInstance];
}

void RenderPassEncoder::drawIndexed(uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    RETURN_IF_FINISHED();

    auto indexSizeInBytes = (m_indexType == MTLIndexTypeUInt16 ? sizeof(uint16_t) : sizeof(uint32_t));
    if (!executePreDrawCommands())
        return;

    auto firstIndexOffsetInBytes = firstIndex * indexSizeInBytes;
    if (NSString* error = errorValidatingDrawIndexed()) {
        makeInvalid(error);
        return;
    }

    id<MTLBuffer> indexBuffer = m_indexBuffer.get() ? m_indexBuffer->buffer() : nil;
    if (firstIndexOffsetInBytes + indexCount * indexSizeInBytes > m_indexBufferSize) {
        makeInvalid(@"Values to drawIndexed are invalid");
        return;
    }

    if (!runIndexBufferValidation(firstInstance, instanceCount))
        return;

    if (!instanceCount || !indexCount)
        return;
    [m_renderCommandEncoder drawIndexedPrimitives:m_primitiveType indexCount:indexCount indexType:m_indexType indexBuffer:indexBuffer indexBufferOffset:(m_indexBufferOffset + firstIndexOffsetInBytes) instanceCount:instanceCount baseVertex:baseVertex baseInstance:firstInstance];
}

void RenderPassEncoder::drawIndexedIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    RETURN_IF_FINISHED();
    if (!isValidToUseWith(indirectBuffer, *this)) {
        makeInvalid(@"drawIndexedIndirect: buffer was invalid");
        return;
    }
    indirectBuffer.setCommandEncoder(m_parentEncoder);
    if (indirectBuffer.isDestroyed())
        return;

    if (!m_indexBuffer) {
        makeInvalid(@"drawIndexedIndirect: index buffer is nil");
        return;
    }

    id<MTLBuffer> indexBuffer = m_indexBuffer->buffer();
    if (!indexBuffer.length)
        return;

    if (!(indirectBuffer.usage() & WGPUBufferUsage_Indirect) || (indirectOffset % 4)) {
        makeInvalid(@"drawIndexedIndirect: validation failed");
        return;
    }

    if (indirectBuffer.initialSize() < indirectOffset + sizeof(MTLDrawIndexedPrimitivesIndirectArguments)) {
        makeInvalid(@"drawIndexedIndirect: validation failed");
        return;
    }

    if (!executePreDrawCommands(&indirectBuffer))
        return;
    [m_renderCommandEncoder drawIndexedPrimitives:m_primitiveType indexType:m_indexType indexBuffer:indexBuffer indexBufferOffset:m_indexBufferOffset indirectBuffer:indirectBuffer.buffer() indirectBufferOffset:indirectOffset];
}

void RenderPassEncoder::drawIndirect(const Buffer& indirectBuffer, uint64_t indirectOffset)
{
    RETURN_IF_FINISHED();
    if (!isValidToUseWith(indirectBuffer, *this)) {
        makeInvalid(@"drawIndirect: buffer was invalid");
        return;
    }
    indirectBuffer.setCommandEncoder(m_parentEncoder);
    if (indirectBuffer.isDestroyed())
        return;
    if (!(indirectBuffer.usage() & WGPUBufferUsage_Indirect) || (indirectOffset % 4)) {
        makeInvalid(@"drawIndirect: validation failed");
        return;
    }

    if (indirectBuffer.initialSize() < indirectOffset + sizeof(MTLDrawPrimitivesIndirectArguments)) {
        makeInvalid(@"drawIndirect: validation failed");
        return;
    }

    if (!executePreDrawCommands(&indirectBuffer))
        return;
    [m_renderCommandEncoder drawPrimitives:m_primitiveType indirectBuffer:indirectBuffer.buffer() indirectBufferOffset:indirectOffset];
}

void RenderPassEncoder::endPass()
{
    if (m_passEnded) {
        m_device->generateAValidationError([NSString stringWithFormat:@"%s: failed as pass is already ended", __PRETTY_FUNCTION__]);
        return;
    }
    m_passEnded = true;

    RETURN_IF_FINISHED();

    auto passIsValid = isValid();
    if (m_debugGroupStackSize || m_occlusionQueryActive || !passIsValid) {
        m_parentEncoder->endEncoding(m_renderCommandEncoder);
        m_renderCommandEncoder = nil;
        m_parentEncoder->makeInvalid([NSString stringWithFormat:@"RenderPassEncoder.endPass failure, m_debugGroupStackSize = %llu, m_occlusionQueryActive = %d, isValid = %d, error = %@", m_debugGroupStackSize, m_occlusionQueryActive, passIsValid, m_lastErrorString]);
        return;
    }

    auto endEncoder = ^{
        m_parentEncoder->endEncoding(m_renderCommandEncoder);
    };
    auto issuedDraw = issuedDrawCall();
    bool useDiscardTextures = m_attachmentsToClear.count || m_clearDepthAttachment || m_clearStencilAttachment;
    bool hasTexturesToClear = m_allColorAttachments.count || m_attachmentsToClear.count || (m_depthStencilAttachmentToClear && (m_clearDepthAttachment || m_clearStencilAttachment));

    if ((!issuedDraw || useDiscardTextures) && hasTexturesToClear) {
        if (m_depthStencilAttachmentToClear && !issuedDraw && !useDiscardTextures) {
            auto pixelFormat = m_depthStencilAttachmentToClear.pixelFormat;
            m_clearDepthAttachment = !Device::isStencilOnlyFormat(pixelFormat);
            m_clearStencilAttachment = pixelFormat == MTLPixelFormatDepth32Float_Stencil8 || pixelFormat == MTLPixelFormatStencil8 || pixelFormat == MTLPixelFormatX32_Stencil8;
        }
        if (useDiscardTextures)
            endEncoder();
        m_parentEncoder->runClearEncoder(useDiscardTextures ? m_attachmentsToClear : m_allColorAttachments, m_depthStencilAttachmentToClear, m_clearDepthAttachment, m_clearStencilAttachment, m_depthClearValue, m_stencilClearValue, useDiscardTextures ? nil : m_renderCommandEncoder);
    } else
        endEncoder();

    m_renderCommandEncoder = nil;
    m_parentEncoder->lock(false);

    if (m_queryBufferIndicesToClear.size() && !occlusionQueryIsDestroyed()) {
        id<MTLBlitCommandEncoder> blitCommandEncoder = m_parentEncoder->ensureBlitCommandEncoder();
        for (auto& offset : m_queryBufferIndicesToClear)
            [blitCommandEncoder fillBuffer:m_visibilityResultBuffer range:NSMakeRange(static_cast<NSUInteger>(offset), sizeof(uint64_t)) value:0];

        m_queryBufferIndicesToClear.clear();
        m_parentEncoder->finalizeBlitCommandEncoder();
    }
}

void RenderPassEncoder::setCommandEncoder(const BindGroupEntryUsageData::Resource& resource)
{
    WTF::switchOn(resource, [&](const RefPtr<const Buffer>& buffer) {
        if (buffer)
            buffer->setCommandEncoder(m_parentEncoder);
        }, [&](const RefPtr<const TextureView>& textureView) {
            if (textureView)
                textureView->setCommandEncoder(m_parentEncoder);
        }, [&](const RefPtr<const ExternalTexture>& externalTexture) {
            if (externalTexture)
                externalTexture->setCommandEncoder(m_parentEncoder);
    });
}

void RenderPassEncoder::executeBundles(Vector<std::reference_wrapper<RenderBundle>>&& bundles)
{
    RETURN_IF_FINISHED();
    m_queryBufferIndicesToClear.remove(m_visibilityResultBufferOffset);

    [m_renderCommandEncoder setViewport: { m_viewportX, m_viewportY, m_viewportWidth, m_viewportHeight, m_minDepth, m_maxDepth } ];

    for (auto& bundle : bundles) {
        auto& renderBundle = bundle.get();
        if (!isValidToUseWith(renderBundle, *this)) {
            makeInvalid([NSString stringWithFormat:@"executeBundles: render bundle is not valid, reason = %@", renderBundle.lastError()]);
            return;
        }

        renderBundle.updateMinMaxDepths(m_minDepth, m_maxDepth);

        if (!renderBundle.validateRenderPass(m_depthReadOnly, m_stencilReadOnly, m_descriptor, m_colorAttachmentViews, m_depthStencilView) || !renderBundle.validatePipeline(m_pipeline.get())) {
            makeInvalid(@"executeBundles: validation failed");
            return;
        }

        incrementDrawCount(renderBundle.drawCount());

        for (RenderBundleICBWithResources* icb in renderBundle.renderBundlesResources()) {
            if (id<MTLDepthStencilState> depthStencilState = icb.depthStencilState)
                [m_renderCommandEncoder setDepthStencilState:depthStencilState];
            [m_renderCommandEncoder setCullMode:icb.cullMode];
            [m_renderCommandEncoder setFrontFacingWinding:icb.frontFace];
            [m_renderCommandEncoder setDepthClipMode:icb.depthClipMode];
            [m_renderCommandEncoder setDepthBias:icb.depthBias slopeScale:icb.depthBiasSlopeScale clamp:icb.depthBiasClamp];
            ASSERT(icb.resources);

            for (const auto& resource : *icb.resources) {
                if ((resource.renderStages & (MTLRenderStageVertex | MTLRenderStageFragment)) && resource.mtlResources.size())
                    [m_renderCommandEncoder useResources:&resource.mtlResources[0] count:resource.mtlResources.size() usage:resource.usage stages:resource.renderStages];

                ASSERT(resource.mtlResources.size() == resource.resourceUsages.size());
                for (size_t i = 0, resourceCount = resource.mtlResources.size(); i < resourceCount; ++i) {
                    auto& resourceUsage = resource.resourceUsages[i];
                    addResourceToActiveResources(resourceUsage.resource, resource.mtlResources[i], resourceUsage.usage);
                    setCommandEncoder(resourceUsage.resource);
                }
            }

            id<MTLIndirectCommandBuffer> indirectCommandBuffer = icb.indirectCommandBuffer;
            [m_renderCommandEncoder executeCommandsInBuffer:indirectCommandBuffer withRange:NSMakeRange(0, indirectCommandBuffer.size)];
        }

        renderBundle.replayCommands(*this);
    }
}

CommandEncoder& RenderPassEncoder::parentEncoder()
{
    return m_parentEncoder;
}

bool RenderPassEncoder::colorDepthStencilTargetsMatch(const RenderPipeline& pipeline) const
{
    return pipeline.colorDepthStencilTargetsMatch(m_descriptor, m_colorAttachmentViews, m_depthStencilView);
}

id<MTLRenderCommandEncoder> RenderPassEncoder::renderCommandEncoder() const
{
    return m_renderCommandEncoder;
}

void RenderPassEncoder::insertDebugMarker(String&& markerLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-insertdebugmarker
    if (!prepareTheEncoderState())
        return;

    [m_renderCommandEncoder insertDebugSignpost:markerLabel];
}

bool RenderPassEncoder::validatePopDebugGroup() const
{
    if (!m_parentEncoder->isLocked())
        return false;

    if (!m_debugGroupStackSize)
        return false;

    return true;
}

void RenderPassEncoder::makeInvalid(NSString* errorString)
{
    m_lastErrorString = errorString;

    if (!m_renderCommandEncoder) {
        m_parentEncoder->makeInvalid([NSString stringWithFormat:@"RenderPassEncoder.makeInvalid, rason = %@", errorString]);
        return;
    }

    m_parentEncoder->setLastError(errorString);
    m_parentEncoder->endEncoding(m_renderCommandEncoder);
    m_renderCommandEncoder = nil;
}

void RenderPassEncoder::popDebugGroup()
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-popdebuggroup

    if (!prepareTheEncoderState())
        return;

    if (!validatePopDebugGroup()) {
        makeInvalid(@"popDebugGroup: validation failed");
        return;
    }

    --m_debugGroupStackSize;
    [m_renderCommandEncoder popDebugGroup];
}

void RenderPassEncoder::pushDebugGroup(String&& groupLabel)
{
    // https://gpuweb.github.io/gpuweb/#dom-gpudebugcommandsmixin-pushdebuggroup

    if (!prepareTheEncoderState())
        return;

    ++m_debugGroupStackSize;
    [m_renderCommandEncoder pushDebugGroup:groupLabel];
}

void RenderPassEncoder::setBindGroup(uint32_t groupIndex, const BindGroup& group, uint32_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    RETURN_IF_FINISHED();

    if (!isValidToUseWith(group, *this)) {
        makeInvalid(@"setBindGroup: invalid bind group");
        return;
    }

    if (groupIndex >= m_device->limits().maxBindGroups) {
        makeInvalid(@"setBindGroup: groupIndex >= limits.maxBindGroups");
        return;
    }

    auto* bindGroupLayout = group.bindGroupLayout();
    if (!bindGroupLayout) {
        makeInvalid(@"GPURenderPassEncoder.setBindGroup: bind group is nil");
        return;
    }
    if (NSString* error = bindGroupLayout->errorValidatingDynamicOffsets(dynamicOffsets, dynamicOffsetCount, group)) {
        makeInvalid([NSString stringWithFormat:@"GPURenderPassEncoder.setBindGroup: %@", error]);
        return;
    }

    m_maxBindGroupSlot = std::max(groupIndex, m_maxBindGroupSlot);
    if (dynamicOffsetCount)
        m_bindGroupDynamicOffsets.set(groupIndex, Vector<uint32_t>(std::span { dynamicOffsets, dynamicOffsetCount }));

    for (const auto& resource : group.resources()) {
        if ((resource.renderStages & (MTLRenderStageVertex | MTLRenderStageFragment)) && resource.mtlResources.size())
            [m_renderCommandEncoder useResources:&resource.mtlResources[0] count:resource.mtlResources.size() usage:resource.usage stages:resource.renderStages];

        ASSERT(resource.mtlResources.size() == resource.resourceUsages.size());
        for (size_t i = 0, sz = resource.mtlResources.size(); i < sz; ++i) {
            auto& resourceUsage = resource.resourceUsages[i];
            addResourceToActiveResources(resourceUsage.resource, resource.mtlResources[i], resourceUsage.usage);
            setCommandEncoder(resourceUsage.resource);
        }
    }

    m_bindGroups.set(groupIndex, group);
}

void RenderPassEncoder::setBlendConstant(const WGPUColor& color)
{
    RETURN_IF_FINISHED();

    [m_renderCommandEncoder setBlendColorRed:color.r green:color.g blue:color.b alpha:color.a];
}

void RenderPassEncoder::setIndexBuffer(const Buffer& buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    RETURN_IF_FINISHED();
    if (!isValidToUseWith(buffer, *this)) {
        makeInvalid(@"setIndexBuffer: invalid buffer");
        return;
    }

    buffer.setCommandEncoder(m_parentEncoder);
    if (buffer.isDestroyed())
        return;

    auto indexSizeInBytes = (format == WGPUIndexFormat_Uint16 ? sizeof(uint16_t) : sizeof(uint32_t));
    if (!(buffer.usage() & WGPUBufferUsage_Index) || (offset % indexSizeInBytes)) {
        makeInvalid(@"setIndexBuffer: validation failed");
        return;
    }

    if (computedSizeOverflows(buffer, offset, size)) {
        makeInvalid(@"setIndexBuffer: computed size overflows");
        return;
    }

    m_indexBuffer = buffer;
    m_indexBufferSize = size;
    m_indexType = format == WGPUIndexFormat_Uint32 ? MTLIndexTypeUInt32 : MTLIndexTypeUInt16;
    m_indexBufferOffset = offset;
    UNUSED_PARAM(size);
    addResourceToActiveResources(&buffer, buffer.buffer(), BindGroupEntryUsage::Input);
}

NSString* RenderPassEncoder::errorValidatingPipeline(const RenderPipeline& pipeline) const
{
    if (!isValidToUseWith(pipeline, *this))
        return @"setPipeline: invalid RenderPipeline";

    if (!pipeline.validateDepthStencilState(m_depthReadOnly, m_stencilReadOnly))
        return @"setPipeline: invalid depth stencil state";

    if (!colorDepthStencilTargetsMatch(pipeline))
        return @"setPipeline: color and depth targets from pass do not match pipeline";

    return nil;
}

void RenderPassEncoder::setPipeline(const RenderPipeline& pipeline)
{
    RETURN_IF_FINISHED();

    if (NSString *error = errorValidatingPipeline(pipeline)) {
        makeInvalid(error);
        return;
    }

    m_primitiveType = pipeline.primitiveType();
    m_pipeline = &pipeline;

    m_vertexDynamicOffsets.resize(pipeline.pipelineLayout().sizeOfVertexDynamicOffsets());
    m_fragmentDynamicOffsets.resize(pipeline.pipelineLayout().sizeOfFragmentDynamicOffsets() + RenderBundleEncoder::startIndexForFragmentDynamicOffsets);

    if (m_fragmentDynamicOffsets.size() < RenderBundleEncoder::startIndexForFragmentDynamicOffsets)
        m_fragmentDynamicOffsets.grow(RenderBundleEncoder::startIndexForFragmentDynamicOffsets);
    static_assert(RenderBundleEncoder::startIndexForFragmentDynamicOffsets > 2, "code path assumes value is greater than 2");
    m_fragmentDynamicOffsets[2] = pipeline.sampleMask();
}

void RenderPassEncoder::setScissorRect(uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    RETURN_IF_FINISHED();
    if (x + width > m_renderTargetWidth || y + height > m_renderTargetHeight) {
        makeInvalid();
        return;
    }
    [m_renderCommandEncoder setScissorRect: { x, y, width, height } ];
}

void RenderPassEncoder::setStencilReference(uint32_t reference)
{
    RETURN_IF_FINISHED();
    [m_renderCommandEncoder setStencilReferenceValue:(reference & 0xFF)];
}

void RenderPassEncoder::setVertexBuffer(uint32_t slot, const Buffer* optionalBuffer, uint64_t offset, uint64_t size)
{
    RETURN_IF_FINISHED()
    if (!optionalBuffer) {
        m_vertexBuffers.remove(slot);
        return;
    }

    auto& buffer = *optionalBuffer;
    buffer.setCommandEncoder(m_parentEncoder);
    if (!isValidToUseWith(buffer, *this)) {
        makeInvalid(@"setVertexBuffer: invalid buffer");
        return;
    }

    if (buffer.isDestroyed())
        return;

    if (computedSizeOverflows(buffer, offset, size)) {
        makeInvalid(@"setVertexBuffer: size overflowed");
        return;
    }

    if (slot >= m_device->limits().maxVertexBuffers || !(buffer.usage() & WGPUBufferUsage_Vertex) || (offset % 4)) {
        makeInvalid(@"setVertexBuffer: validation failed");
        return;
    }

    m_maxVertexBufferSlot = std::max(slot, m_maxVertexBufferSlot);
    id<MTLBuffer> mtlBuffer = buffer.buffer();
    auto bufferLength = mtlBuffer.length;
    m_vertexBuffers.set(slot, BufferAndOffset { .buffer = mtlBuffer, .offset = offset, .size = size });
    if (offset == bufferLength && !size)
        return;
    if (offset >= bufferLength) {
        makeInvalid(@"setVertexBuffer: buffer length is invalid");
        return;
    }
    addResourceToActiveResources(&buffer, mtlBuffer, BindGroupEntryUsage::Input);
}

void RenderPassEncoder::setViewport(float x, float y, float width, float height, float minDepth, float maxDepth)
{
    RETURN_IF_FINISHED();
    if (x < 0 || y < 0 || width < 0 || height < 0 || x + width > m_renderTargetWidth || y + height > m_renderTargetHeight || minDepth < 0 || maxDepth > 1 || minDepth > maxDepth) {
        makeInvalid();
        return;
    }
    m_minDepth = minDepth;
    m_maxDepth = maxDepth;
    m_viewportX = x;
    m_viewportY = y;
    m_viewportWidth = width;
    m_viewportHeight = height;
}

void RenderPassEncoder::setLabel(String&& label)
{
    m_renderCommandEncoder.label = label;
}

#undef RETURN_IF_FINISHED

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuRenderPassEncoderReference(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).ref();
}

void wgpuRenderPassEncoderRelease(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).deref();
}

void wgpuRenderPassEncoderBeginOcclusionQuery(WGPURenderPassEncoder renderPassEncoder, uint32_t queryIndex)
{
    WebGPU::fromAPI(renderPassEncoder).beginOcclusionQuery(queryIndex);
}

void wgpuRenderPassEncoderDraw(WGPURenderPassEncoder renderPassEncoder, uint32_t vertexCount, uint32_t instanceCount, uint32_t firstVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderPassEncoder).draw(vertexCount, instanceCount, firstVertex, firstInstance);
}

void wgpuRenderPassEncoderDrawIndexed(WGPURenderPassEncoder renderPassEncoder, uint32_t indexCount, uint32_t instanceCount, uint32_t firstIndex, int32_t baseVertex, uint32_t firstInstance)
{
    WebGPU::fromAPI(renderPassEncoder).drawIndexed(indexCount, instanceCount, firstIndex, baseVertex, firstInstance);
}

void wgpuRenderPassEncoderDrawIndexedIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderPassEncoder).drawIndexedIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderPassEncoderDrawIndirect(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer indirectBuffer, uint64_t indirectOffset)
{
    WebGPU::fromAPI(renderPassEncoder).drawIndirect(WebGPU::fromAPI(indirectBuffer), indirectOffset);
}

void wgpuRenderPassEncoderEndOcclusionQuery(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).endOcclusionQuery();
}

void wgpuRenderPassEncoderEnd(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).endPass();
}

void wgpuRenderPassEncoderExecuteBundles(WGPURenderPassEncoder renderPassEncoder, size_t bundlesCount, const WGPURenderBundle* bundles)
{
    Vector<std::reference_wrapper<WebGPU::RenderBundle>> bundlesToForward;
    for (uint32_t i = 0; i < bundlesCount; ++i)
        bundlesToForward.append(WebGPU::fromAPI(bundles[i]));
    WebGPU::fromAPI(renderPassEncoder).executeBundles(WTFMove(bundlesToForward));
}

void wgpuRenderPassEncoderInsertDebugMarker(WGPURenderPassEncoder renderPassEncoder, const char* markerLabel)
{
    WebGPU::fromAPI(renderPassEncoder).insertDebugMarker(WebGPU::fromAPI(markerLabel));
}

void wgpuRenderPassEncoderPopDebugGroup(WGPURenderPassEncoder renderPassEncoder)
{
    WebGPU::fromAPI(renderPassEncoder).popDebugGroup();
}

void wgpuRenderPassEncoderPushDebugGroup(WGPURenderPassEncoder renderPassEncoder, const char* groupLabel)
{
    WebGPU::fromAPI(renderPassEncoder).pushDebugGroup(WebGPU::fromAPI(groupLabel));
}

void wgpuRenderPassEncoderSetBindGroup(WGPURenderPassEncoder renderPassEncoder, uint32_t groupIndex, WGPUBindGroup group, size_t dynamicOffsetCount, const uint32_t* dynamicOffsets)
{
    WebGPU::fromAPI(renderPassEncoder).setBindGroup(groupIndex, WebGPU::fromAPI(group), dynamicOffsetCount, dynamicOffsets);
}

void wgpuRenderPassEncoderSetBlendConstant(WGPURenderPassEncoder renderPassEncoder, const WGPUColor* color)
{
    WebGPU::fromAPI(renderPassEncoder).setBlendConstant(*color);
}

void wgpuRenderPassEncoderSetIndexBuffer(WGPURenderPassEncoder renderPassEncoder, WGPUBuffer buffer, WGPUIndexFormat format, uint64_t offset, uint64_t size)
{
    WebGPU::fromAPI(renderPassEncoder).setIndexBuffer(WebGPU::fromAPI(buffer), format, offset, size);
}

void wgpuRenderPassEncoderSetPipeline(WGPURenderPassEncoder renderPassEncoder, WGPURenderPipeline pipeline)
{
    WebGPU::fromAPI(renderPassEncoder).setPipeline(WebGPU::fromAPI(pipeline));
}

void wgpuRenderPassEncoderSetScissorRect(WGPURenderPassEncoder renderPassEncoder, uint32_t x, uint32_t y, uint32_t width, uint32_t height)
{
    WebGPU::fromAPI(renderPassEncoder).setScissorRect(x, y, width, height);
}

void wgpuRenderPassEncoderSetStencilReference(WGPURenderPassEncoder renderPassEncoder, uint32_t reference)
{
    WebGPU::fromAPI(renderPassEncoder).setStencilReference(reference);
}

void wgpuRenderPassEncoderSetVertexBuffer(WGPURenderPassEncoder renderPassEncoder, uint32_t slot, WGPUBuffer buffer, uint64_t offset, uint64_t size)
{
    WebGPU::Buffer* optionalBuffer = nullptr;
    if (buffer)
        optionalBuffer = &WebGPU::fromAPI(buffer);
    WebGPU::fromAPI(renderPassEncoder).setVertexBuffer(slot, optionalBuffer, offset, size);
}

void wgpuRenderPassEncoderSetViewport(WGPURenderPassEncoder renderPassEncoder, float x, float y, float width, float height, float minDepth, float maxDepth)
{
    WebGPU::fromAPI(renderPassEncoder).setViewport(x, y, width, height, minDepth, maxDepth);
}

void wgpuRenderPassEncoderSetLabel(WGPURenderPassEncoder renderPassEncoder, const char* label)
{
    WebGPU::fromAPI(renderPassEncoder).setLabel(WebGPU::fromAPI(label));
}
