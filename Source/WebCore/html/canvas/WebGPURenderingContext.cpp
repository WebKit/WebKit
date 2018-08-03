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
#include "InspectorInstrumentation.h"
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
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/TypedArrayInlines.h>
#include <JavaScriptCore/Uint32Array.h>
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

std::unique_ptr<WebGPURenderingContext> WebGPURenderingContext::create(CanvasBase& canvas)
{
    GPUDevice device;

    if (!device) {
        // FIXME: WebGPU - dispatch an event here for the failure.
        return nullptr;
    }

    auto renderingContext = std::unique_ptr<WebGPURenderingContext>(new WebGPURenderingContext(canvas, WTFMove(device)));
    renderingContext->suspendIfNeeded();

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

WebGPURenderingContext::WebGPURenderingContext(CanvasBase& canvas, GPUDevice&& device)
    : GPUBasedCanvasRenderingContext(canvas)
    , m_device(WTFMove(device))
{
    initializeNewContext();
}

HTMLCanvasElement* WebGPURenderingContext::canvas() const
{
    auto& base = canvasBase();
    if (!is<HTMLCanvasElement>(base))
        return nullptr;
    return &downcast<HTMLCanvasElement>(base);
}

void WebGPURenderingContext::initializeNewContext()
{
    // FIXME: WebGPU - Maybe we should reset a bunch of stuff here.

    IntSize canvasSize = clampedCanvasSize();
    m_device.reshape(canvasSize.width(), canvasSize.height());
}

IntSize WebGPURenderingContext::clampedCanvasSize() const
{
    return IntSize(clamp(canvas()->width(), 1, kMaxTextureSize),
        clamp(canvas()->height(), 1, kMaxTextureSize));
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
    return m_device.platformLayer();
}

void WebGPURenderingContext::markLayerComposited()
{
    m_device.markLayerComposited();
}

void WebGPURenderingContext::reshape(int width, int height)
{
    // FIXME: WebGPU - Do we need to reset stuff here?
    m_device.reshape(width, height);
}

Ref<WebGPULibrary> WebGPURenderingContext::createLibrary(const String& sourceCode)
{
    return WebGPULibrary::create(GPULibrary { m_device, sourceCode }, sourceCode);
}

Ref<WebGPURenderPipelineState> WebGPURenderingContext::createRenderPipelineState(WebGPURenderPipelineDescriptor& descriptor)
{
    return WebGPURenderPipelineState::create(GPURenderPipelineState { m_device, descriptor.descriptor() });
}

Ref<WebGPUDepthStencilState> WebGPURenderingContext::createDepthStencilState(WebGPUDepthStencilDescriptor& descriptor)
{
    return WebGPUDepthStencilState::create(GPUDepthStencilState { m_device, descriptor.descriptor() });
}

Ref<WebGPUComputePipelineState> WebGPURenderingContext::createComputePipelineState(WebGPUFunction& function)
{
    return WebGPUComputePipelineState::create(GPUComputePipelineState { m_device, function.function() });
}

Ref<WebGPUCommandQueue> WebGPURenderingContext::createCommandQueue()
{
    return WebGPUCommandQueue::create(GPUCommandQueue { m_device });
}

Ref<WebGPUDrawable> WebGPURenderingContext::nextDrawable()
{
    return WebGPUDrawable::create(GPUDrawable { m_device });
}

RefPtr<WebGPUBuffer> WebGPURenderingContext::createBuffer(JSC::ArrayBufferView& data)
{
    return WebGPUBuffer::create(GPUBuffer { m_device, data });
}

Ref<WebGPUTexture> WebGPURenderingContext::createTexture(WebGPUTextureDescriptor& descriptor)
{
    return WebGPUTexture::create(GPUTexture { m_device, descriptor.descriptor() });
}

} // namespace WebCore

#endif
