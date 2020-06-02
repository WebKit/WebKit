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

#include "GPUBindGroup.h"
#include "GPUBindGroupAllocator.h"
#include "GPUBindGroupDescriptor.h"
#include "GPUBindGroupLayout.h"
#include "GPUBindGroupLayoutDescriptor.h"
#include "GPUBuffer.h"
#include "GPUBufferDescriptor.h"
#include "GPUCommandBuffer.h"
#include "GPUComputePipeline.h"
#include "GPUComputePipelineDescriptor.h"
#include "GPUErrorScopes.h"
#include "GPUPipelineLayout.h"
#include "GPUPipelineLayoutDescriptor.h"
#include "GPURenderPipeline.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUSampler.h"
#include "GPUSamplerDescriptor.h"
#include "GPUShaderModule.h"
#include "GPUShaderModuleDescriptor.h"
#include "GPUSwapChainDescriptor.h"
#include "GPUTexture.h"
#include "GPUTextureDescriptor.h"
#include <algorithm>
#include <wtf/Optional.h>
#include <wtf/text/StringConcatenate.h>

namespace WebCore {

RefPtr<GPUBuffer> GPUDevice::tryCreateBuffer(const GPUBufferDescriptor& descriptor, GPUBufferMappedOption isMapped, GPUErrorScopes& errorScopes)
{
    return GPUBuffer::tryCreate(*this, descriptor, isMapped, errorScopes);
}

static unsigned maximumMipLevelCount(GPUTextureDimension dimension, GPUExtent3D size)
{
    unsigned width = size.width;
    unsigned height = 1;
    unsigned depth = 1;
    switch (dimension) {
    case GPUTextureDimension::_1d:
        break;
    case GPUTextureDimension::_2d:
        height = size.height;
        break;
    case GPUTextureDimension::_3d:
        height = size.height;
        depth = size.depth;
        break;
    }

    auto maxDimension = std::max(std::max(width, height), depth);
    int i = sizeof(GPUExtent3D::InnerType) * 8 - 1;
    while (!(maxDimension & (1 << i)) && i >= 0)
        --i;
    ASSERT(i >= 0);
    return i + 1;
}

RefPtr<GPUTexture> GPUDevice::tryCreateTexture(const GPUTextureDescriptor& descriptor) const
{
    if (!descriptor.size.width || !descriptor.size.height || !descriptor.size.depth) {
        m_errorScopes->generatePrefixedError("Textures can't have 0 dimensions.");
        return { };
    }

    if (!descriptor.mipLevelCount) {
        m_errorScopes->generatePrefixedError("Textures can't have 0 miplevels.");
        return { };
    }

    if (!descriptor.sampleCount) {
        m_errorScopes->generatePrefixedError("Textures can't have 0 samples.");
        return { };
    }

    // https://developer.apple.com/metal/Metal-Feature-Set-Tables.pdf
    unsigned maxRegularSize = 8192;
    unsigned maxDepthSize = 2048;
    switch (descriptor.dimension) {
    case GPUTextureDimension::_1d:
        if (descriptor.size.width > maxRegularSize) {
            m_errorScopes->generatePrefixedError(makeString("1D texture is too wide: ", descriptor.size.width, " needs to be <= ", maxRegularSize, "."));
            return { };
        }
        if (descriptor.size.height > maxDepthSize) {
            m_errorScopes->generatePrefixedError(makeString("1D texture array has too many layers: ", descriptor.size.height, " needs to be <= ", maxDepthSize, "."));
            return { };
        }
        break;
    case GPUTextureDimension::_2d:
        if (descriptor.size.width > maxRegularSize || descriptor.size.height > maxRegularSize) {
            m_errorScopes->generatePrefixedError(makeString("2D texture is too large:", descriptor.size.width, " and ", descriptor.size.height, " need to be <= ", maxRegularSize, "."));
            return { };
        }
        if (descriptor.size.depth > maxDepthSize) {
            m_errorScopes->generatePrefixedError(makeString("2D texture array has too many layers: ", descriptor.size.depth, " needs to be <= ", maxDepthSize, "."));
            return { };
        }
        break;
    case GPUTextureDimension::_3d:
        if (descriptor.size.width > maxDepthSize || descriptor.size.height > maxDepthSize || descriptor.size.depth > maxDepthSize) {
            m_errorScopes->generatePrefixedError(makeString("3D texture is too large: ", descriptor.size.width, ", ", descriptor.size.height, ", and ", descriptor.size.depth, " need to be <= ", maxDepthSize, "."));
            return { };
        }
        break;
    }

    if (descriptor.dimension == GPUTextureDimension::_1d && descriptor.mipLevelCount != 1) {
        m_errorScopes->generatePrefixedError("1D textures can't have mipmaps");
        return { };
    }

    auto maxLevels = maximumMipLevelCount(descriptor.dimension, descriptor.size);
    if (descriptor.mipLevelCount > maxLevels) {
        m_errorScopes->generatePrefixedError(makeString("Too many miplevels: ", descriptor.mipLevelCount, " needs to be <= ", maxLevels, "."));
        return { };
    }

    if (descriptor.sampleCount != 1 && descriptor.sampleCount != 4) {
        m_errorScopes->generatePrefixedError("Texture sampleCount can only be 1 or 4.");
        return { };
    }

    if (descriptor.sampleCount != 1) {
        if (descriptor.dimension != GPUTextureDimension::_2d) {
            m_errorScopes->generatePrefixedError("Texture dimension incompatible with multisampling.");
            return { };
        }
        if (descriptor.mipLevelCount != 1) {
            m_errorScopes->generatePrefixedError("Multisampled textures can't have mipmaps.");
            return { };
        }
        if (descriptor.size.depth != 1) {
            m_errorScopes->generatePrefixedError("Array textures can't be multisampled.");
            return { };
        }
        if (descriptor.usage & static_cast<GPUTextureUsageFlags>(GPUTextureUsage::Flags::Storage)) {
            m_errorScopes->generatePrefixedError("Multisampled textures can't have STORAGE usage.");
            return { };
        }
        // FIXME: Only some pixel formats are capable of MSAA. When we add those new pixel formats, we'll have to add filtering code here.
    }

    // The height component for 1d textures is the array length.
    // The depth component for 2d textures is the array length.
    if (descriptor.dimension == GPUTextureDimension::_1d && descriptor.size.depth != 1) {
        m_errorScopes->generatePrefixedError("1D textures can't have depth != 1.");
        return { };
    }

    if (descriptor.format == GPUTextureFormat::Depth32floatStencil8 && descriptor.dimension != GPUTextureDimension::_2d) {
        m_errorScopes->generatePrefixedError("Depth/stencil textures must be 2d.");
        return { };
    }

    if (descriptor.format == GPUTextureFormat::Depth32floatStencil8 && descriptor.sampleCount != 1) {
        m_errorScopes->generatePrefixedError("Depth/stencil textures can't be multisampled.");
        return { };
    }

    if (static_cast<GPUTextureUsageFlags>(descriptor.usage) >= static_cast<GPUTextureUsageFlags>(GPUTextureUsage::Flags::MaximumValue)) {
        m_errorScopes->generatePrefixedError("Invalid usage flags.");
        return { };
    }

    return GPUTexture::tryCreate(*this, descriptor, makeRef(*m_errorScopes));
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

RefPtr<GPUShaderModule> GPUDevice::tryCreateShaderModule(const GPUShaderModuleDescriptor& descriptor) const
{
    return GPUShaderModule::tryCreate(*this, descriptor);
}

RefPtr<GPURenderPipeline> GPUDevice::tryCreateRenderPipeline(const GPURenderPipelineDescriptor& descriptor, GPUErrorScopes& errorScopes) const
{
    return GPURenderPipeline::tryCreate(*this, descriptor, errorScopes);
}

RefPtr<GPUComputePipeline> GPUDevice::tryCreateComputePipeline(const GPUComputePipelineDescriptor& descriptor, GPUErrorScopes& errorScopes) const
{
    return GPUComputePipeline::tryCreate(*this, descriptor, errorScopes);
}

RefPtr<GPUBindGroup> GPUDevice::tryCreateBindGroup(const GPUBindGroupDescriptor& descriptor, GPUErrorScopes& errorScopes) const
{
    if (!m_bindGroupAllocator)
        m_bindGroupAllocator = GPUBindGroupAllocator::create(errorScopes);

    return GPUBindGroup::tryCreate(descriptor, *m_bindGroupAllocator);
}

RefPtr<GPUCommandBuffer> GPUDevice::tryCreateCommandBuffer() const
{
    return GPUCommandBuffer::tryCreate(*this);
}

RefPtr<GPUQueue> GPUDevice::tryGetQueue() const
{
    if (!m_queue)
        m_queue = GPUQueue::tryCreate(*this);

    return m_queue;
}
    
void GPUDevice::setSwapChain(RefPtr<GPUSwapChain>&& swapChain)
{
    m_swapChain = WTFMove(swapChain);
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
