/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#include "WebGPUComputePipeline.h"

#if ENABLE(WEBGPU)

#include "GPUComputePipeline.h"
#include "GPUErrorScopes.h"
#include "GPUPipeline.h"
#include "GPUProgrammableStageDescriptor.h"
#include "GPUShaderModule.h"
#include "GPUShaderModuleDescriptor.h"
#include "WebGPUDevice.h"
#include "WebGPUShaderModule.h"
#include <wtf/Optional.h>
#include <wtf/Ref.h>

namespace WebCore {

Ref<WebGPUComputePipeline> WebGPUComputePipeline::create(WebGPUDevice& device, RefPtr<GPUComputePipeline>&& pipeline, GPUErrorScopes& errorScopes, WebGPUPipeline::ShaderData&& computeShader)
{
    return adoptRef(*new WebGPUComputePipeline(device, WTFMove(pipeline), errorScopes, WTFMove(computeShader)));
}

WebGPUComputePipeline::WebGPUComputePipeline(WebGPUDevice& device, RefPtr<GPUComputePipeline>&& pipeline, GPUErrorScopes& errorScopes, WebGPUPipeline::ShaderData&& computeShader)
    : WebGPUPipeline(device, errorScopes)
    , m_computePipeline(WTFMove(pipeline))
    , m_computeShader(WTFMove(computeShader))
{
}

WebGPUComputePipeline::~WebGPUComputePipeline() = default;

bool WebGPUComputePipeline::cloneShaderModules(const WebGPUDevice& device)
{
    if (m_computeShader.module) {
        const auto& computeSource = m_computeShader.module->source();
        m_computeShader.module = WebGPUShaderModule::create(GPUShaderModule::tryCreate(device.device(), { computeSource }), computeSource);
        return true;
    }
    return false;
}

bool WebGPUComputePipeline::recompile(const WebGPUDevice& device)
{
    if (m_computePipeline && m_computeShader.module) {
        if (auto* gpuComputeShaderModule = m_computeShader.module->module()) {
            GPUProgrammableStageDescriptor computeStage(makeRef(*gpuComputeShaderModule), { m_computeShader.entryPoint });
            return m_computePipeline->recompile(device.device(), WTFMove(computeStage));
        }
    }
    return false;
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
