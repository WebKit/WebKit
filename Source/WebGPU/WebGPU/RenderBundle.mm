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
- (instancetype)initWithUsage:(MTLResourceUsage)usage renderStages:(MTLRenderStages)renderStages
{
    if (!(self = [super init]))
        return nil;

    _usage = usage;
    _renderStages = renderStages;

    return self;
}
@end

namespace WebGPU {

RenderBundle::RenderBundle(id<MTLIndirectCommandBuffer> indirectCommandBuffer, RenderBundle::ResourcesContainer* resources, id<MTLRenderPipelineState> renderPipelineState, id<MTLDepthStencilState> depthStencilState, MTLCullMode cullMode, MTLWinding frontFace, MTLDepthClipMode clipMode, Device& device)
    : m_indirectCommandBuffer(indirectCommandBuffer)
    , m_device(device)
    , m_currentPipelineState(renderPipelineState)
    , m_depthStencilState(depthStencilState)
    , m_cullMode(cullMode)
    , m_frontFace(frontFace)
    , m_depthClipMode(clipMode)
{
    constexpr auto maxResourceUsageValue = MTLResourceUsageRead | MTLResourceUsageWrite;
    constexpr auto maxStageValue = MTLRenderStageVertex | MTLRenderStageFragment;
    static_assert(maxResourceUsageValue == 3 && maxStageValue == 3, "Code path assumes MTLResourceUsageRead | MTLResourceUsageWrite == 3 and MTLRenderStageVertex | MTLRenderStageFragment == 3");
    Vector<id<MTLResource>> stageResources[maxStageValue][maxResourceUsageValue];

    for (id<MTLResource> r : resources) {
        ResourceUsageAndRenderStage *usageAndStage = [resources objectForKey:r];
        stageResources[usageAndStage.renderStages - 1][usageAndStage.renderStages - 1].append(r);
    }

    for (size_t stage = 0; stage < maxStageValue; ++stage) {
        for (size_t i = 0; i < maxResourceUsageValue; ++i) {
            Vector<id<MTLResource>> &v = stageResources[stage][i];
            if (v.size()) {
                m_resources.append(BindableResources {
                    .mtlResources = WTFMove(v),
                    .usage = static_cast<MTLResourceUsage>(i + 1),
                    .renderStages = static_cast<MTLRenderStages>(stage + 1)
                });
            }
        }
    }
}

RenderBundle::RenderBundle(Device& device)
    : m_device(device)
{
}

RenderBundle::~RenderBundle() = default;

void RenderBundle::setLabel(String&& label)
{
    m_indirectCommandBuffer.label = label;
}

id<MTLRenderPipelineState> RenderBundle::currentPipelineState() const
{
    return m_currentPipelineState;
}

id<MTLDepthStencilState> RenderBundle::depthStencilState() const
{
    return m_depthStencilState;
}

MTLCullMode RenderBundle::cullMode() const
{
    return m_cullMode;
}

MTLWinding RenderBundle::frontFace() const
{
    return m_frontFace;
}

MTLDepthClipMode RenderBundle::depthClipMode() const
{
    return m_depthClipMode;
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
