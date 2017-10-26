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
#include "RenderPassImpl.h"
#include <memory>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace WebGPU {
    class QueueImpl : public Queue {
    public:
        QueueImpl() = default;
        virtual ~QueueImpl();
        QueueImpl(const QueueImpl&) = delete;
        QueueImpl(QueueImpl&&) = default;
        QueueImpl& operator=(const QueueImpl&) = delete;
        QueueImpl& operator=(QueueImpl&&) = default;

        QueueImpl(vk::Device device, vk::CommandPool commandPool, std::vector<vk::UniqueImageView>& deviceImageViews, vk::SwapchainKHR swapchain, vk::Extent2D size, vk::Format swapchainFormat, vk::Queue&& queue);

    private:
        std::unique_ptr<RenderPass> createRenderPass(const std::vector<std::reference_wrapper<Texture>>* textures) override;
        void commitRenderPass(std::unique_ptr<RenderPass>&&) override;
        std::unique_ptr<ComputePass> createComputePass() override;
        void commitComputePass(std::unique_ptr<ComputePass>&&) override;
        std::unique_ptr<BlitPass> createBlitPass() override;
        void commitBlitPass(std::unique_ptr<BlitPass>&&) override;
        std::unique_ptr<HostAccessPass> createHostAccessPass() override;
        void commitHostAccessPass(std::unique_ptr<HostAccessPass>&&) override;
        void present() override;

        vk::UniqueRenderPass createSpecificRenderPass(const std::vector<vk::ImageView>& imageViews, const std::vector<vk::Format>& formats);
        vk::UniqueFramebuffer createFramebuffer(vk::RenderPass renderpass, const std::vector<vk::ImageView>& imageViews);
        void prepareCurrentFrame();
        vk::CommandBuffer synchronizeResources(PassImpl& pass);
        void commitPass(PassImpl&);

        uint32_t activeSemaphoreIndex{ 0 };
        uint32_t activeSwapchainIndex;
        vk::Device device;
        vk::CommandPool commandPool;
        std::vector<vk::UniqueImageView>& deviceImageViews;
        vk::SwapchainKHR swapchain;
        std::vector<std::vector<vk::UniqueSemaphore>> swapchainBufferSemaphores;
        std::vector<std::vector<vk::UniqueFence>> swapchainBufferFences;
        std::vector<std::vector<std::unique_ptr<PassImpl>>> activePasses;
        std::vector<std::vector<vk::UniqueCommandBuffer>> fixupCommandBuffers;
        std::vector<std::vector<std::size_t>> hostAccessIndices;
        vk::Extent2D size;
        vk::Format swapchainFormat;
        vk::Queue queue;
    };
}

