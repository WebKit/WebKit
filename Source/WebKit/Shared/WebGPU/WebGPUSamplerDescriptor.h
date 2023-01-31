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

#if ENABLE(GPU_PROCESS)

#include "WebGPUObjectDescriptorBase.h"
#include <cstdint>
#include <optional>
#include <pal/graphics/WebGPU/WebGPUAddressMode.h>
#include <pal/graphics/WebGPU/WebGPUCompareFunction.h>
#include <pal/graphics/WebGPU/WebGPUFilterMode.h>

namespace WebKit::WebGPU {

struct SamplerDescriptor : public ObjectDescriptorBase {
    PAL::WebGPU::AddressMode addressModeU { PAL::WebGPU::AddressMode::ClampToEdge };
    PAL::WebGPU::AddressMode addressModeV { PAL::WebGPU::AddressMode::ClampToEdge };
    PAL::WebGPU::AddressMode addressModeW { PAL::WebGPU::AddressMode::ClampToEdge };
    PAL::WebGPU::FilterMode magFilter { PAL::WebGPU::FilterMode::Nearest };
    PAL::WebGPU::FilterMode minFilter { PAL::WebGPU::FilterMode::Nearest };
    PAL::WebGPU::MipmapFilterMode mipmapFilter { PAL::WebGPU::MipmapFilterMode::Nearest };
    float lodMinClamp { 0 };
    float lodMaxClamp { 32 };
    std::optional<PAL::WebGPU::CompareFunction> compare;
    uint16_t maxAnisotropy { 1 };
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
