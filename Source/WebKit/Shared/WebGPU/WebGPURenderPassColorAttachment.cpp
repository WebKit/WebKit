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
#include "WebGPURenderPassColorAttachment.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPURenderPassColorAttachment.h>

namespace WebKit::WebGPU {

std::optional<RenderPassColorAttachment> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::RenderPassColorAttachment& renderPassColorAttachment)
{
    auto view = convertToBacking(renderPassColorAttachment.view);
    if (!view)
        return std::nullopt;

    std::optional<WebGPUIdentifier> resolveTarget;
    if (renderPassColorAttachment.resolveTarget) {
        resolveTarget = convertToBacking(*renderPassColorAttachment.resolveTarget);
        if (!resolveTarget)
            return std::nullopt;
    }

    std::optional<Color> clearValue;
    if (renderPassColorAttachment.clearValue) {
        clearValue = convertToBacking(*renderPassColorAttachment.clearValue);
        if (!clearValue)
            return std::nullopt;
    }

    return { { view, resolveTarget, WTFMove(clearValue), renderPassColorAttachment.loadOp, renderPassColorAttachment.storeOp } };
}

std::optional<PAL::WebGPU::RenderPassColorAttachment> ConvertFromBackingContext::convertFromBacking(const RenderPassColorAttachment& renderPassColorAttachment)
{
    auto* view = convertTextureViewFromBacking(renderPassColorAttachment.view);
    if (!view)
        return std::nullopt;

    PAL::WebGPU::TextureView* resolveTarget = nullptr;
    if (renderPassColorAttachment.resolveTarget) {
        resolveTarget = convertTextureViewFromBacking(renderPassColorAttachment.resolveTarget.value());
        if (!resolveTarget)
            return std::nullopt;
    }

    std::optional<PAL::WebGPU::Color> clearValue;
    if (renderPassColorAttachment.clearValue) {
        clearValue = convertFromBacking(*renderPassColorAttachment.clearValue);
        if (!clearValue)
            return std::nullopt;
    }

    return { { *view, resolveTarget, WTFMove(clearValue), renderPassColorAttachment.loadOp, renderPassColorAttachment.storeOp } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
