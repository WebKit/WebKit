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

#include "WebGPUPipeline.h"
#include <wtf/Forward.h>

namespace WebCore {

class GPUPipeline;
class GPURenderPipeline;
class GPUErrorScopes;
class WebGPUDevice;
class WebGPUShaderModule;

class WebGPURenderPipeline final : public WebGPUPipeline {
public:
    virtual ~WebGPURenderPipeline();

    static Ref<WebGPURenderPipeline> create(WebGPUDevice&, RefPtr<GPURenderPipeline>&&, GPUErrorScopes&, WebGPUPipeline::ShaderData&& vertexShader, WebGPUPipeline::ShaderData&& fragmentShader);

    bool isRenderPipeline() const { return true; }

    bool isValid() const { return renderPipeline(); }
    const GPURenderPipeline* renderPipeline() const { return m_renderPipeline.get(); }
    RefPtr<WebGPUShaderModule> vertexShader() const { return m_vertexShader.module; }
    RefPtr<WebGPUShaderModule> fragmentShader() const { return m_fragmentShader.module; }

    bool cloneShaderModules(const WebGPUDevice&);
    bool recompile(const WebGPUDevice&);

private:
    WebGPURenderPipeline(WebGPUDevice&, RefPtr<GPURenderPipeline>&&, GPUErrorScopes&, WebGPUPipeline::ShaderData&& vertexShader, WebGPUPipeline::ShaderData&& fragmentShader);

    RefPtr<GPURenderPipeline> m_renderPipeline;

    // Preserved for Web Inspector recompilation.
    WebGPUPipeline::ShaderData m_vertexShader;
    WebGPUPipeline::ShaderData m_fragmentShader;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_WEBGPUPIPELINE(WebCore::WebGPURenderPipeline, isRenderPipeline())

#endif // ENABLE(WEBGPU)
