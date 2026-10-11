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
#include "WebGPUVertexState.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPUVertexState.h>

namespace WebKit::WebGPU {

std::optional<VertexState> ConvertToBackingContext::convertToBacking(const ::WebGPU::VertexState& vertexState)
{
    auto base = convertToBacking(vertexState.stage);
    if (!base)
        return std::nullopt;

    Vector<std::optional<VertexBufferLayout>> buffers;
    buffers.reserveInitialCapacity(vertexState.buffers.size());
    for (const auto& buffer : vertexState.buffers) {
        if (buffer) {
            auto convertedBuffer = convertToBacking(*buffer);
            if (!convertedBuffer)
                return std::nullopt;
            buffers.append(WTF::move(*convertedBuffer));
        } else
            buffers.append(std::nullopt);
    }

    return { { WTF::move(*base), WTF::move(buffers) } };
}

std::optional<::WebGPU::VertexState> ConvertFromBackingContext::convertFromBacking(const VertexState& vertexState, RenderPipelineDescriptorStorage& storage)
{
    auto stage = convertFromBacking(static_cast<const ProgrammableStage&>(vertexState), storage.vertexConstants);
    if (!stage)
        return std::nullopt;

    // The attributes of each buffer are sized up front, so that the spans into them stay valid.
    storage.vertexAttributes.resize(vertexState.buffers.size());
    storage.vertexBuffers.reserveInitialCapacity(vertexState.buffers.size());
    for (size_t i = 0; i < vertexState.buffers.size(); ++i) {
        auto& backingBuffer = vertexState.buffers[i];
        if (backingBuffer) {
            auto buffer = convertFromBacking(*backingBuffer, storage.vertexAttributes[i]);
            if (!buffer)
                return std::nullopt;
            storage.vertexBuffers.append(WTF::move(*buffer));
        } else
            storage.vertexBuffers.append(std::nullopt);
    }

    return { { WTF::move(*stage), storage.vertexBuffers.span() } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
