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

#include <WebCore/PredefinedColorSpace.h>
#include <WebCore/WebGPUCppAPI.h>

namespace WebCore {

constexpr ::WebGPU::PredefinedColorSpace convertToWebGPU(PredefinedColorSpace colorSpace)
{
    switch (colorSpace) {
    case PredefinedColorSpace::SRGB:
        return ::WebGPU::PredefinedColorSpace::SRGB;
    case PredefinedColorSpace::SRGBLinear:
        return ::WebGPU::PredefinedColorSpace::SRGBLinear;
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
    case PredefinedColorSpace::DisplayP3:
        return ::WebGPU::PredefinedColorSpace::DisplayP3;
    case PredefinedColorSpace::DisplayP3Linear:
        return ::WebGPU::PredefinedColorSpace::DisplayP3Linear;
#endif
    }
    return ::WebGPU::PredefinedColorSpace::SRGB;
}

constexpr PredefinedColorSpace convertFromWebGPU(::WebGPU::PredefinedColorSpace colorSpace)
{
    switch (colorSpace) {
    case ::WebGPU::PredefinedColorSpace::SRGB:
        return PredefinedColorSpace::SRGB;
    case ::WebGPU::PredefinedColorSpace::SRGBLinear:
        return PredefinedColorSpace::SRGBLinear;
    case ::WebGPU::PredefinedColorSpace::DisplayP3:
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        return PredefinedColorSpace::DisplayP3;
#else
        return PredefinedColorSpace::SRGB;
#endif
    case ::WebGPU::PredefinedColorSpace::DisplayP3Linear:
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        return PredefinedColorSpace::DisplayP3Linear;
#else
        return PredefinedColorSpace::SRGBLinear;
#endif
    }
    return PredefinedColorSpace::SRGB;
}

} // namespace WebCore
