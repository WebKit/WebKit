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

#include "BlitPassImpl.h"

namespace WebGPU {
    BlitPassImpl::BlitPassImpl(vk::Device device, vk::UniqueCommandBuffer&& commandBuffer) : PassImpl(device, std::move(commandBuffer)) {
    }

    void BlitPassImpl::copyTexture(Texture& source, Texture& destination, unsigned int sourceX, unsigned int sourceY, unsigned int destinationX, unsigned int destinationY, unsigned int width, unsigned int height) {
        auto* downcast = dynamic_cast<TextureImpl*>(&source);
        assert(downcast != nullptr);
        auto& src = *downcast;
        downcast = dynamic_cast<TextureImpl*>(&destination);
        assert(downcast != nullptr);
        auto& dst = *downcast;
        const auto subresource = vk::ImageSubresourceLayers()
            .setAspectMask(vk::ImageAspectFlagBits::eColor)
            .setMipLevel(0)
            .setBaseArrayLayer(0)
            .setLayerCount(1);

        const auto imageCopy = vk::ImageCopy()
            .setSrcSubresource(subresource)
            .setSrcOffset(vk::Offset3D(sourceX, sourceY, 0))
            .setDstSubresource(subresource)
            .setDstOffset(vk::Offset3D(destinationX, destinationY, 0))
            .setExtent(vk::Extent3D(width, height, 1));
        commandBuffer->copyImage(src.getImage(), vk::ImageLayout::eGeneral, dst.getImage(), vk::ImageLayout::eGeneral, { imageCopy });

        insertTexture(src);
        insertTexture(dst);
    }
}
