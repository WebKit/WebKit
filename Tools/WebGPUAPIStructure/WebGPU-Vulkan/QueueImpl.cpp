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

#include "QueueImpl.h"
#include "TextureImpl.h"
#include "RenderPassImpl.h"
#include "ComputePassImpl.h"
#include "BlitPassImpl.h"
#include "HostAccessPassImpl.h"
#include <array>
#include <cassert>
#include <boost/optional.hpp>

namespace WebGPU {
    QueueImpl::QueueImpl(vk::Device device, vk::CommandPool commandPool, std::vector<vk::UniqueImageView>& deviceImageViews, vk::SwapchainKHR swapchain, vk::Extent2D size, vk::Format swapchainFormat, vk::Queue&& queue) : device(device), deviceImageViews(deviceImageViews), swapchain(swapchain), commandPool(commandPool), size(size), swapchainFormat(swapchainFormat), queue(std::move(queue)) {
        assert(deviceImageViews.size() > activeSemaphoreIndex);
        for (std::size_t i = 0; i < deviceImageViews.size(); ++i) {
            swapchainBufferSemaphores.push_back({});
            swapchainBufferFences.push_back({});
            activePasses.push_back({});
            fixupCommandBuffers.push_back({});
            hostAccessIndices.push_back({});
        }
        prepareCurrentFrame();
    }

    void QueueImpl::prepareCurrentFrame() {
        auto& currentSemaphoreVector = swapchainBufferSemaphores[activeSemaphoreIndex];
        currentSemaphoreVector.clear();
        swapchainBufferFences[activeSemaphoreIndex].clear();
        activePasses[activeSemaphoreIndex].clear();
        fixupCommandBuffers[activeSemaphoreIndex].clear();
        hostAccessIndices[activeSemaphoreIndex].clear();
        currentSemaphoreVector.emplace_back(device.createSemaphoreUnique(vk::SemaphoreCreateInfo()));
        auto result = device.acquireNextImageKHR(swapchain, std::numeric_limits<uint64_t>::max(), *currentSemaphoreVector[0], vk::Fence());
        assert(result.result == vk::Result::eSuccess);
        activeSwapchainIndex = result.value;
    }

    vk::UniqueRenderPass QueueImpl::createSpecificRenderPass(const std::vector<vk::ImageView>& imageViews, const std::vector<vk::Format>& formats) {
        std::vector<vk::AttachmentDescription> attachmentDescriptions;
        std::vector<vk::AttachmentReference> attachmentReferences;
        for (std::size_t i = 0; i < imageViews.size(); ++i) {
            auto attachmentDescription = vk::AttachmentDescription()
                .setFormat(formats[i])
                .setSamples(vk::SampleCountFlagBits::e1)
                .setLoadOp(vk::AttachmentLoadOp::eClear)
                .setStoreOp(vk::AttachmentStoreOp::eStore)
                .setStencilLoadOp(vk::AttachmentLoadOp::eDontCare)
                .setStencilStoreOp(vk::AttachmentStoreOp::eDontCare)
                .setInitialLayout(vk::ImageLayout::eUndefined)
                .setFinalLayout(vk::ImageLayout::ePresentSrcKHR);
            attachmentDescriptions.emplace_back(std::move(attachmentDescription));

            auto attachmentReference = vk::AttachmentReference().setAttachment(static_cast<uint32_t>(i)).setLayout(vk::ImageLayout::eColorAttachmentOptimal);
            attachmentReferences.emplace_back(std::move(attachmentReference));
        }

        const auto subpassDescription = vk::SubpassDescription()
            .setPipelineBindPoint(vk::PipelineBindPoint::eGraphics)
            .setInputAttachmentCount(0)
            .setPInputAttachments(nullptr)
            .setColorAttachmentCount(static_cast<uint32_t>(attachmentReferences.size()))
            .setPColorAttachments(attachmentReferences.data())
            .setPResolveAttachments(nullptr)
            .setPDepthStencilAttachment(nullptr)
            .setPreserveAttachmentCount(0)
            .setPPreserveAttachments(nullptr);

        const auto renderPassCreateInfo = vk::RenderPassCreateInfo()
            .setAttachmentCount(static_cast<uint32_t>(attachmentDescriptions.size()))
            .setPAttachments(attachmentDescriptions.data())
            .setSubpassCount(1)
            .setPSubpasses(&subpassDescription)
            .setDependencyCount(0)
            .setPDependencies(nullptr);
        return device.createRenderPassUnique(renderPassCreateInfo);
    }

