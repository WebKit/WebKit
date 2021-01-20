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
    class BufferImpl : public Buffer {
    public:
        BufferImpl() = default;
        virtual ~BufferImpl() = default;
        BufferImpl(const BufferImpl&) = delete;
        BufferImpl(BufferImpl&&) = default;
        BufferImpl& operator=(const BufferImpl&) = delete;
        BufferImpl& operator=(BufferImpl&&) = default;

        BufferImpl(std::function<void(BufferImpl&)>&& returnToDevice, unsigned int length, vk::Device device, vk::UniqueDeviceMemory&& memory, vk::UniqueBuffer&& buffer);

        vk::Buffer getBuffer() const { return *buffer; }
        vk::DeviceMemory getDeviceMemory() const { return *memory; }
        void incrementReferenceCount() { ++referenceCount; }
        void decrementReferenceCount();
        unsigned getLength() const { return length; }

    private:
        std::function<void(BufferImpl&)> returnToDevice;
        vk::Device device;
        vk::UniqueDeviceMemory memory;
        vk::UniqueBuffer buffer;
        unsigned int length;
        unsigned int referenceCount{ 0 };
    };
}

