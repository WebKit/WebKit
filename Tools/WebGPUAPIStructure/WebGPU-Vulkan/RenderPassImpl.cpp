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

#include "RenderPassImpl.h"
#include "RenderStateImpl.h"
#include "BufferImpl.h"
#include "TextureImpl.h"
#include <cassert>

namespace WebGPU {
    RenderPassImpl::RenderPassImpl(vk::Device device, vk::UniqueCommandBuffer&& commandBuffer, vk::UniqueFramebuffer&& framebuffer, vk::UniqueRenderPass&& renderPass, const std::vector<std::reference_wrapper<TextureImpl>>& destinationTextures) : PassImpl(device, std::move(commandBuffer)), framebuffer(std::move(framebuffer)), renderPass(std::move(renderPass)) {
        for (auto& destinationTexture : destinationTextures) {
            insertTexture(destinationTexture.get());
        }
    }

    void RenderPassImpl::setRenderState(const RenderState& renderState) {
        auto* downcast = dynamic_cast<const RenderStateImpl*>(&renderState);
        assert(downcast != nullptr);
        auto& state = *downcast;
        currentPipelineLayout = state.getPipelineLayout();
        commandBuffer->bindPipeline(vk::PipelineBindPoint::eGraphics, state.getPipeline());
    }

    void RenderPassImpl::setVertexAttributeBuffers(const std::vector<std::reference_wrapper<Buffer>>& buffers) {
        std::vector<vk::Buffer> vulkanBuffers;
        std::vector<vk::DeviceSize> offsets;
        for (const auto& buffer : buffers) {
            auto* downcast = dynamic_cast<BufferImpl*>(&buffer.get());
            assert(downcast != nullptr);
            auto& buf = *downcast;
            vulkanBuffers.push_back(buf.getBuffer());
            offsets.push_back(0);
            insertBuffer(buf);
        }
        commandBuffer->bindVertexBuffers(0, vulkanBuffers, offsets);
    }

    void RenderPassImpl::setResources(unsigned int index, const std::vector<WebGPU::ResourceReference>& resourceReferences) {
        PassImpl::setResources(vk::PipelineBindPoint::eGraphics, index, resourceReferences);
    }

    void RenderPassImpl::setViewport(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
        auto const viewport = vk::Viewport().setX(static_cast<float>(x)).setY(static_cast<float>(y)).setWidth(static_cast<float>(width)).setHeight(static_cast<float>(height)).setMinDepth(0.0f).setMaxDepth(1.0f);
        commandBuffer->setViewport(0, 1, &viewport);
    }

    void RenderPassImpl::setScissorRect(unsigned int x, unsigned int y, unsigned int width, unsigned int height) {
        vk::Rect2D const scissor(vk::Offset2D(x, y), vk::Extent2D(width, height));
        commandBuffer->setScissor(0, 1, &scissor);
    }

    void RenderPassImpl::draw(unsigned int vertexCount) {
        commandBuffer->draw(vertexCount, 1, 0, 0);
    }
}
