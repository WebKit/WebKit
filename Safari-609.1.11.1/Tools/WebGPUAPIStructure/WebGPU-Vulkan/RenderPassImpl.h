/*
* Copyright (C) 2017 Apple Inc. All rights reserved.
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

#pragma once
#include "WebGPU.h"
#include "PassImpl.h"
#include "BufferImpl.h"
#include "TextureImpl.h"
#include <unordered_set>
#include <vulkan/vulkan.hpp>

namespace WebGPU {
    class RenderPassImpl : public PassImpl, public RenderPass {
    public:
        RenderPassImpl() = default;
        virtual ~RenderPassImpl() = default;
        RenderPassImpl(const RenderPassImpl&) = delete;
        RenderPassImpl(RenderPassImpl&&) = default;
        RenderPassImpl& operator=(const RenderPassImpl&) = delete;
        RenderPassImpl& operator=(RenderPassImpl&&) = default;

        RenderPassImpl(vk::Device device, vk::UniqueCommandBuffer&& commandBuffer, vk::UniqueFramebuffer&& framebuffer, vk::UniqueRenderPass&& renderPass, const std::vector<std::reference_wrapper<TextureImpl>>& destinationTextures);

    private:
        void setRenderState(const RenderState& renderState) override;
        void setVertexAttributeBuffers(const std::vector<std::reference_wrapper<Buffer>>& buffers) override;
        void setResources(unsigned int index, const std::vector<WebGPU::ResourceReference>&) override;
        void setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) override;
        void setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) override;
        void draw(unsigned int vertexCount) override;

        vk::UniqueFramebuffer framebuffer;
        vk::UniqueRenderPass renderPass;
    };
}

