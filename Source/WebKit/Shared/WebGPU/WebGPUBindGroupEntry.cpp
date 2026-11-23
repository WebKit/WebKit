/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "WebGPUBindGroupEntry.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include <WebCore/WebGPUCppAPI.h>

namespace WebKit::WebGPU {

std::optional<::WebGPU::BindGroupEntry> ConvertFromBackingContext::convertFromBacking(const BindGroupEntry& bindGroupEntry)
{
    return WTF::switchOn(bindGroupEntry.resource, [&](WebGPUSamplerIdentifier identifier) -> std::optional<::WebGPU::BindGroupEntry> {
        RefPtr sampler = convertSamplerFromBacking(identifier);
        if (!sampler)
            return std::nullopt;
        return { { bindGroupEntry.binding, { *sampler } } };
    }, [&](WebGPUTextureIdentifier identifier) -> std::optional<::WebGPU::BindGroupEntry> {
        RefPtr texture = convertTextureFromBacking(identifier);
        if (!texture)
            return std::nullopt;
        return { { bindGroupEntry.binding, { *texture } } };
    }, [&](WebGPUTextureViewIdentifier identifier) -> std::optional<::WebGPU::BindGroupEntry> {
        RefPtr textureView = convertTextureViewFromBacking(identifier);
        if (!textureView)
            return std::nullopt;
        return { { bindGroupEntry.binding, { *textureView } } };
    }, [&](const BufferBinding& bufferBinding) -> std::optional<::WebGPU::BindGroupEntry> {
        auto convertedBufferBinding = convertFromBacking(bufferBinding);
        if (!convertedBufferBinding)
            return std::nullopt;
        return { { bindGroupEntry.binding, { *convertedBufferBinding } } };
    }, [&](WebGPUExternalTextureIdentifier identifier) -> std::optional<::WebGPU::BindGroupEntry> {
        auto externalTexture = convertExternalTextureFromBacking(identifier);
        if (!externalTexture.get())
            return std::nullopt;
        return { { bindGroupEntry.binding, { *externalTexture.get() } } };
    });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
