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
#include <memory>
#include <cstdint>
#include <string>
#include <vector>
#include <list>
#include <unordered_map>
#include <vulkan/vulkan.hpp>
#include "QueueImpl.h"
#include "BufferImpl.h"
#include "TextureImpl.h"
#include "SamplerImpl.h"
#include "RenderStateImpl.h"
#include "ComputeStateImpl.h"
#include <cassert>

namespace WebGPU {
    class DeviceImpl : public Device {
    public:
        DeviceImpl() = default;
        virtual ~DeviceImpl();
        DeviceImpl(const DeviceImpl&) = delete;
        DeviceImpl(DeviceImpl&&) = default;
        DeviceImpl& operator=(const DeviceImpl&) = delete;
        DeviceImpl& operator=(DeviceImpl&&) = default;

        DeviceImpl(HINSTANCE hInstance, HWND hWnd);

    private:
        class UniqueDebugReportCallbackEXT {
        public:
            UniqueDebugReportCallbackEXT() = default;
            UniqueDebugReportCallbackEXT(const UniqueDebugReportCallbackEXT&) = delete;
            UniqueDebugReportCallbackEXT(UniqueDebugReportCallbackEXT&& other) : debugReportCallback(std::move(other.debugReportCallback)), instance(std::move(other.instance)) {
                other.debugReportCallback = vk::DebugReportCallbackEXT();
            }
            UniqueDebugReportCallbackEXT& operator=(const UniqueDebugReportCallbackEXT&) = delete;
            UniqueDebugReportCallbackEXT& operator=(UniqueDebugReportCallbackEXT&& other) {
                destroy();
                debugReportCallback = std::move(other.debugReportCallback);
                instance = std::move(other.instance);
                other.debugReportCallback = vk::DebugReportCallbackEXT();
                return *this;
            }

            UniqueDebugReportCallbackEXT(const vk::Instance& instance, const vk::DebugReportCallbackCreateInfoEXT& info) : instance(instance) {
                PFN_vkCreateDebugReportCallbackEXT createDebugReportCallback = reinterpret_cast<PFN_vkCreateDebugReportCallbackEXT>(instance.getProcAddr("vkCreateDebugReportCallbackEXT"));
                assert(createDebugReportCallback != nullptr);
                const VkDebugReportCallbackCreateInfoEXT infoValue = info;
                VkDebugReportCallbackEXT debugReportCallback;
                auto result = createDebugReportCallback(instance, &infoValue, nullptr, &debugReportCallback);
                assert(result == VK_SUCCESS);
                this->debugReportCallback = debugReportCallback;
            }

            ~UniqueDebugReportCallbackEXT() {
                destroy();
            }

            void destroy() {
                if (debugReportCallback != vk::DebugReportCallbackEXT()) {
                    PFN_vkDestroyDebugReportCallbackEXT destroyDebugReportCallback = reinterpret_cast<PFN_vkDestroyDebugReportCallbackEXT>(instance.getProcAddr("vkDestroyDebugReportCallbackEXT"));
                    assert(destroyDebugReportCallback != nullptr);
                    destroyDebugReportCallback(instance, debugReportCallback, nullptr);
                }
            }
            vk::DebugReportCallbackEXT debugReportCallback;
            vk::Instance instance;
        };

        Queue& getCommandQueue() override;
        RenderState& getRenderState(const std::vector<uint8_t>& vertexShader, const std::string& vertexShaderName, const std::vector<uint8_t>& fragmentShader, const std::string& fragmentShaderName, const std::vector<unsigned int>& vertexAttributeBufferStrides, const std::vector<RenderState::VertexAttribute>& vertexAttributes, const std::vector<std::vector<ResourceType>>& resources, const std::vector<PixelFormat>* colorPixelFormats) override;
        ComputeState& getComputeState(const std::vector<uint8_t>& shader, const std::string& shaderName, const std::vector<std::vector<ResourceType>>& resources) override;
        BufferHolder getBuffer(unsigned int length) override;
        void returnBuffer(Buffer&) override;
        TextureHolder getTexture(unsigned int width, unsigned int height, PixelFormat format) override;
        void returnTexture(Texture&) override;
        SamplerHolder getSampler(AddressMode addressMode, Filter filter) override;
        void returnSampler(Sampler&) override;

        vk::UniqueShaderModule prepareShader(const std::vector<uint8_t>& shader);
        std::pair<vk::UniquePipelineLayout, std::vector<vk::UniqueDescriptorSetLayout>> createPipelineLayout(const std::vector<std::vector<ResourceType>>& resources);
        vk::UniqueRenderPass createCompatibleRenderPass(const std::vector<PixelFormat>* colorPixelFormats);

        vk::UniqueInstance instance;
        UniqueDebugReportCallbackEXT debugReportCallback;
        vk::PhysicalDevice gpu;
        vk::UniqueSurfaceKHR surface;
        vk::UniqueDevice device;
        vk::PhysicalDeviceMemoryProperties memoryProperties;
        vk::UniqueSwapchainKHR swapchain;
        std::vector<vk::Image> swapchainImages;
        std::vector<vk::UniqueImageView> swapchainImageViews;
        vk::UniquePipelineCache pipelineCache;
        std::unique_ptr<QueueImpl> queue;
        vk::UniqueCommandPool commandPool;
        std::list<RenderStateImpl> renderStates;
        std::list<ComputeStateImpl> computeStates;

        template<typename T>
        struct ResourcePool {
            std::list<T> inUse;
            std::list<T> freeList;
        };

        std::unordered_map<unsigned int, ResourcePool<BufferImpl>> bufferCache;

        struct TextureParameters {
            bool operator==(const TextureParameters& other) const {
                return width == other.width && height == other.height && format == other.format;
            }

            unsigned int width;
            unsigned int height;
            PixelFormat format;
        };
        struct TextureParametersHash {
            std::size_t operator()(const TextureParameters& textureParameters) const {
                std::hash<unsigned int> uintHash;
                return uintHash(textureParameters.width) | uintHash(textureParameters.height) | uintHash(static_cast<unsigned int>(textureParameters.format));
            }
        };
        std::unordered_map<TextureParameters, ResourcePool<TextureImpl>, TextureParametersHash> textureCache;

        std::unordered_map<uint32_t, ResourcePool<SamplerImpl>> samplerCache;
    };
}
