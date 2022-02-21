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

#pragma once

#include "GPUDepthStencilState.h"
#include "GPUFragmentState.h"
#include "GPUMultisampleState.h"
#include "GPUPipelineDescriptorBase.h"
#include "GPUPrimitiveState.h"
#include "GPUVertexState.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPURenderPipelineDescriptor.h>

namespace WebCore {

struct GPURenderPipelineDescriptor : public GPUPipelineDescriptorBase {
    PAL::WebGPU::RenderPipelineDescriptor convertToBacking() const
    {
        return {
            {
                { label },
                layout ? &layout->backing() : nullptr,
            },
            vertex.convertToBacking(),
            primitive ? std::optional { primitive->convertToBacking() } : std::nullopt,
            depthStencil ? std::optional { depthStencil->convertToBacking() } : std::nullopt,
            multisample ? std::optional { multisample->convertToBacking() } : std::nullopt,
            fragment ? std::optional { fragment->convertToBacking() } : std::nullopt,
        };
    }

    GPUVertexState vertex;
    std::optional<GPUPrimitiveState> primitive;
    std::optional<GPUDepthStencilState> depthStencil;
    std::optional<GPUMultisampleState> multisample;
    std::optional<GPUFragmentState> fragment;
};

}