    vk::UniqueFramebuffer QueueImpl::createFramebuffer(vk::RenderPass renderpass, const std::vector<vk::ImageView>& imageViews) {
        const auto framebufferCreateInfo = vk::FramebufferCreateInfo()
            .setRenderPass(renderpass)
            .setAttachmentCount(static_cast<uint32_t>(imageViews.size()))
            .setPAttachments(imageViews.data())
            .setWidth(size.width)
            .setHeight(size.height)
            .setLayers(1);
        return device.createFramebufferUnique(framebufferCreateInfo);
    }

    std::unique_ptr<RenderPass> QueueImpl::createRenderPass(const std::vector<std::reference_wrapper<Texture>>* textures) {
        std::vector<vk::ImageView> imageViews;
        std::vector<vk::ClearValue> clearValues;
        std::vector<vk::Format> formats;
        std::vector<std::reference_wrapper<TextureImpl>> destinationTextures;
        if (textures == nullptr) {
            imageViews.emplace_back(*deviceImageViews[activeSwapchainIndex]);
            clearValues.emplace_back(vk::ClearColorValue(std::array<float, 4>({ 0.0f, 0.0f, 0.0f, 1.0f })));
            formats.push_back(swapchainFormat);
        }
        else {
            for (auto& textureWrapper : *textures) {
                Texture& texture = textureWrapper;
                auto downcast = dynamic_cast<TextureImpl*>(&texture);
                assert(downcast != nullptr);
                TextureImpl& downcastedTexture = *downcast;
                destinationTextures.emplace_back(downcastedTexture);
                imageViews.emplace_back(downcastedTexture.getImageView());
                clearValues.emplace_back(vk::ClearColorValue(std::array<float, 4>({ 0.0f, 0.0f, 0.0f, 1.0f })));
                formats.emplace_back(downcastedTexture.getFormat());
            }
        }

        const auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo().setCommandPool(commandPool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1);
        auto commandBuffers = device.allocateCommandBuffersUnique(commandBufferAllocateInfo);
        auto& commandBuffer = commandBuffers[0];
        const auto commandBufferBeginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer->begin(commandBufferBeginInfo);
        auto renderPass = createSpecificRenderPass(imageViews, formats);
        auto framebuffer = createFramebuffer(*renderPass, imageViews);
        const auto renderPassBeginInfo = vk::RenderPassBeginInfo()
            .setRenderPass(*renderPass)
            .setFramebuffer(*framebuffer)
            .setRenderArea(vk::Rect2D(vk::Offset2D(0, 0), size))
            .setClearValueCount(static_cast<uint32_t>(clearValues.size()))
            .setPClearValues(clearValues.data());
        commandBuffer->beginRenderPass(renderPassBeginInfo, vk::SubpassContents::eInline);
        return std::unique_ptr<RenderPass>(new RenderPassImpl(device, std::move(commandBuffer), std::move(framebuffer), std::move(renderPass), destinationTextures));
    }

    void QueueImpl::commitRenderPass(std::unique_ptr<RenderPass>&& renderPass) {
        auto* downcast = dynamic_cast<PassImpl*>(renderPass.get());
        assert(downcast != nullptr);
        renderPass.release();
        std::unique_ptr<PassImpl> pass(downcast);

        commitPass(*pass);

        activePasses[activeSemaphoreIndex].emplace_back(std::move(pass));
    }

