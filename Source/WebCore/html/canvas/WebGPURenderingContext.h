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

#if ENABLE(WEBGPU)

#include "GPUBasedCanvasRenderingContext.h"
#include "GPUDevice.h"
#include <runtime/ArrayBuffer.h>

namespace WebCore {

class WebGPUCommandQueue;
class WebGPUComputePipelineState;
class WebGPUFunction;
class WebGPULibrary;
class WebGPURenderPipelineDescriptor;
class WebGPURenderPipelineState;
class WebGPUDepthStencilDescriptor;
class WebGPUDepthStencilState;
class WebGPURenderPassDescriptor;
class WebGPUDrawable;
class WebGPUBuffer;
class WebGPUTexture;
class WebGPUTextureDescriptor;

class WebGPURenderingContext : public GPUBasedCanvasRenderingContext {
public:
    static std::unique_ptr<WebGPURenderingContext> create(HTMLCanvasElement&);

    bool isWebGPU() const final { return true; }

    void reshape(int width, int height) final;

    void markLayerComposited() final;

    PlatformLayer* platformLayer() const override;

    RefPtr<GPUDevice> device() { return m_device; }

    // WebGPU entry points

    RefPtr<WebGPULibrary> createLibrary(const String);

    RefPtr<WebGPURenderPipelineState> createRenderPipelineState(WebGPURenderPipelineDescriptor&);

    RefPtr<WebGPUDepthStencilState> createDepthStencilState(WebGPUDepthStencilDescriptor&);

    RefPtr<WebGPUComputePipelineState> createComputePipelineState(WebGPUFunction&);

    RefPtr<WebGPUCommandQueue> createCommandQueue();

    RefPtr<WebGPUDrawable> nextDrawable();

    RefPtr<WebGPUBuffer> createBuffer(ArrayBufferView& data);

    RefPtr<WebGPUTexture> createTexture(WebGPUTextureDescriptor&);

protected:
    // ActiveDOMObject
    bool hasPendingActivity() const override;
    void stop() override;
    const char* activeDOMObjectName() const override;
    bool canSuspendForDocumentSuspension() const override;

    IntSize clampedCanvasSize() const;
    void initializeNewContext();

private:
    WebGPURenderingContext(HTMLCanvasElement&, Ref<GPUDevice>&&);

    RefPtr<GPUDevice> m_device;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CANVASRENDERINGCONTEXT(WebCore::WebGPURenderingContext, isWebGPU())

#endif
