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

#include "GPUIntegralTypes.h"
#include <cstdint>
#include <pal/graphics/WebGPU/WebGPUMapMode.h>
#include <wtf/RefCounted.h>

namespace WebCore {

using GPUMapModeFlags = uint32_t;

class GPUMapMode : public RefCounted<GPUMapMode> {
public:
    static constexpr GPUFlagsConstant READ  = 0x0001;
    static constexpr GPUFlagsConstant WRITE = 0x0002;
};

inline PAL::WebGPU::MapModeFlags convertMapModeFlagsToBacking(GPUMapModeFlags mapModeFlags)
{
    PAL::WebGPU::MapModeFlags result;
    if (mapModeFlags & GPUMapMode::READ)
        result.add(PAL::WebGPU::MapMode::Read);
    if (mapModeFlags & GPUMapMode::WRITE)
        result.add(PAL::WebGPU::MapMode::Write);
    return result;
}

}