    std::unique_ptr<ComputePass> QueueImpl::createComputePass() {
        const auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo().setCommandPool(commandPool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1);
        auto commandBuffers = device.allocateCommandBuffersUnique(commandBufferAllocateInfo);
        auto& commandBuffer = commandBuffers[0];
        const auto commandBufferBeginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer->begin(commandBufferBeginInfo);
        return std::unique_ptr<ComputePass>(new ComputePassImpl(device, std::move(commandBuffer)));
    }

    void QueueImpl::commitComputePass(std::unique_ptr<ComputePass>&& computePass) {
        auto* downcast = dynamic_cast<PassImpl*>(computePass.get());
        assert(downcast != nullptr);
        computePass.release();
        std::unique_ptr<PassImpl> pass(downcast);

        commitPass(*pass);

        activePasses[activeSemaphoreIndex].emplace_back(std::move(pass));
    }

    std::unique_ptr<BlitPass> QueueImpl::createBlitPass() {
        const auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo().setCommandPool(commandPool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1);
        auto commandBuffers = device.allocateCommandBuffersUnique(commandBufferAllocateInfo);
        auto& commandBuffer = commandBuffers[0];
        const auto commandBufferBeginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer->begin(commandBufferBeginInfo);
        return std::unique_ptr<BlitPass>(new BlitPassImpl(device, std::move(commandBuffer)));
    }

    void QueueImpl::commitBlitPass(std::unique_ptr<BlitPass>&& blitPass) {
        auto* downcast = dynamic_cast<PassImpl*>(blitPass.get());
        assert(downcast != nullptr);
        blitPass.release();
        std::unique_ptr<PassImpl> pass(downcast);

        commitPass(*pass);

        activePasses[activeSemaphoreIndex].emplace_back(std::move(pass));
    }

    std::unique_ptr<HostAccessPass> QueueImpl::createHostAccessPass() {
        const auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo().setCommandPool(commandPool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1);
        auto commandBuffers = device.allocateCommandBuffersUnique(commandBufferAllocateInfo);
        auto& commandBuffer = commandBuffers[0];
        const auto commandBufferBeginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer->begin(commandBufferBeginInfo);
        return std::unique_ptr<HostAccessPass>(new HostAccessPassImpl(device, std::move(commandBuffer)));
    }

    void QueueImpl::commitHostAccessPass(std::unique_ptr<HostAccessPass>&& hostAccessPass) {
        {
            auto* downcast = dynamic_cast<HostAccessPassImpl*>(hostAccessPass.get());
            assert(downcast != nullptr);
            auto& pass = *downcast;
            // FIXME: Asking the GPU to wait on the CPU causes my (NVidia) driver to crash inside vkQueuePresentKHR().
            // However, I don't think this is a structural problem; we could just as easily have created a new resource
            // and used the transfer queue to copy the contents.
            // pass.getCommandBuffer().waitEvents({ pass.getFinishedEvent() }, vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, {}, {}, {});
        }

        auto* downcast = dynamic_cast<PassImpl*>(hostAccessPass.get());
        assert(downcast != nullptr);
        hostAccessPass.release();
        std::unique_ptr<PassImpl> pass(downcast);

        hostAccessIndices[activeSemaphoreIndex].push_back(activePasses[activeSemaphoreIndex].size());

        commitPass(*pass);

        activePasses[activeSemaphoreIndex].emplace_back(std::move(pass));
    }

    void QueueImpl::present() {
        vk::Semaphore waitSemaphore = *swapchainBufferSemaphores[activeSemaphoreIndex].back();
        vk::Result result;
        const auto presentInfo = vk::PresentInfoKHR()
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&waitSemaphore)
            .setSwapchainCount(1)
            .setPSwapchains(&swapchain)
            .setPImageIndices(&activeSwapchainIndex)
            .setPResults(&result);
        auto result2 = queue.presentKHR(presentInfo);
        assert(result == vk::Result::eSuccess);
        assert(result2 == vk::Result::eSuccess);

