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
#import "PresentationContext.h"

#import "APIConversions.h"
#import "Adapter.h"
#import "PresentationContextCoreAnimation.h"
#import "PresentationContextIOSurface.h"

namespace WebGPU {

Ref<PresentationContext> Device::createSwapChain(PresentationContext& presentationContext, const WGPUSwapChainDescriptor& descriptor)
{
    presentationContext.configure(*this, descriptor);
    return presentationContext;
}

Ref<PresentationContext> PresentationContext::create(const WGPUSurfaceDescriptor& descriptor)
{
    if (!descriptor.nextInChain || descriptor.nextInChain->next)
        return PresentationContext::createInvalid();

    switch (static_cast<unsigned>(descriptor.nextInChain->sType)) {
    case WGPUSTypeExtended_SurfaceDescriptorCocoaSurfaceBacking:
        return PresentationContextIOSurface::create(descriptor);
    case WGPUSType_SurfaceDescriptorFromMetalLayer:
        return PresentationContextCoreAnimation::create(descriptor);
    default:
        return PresentationContext::createInvalid();
    }
}

PresentationContext::PresentationContext() = default;

PresentationContext::~PresentationContext() = default;

WGPUTextureFormat PresentationContext::getPreferredFormat(const Adapter&)
{
    return WGPUTextureFormat_BGRA8Unorm;
}

void PresentationContext::configure(Device&, const WGPUSwapChainDescriptor&)
{
}

void PresentationContext::unconfigure()
{
}

void PresentationContext::present()
{
}

Texture* PresentationContext::getCurrentTexture()
{
    return nullptr;
}

TextureView* PresentationContext::getCurrentTextureView()
{
    return nullptr;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuSurfaceRelease(WGPUSurface surface)
{
    WebGPU::fromAPI(surface).deref();
}

void wgpuSwapChainRelease(WGPUSwapChain swapChain)
{
    auto& presentationContext = WebGPU::fromAPI(swapChain);

    presentationContext.unconfigure(); // It doesn't make sense to have multiple swap chains targetting a single surface.

    presentationContext.deref();
}

WGPUTextureFormat wgpuSurfaceGetPreferredFormat(WGPUSurface surface, WGPUAdapter adapter)
{
    return WebGPU::fromAPI(surface).getPreferredFormat(WebGPU::fromAPI(adapter));
}

WGPUTexture wgpuSwapChainGetCurrentTexture(WGPUSwapChain swapChain)
{
    return WebGPU::fromAPI(swapChain).getCurrentTexture();
}

WGPUTextureView wgpuSwapChainGetCurrentTextureView(WGPUSwapChain swapChain)
{
    return WebGPU::fromAPI(swapChain).getCurrentTextureView();
}

void wgpuSwapChainPresent(WGPUSwapChain swapChain)
{
    WebGPU::fromAPI(swapChain).present();
}
