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
#include <vulkan/vulkan.hpp>

namespace WebGPU {
    class SamplerImpl : public Sampler {
    public:
        SamplerImpl() = default;
        virtual ~SamplerImpl() = default;
        SamplerImpl(const SamplerImpl&) = delete;
        SamplerImpl(SamplerImpl&&) = default;
        SamplerImpl& operator=(const SamplerImpl&) = delete;
        SamplerImpl& operator=(SamplerImpl&&) = default;

        SamplerImpl(std::function<void(SamplerImpl&)>&& returnToDevice, vk::Filter filter, vk::SamplerMipmapMode mipmapMode, vk::SamplerAddressMode addressMode, vk::Device device, vk::UniqueSampler&& sampler);

        vk::Sampler getSampler() { return *sampler; }
        void incrementReferenceCount() { ++referenceCount; }
        void decrementReferenceCount();
        vk::Filter getFilter() { return filter; }
        vk::SamplerMipmapMode getMipmapMode() { return mipmapMode; }
        vk::SamplerAddressMode getAddressMode() { return addressMode; }

    private:
        std::function<void(SamplerImpl&)> returnToDevice;
        vk::Device device;
        vk::UniqueSampler sampler;
        unsigned int referenceCount{ 0 };
        vk::Filter filter;
        vk::SamplerMipmapMode mipmapMode;
        vk::SamplerAddressMode addressMode;
    };
}

