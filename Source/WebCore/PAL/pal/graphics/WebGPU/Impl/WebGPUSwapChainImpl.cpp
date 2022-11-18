/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebGPUSwapChainImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUConvertToBackingContext.h"
#include "WebGPUSurfaceDescriptor.h"
#include "WebGPUSurfaceImpl.h"
#include <WebGPU/WebGPUExt.h>
#include <wtf/CompletionHandler.h>

namespace PAL::WebGPU {

SwapChainImpl::SwapChainImpl(WGPUSurface surface, WGPUSwapChain swapChain)
    : m_backing(swapChain)
    , m_surface(surface)
{
}

SwapChainImpl::~SwapChainImpl()
{
    wgpuSwapChainRelease(m_backing);
}

void SwapChainImpl::destroy()
{
    wgpuSwapChainRelease(m_backing);
}

void SwapChainImpl::prepareForDisplay(CompletionHandler<void(WTF::MachSendRight&&)>&& completionHandler)
{
    wgpuSwapChainPresent(m_backing);
    auto ioSurface = wgpuSurfaceCocoaCustomSurfaceGetDisplayBuffer(m_surface);
    completionHandler(MachSendRight::adopt(IOSurfaceCreateMachPort(ioSurface)));
}

void SwapChainImpl::setLabelInternal(const String& label)
{
    UNUSED_PARAM(label);
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
