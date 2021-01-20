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
#include "PassImpl.h"
#include "BufferImpl.h"
#include <vulkan/vulkan.hpp>

namespace WebGPU {
    class HostAccessPassImpl : public PassImpl, public HostAccessPass {
    public:
        HostAccessPassImpl() = default;
        virtual ~HostAccessPassImpl() = default;
        HostAccessPassImpl(const HostAccessPassImpl&) = delete;
        HostAccessPassImpl(HostAccessPassImpl&&) = default;
        HostAccessPassImpl& operator=(const HostAccessPassImpl&) = delete;
        HostAccessPassImpl& operator=(HostAccessPassImpl&&) = default;

        HostAccessPassImpl(vk::Device device, vk::UniqueCommandBuffer&& commandBuffer);

        vk::Event getFinishedEvent() const { return *finishedEvent; }

        void execute();

    private:
        void overwriteBuffer(Buffer& buffer, const std::vector<uint8_t>& newData) override;
        boost::unique_future<std::vector<uint8_t>> getBufferContents(Buffer& buffer) override;

        vk::UniqueEvent finishedEvent;
        std::vector<std::reference_wrapper<BufferImpl>> buffersToOverwrite;
        std::vector<std::vector<uint8_t>> bufferOverwriteData;
        std::vector<std::reference_wrapper<BufferImpl>> buffersToRead;
        std::vector<boost::promise<std::vector<uint8_t>>> readPromises;
    };
}
