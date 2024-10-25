/*
 * Copyright (c) 2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "XRSubImage.h"

#import "APIConversions.h"
#import "Device.h"
#import "Texture.h"

#import <wtf/CheckedArithmetic.h>
#import <wtf/StdLibExtras.h>

namespace WebGPU {

XRSubImage::XRSubImage(bool, Device& device)
    : m_device(device)
{
}

XRSubImage::XRSubImage(Device& device)
    : m_device(device)
{
}

XRSubImage::~XRSubImage() = default;

Ref<XRSubImage> Device::createXRSubImage()
{
    if (!isValid())
        return XRSubImage::createInvalid(*this);

    return XRSubImage::create(*this);
}

void XRSubImage::setLabel(String&&)
{
}

bool XRSubImage::isValid() const
{
    return true;
}

void XRSubImage::update(id<MTLTexture> colorTexture, id<MTLTexture> depthTexture, size_t currentTextureIndex, const std::pair<id<MTLSharedEvent>, uint64_t>& sharedEvent)
{
    RefPtr device = m_device.get();
    if (!device)
        return;

    m_currentTextureIndex = currentTextureIndex;
    auto* texture = this->colorTexture();
    if (!texture || texture->texture() != colorTexture) {
        auto colorFormat = WGPUTextureFormat_BGRA8UnormSrgb;
        WGPUTextureDescriptor colorTextureDescriptor = {
            .nextInChain = nullptr,
            .label = "color texture",
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = static_cast<uint32_t>(colorTexture.width),
                .height = static_cast<uint32_t>(colorTexture.height),
                .depthOrArrayLayers = static_cast<uint32_t>(colorTexture.arrayLength),
            },
            .format = colorFormat,
            .mipLevelCount = 1,
            .sampleCount = static_cast<uint32_t>(colorTexture.sampleCount),
            .viewFormatCount = 1,
            .viewFormats = &colorFormat,
        };
        auto newTexture = Texture::create(colorTexture, colorTextureDescriptor, { colorFormat }, *device);
        newTexture->updateCompletionEvent(sharedEvent);
        m_colorTextures.set(currentTextureIndex, newTexture.ptr());
    } else
        texture->updateCompletionEvent(sharedEvent);

    if (texture = this->depthTexture(); !texture || texture->texture() != depthTexture) {
        auto depthFormat = WGPUTextureFormat_Depth24PlusStencil8;
        WGPUTextureDescriptor depthTextureDescriptor = {
            .nextInChain = nullptr,
            .label = "depth texture",
            .usage = WGPUTextureUsage_RenderAttachment,
            .dimension = WGPUTextureDimension_2D,
            .size = {
                .width = static_cast<uint32_t>(depthTexture.width),
                .height = static_cast<uint32_t>(depthTexture.height),
                .depthOrArrayLayers = static_cast<uint32_t>(depthTexture.arrayLength),
            },
            .format = depthFormat,
            .mipLevelCount = 1,
            .sampleCount = static_cast<uint32_t>(depthTexture.sampleCount),
            .viewFormatCount = 1,
            .viewFormats = &depthFormat,
        };
        m_depthTextures.set(currentTextureIndex, Texture::create(depthTexture, depthTextureDescriptor, { depthFormat }, *device));
    }
}

Texture* XRSubImage::colorTexture()
{
    if (auto it = m_colorTextures.find(m_currentTextureIndex); it != m_colorTextures.end())
        return it->value.get();
    return nullptr;
}

Texture* XRSubImage::depthTexture()
{
    if (auto it = m_depthTextures.find(m_currentTextureIndex); it != m_depthTextures.end())
        return it->value.get();
    return nullptr;
}

RefPtr<XRSubImage> XRBinding::getViewSubImage(XRProjectionLayer& projectionLayer)
{
    return device().getXRViewSubImage(projectionLayer);
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuXRSubImageReference(WGPUXRSubImage subImage)
{
    WebGPU::fromAPI(subImage).ref();
}

void wgpuXRSubImageRelease(WGPUXRSubImage subImage)
{
    WebGPU::fromAPI(subImage).deref();
}

WGPUTexture wgpuXRSubImageGetColorTexture(WGPUXRSubImage subImage)
{
    return WebGPU::protectedFromAPI(subImage)->colorTexture();
}

WGPUTexture wgpuXRSubImageGetDepthStencilTexture(WGPUXRSubImage subImage)
{
    return WebGPU::protectedFromAPI(subImage)->depthTexture();
}
