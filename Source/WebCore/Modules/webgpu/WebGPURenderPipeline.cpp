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

#include "config.h"
#include "WebGPURenderPipeline.h"

#if ENABLE(WEBGPU)

#include "GPUErrorScopes.h"
#include "GPUPipeline.h"
#include "GPUProgrammableStageDescriptor.h"
#include "GPURenderPipeline.h"
#include "WebGPUDevice.h"
#include <wtf/Optional.h>
#include <wtf/Ref.h>

namespace WebCore {

Ref<WebGPURenderPipeline> WebGPURenderPipeline::create(WebGPUDevice& device, RefPtr<GPURenderPipeline>&& pipeline, GPUErrorScopes& errorScopes, Optional<WebGPUPipeline::ShaderData> vertexShader, Optional<WebGPUPipeline::ShaderData> fragmentShader)
{
    return adoptRef(*new WebGPURenderPipeline(device, WTFMove(pipeline), errorScopes, vertexShader, fragmentShader));
}

WebGPURenderPipeline::WebGPURenderPipeline(WebGPUDevice& device, RefPtr<GPURenderPipeline>&& pipeline, GPUErrorScopes& errorScopes, Optional<WebGPUPipeline::ShaderData> vertexShader, Optional<WebGPUPipeline::ShaderData> fragmentShader)
    : WebGPUPipeline(device, errorScopes)
    , m_renderPipeline(WTFMove(pipeline))
    , m_vertexShader(vertexShader)
    , m_fragmentShader(fragmentShader)
{
}

WebGPURenderPipeline::~WebGPURenderPipeline() = default;

bool WebGPURenderPipeline::recompile(const WebGPUDevice& device)
{
    if (m_renderPipeline && m_vertexShader) {
        if (auto& webGPUVertexShaderModule = m_vertexShader.value().module) {
            if (auto* gpuVertexShaderModule = webGPUVertexShaderModule->module()) {
                GPUProgrammableStageDescriptor vertexStage(makeRef(*gpuVertexShaderModule), { m_vertexShader.value().entryPoint });
                Optional<GPUProgrammableStageDescriptor> fragmentStage;
                if (m_fragmentShader) {
                    if (auto& webGPUFragmentShaderModule = m_fragmentShader.value().module) {
                        if (auto* gpuFragmentShaderModule = webGPUFragmentShaderModule->module())
                            fragmentStage = GPUProgrammableStageDescriptor(makeRef(*gpuFragmentShaderModule), { m_fragmentShader.value().entryPoint });
                    }
                }
                return m_renderPipeline->recompile(device.device(), WTFMove(vertexStage), WTFMove(fragmentStage));
            }
        }
    }
    return false;
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
