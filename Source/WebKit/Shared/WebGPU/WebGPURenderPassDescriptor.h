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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "WebGPUIdentifier.h"
#include "WebGPUObjectDescriptorBase.h"
#include "WebGPURenderPassColorAttachment.h"
#include "WebGPURenderPassDepthStencilAttachment.h"
#include "WebGPURenderPassTimestampWrites.h"
#include <optional>
#include <wtf/Vector.h>

namespace WebKit::WebGPU {

struct RenderPassDescriptor : public ObjectDescriptorBase {
    Vector<std::optional<RenderPassColorAttachment>> colorAttachments;
    std::optional<RenderPassDepthStencilAttachment> depthStencilAttachment;
    WebGPUIdentifier occlusionQuerySet;
    RenderPassTimestampWrites timestampWrites;

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << static_cast<const ObjectDescriptorBase&>(*this);
        encoder << colorAttachments;
        encoder << depthStencilAttachment;
        encoder << occlusionQuerySet;
        encoder << timestampWrites;
    }

    template<class Decoder> static std::optional<RenderPassDescriptor> decode(Decoder& decoder)
    {
        std::optional<ObjectDescriptorBase> objectDescriptorBase;
        decoder >> objectDescriptorBase;
        if (!objectDescriptorBase)
            return std::nullopt;

        std::optional<Vector<std::optional<RenderPassColorAttachment>>> colorAttachments;
        decoder >> colorAttachments;
        if (!colorAttachments)
            return std::nullopt;

        std::optional<std::optional<RenderPassDepthStencilAttachment>> depthStencilAttachment;
        decoder >> depthStencilAttachment;
        if (!depthStencilAttachment)
            return std::nullopt;

        std::optional<WebGPUIdentifier> occlusionQuerySet;
        decoder >> occlusionQuerySet;
        if (!occlusionQuerySet)
            return std::nullopt;

        std::optional<RenderPassTimestampWrites> timestampWrites;
        decoder >> timestampWrites;
        if (!timestampWrites)
            return std::nullopt;

        return { { WTFMove(*objectDescriptorBase), WTFMove(*colorAttachments), WTFMove(*depthStencilAttachment), WTFMove(*occlusionQuerySet), WTFMove(*timestampWrites) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
