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

NODELETE void wgpuTextureAddRef(WGPUTexture texture)
{
    WebGPU::CAPI::fromAPI(texture).ref();
}

void wgpuTextureRelease(WGPUTexture texture)
{
    @autoreleasepool {
        WebGPU::CAPI::fromAPI(texture).deref();
    }
}

WGPUTextureView wgpuTextureCreateView(WGPUTexture texture, const WGPUTextureViewDescriptor* descriptor)
{
    @autoreleasepool {
        Ref protectedTexture = WebGPU::CAPI::fromAPI(texture);
        // A null descriptor is a descriptor with all members at their defaults.
        std::optional<WebGPU::TextureViewDescriptor> apiDescriptor;
        if (descriptor) {
            apiDescriptor = WebGPU::CAPI::fromAPI(*descriptor);
            if (!apiDescriptor) {
                protect(protectedTexture->device())->generateAValidationError("GPUTextureViewDescriptor has an invalid enum value or usage bit"_s);
                return WebGPU::CAPI::releaseToAPI(protectedTexture->createInvalidView());
            }
        }
        return WebGPU::CAPI::releaseToAPI(protectedTexture->createView(apiDescriptor));
    }
}

void wgpuTextureDestroy(WGPUTexture texture)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(texture))->destroy();
    }
}

void wgpuTextureSetLabel(WGPUTexture texture, WGPUStringView label)
{
    @autoreleasepool {
        protect(WebGPU::CAPI::fromAPI(texture))->setLabel(WebGPU::CAPI::fromAPI(label));
    }
}

uint32_t wgpuTextureGetDepthOrArrayLayers(WGPUTexture texture)
{
    @autoreleasepool {
        return protect(WebGPU::CAPI::fromAPI(texture))->depthOrArrayLayers();
    }
}

WGPUTextureDimension wgpuTextureGetDimension(WGPUTexture texture)
{
    @autoreleasepool {
        return WebGPU::CAPI::toAPI(protect(WebGPU::CAPI::fromAPI(texture))->dimension());
    }
}

WGPUTextureFormat wgpuTextureGetFormat(WGPUTexture texture)
{
    @autoreleasepool {
        auto format = protect(WebGPU::CAPI::fromAPI(texture))->optionalFormat();
        return format ? WebGPU::CAPI::toAPI(*format) : WGPUTextureFormat_Undefined;
    }
}

uint32_t wgpuTextureGetHeight(WGPUTexture texture)
{
    @autoreleasepool {
        return protect(WebGPU::CAPI::fromAPI(texture))->height();
    }
}

uint32_t wgpuTextureGetWidth(WGPUTexture texture)
{
    @autoreleasepool {
        return protect(WebGPU::CAPI::fromAPI(texture))->width();
    }
}

uint32_t wgpuTextureGetMipLevelCount(WGPUTexture texture)
{
    @autoreleasepool {
        return protect(WebGPU::CAPI::fromAPI(texture))->mipLevelCount();
    }
}

uint32_t wgpuTextureGetSampleCount(WGPUTexture texture)
{
    @autoreleasepool {
        return protect(WebGPU::CAPI::fromAPI(texture))->sampleCount();
    }
}

WGPUTextureUsage wgpuTextureGetUsage(WGPUTexture texture)
{
    @autoreleasepool {
        return WebGPU::CAPI::toAPI(protect(WebGPU::CAPI::fromAPI(texture))->usage());
    }
}
