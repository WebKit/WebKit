/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#if ENABLE(WEBGPU)

#include "GPUDevice.h"
#include "WebGPUAdapter.h"
#include "WebGPUBindGroupLayoutDescriptor.h"
#include "WebGPUBufferDescriptor.h"
#include "WebGPUQueue.h"

#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class ScriptExecutionContext;
class WebGPUBindGroupLayout;
class WebGPUBuffer;
class WebGPUCommandBuffer;
class WebGPUPipelineLayout;
class WebGPURenderPipeline;
class WebGPUShaderModule;

struct WebGPUPipelineLayoutDescriptor;
struct WebGPURenderPipelineDescriptor;
struct WebGPUShaderModuleDescriptor;

class WebGPUDevice : public RefCounted<WebGPUDevice> {
public:
    static RefPtr<WebGPUDevice> create(Ref<WebGPUAdapter>&&);

    const WebGPUAdapter& adapter() const { return m_adapter.get(); }
    const GPUDevice& device() const { return m_device.get(); }

    RefPtr<WebGPUBuffer> createBuffer(WebGPUBufferDescriptor&&) const;

    Ref<WebGPUBindGroupLayout> createBindGroupLayout(WebGPUBindGroupLayoutDescriptor&&) const;
    Ref<WebGPUPipelineLayout> createPipelineLayout(WebGPUPipelineLayoutDescriptor&&) const;

    RefPtr<WebGPUShaderModule> createShaderModule(WebGPUShaderModuleDescriptor&&) const;
    RefPtr<WebGPURenderPipeline> createRenderPipeline(WebGPURenderPipelineDescriptor&&) const;

    RefPtr<WebGPUCommandBuffer> createCommandBuffer() const;
    RefPtr<WebGPUQueue> getQueue();

private:
    WebGPUDevice(Ref<WebGPUAdapter>&&, Ref<GPUDevice>&&);

    Ref<WebGPUAdapter> m_adapter;
    Ref<GPUDevice> m_device;
    RefPtr<WebGPUQueue> m_queue;
};

} // namespace WebCore

#endif // ENABLE(WEBGPU)
