/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
#import "SwapChain.h"

#import "Device.h"
#import "TextureView.h"
#import "WebGPUExt.h"

namespace WebGPU {

RefPtr<SwapChain> Device::createSwapChain(const Surface& surface, const WGPUSwapChainDescriptor* descriptor)
{
    UNUSED_PARAM(surface);
    UNUSED_PARAM(descriptor);
    return SwapChain::create();
}

SwapChain::SwapChain() = default;

SwapChain::~SwapChain() = default;

Ref<TextureView> SwapChain::getCurrentTextureView()
{
    return TextureView::create();
}

void SwapChain::present()
{
}

} // namespace WebGPU

void wgpuSwapChainRelease(WGPUSwapChain swapChain)
{
    delete swapChain;
}

WGPUTextureView wgpuSwapChainGetCurrentTextureView(WGPUSwapChain swapChain)
{
    return new WGPUTextureViewImpl { swapChain->swapChain->getCurrentTextureView() };
}

void wgpuSwapChainPresent(WGPUSwapChain swapChain)
{
    swapChain->swapChain->present();
}

