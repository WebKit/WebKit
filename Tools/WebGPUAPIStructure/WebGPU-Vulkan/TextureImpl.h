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
#include <functional>
#include <vulkan/vulkan.hpp>

namespace WebGPU {
    class TextureImpl : public Texture {
    public:
        TextureImpl() = default;
        virtual ~TextureImpl() = default;
        TextureImpl(const TextureImpl&) = delete;
        TextureImpl(TextureImpl&&) = default;
        TextureImpl& operator=(const TextureImpl&) = delete;
        TextureImpl& operator=(TextureImpl&&) = default;

        TextureImpl(std::function<void(TextureImpl&)>&& returnToDevice, unsigned int width, unsigned int height, vk::Format format, vk::Device device, vk::UniqueDeviceMemory&& memory, vk::UniqueImage&& image, vk::UniqueImageView&& imageView);

        vk::Image getImage() const { return *image; }
        vk::ImageView getImageView() const { return *imageView; }
        vk::Format getFormat() const { return format; }
        void incrementReferenceCount() { ++referenceCount; }
        void decrementReferenceCount();
        unsigned int getWidth() const { return width; }
        unsigned int getHeight() const { return height; }
        bool getTransferredToGPU() const { return transferredToGPU; }
        void setTransferredToGPU(bool transferredToGPU) { this->transferredToGPU = transferredToGPU; }

    private:
        std::function<void(TextureImpl&)> returnToDevice;
        vk::Device device;
        vk::UniqueDeviceMemory memory;
        vk::UniqueImage image;
        vk::UniqueImageView imageView;
        vk::Format format;
        unsigned int referenceCount{ 0 };
        unsigned int width;
        unsigned int height;
        bool transferredToGPU{ false };
    };
}

