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

RenderBundle::RenderBundle(NSArray<RenderBundleICBWithResources*> *resources, RefPtr<RenderBundleEncoder> encoder, Device& device)
    : m_device(device)
    , m_renderBundleEncoder(encoder)
{
    m_renderBundlesResources = resources;
}

RenderBundle::RenderBundle(Device& device)
    : m_device(device)
{
}

RenderBundle::~RenderBundle() = default;

void RenderBundle::setLabel(String&& label)
{
    m_renderBundlesResources.firstObject.indirectCommandBuffer.label = label;
}

void RenderBundle::replayCommands(id<MTLRenderCommandEncoder> commandEncoder) const
{
    if (m_renderBundleEncoder)
        m_renderBundleEncoder->replayCommands(commandEncoder);
}

void RenderBundle::updateMinMaxDepths(float minDepth, float maxDepth)
{
    if (m_minDepth == minDepth && m_maxDepth == maxDepth)
        return;

    m_minDepth = minDepth;
    m_maxDepth = maxDepth;
    float twoFloats[2] = { m_minDepth, m_maxDepth };
    for (RenderBundleICBWithResources* icb in m_renderBundlesResources)
        m_device->getQueue().writeBuffer(icb.fragmentDynamicOffsetsBuffer, 0, twoFloats, sizeof(float) * 2);
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