        activeSemaphoreIndex = (activeSemaphoreIndex + 1) % deviceImageViews.size();

        assert(swapchainBufferFences[activeSemaphoreIndex].size() == activePasses[activeSemaphoreIndex].size());
        std::vector<vk::Fence> fences;
        std::size_t index = 0;
        for (std::size_t hostAccessIndex : hostAccessIndices[activeSemaphoreIndex]) {
            for (; index < hostAccessIndex; ++index)
                fences.push_back(*swapchainBufferFences[activeSemaphoreIndex][index]);
            result = device.waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
            device.resetFences(fences);
            assert(result == vk::Result::eSuccess);
            fences.clear();

            auto* downcast = dynamic_cast<HostAccessPassImpl*>(activePasses[activeSemaphoreIndex][index].get());
            assert(downcast);
            auto& hostAccessPass = *downcast;
            hostAccessPass.execute();
            device.setEvent(hostAccessPass.getFinishedEvent());
        }
        for (; index < swapchainBufferFences[activeSemaphoreIndex].size(); ++index)
            fences.push_back(*swapchainBufferFences[activeSemaphoreIndex][index]);
        result = device.waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
        device.resetFences(fences);
        assert(result == vk::Result::eSuccess);

        prepareCurrentFrame();
    }
   
    void QueueImpl::commitPass(PassImpl& pass) {
        pass.getCommandBuffer().end();
        const vk::PipelineStageFlags waitStageMask[] = { vk::PipelineStageFlagBits::eTopOfPipe };
        vk::Semaphore waitSemaphore = *swapchainBufferSemaphores[activeSemaphoreIndex].back();
        swapchainBufferSemaphores[activeSemaphoreIndex].emplace_back(device.createSemaphoreUnique(vk::SemaphoreCreateInfo()));
        vk::Semaphore signalSemaphore = *swapchainBufferSemaphores[activeSemaphoreIndex].back();
        swapchainBufferFences[activeSemaphoreIndex].push_back(device.createFenceUnique(vk::FenceCreateInfo()));
        vk::Fence signalFence = *swapchainBufferFences[activeSemaphoreIndex].back();

        // FIXME: We may want to defer this to Present time in order to coalesce the barriers better than just doing them lazily here.
        // FIXME: Compute-only workflows may never call present, which means we need some sort of flush() call. Or we can just say that
        // present() is the same thing as flush, and is smart enough to not actually present if it isn't hooked up to a display.
        auto fixup1 = synchronizeResources(pass);
        auto commandBuffer = pass.getCommandBuffer();
        // FIXME: This is done eagerly because the API may want to update the contents of a resource from the CPU.
        // Instead, we should schedule this update along with the queue so we can insert the correct barriers in front of it.
        auto fixup2 = synchronizeResources(pass);

        vk::CommandBuffer commandBuffers[] = { fixup1, commandBuffer, fixup2 };

        auto submitInfo = vk::SubmitInfo()
            .setWaitSemaphoreCount(1)
            .setPWaitSemaphores(&waitSemaphore)
            .setPWaitDstStageMask(waitStageMask)
            .setCommandBufferCount(3)
            .setPCommandBuffers(commandBuffers)
            .setSignalSemaphoreCount(1)
            .setPSignalSemaphores(&signalSemaphore);
        queue.submit({ submitInfo }, signalFence);
    }

    vk::CommandBuffer QueueImpl::synchronizeResources(PassImpl& pass) {
        const auto commandBufferAllocateInfo = vk::CommandBufferAllocateInfo().setCommandPool(commandPool).setLevel(vk::CommandBufferLevel::ePrimary).setCommandBufferCount(1);
        auto commandBuffers = device.allocateCommandBuffersUnique(commandBufferAllocateInfo);
        auto& commandBuffer = commandBuffers[0];
        const auto commandBufferBeginInfo = vk::CommandBufferBeginInfo().setFlags(vk::CommandBufferUsageFlagBits::eOneTimeSubmit);
        commandBuffer->begin(commandBufferBeginInfo);
        // FIXME: Issue smaller, cheaper, more specific barriers than these.
        std::vector<vk::BufferMemoryBarrier> bufferMemoryBarriers;
        pass.iterateBuffers([&](BufferImpl& buffer) {
            bufferMemoryBarriers.emplace_back(vk::BufferMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eIndexRead | vk::AccessFlagBits::eVertexAttributeRead | vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eHostRead | vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eIndirectCommandRead | vk::AccessFlagBits::eIndexRead | vk::AccessFlagBits::eVertexAttributeRead | vk::AccessFlagBits::eUniformRead | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eHostRead | vk::AccessFlagBits::eHostWrite)
                .setSrcQueueFamilyIndex(0)
                .setDstQueueFamilyIndex(0)
                .setBuffer(buffer.getBuffer())
                .setOffset(0)
                .setSize(VK_WHOLE_SIZE));
        });
        std::vector<vk::ImageMemoryBarrier> imageMemoryBarriers;
        pass.iterateTextures([&](TextureImpl& texture) {
            imageMemoryBarriers.emplace_back(vk::ImageMemoryBarrier()
                .setSrcAccessMask(vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eHostRead | vk::AccessFlagBits::eHostWrite)
                .setDstAccessMask(vk::AccessFlagBits::eInputAttachmentRead | vk::AccessFlagBits::eShaderRead | vk::AccessFlagBits::eShaderWrite | vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite | vk::AccessFlagBits::eHostRead | vk::AccessFlagBits::eHostWrite)
                .setOldLayout(texture.getTransferredToGPU() ? vk::ImageLayout::eGeneral : vk::ImageLayout::ePreinitialized)
                .setNewLayout(vk::ImageLayout::eGeneral)
                .setSrcQueueFamilyIndex(0)
                .setDstQueueFamilyIndex(0)
                .setImage(texture.getImage())
                .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
            texture.setTransferredToGPU(true);
        });
        commandBuffer->pipelineBarrier(vk::PipelineStageFlagBits::eAllCommands, vk::PipelineStageFlagBits::eAllCommands, vk::DependencyFlagBits(), {}, bufferMemoryBarriers, imageMemoryBarriers);
        commandBuffer->end();

        auto& frameCommandBuffers = fixupCommandBuffers[activeSemaphoreIndex];
        frameCommandBuffers.push_back(std::move(commandBuffer));
        return *frameCommandBuffers.back();
    }

    QueueImpl::~QueueImpl() {
        for (std::size_t i = 0; i < deviceImageViews.size(); ++i) {
            activeSemaphoreIndex = (activeSemaphoreIndex + 1) % deviceImageViews.size();

            assert(swapchainBufferFences[activeSemaphoreIndex].size() == activePasses[activeSemaphoreIndex].size());
            std::vector<vk::Fence> fences;
            for (std::size_t index = 0; index < swapchainBufferFences[activeSemaphoreIndex].size(); ++index)
                fences.push_back(*swapchainBufferFences[activeSemaphoreIndex][index]);
            auto result = device.waitForFences(fences, VK_TRUE, std::numeric_limits<uint64_t>::max());
            device.resetFences(fences);
            assert(result == vk::Result::eSuccess);

            swapchainBufferSemaphores[activeSemaphoreIndex].clear();
            swapchainBufferFences[activeSemaphoreIndex].clear();
            activePasses[activeSemaphoreIndex].clear();
            fixupCommandBuffers[activeSemaphoreIndex].clear();
            hostAccessIndices[activeSemaphoreIndex].clear();
        }
    }
}
