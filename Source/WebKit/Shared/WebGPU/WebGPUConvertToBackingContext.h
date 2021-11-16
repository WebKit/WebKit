/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

#include "WebGPUIdentifier.h"
#include <wtf/RefCounted.h>

namespace PAL::WebGPU {

class Adapter;
class BindGroup;
class BindGroupLayout;
class Buffer;
class CommandBuffer;
class CommandEncoder;
class ComputePassEncoder;
class ComputePipeline;
class Device;
class GPU;
class PipelineLayout;
class QuerySet;
class Queue;
class RenderBundleEncoder;
class RenderBundle;
class RenderPassEncoder;
class RenderPipeline;
class Sampler;
class ShaderModule;
class Texture;
class TextureView;

} // namespace PAL::WebGPU

namespace WebKit::WebGPU {

class ConvertToBackingContext : public RefCounted<ConvertToBackingContext> {
public:
    virtual ~ConvertToBackingContext() = default;

    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Adapter&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::BindGroup&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::BindGroupLayout&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Buffer&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::CommandBuffer&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::CommandEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::ComputePassEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::ComputePipeline&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Device&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::GPU&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::PipelineLayout&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::QuerySet&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Queue&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderBundleEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderBundle&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderPassEncoder&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::RenderPipeline&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Sampler&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::ShaderModule&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::Texture&) = 0;
    virtual WebGPUIdentifier convertToBacking(const PAL::WebGPU::TextureView&) = 0;
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
