/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebGPUBindGroupLayoutEntry.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUBindGroupLayoutEntry.h>

namespace WebKit::WebGPU {

std::optional<BindGroupLayoutEntry> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::BindGroupLayoutEntry& bindGroupLayoutEntry)
{
    std::optional<BufferBindingLayout> buffer;
    if (bindGroupLayoutEntry.buffer) {
        buffer = convertToBacking(*bindGroupLayoutEntry.buffer);
        if (!buffer)
            return std::nullopt;
    }

    std::optional<SamplerBindingLayout> sampler;
    if (bindGroupLayoutEntry.sampler) {
        sampler = convertToBacking(*bindGroupLayoutEntry.sampler);
        if (!sampler)
            return std::nullopt;
    }

    std::optional<TextureBindingLayout> texture;
    if (bindGroupLayoutEntry.texture) {
        texture = convertToBacking(*bindGroupLayoutEntry.texture);
        if (!texture)
            return std::nullopt;
    }

    std::optional<StorageTextureBindingLayout> storageTexture;
    if (bindGroupLayoutEntry.storageTexture) {
        storageTexture = convertToBacking(*bindGroupLayoutEntry.storageTexture);
        if (!storageTexture)
            return std::nullopt;
    }

    std::optional<ExternalTextureBindingLayout> externalTexture;
    if (bindGroupLayoutEntry.externalTexture) {
        externalTexture = convertToBacking(*bindGroupLayoutEntry.externalTexture);
        if (!externalTexture)
            return std::nullopt;
    }

    return { { bindGroupLayoutEntry.binding, bindGroupLayoutEntry.visibility, WTFMove(buffer), WTFMove(sampler), WTFMove(texture), WTFMove(storageTexture), WTFMove(externalTexture) } };
}

std::optional<PAL::WebGPU::BindGroupLayoutEntry> ConvertFromBackingContext::convertFromBacking(const BindGroupLayoutEntry& bindGroupLayoutEntry)
{
    std::optional<PAL::WebGPU::BufferBindingLayout> buffer;
    if (bindGroupLayoutEntry.buffer) {
        buffer = convertFromBacking(*bindGroupLayoutEntry.buffer);
        if (!buffer)
            return std::nullopt;
    }

    std::optional<PAL::WebGPU::SamplerBindingLayout> sampler;
    if (bindGroupLayoutEntry.sampler) {
        sampler = convertFromBacking(*bindGroupLayoutEntry.sampler);
        if (!sampler)
            return std::nullopt;
    }

    std::optional<PAL::WebGPU::TextureBindingLayout> texture;
    if (bindGroupLayoutEntry.texture) {
        texture = convertFromBacking(*bindGroupLayoutEntry.texture);
        if (!texture)
            return std::nullopt;
    }

    std::optional<PAL::WebGPU::StorageTextureBindingLayout> storageTexture;
    if (bindGroupLayoutEntry.storageTexture) {
        storageTexture = convertFromBacking(*bindGroupLayoutEntry.storageTexture);
        if (!storageTexture)
            return std::nullopt;
    }

    std::optional<PAL::WebGPU::ExternalTextureBindingLayout> externalTexture;
    if (bindGroupLayoutEntry.externalTexture) {
        externalTexture = convertFromBacking(*bindGroupLayoutEntry.externalTexture);
        if (!externalTexture)
            return std::nullopt;
    }

    return { { bindGroupLayoutEntry.binding, bindGroupLayoutEntry.visibility, WTFMove(buffer), WTFMove(sampler), WTFMove(texture), WTFMove(storageTexture), WTFMove(externalTexture) } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
