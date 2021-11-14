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

#include <cstdint>
#include <pal/graphics/WebGPU/WebGPUBlendOperation.h>

namespace WebCore {

enum class GPUBlendOperation : uint8_t {
    Add,
    Subtract,
    ReverseSubtract,
    Min,
    Max,
};

inline PAL::WebGPU::BlendOperation convertToBacking(GPUBlendOperation blendOperation)
{
    switch (blendOperation) {
    case GPUBlendOperation::Add:
        return PAL::WebGPU::BlendOperation::Add;
    case GPUBlendOperation::Subtract:
        return PAL::WebGPU::BlendOperation::Subtract;
    case GPUBlendOperation::ReverseSubtract:
        return PAL::WebGPU::BlendOperation::ReverseSubtract;
    case GPUBlendOperation::Min:
        return PAL::WebGPU::BlendOperation::Min;
    case GPUBlendOperation::Max:
        return PAL::WebGPU::BlendOperation::Max;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

}
