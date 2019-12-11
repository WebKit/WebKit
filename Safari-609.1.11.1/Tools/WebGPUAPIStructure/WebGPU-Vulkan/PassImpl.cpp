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

#include "PassImpl.h"

namespace WebGPU {
    PassImpl::PassImpl(vk::Device device, vk::UniqueCommandBuffer&& commandBuffer) : device(device), commandBuffer(std::move(commandBuffer)) {
    }

    // Keep this in sync with DeviceImpl::createPipelineLayout().
    class ResourceVisitor : public boost::static_visitor<void> {
    public:
        void operator()(const TextureReference& texture) {
            auto* downcast = dynamic_cast<TextureImpl*>(&texture.get());
            assert(downcast != nullptr);
            auto& tex = *downcast;
            bindings.emplace_back(vk::DescriptorSetLayoutBinding().setBinding(count).setDescriptorType(vk::DescriptorType::eSampledImage).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll));
            descriptorImageInfos.emplace_back(vk::DescriptorImageInfo().setImageView(tex.getImageView()).setImageLayout(vk::ImageLayout::eGeneral));
            writeDescriptorSets.emplace_back(vk::WriteDescriptorSet().setDstBinding(count).setDstArrayElement(0).setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eSampledImage));
            textures.push_back(tex);
            ++count;
            ++imageCount;
        }

        void operator()(const SamplerReference& samplerObject) {
            auto* downcast = dynamic_cast<SamplerImpl*>(&samplerObject.get());
            assert(downcast != nullptr);
            auto& sampler = *downcast;
            bindings.emplace_back(vk::DescriptorSetLayoutBinding().setBinding(count).setDescriptorType(vk::DescriptorType::eSampler).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll));
            descriptorImageInfos.emplace_back(vk::DescriptorImageInfo().setSampler(sampler.getSampler()));
            writeDescriptorSets.emplace_back(vk::WriteDescriptorSet().setDstBinding(count).setDstArrayElement(0).setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eSampler));
            samplers.push_back(sampler);
            ++count;
            ++samplerCount;
        }

        void operator()(const UniformBufferObjectReference& uniformBufferObject) {
            auto* downcast = dynamic_cast<BufferImpl*>(&uniformBufferObject.get());
            assert(downcast != nullptr);
            auto& ubo = *downcast;
            bindings.emplace_back(vk::DescriptorSetLayoutBinding().setBinding(count).setDescriptorType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll));
            descriptorBufferInfos.emplace_back(vk::DescriptorBufferInfo().setBuffer(ubo.getBuffer()).setRange(VK_WHOLE_SIZE));
            writeDescriptorSets.emplace_back(vk::WriteDescriptorSet().setDstBinding(count).setDstArrayElement(0).setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eUniformBuffer));
            buffers.push_back(ubo);
            ++count;
            ++uniformBufferCount;
        }

        void operator()(const ShaderStorageBufferObjectReference& shaderStorageBufferObject) {
            auto* downcast = dynamic_cast<BufferImpl*>(&shaderStorageBufferObject.get());
            assert(downcast != nullptr);
            auto& ssbo = *downcast;
            bindings.emplace_back(vk::DescriptorSetLayoutBinding().setBinding(count).setDescriptorType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll));
            descriptorBufferInfos.emplace_back(vk::DescriptorBufferInfo().setBuffer(ssbo.getBuffer()).setOffset(0).setRange(VK_WHOLE_SIZE));
            writeDescriptorSets.emplace_back(vk::WriteDescriptorSet().setDstBinding(count).setDstArrayElement(0).setDescriptorCount(1).setDescriptorType(vk::DescriptorType::eStorageBuffer));
            buffers.push_back(ssbo);
            ++count;
            ++storageBufferCount;
        }

        const std::vector<vk::DescriptorSetLayoutBinding>& getBindings() const { return bindings; }
        std::vector<vk::WriteDescriptorSet>&& releaseWriteDescriptorSets() { return std::move(writeDescriptorSets); }
        const std::vector<vk::DescriptorImageInfo>& getDescriptorImageInfos() const { return descriptorImageInfos; }
        const std::vector<vk::DescriptorBufferInfo>& getDescriptorBufferInfos() const { return descriptorBufferInfos; }
        const std::vector<std::reference_wrapper<BufferImpl>>& getBuffers() const { return buffers; }
        const std::vector<std::reference_wrapper<TextureImpl>>& getTextures() const { return textures; }
        const std::vector<std::reference_wrapper<SamplerImpl>>& getSamplers() const { return samplers; }
        uint32_t getImageCount() const { return imageCount; }
        uint32_t getSamplerCount() const { return samplerCount; }
        uint32_t getUniformBufferCount() const { return uniformBufferCount; }
        uint32_t getStorageBufferCount() const { return storageBufferCount; }

    private:
        std::vector<vk::DescriptorSetLayoutBinding> bindings;
        std::vector<vk::WriteDescriptorSet> writeDescriptorSets;
        std::vector<vk::DescriptorImageInfo> descriptorImageInfos;
        std::vector<vk::DescriptorBufferInfo> descriptorBufferInfos;
        std::vector<std::reference_wrapper<BufferImpl>> buffers;
        std::vector<std::reference_wrapper<TextureImpl>> textures;
        std::vector<std::reference_wrapper<SamplerImpl>> samplers;
        uint32_t count{ 0 };
        uint32_t imageCount{ 0 };
        uint32_t samplerCount{ 0 };
        uint32_t uniformBufferCount{ 0 };
        uint32_t storageBufferCount{ 0 };
    };

    void PassImpl::setResources(vk::PipelineBindPoint pipelineBindPoint, unsigned int index, const std::vector<WebGPU::ResourceReference>& resourceReferences) {
        ResourceVisitor resourceVisitor;
        for (const auto& resourceReference : resourceReferences)
            boost::apply_visitor(resourceVisitor, resourceReference);
        vk::DescriptorPoolSize const poolSizes[] = {
            vk::DescriptorPoolSize().setType(vk::DescriptorType::eSampledImage).setDescriptorCount(resourceVisitor.getImageCount()),
            vk::DescriptorPoolSize().setType(vk::DescriptorType::eSampler).setDescriptorCount(resourceVisitor.getSamplerCount()),
            vk::DescriptorPoolSize().setType(vk::DescriptorType::eUniformBuffer).setDescriptorCount(resourceVisitor.getUniformBufferCount()),
            vk::DescriptorPoolSize().setType(vk::DescriptorType::eStorageBuffer).setDescriptorCount(resourceVisitor.getStorageBufferCount()) };
        const auto descriptorPoolCreateInfo = vk::DescriptorPoolCreateInfo().setMaxSets(1).setPoolSizeCount(sizeof(poolSizes) / sizeof(poolSizes[0])).setPPoolSizes(poolSizes);
        descriptorPools.emplace_back(device.createDescriptorPoolUnique(descriptorPoolCreateInfo));
        auto descriptorPool = *descriptorPools.back();

        const auto& bindings = resourceVisitor.getBindings();
        const auto descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo().setBindingCount(static_cast<uint32_t>(bindings.size())).setPBindings(bindings.data());
        descriptorSetLayouts.emplace_back(device.createDescriptorSetLayoutUnique(descriptorSetLayoutCreateInfo));
        auto descriptorSetLayout = *descriptorSetLayouts.back();
        // FIXME: Does the argument to setPSetLayouts below need to be pointer-equal to one of the descriptor sets used to create the Vulkan GraphicsPipeline?
        // FIXME: Validate that index |index| of the graphics pipeline's descriptor layout matches the specified descriptors.
        const auto descriptorSetAllocateInfo = vk::DescriptorSetAllocateInfo().setDescriptorPool(descriptorPool).setDescriptorSetCount(1).setPSetLayouts(&descriptorSetLayout);
        auto descriptorSets = device.allocateDescriptorSetsUnique(descriptorSetAllocateInfo);
        this->descriptorSets.emplace_back(std::move(descriptorSets[0]));
        auto descriptorSet = *this->descriptorSets.back();
        std::vector<vk::WriteDescriptorSet> writeDescriptorSets = resourceVisitor.releaseWriteDescriptorSets();
        std::size_t imageIndex = 0;
        std::size_t bufferIndex = 0;
        for (auto& writeDescriptorSet : writeDescriptorSets) {
            switch (writeDescriptorSet.descriptorType) {
            case vk::DescriptorType::eSampledImage:
                writeDescriptorSet.setPImageInfo(&resourceVisitor.getDescriptorImageInfos()[imageIndex++]);
                break;
            case vk::DescriptorType::eSampler:
                writeDescriptorSet.setPImageInfo(&resourceVisitor.getDescriptorImageInfos()[imageIndex++]);
                break;
            case vk::DescriptorType::eUniformBuffer:
                writeDescriptorSet.setPBufferInfo(&resourceVisitor.getDescriptorBufferInfos()[bufferIndex++]);
                break;
            case vk::DescriptorType::eStorageBuffer:
                writeDescriptorSet.setPBufferInfo(&resourceVisitor.getDescriptorBufferInfos()[bufferIndex++]);
                break;
            }
            writeDescriptorSet.setDstSet(descriptorSet);
        }
        device.updateDescriptorSets(writeDescriptorSets, {});
        assert(currentPipelineLayout != vk::PipelineLayout());
        commandBuffer->bindDescriptorSets(pipelineBindPoint, currentPipelineLayout, index, { descriptorSet }, {});

        for (auto& buffer : resourceVisitor.getBuffers())
            insertBuffer(buffer);
        for (auto& texture : resourceVisitor.getTextures())
            insertTexture(texture);
        for (auto& sampler : resourceVisitor.getSamplers())
            insertSampler(sampler);
    }

    void PassImpl::insertBuffer(BufferImpl& buffer) {
        ResourceReference<BufferImpl> reference(buffer);
        if (buffers.find(reference) == buffers.end())
            buffers.insert(std::move(reference));
    }

    void PassImpl::insertTexture(TextureImpl& texture) {
        ResourceReference<TextureImpl> reference(texture);
        if (textures.find(reference) == textures.end())
            textures.insert(std::move(reference));
    }

    void PassImpl::insertSampler(SamplerImpl& sampler) {
        ResourceReference<SamplerImpl> reference(sampler);
        if (samplers.find(reference) == samplers.end())
            samplers.insert(std::move(reference));
    }
}
