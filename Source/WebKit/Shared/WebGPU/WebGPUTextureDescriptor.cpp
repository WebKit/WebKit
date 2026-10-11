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
#include "WebGPUTextureDescriptor.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPUCppAPI.h>

namespace WebKit::WebGPU {

std::optional<TextureDescriptor> ConvertToBackingContext::convertToBacking(const ::WebGPU::TextureDescriptor& textureDescriptor)
{
    return { { { textureDescriptor.label }, textureDescriptor.size, textureDescriptor.mipLevelCount, textureDescriptor.sampleCount, textureDescriptor.dimension, textureDescriptor.format, textureDescriptor.usage, Vector(textureDescriptor.viewFormats) } };
}

// The descriptor borrows the view formats from textureDescriptor.
std::optional<::WebGPU::TextureDescriptor> ConvertFromBackingContext::convertFromBacking(const TextureDescriptor& textureDescriptor)
{
    return ::WebGPU::TextureDescriptor {
        .label = textureDescriptor.label,
        .usage = textureDescriptor.usage,
        .dimension = textureDescriptor.dimension,
        .size = textureDescriptor.size,
        .format = textureDescriptor.format,
        .mipLevelCount = textureDescriptor.mipLevelCount,
        .sampleCount = textureDescriptor.sampleCount,
        .viewFormats = textureDescriptor.viewFormats.span(),
    };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
