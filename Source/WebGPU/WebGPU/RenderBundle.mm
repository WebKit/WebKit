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
#import "RenderBundle.h"

#import "APIConversions.h"

@implementation ResourceUsageAndRenderStage
- (instancetype)initWithUsage:(MTLResourceUsage)usage renderStages:(MTLRenderStages)renderStages entryUsage:(OptionSet<WebGPU::BindGroupEntryUsage>)entryUsage binding:(uint32_t)binding resource:(WebGPU::BindGroupEntryUsageData::Resource)resource
{
    if (!(self = [super init]))
        return nil;

    _usage = usage;
    _renderStages = renderStages;
    _entryUsage = entryUsage;
    _binding = binding;
    _resource = resource;

    return self;
}
@end

namespace WebGPU {

RenderBundle::RenderBundle(NSArray<RenderBundleICBWithResources*> *resources, RefPtr<RenderBundleEncoder> encoder, const WGPURenderBundleEncoderDescriptor& descriptor, uint64_t commandCount, Device& device)
    : m_device(device)
    , m_renderBundleEncoder(encoder)
    , m_renderBundlesResources(resources)
    , m_descriptor(descriptor)
    , m_descriptorColorFormats(descriptor.colorFormats ? Vector<WGPUTextureFormat>(std::span { descriptor.colorFormats, descriptor.colorFormatCount }) : Vector<WGPUTextureFormat>())
    , m_commandCount(commandCount)
{
    if (m_descriptorColorFormats.size())
        m_descriptor.colorFormats = &m_descriptorColorFormats[0];

    ASSERT(m_renderBundleEncoder || m_renderBundlesResources.count);
}

RenderBundle::RenderBundle(Device& device, NSString* errorString)
    : m_device(device)
    , m_lastErrorString(errorString)
{
}

RenderBundle::~RenderBundle() = default;

bool RenderBundle::isValid() const
{
    return m_renderBundleEncoder || m_renderBundlesResources.count;
}

void RenderBundle::setLabel(String&& label)
{
    m_renderBundlesResources.firstObject.indirectCommandBuffer.label = label;
}

void RenderBundle::replayCommands(RenderPassEncoder& renderPassEncoder) const
{
    if (m_renderBundleEncoder)
        m_renderBundleEncoder->replayCommands(renderPassEncoder);
}

void RenderBundle::updateMinMaxDepths(float minDepth, float maxDepth)
{
    if (m_minDepth == minDepth && m_maxDepth == maxDepth)
        return;

    m_minDepth = minDepth;
    m_maxDepth = maxDepth;
    float twoFloats[2] = { m_minDepth, m_maxDepth };
    for (RenderBundleICBWithResources* icb in m_renderBundlesResources)
        m_device->getQueue().writeBuffer(icb.fragmentDynamicOffsetsBuffer, 0, { reinterpret_cast<uint8_t*>(twoFloats), sizeof(float) * 2 });
}

uint64_t RenderBundle::drawCount() const
{
    return m_commandCount;
}

bool RenderBundle::validateRenderPass(bool depthReadOnly, bool stencilReadOnly, const WGPURenderPassDescriptor& descriptor, const Vector<RefPtr<TextureView>>& colorAttachmentViews, const RefPtr<TextureView>& depthStencilView) const
{
    if (depthReadOnly && !m_descriptor.depthReadOnly)
        return false;

    if (stencilReadOnly && !m_descriptor.stencilReadOnly)
        return false;

    if (m_descriptor.colorFormatCount != descriptor.colorAttachmentCount)
        return false;

    uint32_t defaultRasterSampleCount = 0;
    for (size_t i = 0, colorFormatCount = std::max(descriptor.colorAttachmentCount, m_descriptor.colorFormatCount); i < colorFormatCount; ++i) {
        auto descriptorColorFormat = i < m_descriptor.colorFormatCount ? m_descriptor.colorFormats[i] : WGPUTextureFormat_Undefined;
        if (i >= descriptor.colorAttachmentCount) {
            if (descriptorColorFormat == WGPUTextureFormat_Undefined)
                continue;
            return false;
        }
        auto* attachmentView = colorAttachmentViews[i].get();
        if (!attachmentView) {
            if (descriptorColorFormat == WGPUTextureFormat_Undefined)
                continue;
            return false;
        }
        auto& texture = *attachmentView;
        if (descriptorColorFormat != texture.format())
            return false;
        defaultRasterSampleCount = texture.sampleCount();
    }

    if (auto* depthStencil = descriptor.depthStencilAttachment) {
        if (!depthStencilView) {
            if (m_descriptor.depthStencilFormat != WGPUTextureFormat_Undefined)
                return false;
        } else {
            auto& texture = *depthStencilView.get();
            if (texture.format() != m_descriptor.depthStencilFormat)
                return false;
            defaultRasterSampleCount = texture.sampleCount();
        }
    } else if (m_descriptor.depthStencilFormat != WGPUTextureFormat_Undefined)
        return false;

    if (m_descriptor.sampleCount != defaultRasterSampleCount)
        return false;

    return true;
}

bool RenderBundle::validatePipeline(const RenderPipeline*)
{
    return true;
}

NSString* RenderBundle::lastError() const
{
    return m_lastErrorString;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuRenderBundleReference(WGPURenderBundle renderBundle)
{
    WebGPU::fromAPI(renderBundle).ref();
}

void wgpuRenderBundleRelease(WGPURenderBundle renderBundle)
{
    WebGPU::fromAPI(renderBundle).deref();
}

void wgpuRenderBundleSetLabel(WGPURenderBundle renderBundle, const char* label)
{
    WebGPU::fromAPI(renderBundle).setLabel(WebGPU::fromAPI(label));
}
