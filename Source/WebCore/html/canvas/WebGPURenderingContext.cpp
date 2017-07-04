/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
 * Copyright (C) 2017 Yuichiro Kikura (y.kikura@gmail.com)
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

#include "config.h"
#include "WebGPURenderingContext.h"

#if ENABLE(WEBGPU)

#include "Document.h"
#include "FrameView.h"
#include "GPUDevice.h"
#include "WebGPUBuffer.h"
#include "WebGPUCommandQueue.h"
#include "WebGPUComputePipelineState.h"
#include "WebGPUDepthStencilDescriptor.h"
#include "WebGPUDepthStencilState.h"
#include "WebGPUDrawable.h"
#include "WebGPUFunction.h"
#include "WebGPULibrary.h"
#include "WebGPURenderPassDescriptor.h"
#include "WebGPURenderPipelineDescriptor.h"
#include "WebGPURenderPipelineState.h"
#include "WebGPUTexture.h"
#include "WebGPUTextureDescriptor.h"

#include <runtime/ArrayBuffer.h>
#include <runtime/JSCInlines.h>
#include <runtime/TypedArrayInlines.h>
#include <runtime/Uint32Array.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace {

int clamp(int value, int min, int max)
{
    if (value < min)
        value = min;
    if (value > max)
        value = max;
    return value;
}

}

namespace WebCore {

static const int kMaxTextureSize = 4096;


std::unique_ptr<WebGPURenderingContext> WebGPURenderingContext::create(HTMLCanvasElement& canvas)
{
    RefPtr<GPUDevice> device(GPUDevice::create());

    if (!device) {
        // FIXME: WebGPU - dispatch an event here for the failure.
        return nullptr;
    }

    std::unique_ptr<WebGPURenderingContext> renderingContext = nullptr;
    renderingContext = std::unique_ptr<WebGPURenderingContext>(new WebGPURenderingContext(canvas, device.releaseNonNull()));
    renderingContext->suspendIfNeeded();

    return renderingContext;
}

WebGPURenderingContext::WebGPURenderingContext(HTMLCanvasElement& canvas, Ref<GPUDevice>&& device)
    : GPUBasedCanvasRenderingContext(canvas)
    , m_device(WTFMove(device))
{
    initializeNewContext();
}

void WebGPURenderingContext::initializeNewContext()
{
    // FIXME: WebGPU - Maybe we should reset a bunch of stuff here.

    IntSize canvasSize = clampedCanvasSize();
    m_device->reshape(canvasSize.width(), canvasSize.height());
}

IntSize WebGPURenderingContext::clampedCanvasSize() const
{
    return IntSize(clamp(canvas().width(), 1, kMaxTextureSize),
        clamp(canvas().height(), 1, kMaxTextureSize));
}

bool WebGPURenderingContext::hasPendingActivity() const
{
    return false;
}

void WebGPURenderingContext::stop()
{
}

const char* WebGPURenderingContext::activeDOMObjectName() const
{
    return "WebGPURenderingContext";
}

bool WebGPURenderingContext::canSuspendForDocumentSuspension() const
{
    return false;
}

PlatformLayer* WebGPURenderingContext::platformLayer() const
{
    return m_device->platformLayer();
}

void WebGPURenderingContext::markLayerComposited()
{
    m_device->markLayerComposited();
}

void WebGPURenderingContext::reshape(int width, int height)
{
    // FIXME: WebGPU - Do we need to reset stuff here?
    m_device->reshape(width, height);
}

RefPtr<WebGPULibrary> WebGPURenderingContext::createLibrary(const String sourceCode)
{
    RefPtr<WebGPULibrary> library = WebGPULibrary::create(this, sourceCode);
    return library;
}

RefPtr<WebGPURenderPipelineState> WebGPURenderingContext::createRenderPipelineState(WebGPURenderPipelineDescriptor& descriptor)
{
    RefPtr<WebGPURenderPipelineState> state = WebGPURenderPipelineState::create(this, &descriptor);
    return state;
}

RefPtr<WebGPUDepthStencilState> WebGPURenderingContext::createDepthStencilState(WebGPUDepthStencilDescriptor& descriptor)
{
    RefPtr<WebGPUDepthStencilState> state = WebGPUDepthStencilState::create(this, &descriptor);
    return state;
}

RefPtr<WebGPUComputePipelineState> WebGPURenderingContext::createComputePipelineState(WebGPUFunction& function)
{
    RefPtr<WebGPUComputePipelineState> state = WebGPUComputePipelineState::create(this, &function);
    return state;
}

RefPtr<WebGPUCommandQueue> WebGPURenderingContext::createCommandQueue()
{
    RefPtr<WebGPUCommandQueue> queue = WebGPUCommandQueue::create(this);
    return queue;
}

RefPtr<WebGPUDrawable> WebGPURenderingContext::nextDrawable()
{
    RefPtr<WebGPUDrawable> drawable = WebGPUDrawable::create(this);
    return drawable;
}

RefPtr<WebGPUBuffer> WebGPURenderingContext::createBuffer(ArrayBufferView& data)
{
    RefPtr<WebGPUBuffer> buffer = WebGPUBuffer::create(this, &data);
    return buffer;
}

RefPtr<WebGPUTexture> WebGPURenderingContext::createTexture(WebGPUTextureDescriptor& descriptor)
{
    RefPtr<WebGPUTexture> texture = WebGPUTexture::create(this, &descriptor);
    return texture;
}

} // namespace WebCore

#endif
