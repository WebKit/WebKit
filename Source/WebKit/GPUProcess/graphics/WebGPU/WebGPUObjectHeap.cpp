/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebGPUObjectHeap.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteAdapter.h"
#include "RemoteBindGroup.h"
#include "RemoteBindGroupLayout.h"
#include "RemoteBuffer.h"
#include "RemoteCommandBuffer.h"
#include "RemoteCommandEncoder.h"
#include "RemoteComputePassEncoder.h"
#include "RemoteComputePipeline.h"
#include "RemoteDevice.h"
#include "RemoteExternalTexture.h"
#include "RemoteGPU.h"
#include "RemotePipelineLayout.h"
#include "RemoteQuerySet.h"
#include "RemoteQueue.h"
#include "RemoteRenderBundle.h"
#include "RemoteRenderBundleEncoder.h"
#include "RemoteRenderPassEncoder.h"
#include "RemoteRenderPipeline.h"
#include "RemoteSampler.h"
#include "RemoteShaderModule.h"
#include "RemoteTexture.h"
#include "RemoteTextureView.h"

namespace WebKit::WebGPU {

ObjectHeap::ObjectHeap()
{
}

ObjectHeap::~ObjectHeap()
{
}

void ObjectHeap::addObject(RemoteAdapter& adapter)
{
    auto result = m_objects.add(&adapter, adapter);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteBindGroup& bindGroup)
{
    auto result = m_objects.add(&bindGroup, bindGroup);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteBindGroupLayout& bindGroupLayout)
{
    auto result = m_objects.add(&bindGroupLayout, bindGroupLayout);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteBuffer& buffer)
{
    auto result = m_objects.add(&buffer, buffer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteCommandBuffer& commandBuffer)
{
    auto result = m_objects.add(&commandBuffer, commandBuffer);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteCommandEncoder& commandEncoder)
{
    auto result = m_objects.add(&commandEncoder, commandEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteComputePassEncoder& computePassEncoder)
{
    auto result = m_objects.add(&computePassEncoder, computePassEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteComputePipeline& computePipeline)
{
    auto result = m_objects.add(&computePipeline, computePipeline);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteDevice& device)
{
    auto result = m_objects.add(&device, device);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteExternalTexture& externalTexture)
{
    auto result = m_objects.add(&externalTexture, externalTexture);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteGPU& gpu)
{
    auto result = m_objects.add(&gpu, gpu);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemotePipelineLayout& pipelineLayout)
{
    auto result = m_objects.add(&pipelineLayout, pipelineLayout);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteQuerySet& querySet)
{
    auto result = m_objects.add(&querySet, querySet);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteQueue& queue)
{
    auto result = m_objects.add(&queue, queue);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteRenderBundleEncoder& renderBundleEncoder)
{
    auto result = m_objects.add(&renderBundleEncoder, renderBundleEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteRenderBundle& renderBundle)
{
    auto result = m_objects.add(&renderBundle, renderBundle);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteRenderPassEncoder& renderPassEncoder)
{
    auto result = m_objects.add(&renderPassEncoder, renderPassEncoder);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteRenderPipeline& renderPipeline)
{
    auto result = m_objects.add(&renderPipeline, renderPipeline);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteSampler& sampler)
{
    auto result = m_objects.add(&sampler, sampler);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteShaderModule& shaderModule)
{
    auto result = m_objects.add(&shaderModule, shaderModule);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteTexture& texture)
{
    auto result = m_objects.add(&texture, texture);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::addObject(RemoteTextureView& textureView)
{
    auto result = m_objects.add(&textureView, textureView);
    ASSERT_UNUSED(result, result.isNewEntry);
}

void ObjectHeap::removeObject(RemoteAdapter& adapter)
{
    auto result = m_objects.remove(&adapter);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteBindGroup& bindGroup)
{
    auto result = m_objects.remove(&bindGroup);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteBindGroupLayout& bindGroupLayout)
{
    auto result = m_objects.remove(&bindGroupLayout);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteBuffer& buffer)
{
    auto result = m_objects.remove(&buffer);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteCommandBuffer& commandBuffer)
{
    auto result = m_objects.remove(&commandBuffer);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteCommandEncoder& commandEncoder)
{
    auto result = m_objects.remove(&commandEncoder);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteComputePassEncoder& computePassEncoder)
{
    auto result = m_objects.remove(&computePassEncoder);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteComputePipeline& computePipeline)
{
    auto result = m_objects.remove(&computePipeline);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteDevice& device)
{
    auto result = m_objects.remove(&device);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteExternalTexture& externalTexture)
{
    auto result = m_objects.remove(&externalTexture);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteGPU& gpu)
{
    auto result = m_objects.remove(&gpu);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemotePipelineLayout& pipelineLayout)
{
    auto result = m_objects.remove(&pipelineLayout);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteQuerySet& querySet)
{
    auto result = m_objects.remove(&querySet);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteQueue& queue)
{
    auto result = m_objects.remove(&queue);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteRenderBundleEncoder& renderBundleEncoder)
{
    auto result = m_objects.remove(&renderBundleEncoder);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteRenderBundle& renderBundle)
{
    auto result = m_objects.remove(&renderBundle);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteRenderPassEncoder& renderPassEncoder)
{
    auto result = m_objects.remove(&renderPassEncoder);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteRenderPipeline& renderPipeline)
{
    auto result = m_objects.remove(&renderPipeline);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteSampler& sampler)
{
    auto result = m_objects.remove(&sampler);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteShaderModule& shaderModule)
{
    auto result = m_objects.remove(&shaderModule);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteTexture& texture)
{
    auto result = m_objects.remove(&texture);
    ASSERT_UNUSED(result, result);
}

void ObjectHeap::removeObject(RemoteTextureView& textureView)
{
    auto result = m_objects.remove(&textureView);
    ASSERT_UNUSED(result, result);
}

} // namespace WebKit::WebGPU

#endif // HAVE(GPU_PROCESS)
