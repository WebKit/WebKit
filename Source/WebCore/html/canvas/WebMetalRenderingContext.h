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

#pragma once

#if ENABLE(WEBMETAL)

#include "GPUBasedCanvasRenderingContext.h"
#include "GPULegacyDevice.h"

namespace JSC {
class ArrayBufferView;
}

namespace WebCore {

class WebMetalBuffer;
class WebMetalCommandQueue;
class WebMetalComputePipelineState;
class WebMetalDepthStencilDescriptor;
class WebMetalDepthStencilState;
class WebMetalDrawable;
class WebMetalFunction;
class WebMetalLibrary;
class WebMetalRenderPipelineDescriptor;
class WebMetalRenderPipelineState;
class WebMetalTexture;
class WebMetalTextureDescriptor;

class WebMetalRenderingContext final : public GPUBasedCanvasRenderingContext {
public:
    static std::unique_ptr<WebMetalRenderingContext> create(CanvasBase&);

    // FIXME: IDL file says this is not nullable, but this function can return null.
    HTMLCanvasElement* canvas() const;

    Ref<WebMetalLibrary> createLibrary(const String&);
    Ref<WebMetalRenderPipelineState> createRenderPipelineState(WebMetalRenderPipelineDescriptor&);
    Ref<WebMetalDepthStencilState> createDepthStencilState(WebMetalDepthStencilDescriptor&);
    Ref<WebMetalComputePipelineState> createComputePipelineState(WebMetalFunction&);
    Ref<WebMetalCommandQueue> createCommandQueue();
    Ref<WebMetalDrawable> nextDrawable();
    RefPtr<WebMetalBuffer> createBuffer(JSC::ArrayBufferView&);
    Ref<WebMetalTexture> createTexture(WebMetalTextureDescriptor&);

    const GPULegacyDevice& device() const { return m_device; }

private:
    WebMetalRenderingContext(CanvasBase&, GPULegacyDevice&&);

    bool hasPendingActivity() const final;
    void stop() final;
    const char* activeDOMObjectName() const final;
    bool canSuspendForDocumentSuspension() const final;

    IntSize clampedCanvasSize() const;
    void initializeNewContext();

    bool isWebMetal() const final { return true; }

    void reshape(int width, int height) final;
    void markLayerComposited() final;
    PlatformLayer* platformLayer() const final;

    GPULegacyDevice m_device;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebMetalRenderingContext, isWebMetal())

#endif
