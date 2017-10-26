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

#include "DeviceImpl.h"
#include "QueueImpl.h"
#include "RenderStateImpl.h"
#include "ComputeStateImpl.h"
#include "QueueImpl.h"
#include <cassert>
#include <limits>
#include <sstream>

namespace WebGPU {
    static const vk::Format framebufferColorPixelFormat = vk::Format::eR8G8B8A8Srgb;

    std::unique_ptr<Device> Device::create(HINSTANCE hInstance, HWND hWnd) {
        return std::unique_ptr<Device>(new DeviceImpl(hInstance, hWnd));
    }

    static vk::Format convertPixelFormat(PixelFormat pixelFormat) {
        switch (pixelFormat) {
        case PixelFormat::RGBA8sRGB:
            return vk::Format::eR8G8B8A8Srgb;
        case PixelFormat::RGBA8:
            return vk::Format::eR8G8B8A8Unorm;
        default:
            assert(false);
            return vk::Format::eR8G8B8A8Unorm;
        }
    }

    static PixelFormat convertFormat(vk::Format format) {
        switch (format) {
        case vk::Format::eR8G8B8A8Srgb:
            return PixelFormat::RGBA8sRGB;
        case vk::Format::eR8G8B8A8Unorm:
            return PixelFormat::RGBA8;
        default:
            assert(false);
            return PixelFormat::RGBA8;
        }
    }

    static VkBool32 debugReport(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char* pLayerPrefix, const char* pMessage, void* pUserData) {
        vk::DebugReportFlagsEXT flagsObject = static_cast<vk::DebugReportFlagBitsEXT>(flags);
        vk::DebugReportObjectTypeEXT objectTypeObject = static_cast<vk::DebugReportObjectTypeEXT>(objectType);
        std::ostringstream ss;
        ss << "Vulkan Log: " << vk::to_string(flagsObject) << " " << vk::to_string(objectTypeObject) << " " << object << " " << location << " " << messageCode << " " << std::string(pLayerPrefix) << " " << std::string(pMessage) << std::endl;
        OutputDebugStringA(ss.str().c_str());
        return VK_FALSE;
    }

