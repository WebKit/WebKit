/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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
#include "WebGPUImageCopyTexture.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUImageCopyTexture.h>

namespace WebKit::WebGPU {

std::optional<ImageCopyTexture> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::ImageCopyTexture& imageCopyTexture)
{
    auto texture = convertToBacking(imageCopyTexture.texture);
    if (!texture)
        return std::nullopt;

    std::optional<Origin3D> origin;
    if (imageCopyTexture.origin) {
        origin = convertToBacking(*imageCopyTexture.origin);
        if (!origin)
            return std::nullopt;
    }

    return { { texture, imageCopyTexture.mipLevel, WTFMove(origin), imageCopyTexture.aspect } };
}

std::optional<PAL::WebGPU::ImageCopyTexture> ConvertFromBackingContext::convertFromBacking(const ImageCopyTexture& imageCopyTexture)
{
    auto* texture = convertTextureFromBacking(imageCopyTexture.texture);
    if (!texture)
        return std::nullopt;

    std::optional<PAL::WebGPU::Origin3D> origin;
    if (imageCopyTexture.origin) {
        origin = convertFromBacking(*imageCopyTexture.origin);
        if (!origin)
            return std::nullopt;
    }

    return { { *texture, imageCopyTexture.mipLevel, WTFMove(origin), imageCopyTexture.aspect } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
