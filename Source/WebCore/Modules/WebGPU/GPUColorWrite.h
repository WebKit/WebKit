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

#pragma once

#include "GPUBlendFactor.h"
#include "GPUBlendOperation.h"
#include "GPUIntegralTypes.h"
#include "WebGPUColorWrite.h"
#include <cstdint>
#include <wtf/RefCounted.h>

namespace WebCore {

using GPUColorWriteFlags = uint32_t;
class GPUColorWrite : public RefCounted<GPUColorWrite> {
public:
    static constexpr GPUFlagsConstant RED   = 0x1;
    static constexpr GPUFlagsConstant GREEN = 0x2;
    static constexpr GPUFlagsConstant BLUE  = 0x4;
    static constexpr GPUFlagsConstant ALPHA = 0x8;
    static constexpr GPUFlagsConstant ALL   = 0xF;
};

inline WebGPU::ColorWriteFlags convertColorWriteFlagsToBacking(GPUColorWriteFlags colorWriteFlags)
{
    WebGPU::ColorWriteFlags result;
    if (colorWriteFlags & GPUColorWrite::RED)
        result.add(WebGPU::ColorWrite::Red);
    if (colorWriteFlags & GPUColorWrite::GREEN)
        result.add(WebGPU::ColorWrite::Green);
    if (colorWriteFlags & GPUColorWrite::BLUE)
        result.add(WebGPU::ColorWrite::Blue);
    if (colorWriteFlags & GPUColorWrite::ALPHA)
        result.add(WebGPU::ColorWrite::Alpha);
    return result;
}

}