    DeviceImpl::DeviceImpl(HINSTANCE hInstance, HWND hWnd) {
        auto layerProperties = vk::enumerateInstanceLayerProperties();
        auto const app = vk::ApplicationInfo()
            .setPApplicationName("WebGPU")
            .setApplicationVersion(0)
            .setPEngineName("WebGPU")
            .setEngineVersion(0)
            .setApiVersion(VK_API_VERSION_1_0);
        const char* const instanceValidationLayers[] = { "VK_LAYER_LUNARG_standard_validation", "VK_LAYER_GOOGLE_threading", "VK_LAYER_LUNARG_parameter_validation", "VK_LAYER_LUNARG_object_tracker", "VK_LAYER_LUNARG_core_validation", "VK_LAYER_GOOGLE_unique_objects" };
        const char* const instanceExtensionNames[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME, VK_EXT_DEBUG_REPORT_EXTENSION_NAME };
        const auto debugReportCallbackCreateInfo = vk::DebugReportCallbackCreateInfoEXT()
            .setFlags(vk::DebugReportFlagBitsEXT::eInformation | vk::DebugReportFlagBitsEXT::eWarning | vk::DebugReportFlagBitsEXT::ePerformanceWarning | vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eDebug)
            .setPfnCallback(&debugReport);
        const auto instanceInfo = vk::InstanceCreateInfo()
            .setPNext(&debugReportCallbackCreateInfo)
            .setPApplicationInfo(&app)
            .setEnabledLayerCount(1)
            .setPpEnabledLayerNames(instanceValidationLayers)
            .setEnabledExtensionCount(3)
            .setPpEnabledExtensionNames(instanceExtensionNames);
        instance = vk::createInstanceUnique(instanceInfo);

        debugReportCallback = UniqueDebugReportCallbackEXT(*instance, debugReportCallbackCreateInfo);

        auto devices = instance->enumeratePhysicalDevices();
        gpu = devices[0];

        auto const surfaceCreateInfo = vk::Win32SurfaceCreateInfoKHR().setHinstance(hInstance).setHwnd(hWnd);
        surface = instance->createWin32SurfaceKHRUnique(surfaceCreateInfo);

        auto surfaceCapabilities = gpu.getSurfaceCapabilitiesKHR(*surface);

        auto queueFamilyProperties = gpu.getQueueFamilyProperties();

        bool foundQueueIndex = false;
        uint32_t queueFamilyIndex;
        for (uint32_t i = 0; i < queueFamilyProperties.size(); ++i) {
            bool supportsPresent = gpu.getSurfaceSupportKHR(i, *surface);
            auto& queueFamilyProperty = queueFamilyProperties[i];
            if (supportsPresent && (queueFamilyProperty.queueFlags & vk::QueueFlagBits::eGraphics) && (queueFamilyProperty.queueFlags & vk::QueueFlagBits::eCompute) && (queueFamilyProperty.queueFlags & vk::QueueFlagBits::eTransfer)) {
                queueFamilyIndex = i;
                foundQueueIndex = true;
                break;
            }
        }
        assert(foundQueueIndex);

        float priority = 0.0;
        vk::DeviceQueueCreateInfo queueCreateInfo;
        queueCreateInfo.setQueueFamilyIndex(queueFamilyIndex);
        queueCreateInfo.setQueueCount(1);
        queueCreateInfo.setPQueuePriorities(&priority);
        char const *deviceExtensionNames[1] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
        auto deviceCreateInfo = vk::DeviceCreateInfo()
            .setQueueCreateInfoCount(1)
            .setPQueueCreateInfos(&queueCreateInfo)
            .setEnabledLayerCount(0)
            .setPpEnabledLayerNames(nullptr)
            .setEnabledExtensionCount(1)
            .setPpEnabledExtensionNames(deviceExtensionNames)
            .setPEnabledFeatures(nullptr);
        device = gpu.createDeviceUnique(deviceCreateInfo);

        const auto commandPoolCreateInfo = vk::CommandPoolCreateInfo().setQueueFamilyIndex(queueFamilyIndex);
        commandPool = device->createCommandPoolUnique(commandPoolCreateInfo);

        memoryProperties = gpu.getMemoryProperties();

        const auto swapchainCreateInfo = vk::SwapchainCreateInfoKHR()
            .setSurface(*surface)
            .setMinImageCount(3)
            .setImageFormat(framebufferColorPixelFormat)
            .setImageColorSpace(vk::ColorSpaceKHR::eSrgbNonlinear)
            .setImageExtent({ surfaceCapabilities.currentExtent.width, surfaceCapabilities.currentExtent.height })
            .setImageArrayLayers(1)
            .setImageUsage(vk::ImageUsageFlagBits::eColorAttachment)
            .setImageSharingMode(vk::SharingMode::eExclusive)
            .setQueueFamilyIndexCount(0)
            .setPQueueFamilyIndices(nullptr)
            .setPreTransform(vk::SurfaceTransformFlagBitsKHR::eIdentity)
            .setCompositeAlpha(vk::CompositeAlphaFlagBitsKHR::eOpaque)
            .setPresentMode(vk::PresentModeKHR::eFifo)
            .setClipped(true);
        swapchain = device->createSwapchainKHRUnique(swapchainCreateInfo);

        swapchainImages = device->getSwapchainImagesKHR(*swapchain);
        for (auto& swapchainImage : swapchainImages) {
            auto imageViewCreateInfo = vk::ImageViewCreateInfo()
                .setViewType(vk::ImageViewType::e2D)
                .setFormat(framebufferColorPixelFormat)
                .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1))
                .setImage(swapchainImage);
            swapchainImageViews.push_back(device->createImageViewUnique(imageViewCreateInfo));
        }

        queue = std::unique_ptr<QueueImpl>(new QueueImpl(*device, *commandPool, swapchainImageViews, *swapchain, surfaceCapabilities.currentExtent, framebufferColorPixelFormat, device->getQueue(queueFamilyIndex, 0)));

        vk::PipelineCacheCreateInfo pipelineCacheCreateInfo;
        pipelineCache = device->createPipelineCacheUnique(pipelineCacheCreateInfo);
    }

    Queue& DeviceImpl::getCommandQueue() {
        return *queue;
    }

    vk::UniqueShaderModule DeviceImpl::prepareShader(const std::vector<uint8_t>& shader) {
        auto const moduleCreateInfo = vk::ShaderModuleCreateInfo().setCodeSize(shader.size()).setPCode(reinterpret_cast<const uint32_t*>(shader.data()));
        return device->createShaderModuleUnique(moduleCreateInfo);
    }

    std::pair<vk::UniquePipelineLayout, std::vector<vk::UniqueDescriptorSetLayout>> DeviceImpl::createPipelineLayout(const std::vector<std::vector<ResourceType>>& resources) {
        std::vector<vk::UniqueDescriptorSetLayout> descriptorSetLayouts;
        for (auto& resourceSet : resources) {
            std::vector<vk::DescriptorSetLayoutBinding> bindings;
            for (std::size_t i = 0; i < resourceSet.size(); ++i) {
                vk::DescriptorType descriptorType;
                switch (resourceSet[i]) {
                case ResourceType::Texture:
                    descriptorType = vk::DescriptorType::eSampledImage;
                    break;
                case ResourceType::Sampler:
                    descriptorType = vk::DescriptorType::eSampler;
                    break;
                case ResourceType::UniformBufferObject:
                    descriptorType = vk::DescriptorType::eUniformBuffer;
                    break;
                case ResourceType::ShaderStorageBufferObject:
                    descriptorType = vk::DescriptorType::eStorageBuffer;
                    break;
                }
                // Keep this in sync with RenderPassImpl::setResources().
                bindings.emplace_back(vk::DescriptorSetLayoutBinding().setBinding(static_cast<uint32_t>(i)).setDescriptorType(descriptorType).setDescriptorCount(1).setStageFlags(vk::ShaderStageFlagBits::eAll));
            }
            const auto descriptorSetLayoutCreateInfo = vk::DescriptorSetLayoutCreateInfo().setBindingCount(static_cast<uint32_t>(resourceSet.size())).setPBindings(bindings.data());
            descriptorSetLayouts.emplace_back(device->createDescriptorSetLayoutUnique(descriptorSetLayoutCreateInfo));
        }
        std::vector<vk::DescriptorSetLayout> weakDescriptorSetLayouts;
        for (auto& descriptorSetLayout : descriptorSetLayouts)
            weakDescriptorSetLayouts.push_back(*descriptorSetLayout);
        const auto pipelineLayoutCreateInfo = vk::PipelineLayoutCreateInfo().setSetLayoutCount(static_cast<uint32_t>(weakDescriptorSetLayouts.size())).setPSetLayouts(weakDescriptorSetLayouts.data());
        auto pipelineLayout = device->createPipelineLayoutUnique(pipelineLayoutCreateInfo);
        return std::make_pair(std::move(pipelineLayout), std::move(descriptorSetLayouts));
    }

    vk::UniqueRenderPass DeviceImpl::createCompatibleRenderPass(const std::vector<PixelFormat>* colorPixelFormats) {
        std::vector<vk::AttachmentDescription> attachmentDescriptions;
        std::vector<vk::AttachmentReference> attachmentReferences;

        if (colorPixelFormats == nullptr) {
            attachmentDescriptions.emplace_back(vk::AttachmentDescription().setFormat(framebufferColorPixelFormat).setSamples(vk::SampleCountFlagBits::e1));
            attachmentReferences.emplace_back(vk::AttachmentReference().setAttachment(0));
        }
        else {
            for (std::size_t i = 0; i < colorPixelFormats->size(); ++i) {
                vk::Format format = convertPixelFormat((*colorPixelFormats)[i]);
                attachmentDescriptions.emplace_back(vk::AttachmentDescription().setFormat(format).setSamples(vk::SampleCountFlagBits::e1));
                attachmentReferences.emplace_back(vk::AttachmentReference().setAttachment(static_cast<uint32_t>(i)));
            }
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
        return device->createRenderPassUnique(renderPassCreateInfo);
    }

    static vk::Format convertVertexFormat(RenderState::VertexFormat vertexFormat) {
        switch (vertexFormat) {
        case RenderState::VertexFormat::Float:
            return vk::Format::eR32Sfloat;
        case RenderState::VertexFormat::Float2:
            return vk::Format::eR32G32Sfloat;
        case RenderState::VertexFormat::Float3:
            return vk::Format::eR32G32B32Sfloat;
        case RenderState::VertexFormat::Float4:
            return vk::Format::eR32G32B32A32Sfloat;
        default:
            assert(false);
            return vk::Format::eR32Sfloat;
        }
    }

    RenderState& DeviceImpl::getRenderState(const std::vector<uint8_t>& vertexShader, const std::string& vertexShaderName, const std::vector<uint8_t>& fragmentShader, const std::string& fragmentShaderName, const std::vector<unsigned int>& vertexAttributeBufferStrides, const std::vector<RenderState::VertexAttribute>& vertexAttributes, const std::vector<std::vector<ResourceType>>& resources, const std::vector<PixelFormat>* colorPixelFormats) {
        auto vertexShaderModule = prepareShader(vertexShader);
        auto fragmentShaderModule = prepareShader(fragmentShader);
        const vk::PipelineShaderStageCreateInfo shaderStages[2] = {
            vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eVertex).setModule(*vertexShaderModule).setPName(vertexShaderName.c_str()),
            vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eFragment).setModule(*fragmentShaderModule).setPName(fragmentShaderName.c_str()) };

        std::vector<vk::VertexInputBindingDescription> bindingDescriptions;
        for (std::size_t i = 0; i < vertexAttributeBufferStrides.size(); ++i) {
            unsigned int vertexAttributeBufferStride = vertexAttributeBufferStrides[i];
            bindingDescriptions.emplace_back(vk::VertexInputBindingDescription().setBinding(static_cast<uint32_t>(i)).setStride(vertexAttributeBufferStride).setInputRate(vk::VertexInputRate::eVertex));
        }

        std::vector<vk::VertexInputAttributeDescription> attributeDescriptions;
        for (std::size_t i = 0; i < vertexAttributes.size(); ++i) {
            const auto& vertexAttribute = vertexAttributes[i];
            attributeDescriptions.push_back(vk::VertexInputAttributeDescription().setLocation(static_cast<uint32_t>(i)).setBinding(vertexAttribute.vertexAttributeBufferIndex).setFormat(convertVertexFormat(vertexAttribute.format)).setOffset(vertexAttribute.offsetWithinStride));
        }

        const auto vertexInputInfo = vk::PipelineVertexInputStateCreateInfo()
            .setVertexBindingDescriptionCount(static_cast<uint32_t>(vertexAttributeBufferStrides.size()))
            .setPVertexBindingDescriptions(bindingDescriptions.data())
            .setVertexAttributeDescriptionCount(static_cast<uint32_t>(vertexAttributes.size()))
            .setPVertexAttributeDescriptions(attributeDescriptions.data());

        const auto inputAssemblyInfo = vk::PipelineInputAssemblyStateCreateInfo().setTopology(vk::PrimitiveTopology::eTriangleList);

        const auto viewportInfo = vk::PipelineViewportStateCreateInfo().setViewportCount(1).setScissorCount(1);

        const auto rasterizationInfo = vk::PipelineRasterizationStateCreateInfo()
            .setDepthClampEnable(VK_FALSE)
            .setRasterizerDiscardEnable(VK_FALSE)
            .setPolygonMode(vk::PolygonMode::eFill)
            .setCullMode(vk::CullModeFlagBits::eBack)
            .setFrontFace(vk::FrontFace::eClockwise)
            .setDepthBiasEnable(VK_FALSE)
            .setLineWidth(1.0f);

        const auto multisampleInfo = vk::PipelineMultisampleStateCreateInfo();

        const auto stencilOp = vk::StencilOpState()
            .setFailOp(vk::StencilOp::eKeep)
            .setPassOp(vk::StencilOp::eKeep)
            .setCompareOp(vk::CompareOp::eAlways);

        const auto depthStencilInfo = vk::PipelineDepthStencilStateCreateInfo()
            .setDepthTestEnable(VK_FALSE)
            .setDepthWriteEnable(VK_FALSE)
            .setDepthCompareOp(vk::CompareOp::eNever)
            .setDepthBoundsTestEnable(VK_FALSE)
            .setStencilTestEnable(VK_FALSE)
            .setFront(stencilOp)
            .setBack(stencilOp);

        const vk::PipelineColorBlendAttachmentState colorBlendAttachments[1] = {
            vk::PipelineColorBlendAttachmentState().setColorWriteMask(
                vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG | vk::ColorComponentFlagBits::eB |
                vk::ColorComponentFlagBits::eA) };

        const auto colorBlendInfo =
            vk::PipelineColorBlendStateCreateInfo().setAttachmentCount(1).setPAttachments(colorBlendAttachments);

        const vk::DynamicState dynamicStates[2] = { vk::DynamicState::eViewport, vk::DynamicState::eScissor };

        const auto dynamicStateInfo = vk::PipelineDynamicStateCreateInfo().setPDynamicStates(dynamicStates).setDynamicStateCount(2);

        auto pipelineLayout = createPipelineLayout(resources);
        auto compatibleRenderPass = createCompatibleRenderPass(colorPixelFormats);

        const auto graphicsPipelineCreateInfo = vk::GraphicsPipelineCreateInfo()
            .setStageCount(2)
            .setPStages(shaderStages)
            .setPVertexInputState(&vertexInputInfo)
            .setPInputAssemblyState(&inputAssemblyInfo)
            .setPViewportState(&viewportInfo)
            .setPRasterizationState(&rasterizationInfo)
            .setPMultisampleState(&multisampleInfo)
            .setPDepthStencilState(&depthStencilInfo)
            .setPColorBlendState(&colorBlendInfo)
            .setPDynamicState(&dynamicStateInfo)
            .setLayout(*pipelineLayout.first)
            .setRenderPass(*compatibleRenderPass);
        auto graphicsPipeline = device->createGraphicsPipelineUnique(*pipelineCache, graphicsPipelineCreateInfo);
        renderStates.emplace_back(RenderStateImpl(std::move(graphicsPipeline), std::move(pipelineLayout.second), std::move(pipelineLayout.first), std::move(compatibleRenderPass)));
        return renderStates.back();
    }

    ComputeState& DeviceImpl::getComputeState(const std::vector<uint8_t>& shader, const std::string& shaderName, const std::vector<std::vector<ResourceType>>& resources) {
        auto shaderModule = prepareShader(shader);
        const auto shaderStage = vk::PipelineShaderStageCreateInfo().setStage(vk::ShaderStageFlagBits::eCompute).setModule(*shaderModule).setPName(shaderName.c_str());

        auto pipelineLayout = createPipelineLayout(resources);

        const auto computePipelineCreateInfo = vk::ComputePipelineCreateInfo()
            .setStage(shaderStage)
            .setLayout(*pipelineLayout.first);
        auto computePipeline = device->createComputePipelineUnique(*pipelineCache, computePipelineCreateInfo);
        computeStates.emplace_back(ComputeStateImpl(std::move(computePipeline), std::move(pipelineLayout.second), std::move(pipelineLayout.first)));
        return computeStates.back();
    }

    BufferHolder DeviceImpl::getBuffer(unsigned int length) {
        auto result = bufferCache.emplace(length, ResourcePool<BufferImpl>());
        auto insertionTookPlace = result.second;
        auto& resourcePool = result.first->second;
        if (!resourcePool.freeList.empty()) {
            // FIXME: Zero-fill the texture somehow.
            auto buffer = std::move(resourcePool.freeList.back());
            resourcePool.freeList.pop_back();
            buffer.incrementReferenceCount();
            resourcePool.inUse.emplace_back(std::move(buffer));
            return BufferHolder(*this, resourcePool.inUse.back());
        }

        const auto bufferCreateInfo = vk::BufferCreateInfo().setSize(length).setUsage(vk::BufferUsageFlagBits::eTransferSrc | vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eStorageBuffer | vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndirectBuffer);
        auto buffer = device->createBufferUnique(bufferCreateInfo);
        auto memoryRequirements = device->getBufferMemoryRequirements(*buffer);
        auto memoryAllocateInfo = vk::MemoryAllocateInfo().setAllocationSize(memoryRequirements.size);
        bool found = false;
        for (int i = 0; i < VK_MAX_MEMORY_TYPES; ++i) {
            if ((memoryRequirements.memoryTypeBits & (1 << i)) && (memoryProperties.memoryTypes[i].propertyFlags & (vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent))) {
                memoryAllocateInfo.setMemoryTypeIndex(i);
                found = true;
                break;
            }
        }
        assert(found);
        auto memory = device->allocateMemoryUnique(memoryAllocateInfo);
        device->bindBufferMemory(*buffer, *memory, 0);
        // FIXME: Zero-fill the texture somehow.
        std::function<void(BufferImpl&)> returnToDevice = [&](BufferImpl& buffer) {
            auto iterator = bufferCache.find(buffer.getLength());
            assert(iterator != bufferCache.end());
            auto& resourcePool = iterator->second;
            for (auto listIterator = resourcePool.inUse.begin(); listIterator != resourcePool.inUse.end(); ++listIterator) {
                if (&(*listIterator) == &buffer) {
                    auto buffer = std::move(*listIterator);
                    resourcePool.inUse.erase(listIterator);
                    resourcePool.freeList.emplace_back(std::move(buffer));
                    return;
                }
            }
            assert(false);
        };
        BufferImpl newBuffer(std::move(returnToDevice), length, *device, std::move(memory), std::move(buffer));
        newBuffer.incrementReferenceCount();
        resourcePool.inUse.emplace_back(std::move(newBuffer));
        return BufferHolder(*this, resourcePool.inUse.back());
    }

    void DeviceImpl::returnBuffer(Buffer& buffer) {
        auto* downcast = dynamic_cast<BufferImpl*>(&buffer);
        assert(downcast != nullptr);
        downcast->decrementReferenceCount();
    }

    TextureHolder DeviceImpl::getTexture(unsigned int width, unsigned int height, PixelFormat format) {
        TextureParameters textureParameters = { width, height, format };
        auto result = textureCache.emplace(std::move(textureParameters), ResourcePool<TextureImpl>());
        auto insertionTookPlace = result.second;
        auto& resourcePool = result.first->second;
        if (!resourcePool.freeList.empty()) {
            // FIXME: Zero-fill the texture somehow.
            auto buffer = std::move(resourcePool.freeList.back());
            resourcePool.freeList.pop_back();
            resourcePool.inUse.emplace_back(std::move(buffer));
            return TextureHolder(*this, resourcePool.inUse.back());
        }

        auto vulkanPixelFormat = convertPixelFormat(format);
        auto formatProperties = gpu.getFormatProperties(vulkanPixelFormat);
        assert(formatProperties.linearTilingFeatures & vk::FormatFeatureFlagBits::eSampledImage);

        // FIXME: Use staging textures so we can write into the texture linearly.
        const auto imageCreateInfo = vk::ImageCreateInfo()
            .setImageType(vk::ImageType::e2D)
            .setFormat(vulkanPixelFormat)
            .setExtent({ width, height, 1 })
            .setMipLevels(1)
            .setArrayLayers(1)
            .setSamples(vk::SampleCountFlagBits::e1)
            .setTiling(vk::ImageTiling::eOptimal)
            .setUsage(vk::ImageUsageFlagBits::eTransferSrc | vk::ImageUsageFlagBits::eTransferDst | vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eColorAttachment)
            .setSharingMode(vk::SharingMode::eExclusive)
            .setQueueFamilyIndexCount(0)
            .setPQueueFamilyIndices(nullptr)
            .setInitialLayout(vk::ImageLayout::ePreinitialized);
        auto image = device->createImageUnique(imageCreateInfo);
        auto memoryRequirements = device->getImageMemoryRequirements(*image);
        auto memoryAllocateInfo = vk::MemoryAllocateInfo().setAllocationSize(memoryRequirements.size);
        bool found = false;
        for (int i = 0; i < VK_MAX_MEMORY_TYPES; ++i) {
            if ((memoryRequirements.memoryTypeBits & (1 << i))) {
                memoryAllocateInfo.setMemoryTypeIndex(i);
                found = true;
                break;
            }
        }
        assert(found);
        auto memory = device->allocateMemoryUnique(memoryAllocateInfo);
        device->bindImageMemory(*image, *memory, 0);
        // FIXME: Zero-fill the texture somehow.
        const auto imageViewCreateInfo = vk::ImageViewCreateInfo()
            .setImage(*image)
            .setViewType(vk::ImageViewType::e2D)
            .setFormat(vulkanPixelFormat)
            .setSubresourceRange(vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));;
        auto imageView = device->createImageViewUnique(imageViewCreateInfo);
        std::function<void(TextureImpl&)> returnToDevice = [&](TextureImpl& texture) {
            TextureParameters textureParameters = { texture.getWidth(), texture.getHeight(), convertFormat(texture.getFormat()) };
            auto iterator = textureCache.find(textureParameters);
            assert(iterator != textureCache.end());
            auto& resourcePool = iterator->second;
            for (auto listIterator = resourcePool.inUse.begin(); listIterator != resourcePool.inUse.end(); ++listIterator) {
                if (&(*listIterator) == &texture) {
                    auto buffer = std::move(*listIterator);
                    resourcePool.inUse.erase(listIterator);
                    resourcePool.freeList.emplace_back(std::move(buffer));
                    return;
                }
            }
            assert(false);
        };
        TextureImpl newTexture(std::move(returnToDevice), width, height, vulkanPixelFormat, *device, std::move(memory), std::move(image), std::move(imageView));
        newTexture.incrementReferenceCount();
        resourcePool.inUse.emplace_back(std::move(newTexture));
        return TextureHolder(*this, resourcePool.inUse.back());
    }

    void DeviceImpl::returnTexture(Texture& texture) {
        auto* downcast = dynamic_cast<TextureImpl*>(&texture);
        assert(downcast != nullptr);
        downcast->decrementReferenceCount();
    }

    SamplerHolder DeviceImpl::getSampler(AddressMode addressMode, Filter filter) {
        auto result = samplerCache.emplace((static_cast<uint32_t>(addressMode) << 8) | static_cast<uint32_t>(filter), ResourcePool<SamplerImpl>());
        auto insertionTookPlace = result.second;
        auto& resourcePool = result.first->second;
        if (!resourcePool.freeList.empty()) {
            auto sampler = std::move(resourcePool.freeList.back());
            resourcePool.freeList.pop_back();
            sampler.incrementReferenceCount();
            resourcePool.inUse.emplace_back(std::move(sampler));
            return SamplerHolder(*this, resourcePool.inUse.back());
        }

        vk::Filter vulkanFilter;
        vk::SamplerMipmapMode mipmapMode;
        switch (filter) {
        case Filter::Nearest:
            vulkanFilter = vk::Filter::eNearest;
            mipmapMode = vk::SamplerMipmapMode::eNearest;
            break;
        case Filter::Linear:
            vulkanFilter = vk::Filter::eLinear;
            mipmapMode = vk::SamplerMipmapMode::eLinear;
            break;
        default:
            assert(false);
        }

        vk::SamplerAddressMode vulkanAddressMode;
        switch (addressMode) {
        case AddressMode::ClampToEdge:
            vulkanAddressMode = vk::SamplerAddressMode::eClampToEdge;
            break;
        case AddressMode::MirrorClampToEdge:
            vulkanAddressMode = vk::SamplerAddressMode::eMirrorClampToEdge;
            break;
        case AddressMode::Repeat:
            vulkanAddressMode = vk::SamplerAddressMode::eRepeat;
            break;
        case AddressMode::MirrorRepeat:
            vulkanAddressMode = vk::SamplerAddressMode::eMirroredRepeat;
            break;
        }

        const auto samplerCreateInfo = vk::SamplerCreateInfo()
            .setMagFilter(vulkanFilter)
            .setMinFilter(vulkanFilter)
            .setMipmapMode(mipmapMode)
            .setAddressModeU(vulkanAddressMode)
            .setAddressModeV(vulkanAddressMode)
            .setAddressModeW(vulkanAddressMode);
        auto sampler = device->createSamplerUnique(samplerCreateInfo);

        std::function<void(SamplerImpl&)> returnToDevice = [&](SamplerImpl& sampler) {
            AddressMode addressMode;
            switch (sampler.getAddressMode()) {
            case vk::SamplerAddressMode::eClampToEdge:
                addressMode = AddressMode::ClampToEdge;
                break;
            case vk::SamplerAddressMode::eMirrorClampToEdge:
                addressMode = AddressMode::MirrorClampToEdge;
                break;
            case vk::SamplerAddressMode::eRepeat:
                addressMode = AddressMode::Repeat;
                break;
            case vk::SamplerAddressMode::eMirroredRepeat:
                addressMode = AddressMode::MirrorRepeat;
                break;
            default:
                assert(false);
            }
            Filter filter;
            switch (sampler.getFilter()) {
            case vk::Filter::eNearest:
                filter = Filter::Nearest;
                break;
            case vk::Filter::eLinear:
                filter = Filter::Linear;
                break;
            default:
                assert(false);
            }

            auto iterator = samplerCache.find((static_cast<uint32_t>(addressMode) << 8) | static_cast<uint32_t>(filter));
            assert(iterator != samplerCache.end());
            auto& resourcePool = iterator->second;
            for (auto listIterator = resourcePool.inUse.begin(); listIterator != resourcePool.inUse.end(); ++listIterator) {
                if (&(*listIterator) == &sampler) {
                    auto sampler = std::move(*listIterator);
                    resourcePool.inUse.erase(listIterator);
                    resourcePool.freeList.emplace_back(std::move(sampler));
                    return;
                }
            }
            assert(false);
        };
        SamplerImpl newSampler(std::move(returnToDevice), vulkanFilter, mipmapMode, vulkanAddressMode, *device, std::move(sampler));
        newSampler.incrementReferenceCount();
        resourcePool.inUse.emplace_back(std::move(newSampler));
        return SamplerHolder(*this, resourcePool.inUse.back());
    }

    void DeviceImpl::returnSampler(Sampler& sampler) {
        auto* downcast = dynamic_cast<SamplerImpl*>(&sampler);
        assert(downcast != nullptr);
        downcast->decrementReferenceCount();
    }

    DeviceImpl::~DeviceImpl() {
        for (std::size_t i = 0; i < swapchainImages.size(); ++i)
            static_cast<Queue*>(queue.get())->present();
    }
}
