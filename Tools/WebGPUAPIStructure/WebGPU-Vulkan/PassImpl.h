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
#include "BufferImpl.h"
#include "TextureImpl.h"
#include "SamplerImpl.h"
#include <unordered_set>
#include <vulkan/vulkan.hpp>

namespace WebGPU {
    class PassImpl {
    public:
        PassImpl() = default;
        virtual ~PassImpl() = default;
        PassImpl(const PassImpl&) = delete;
        PassImpl(PassImpl&&) = default;
        PassImpl& operator=(const PassImpl&) = delete;
        PassImpl& operator=(PassImpl&&) = default;

        PassImpl(vk::Device device, vk::UniqueCommandBuffer&& commandBuffer);

        vk::CommandBuffer getCommandBuffer() const { return *commandBuffer; }

        template<typename T>
        void iterateBuffers(T t) {
            for (const auto& buffer : buffers)
                t(buffer.get());
        }

        template<typename T>
        void iterateTextures(T t) {
            for (const auto& texture : textures)
                t(texture.get());
        }

    protected:
        void setResources(vk::PipelineBindPoint pipelineBindPoint, unsigned int index, const std::vector<WebGPU::ResourceReference>&);
        void insertBuffer(BufferImpl& buffer);
        void insertTexture(TextureImpl& texture);
        void insertSampler(SamplerImpl& sampler);

        template<typename T>
        struct ResourceReference {
            ResourceReference() = delete;
            ResourceReference(T& resource) : resource(&resource) {
                resource.incrementReferenceCount();
            }
            ~ResourceReference() {
                if (resource != nullptr)
                    resource->decrementReferenceCount();
            }
            ResourceReference(const ResourceReference&) = delete;
            ResourceReference(ResourceReference&& resourceReference) : resource(&resourceReference.release()) {
            }
            ResourceReference& operator=(const ResourceReference&) = delete;
            ResourceReference& operator=(ResourceReference&& resourceReference) {
                if (resource != nullptr)
                    resource->decrementReferenceCount();
                resource = &resourceReference.release();
            }

            bool operator==(const ResourceReference<T>& other) const {
                return resource == other.resource;
            }

            T& get() const {
                return *resource;
            }

            T& release() {
                T& result = *resource;
                resource = nullptr;
                return result;
            }

            T* resource;
        };

        template<typename T>
        struct ResourceReferenceHash {
            std::size_t operator()(const ResourceReference<T>& resourceReference) const {
                std::hash<T*> hash;
                return hash(&resourceReference.get());
            }
        };

        vk::Device device;
        vk::PipelineLayout currentPipelineLayout;
        vk::UniqueCommandBuffer commandBuffer;
        std::vector<vk::UniqueDescriptorPool> descriptorPools;
        std::vector<vk::UniqueDescriptorSetLayout> descriptorSetLayouts;
        std::vector<vk::UniqueDescriptorSet> descriptorSets;
        std::unordered_set<ResourceReference<BufferImpl>, ResourceReferenceHash<BufferImpl>> buffers;
        std::unordered_set<ResourceReference<TextureImpl>, ResourceReferenceHash<TextureImpl>> textures;
        std::unordered_set<ResourceReference<SamplerImpl>, ResourceReferenceHash<SamplerImpl>> samplers;
    };
}
