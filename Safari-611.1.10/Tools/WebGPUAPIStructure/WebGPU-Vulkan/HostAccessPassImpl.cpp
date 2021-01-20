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

#include "HostAccessPassImpl.h"
#include <cassert>

namespace WebGPU {
    HostAccessPassImpl::HostAccessPassImpl(vk::Device device, vk::UniqueCommandBuffer&& commandBuffer) : PassImpl(device, std::move(commandBuffer)), finishedEvent(device.createEventUnique(vk::EventCreateInfo())) {
        // Our regular inter-command-buffer synchronization also issues barriers with eHostRead and eHostWrite, so
        // the host just needs to wait for any previously-committed command buffers to finish (which it already has
        // to do because we can only have a few framebuffer images we can draw into). Therefore, we only append a
        // single command into the command buffer: waitEvents(), which the CPU will signal when it's done with its
        // work. Therefore, the host has ownership of the resources while the GPU is waiting. As soon as its done
        // waiting, our regular inter-command-buffer synchronization will issue the relevant barriers with eHostRead
        // and eHostWrite.
    }

    void HostAccessPassImpl::overwriteBuffer(Buffer& buffer, const std::vector<uint8_t>& newData) {
        auto* downcast = dynamic_cast<BufferImpl*>(&buffer);
        assert(downcast != nullptr);
        auto& buf = *downcast;

        assert(buf.getLength() == newData.size());

        buffersToOverwrite.push_back(buf);
        bufferOverwriteData.push_back(newData);

        insertBuffer(buf);
    }

    boost::unique_future<std::vector<uint8_t>> HostAccessPassImpl::getBufferContents(Buffer& buffer) {
        auto* downcast = dynamic_cast<BufferImpl*>(&buffer);
        assert(downcast != nullptr);
        auto& buf = *downcast;

        buffersToRead.push_back(buf);

        insertBuffer(buf);

        readPromises.emplace_back(boost::promise<std::vector<uint8_t>>());
        return readPromises.back().get_future();
    }

    void HostAccessPassImpl::execute() {
        assert(buffersToOverwrite.size() == bufferOverwriteData.size());
        for (std::size_t i = 0; i < buffersToOverwrite.size(); ++i) {
            BufferImpl& buffer = buffersToOverwrite[i];
            auto& data = bufferOverwriteData[i];
            void* cpuAddress = device.mapMemory(buffer.getDeviceMemory(), 0, VK_WHOLE_SIZE);
            memcpy(cpuAddress, data.data(), buffer.getLength());
            device.unmapMemory(buffer.getDeviceMemory());
        }

        assert(buffersToRead.size() == readPromises.size());
        for (std::size_t i = 0; i < buffersToRead.size(); ++i) {
            BufferImpl& buffer = buffersToRead[i];
            std::vector<uint8_t> data(buffer.getLength());
            void* cpuAddress = device.mapMemory(buffer.getDeviceMemory(), 0, VK_WHOLE_SIZE);
            memcpy(data.data(), cpuAddress, buffer.getLength());
            device.unmapMemory(buffer.getDeviceMemory());
            readPromises[i].set_value(data);
        }
    }
}
