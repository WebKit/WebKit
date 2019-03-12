/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "GPUDevice.h"

#if ENABLE(WEBGPU)

#include "GPUBindGroupLayout.h"
#include "GPUBindGroupLayoutDescriptor.h"
#include "GPUBuffer.h"
#include "GPUBufferDescriptor.h"
#include "GPUCommandBuffer.h"
#include "GPUPipelineLayout.h"
#include "GPUPipelineLayoutDescriptor.h"
#include "GPURenderPipeline.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUSampler.h"
#include "GPUSamplerDescriptor.h"
#include "GPUShaderModule.h"
#include "GPUShaderModuleDescriptor.h"
#include "GPUSwapChain.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"

namespace WebCore {

RefPtr<GPUBuffer> GPUDevice::tryCreateBuffer(GPUBufferDescriptor&& descriptor)
{
    return GPUBuffer::tryCreate(makeRef(*this), WTFMove(descriptor));
}

RefPtr<GPUTexture> GPUDevice::tryCreateTexture(GPUTextureDescriptor&& descriptor) const
{
    return GPUTexture::tryCreate(*this, WTFMove(descriptor));
}

RefPtr<GPUSampler> GPUDevice::tryCreateSampler(const GPUSamplerDescriptor& descriptor) const
{
    return GPUSampler::tryCreate(*this, descriptor);
}

RefPtr<GPUBindGroupLayout> GPUDevice::tryCreateBindGroupLayout(const GPUBindGroupLayoutDescriptor& descriptor) const
{
    return GPUBindGroupLayout::tryCreate(*this, descriptor);
}

Ref<GPUPipelineLayout> GPUDevice::createPipelineLayout(GPUPipelineLayoutDescriptor&& descriptor) const
{
    return GPUPipelineLayout::create(WTFMove(descriptor));
}

RefPtr<GPUShaderModule> GPUDevice::createShaderModule(GPUShaderModuleDescriptor&& descriptor) const
{
    return GPUShaderModule::create(*this, WTFMove(descriptor));
}

RefPtr<GPURenderPipeline> GPUDevice::createRenderPipeline(GPURenderPipelineDescriptor&& descriptor) const
{
    return GPURenderPipeline::create(*this, WTFMove(descriptor));
}

RefPtr<GPUCommandBuffer> GPUDevice::createCommandBuffer()
{
    return GPUCommandBuffer::create(*this);
}

RefPtr<GPUSwapChain> GPUDevice::tryCreateSwapChain(const GPUSwapChainDescriptor& descriptor, int width, int height) const
{
    m_swapChain = GPUSwapChain::tryCreate(*this, descriptor, width, height);
    return m_swapChain;
}

RefPtr<GPUQueue> GPUDevice::getQueue() const
{
    if (!m_queue)
        m_queue = GPUQueue::tryCreate(*this);

    return m_queue;
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
