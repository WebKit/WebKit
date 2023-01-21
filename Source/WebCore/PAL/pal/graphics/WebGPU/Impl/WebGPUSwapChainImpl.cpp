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
#include "WebGPUSurfaceImpl.h"
#include "WebGPUTextureImpl.h"
#include "WebGPUTextureViewImpl.h"
#include <WebGPU/WebGPUExt.h>
#include <wtf/CompletionHandler.h>

namespace PAL::WebGPU {

SwapChainImpl::SwapChainImpl(WGPUSwapChain swapChain, TextureFormat format, ConvertToBackingContext& convertToBackingContext)
    : m_format(format)
    , m_backing(swapChain)
    , m_convertToBackingContext(convertToBackingContext)
{
}

SwapChainImpl::~SwapChainImpl()
{
    wgpuSwapChainRelease(m_backing);
}

void SwapChainImpl::clearCurrentTextureAndView()
{
    m_currentTexture = nullptr;
    m_currentTextureView = nullptr;
}

void SwapChainImpl::ensureCurrentTextureAndView()
{
    ASSERT(static_cast<bool>(m_currentTexture) == static_cast<bool>(m_currentTextureView));

    if (m_currentTexture && m_currentTextureView)
        return;

    m_currentTexture = TextureImpl::wrap(wgpuSwapChainGetCurrentTexture(m_backing), m_format, TextureDimension::_2d, m_convertToBackingContext).ptr();
    m_currentTextureView = TextureViewImpl::wrap(wgpuSwapChainGetCurrentTextureView(m_backing), m_convertToBackingContext).ptr();
}

Texture& SwapChainImpl::getCurrentTexture()
{
    ensureCurrentTextureAndView();
    return *m_currentTexture;
}

TextureView& SwapChainImpl::getCurrentTextureView()
{
    ensureCurrentTextureAndView();
    return *m_currentTextureView;
}

void SwapChainImpl::present()
{
    wgpuSwapChainPresent(m_backing);
    clearCurrentTextureAndView();
}

void SwapChainImpl::setLabelInternal(const String&)
{
}

} // namespace PAL::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
