/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if ENABLE(WEBXR) && PLATFORM(COCOA)

#import <Metal/Metal.h>
#import <WebCore/WebGPUCppAPI.h>
#import <optional>

namespace WebKit {

// The formats an XRGPUBinding asks for, mapped to what a cp_layer_renderer_configuration takes.
// Shared so the format handed to the UI client and the one used to build a layer directly cannot
// drift apart.
inline MTLPixelFormat toMTLPixelFormat(::WebGPU::TextureFormat format)
{
    switch (format) {
    case ::WebGPU::TextureFormat::Bgra8unorm:
        return MTLPixelFormatBGRA8Unorm_sRGB;
    case ::WebGPU::TextureFormat::Rgba8unorm:
        return MTLPixelFormatRGBA8Unorm_sRGB;
    case ::WebGPU::TextureFormat::Rgba16float:
        return MTLPixelFormatRGBA16Float;
    case ::WebGPU::TextureFormat::Depth32float:
    case ::WebGPU::TextureFormat::Depth24plus:
        return MTLPixelFormatDepth32Float;
    case ::WebGPU::TextureFormat::Depth32floatStencil8:
    case ::WebGPU::TextureFormat::Depth24plusStencil8:
        return MTLPixelFormatDepth32Float_Stencil8;
    default:
        return MTLPixelFormatInvalid;
    }
}

inline MTLPixelFormat toMTLPixelFormat(std::optional<::WebGPU::TextureFormat> optionalFormat)
{
    return optionalFormat ? toMTLPixelFormat(*optionalFormat) : MTLPixelFormatInvalid;
}

} // namespace WebKit

#endif // ENABLE(WEBXR) && PLATFORM(COCOA)
