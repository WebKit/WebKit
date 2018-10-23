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
#include "WebMetalRenderingContext.h"

#if ENABLE(WEBMETAL)

#include "Document.h"
#include "FrameView.h"
#include "GPULegacyDevice.h"
#include "InspectorInstrumentation.h"
#include "WebMetalBuffer.h"
#include "WebMetalCommandQueue.h"
#include "WebMetalComputePipelineState.h"
#include "WebMetalDepthStencilDescriptor.h"
#include "WebMetalDepthStencilState.h"
#include "WebMetalDrawable.h"
#include "WebMetalFunction.h"
#include "WebMetalLibrary.h"
#include "WebMetalRenderPassDescriptor.h"
#include "WebMetalRenderPipelineDescriptor.h"
#include "WebMetalRenderPipelineState.h"
#include "WebMetalTexture.h"
#include "WebMetalTextureDescriptor.h"
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

std::unique_ptr<WebMetalRenderingContext> WebMetalRenderingContext::create(CanvasBase& canvas)
{
    GPULegacyDevice device;

    if (!device) {
        // FIXME: WebMetal - dispatch an event here for the failure.
        return nullptr;
    }

    auto renderingContext = std::unique_ptr<WebMetalRenderingContext>(new WebMetalRenderingContext(canvas, WTFMove(device)));
    renderingContext->suspendIfNeeded();

    InspectorInstrumentation::didCreateCanvasRenderingContext(*renderingContext);

    return renderingContext;
}

WebMetalRenderingContext::WebMetalRenderingContext(CanvasBase& canvas, GPULegacyDevice&& device)
    : GPUBasedCanvasRenderingContext(canvas)
    , m_device(WTFMove(device))
{
    initializeNewContext();
}

HTMLCanvasElement* WebMetalRenderingContext::canvas() const
{
    auto& base = canvasBase();
    if (!is<HTMLCanvasElement>(base))
        return nullptr;
    return &downcast<HTMLCanvasElement>(base);
}

void WebMetalRenderingContext::initializeNewContext()
{
    // FIXME: WebMetal - Maybe we should reset a bunch of stuff here.

    IntSize canvasSize = clampedCanvasSize();
    m_device.reshape(canvasSize.width(), canvasSize.height());
}

IntSize WebMetalRenderingContext::clampedCanvasSize() const
{
    return IntSize(clamp(canvas()->width(), 1, kMaxTextureSize),
        clamp(canvas()->height(), 1, kMaxTextureSize));
}

bool WebMetalRenderingContext::hasPendingActivity() const
{
    return false;
}

void WebMetalRenderingContext::stop()
{
}

const char* WebMetalRenderingContext::activeDOMObjectName() const
{
    return "WebMetalRenderingContext";
}

bool WebMetalRenderingContext::canSuspendForDocumentSuspension() const
{
    return false;
}

PlatformLayer* WebMetalRenderingContext::platformLayer() const
{
    return m_device.platformLayer();
}

void WebMetalRenderingContext::markLayerComposited()
{
    m_device.markLayerComposited();
}

void WebMetalRenderingContext::reshape(int width, int height)
{
    // FIXME: WebMetal - Do we need to reset stuff here?
    m_device.reshape(width, height);
}

Ref<WebMetalLibrary> WebMetalRenderingContext::createLibrary(const String& sourceCode)
{
    return WebMetalLibrary::create(GPULegacyLibrary { m_device, sourceCode }, sourceCode);
}

Ref<WebMetalRenderPipelineState> WebMetalRenderingContext::createRenderPipelineState(WebMetalRenderPipelineDescriptor& descriptor)
{
    return WebMetalRenderPipelineState::create(GPULegacyRenderPipelineState { m_device, descriptor.descriptor() });
}

Ref<WebMetalDepthStencilState> WebMetalRenderingContext::createDepthStencilState(WebMetalDepthStencilDescriptor& descriptor)
{
    return WebMetalDepthStencilState::create(GPULegacyDepthStencilState { m_device, descriptor.descriptor() });
}

Ref<WebMetalComputePipelineState> WebMetalRenderingContext::createComputePipelineState(WebMetalFunction& function)
{
    return WebMetalComputePipelineState::create(GPULegacyComputePipelineState { m_device, function.function() });
}

Ref<WebMetalCommandQueue> WebMetalRenderingContext::createCommandQueue()
{
    return WebMetalCommandQueue::create(GPULegacyCommandQueue { m_device });
}

Ref<WebMetalDrawable> WebMetalRenderingContext::nextDrawable()
{
    return WebMetalDrawable::create(GPULegacyDrawable { m_device });
}

RefPtr<WebMetalBuffer> WebMetalRenderingContext::createBuffer(JSC::ArrayBufferView& data)
{
    return WebMetalBuffer::create(GPULegacyBuffer { m_device, data });
}

Ref<WebMetalTexture> WebMetalRenderingContext::createTexture(WebMetalTextureDescriptor& descriptor)
{
    return WebMetalTexture::create(GPULegacyTexture { m_device, descriptor.descriptor() });
}

} // namespace WebCore

#endif
