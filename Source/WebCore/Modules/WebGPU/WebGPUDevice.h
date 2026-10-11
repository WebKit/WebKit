/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include <WebCore/WebGPUCppAPI.h>
#include <wtf/CompletionHandler.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct WebGPUComputePipelineDescriptor;
struct WebGPURenderPipelineDescriptor;
struct WebGPUShaderModuleDescriptor;

// WebCore keeps these descriptors, which own their arrays. These functions create the objects from
// them through the ::WebGPU::Device, which borrows the arrays for the call.
RefPtr<::WebGPU::ShaderModule> createShaderModule(::WebGPU::Device&, const WebGPUShaderModuleDescriptor&);
RefPtr<::WebGPU::ComputePipeline> createComputePipeline(::WebGPU::Device&, const WebGPUComputePipelineDescriptor&);
RefPtr<::WebGPU::RenderPipeline> createRenderPipeline(::WebGPU::Device&, const WebGPURenderPipelineDescriptor&);
void createComputePipelineAsync(::WebGPU::Device&, const WebGPUComputePipelineDescriptor&, CompletionHandler<void(std::expected<Ref<::WebGPU::ComputePipeline>, ::WebGPU::PipelineError>&&)>&&);
void createRenderPipelineAsync(::WebGPU::Device&, const WebGPURenderPipelineDescriptor&, CompletionHandler<void(std::expected<Ref<::WebGPU::RenderPipeline>, ::WebGPU::PipelineError>&&)>&&);
void createComputePipelineWithPipelineLayoutFromPipelineAsync(::WebGPU::Device&, const WebGPUComputePipelineDescriptor&, const ::WebGPU::ComputePipeline& pipelineToReplace, CompletionHandler<void(std::expected<Ref<::WebGPU::ComputePipeline>, ::WebGPU::PipelineError>&&)>&&);
void createRenderPipelineWithPipelineLayoutFromPipelineAsync(::WebGPU::Device&, const WebGPURenderPipelineDescriptor&, const ::WebGPU::RenderPipeline& pipelineToReplace, CompletionHandler<void(std::expected<Ref<::WebGPU::RenderPipeline>, ::WebGPU::PipelineError>&&)>&&);

} // namespace WebCore
