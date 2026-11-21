/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"

#import "CAPIConversions.h"
#import "WebGPUCppConversions.h"

NODELETE void wgpuSurfaceAddRef(WGPUSurface surface)
{
    WebGPU::CAPI::fromAPI(surface).ref();
}

void wgpuSurfaceRelease(WGPUSurface surface)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(surface).deref();
    }
}

// The capabilities of the surfaces that present into a CAMetalLayer. The arrays are static, so
// wgpuSurfaceCapabilitiesFreeMembers() has nothing to free.
WGPUStatus wgpuSurfaceGetCapabilities(WGPUSurface, WGPUAdapter, WGPUSurfaceCapabilities* capabilities)
{
    @autoreleasepool {
        static constexpr WGPUTextureFormat formats[] = { WGPUTextureFormat_BGRA8Unorm, WGPUTextureFormat_BGRA8UnormSrgb, WGPUTextureFormat_RGBA16Float };
        static constexpr WGPUPresentMode presentModes[] = { WGPUPresentMode_Fifo };
        static constexpr WGPUCompositeAlphaMode alphaModes[] = { WGPUCompositeAlphaMode_Opaque, WGPUCompositeAlphaMode_Premultiplied };
        capabilities->usages = WGPUTextureUsage_RenderAttachment | WGPUTextureUsage_CopySrc | WGPUTextureUsage_CopyDst | WGPUTextureUsage_TextureBinding;
        capabilities->formatCount = std::size(formats);
        capabilities->formats = formats;
        capabilities->presentModeCount = std::size(presentModes);
        capabilities->presentModes = presentModes;
        capabilities->alphaModeCount = std::size(alphaModes);
        capabilities->alphaModes = alphaModes;
        return WGPUStatus_Success;
    }
}

void wgpuSurfaceCapabilitiesFreeMembers(WGPUSurfaceCapabilities)
{
}

// A configuration that does not convert unconfigures the surface. WGPUCompositeAlphaMode_Auto is
// opaque, and the present mode is always FIFO.
void wgpuSurfaceConfigure(WGPUSurface surface, const WGPUSurfaceConfiguration* configuration)
{
    @autoreleasepool {
        Ref presentationContext = WebGPU::CAPI::fromAPI(surface);
        auto format = WebGPU::CAPI::fromAPI(configuration->format);
        auto usage = WebGPU::CAPI::textureUsageFromAPI(configuration->usage);
        Vector<WebGPU::TextureFormat> viewFormats;
        for (auto viewFormat : unsafeMakeSpan(configuration->viewFormats, configuration->viewFormatCount)) {
            auto apiViewFormat = WebGPU::CAPI::fromAPI(viewFormat);
            if (!apiViewFormat) {
                format = std::nullopt;
                break;
            }
            viewFormats.append(*apiViewFormat);
        }
        if (!configuration->device || !format || !usage) {
            presentationContext->unconfigure();
            return;
        }
        presentationContext->configure({
            .device = WebGPU::CAPI::fromAPI(configuration->device),
            .format = *format,
            .usage = *usage,
            .viewFormats = viewFormats.span(),
            .compositingAlphaMode = configuration->alphaMode == WGPUCompositeAlphaMode_Premultiplied ? WebGPU::CanvasAlphaMode::Premultiplied : WebGPU::CanvasAlphaMode::Opaque,
            .width = configuration->width,
            .height = configuration->height,
        });
    }
}

void wgpuSurfaceUnconfigure(WGPUSurface surface)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(surface))->unconfigure();
    }
}

// The caller owns the reference to the texture.
void wgpuSurfaceGetCurrentTexture(WGPUSurface surface, WGPUSurfaceTexture* surfaceTexture)
{
    @autoreleasepool {
        RefPtr texture = protect(WebGPU::CAPI::fromAPI(surface))->getCurrentTexture(0);
        surfaceTexture->status = texture ? WGPUSurfaceGetCurrentTextureStatus_SuccessOptimal : WGPUSurfaceGetCurrentTextureStatus_Error;
        surfaceTexture->texture = WebGPU::CAPI::releaseToAPI(WTF::move(texture));
    }
}

WGPUStatus wgpuSurfacePresent(WGPUSurface surface)
{
    @autoreleasepool {
        Ref presentationContext = WebGPU::CAPI::fromAPI(surface);
        if (!presentationContext->isValid())
            return WGPUStatus_Error;
        presentationContext->present(0);
        return WGPUStatus_Success;
    }
}

void wgpuSurfaceSetLabel(WGPUSurface surface, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(surface))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}
