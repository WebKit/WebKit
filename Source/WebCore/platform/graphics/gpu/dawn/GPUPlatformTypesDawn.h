/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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

#include <memory>
#include <webgpu/webgpu.h>

namespace WebCore {

using PlatformGPUBufferOffset = std::uint64_t;

// Dawn types
using PlatformBuffer = WGPUBufferImpl;
using PlatformCommandBuffer = WGPUCommandBufferImpl;
using PlatformComputePassEncoder = WGPUComputePassEncoderImpl;
using PlatformComputePipeline = WGPUComputePipelineImpl;
using PlatformDevice = WGPUDeviceImpl;
using PlatformDrawable = WGPUSurfaceImpl;
using PlatformProgrammablePassEncoder = WGPUCommandEncoderImpl;
using PlatformQueue = WGPUQueueImpl;
using PlatformRenderPassEncoder = WGPURenderPassEncoderImpl;
using PlatformRenderPipeline = WGPURenderPipelineImpl;
using PlatformSampler = WGPUSamplerImpl;
using PlatformShaderModule = WGPUShaderModuleImpl;
using PlatformSwapLayer = WGPUSwapChainImpl;
using PlatformTexture = WGPUTextureImpl;

template<typename T>
struct HandleDeleter {
public:
    void operator()(T* handle) = delete;
};

template<>
struct HandleDeleter<PlatformBuffer> {
    void operator()(PlatformBuffer* handle)
    {
        if (handle)
            wgpuBufferDestroy(handle);
    }
};

template<>
struct HandleDeleter<PlatformCommandBuffer> {
    void operator()(PlatformCommandBuffer* handle)
    {
        if (handle)
            wgpuCommandBufferRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformComputePassEncoder> {
    void operator()(PlatformComputePassEncoder* handle)
    {
        if (handle)
            wgpuComputePassEncoderRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformComputePipeline> {
    void operator()(PlatformComputePipeline* handle)
    {
        if (handle)
            wgpuComputePipelineRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformDevice> {
    void operator()(PlatformDevice* handle)
    {
        if (handle)
            wgpuDeviceRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformDrawable> {
    void operator()(PlatformDrawable* handle)
    {
        if (handle)
            wgpuSurfaceRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformProgrammablePassEncoder> {
    void operator()(PlatformProgrammablePassEncoder* handle)
    {
        if (handle)
            wgpuCommandEncoderRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformQueue> {
    void operator()(PlatformQueue* handle)
    {
        if (handle)
            wgpuQueueRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformRenderPassEncoder> {
    void operator()(PlatformRenderPassEncoder* handle)
    {
        if (handle)
            wgpuRenderPassEncoderRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformRenderPipeline> {
    void operator()(PlatformRenderPipeline* handle)
    {
        if (handle)
            wgpuRenderPipelineRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformSampler> {
    void operator()(PlatformSampler* handle)
    {
        if (handle)
            wgpuSamplerRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformShaderModule> {
    void operator()(PlatformShaderModule* handle)
    {
        if (handle)
            wgpuShaderModuleRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformSwapLayer> {
    void operator()(PlatformSwapLayer* handle)
    {
        if (handle)
            wgpuSwapChainRelease(handle);
    }
};

template<>
struct HandleDeleter<PlatformTexture> {
    void operator()(PlatformTexture* handle)
    {
        if (handle)
            wgpuTextureRelease(handle);
    }
};

using PlatformBufferSmartPtr = std::unique_ptr<PlatformBuffer, HandleDeleter<PlatformBuffer>>;
using PlatformCommandBufferSmartPtr = std::unique_ptr<PlatformCommandBuffer, HandleDeleter<PlatformCommandBuffer>>;
using PlatformComputePassEncoderSmartPtr = std::unique_ptr<PlatformComputePassEncoder, HandleDeleter<PlatformComputePassEncoder>>;
using PlatformComputePipelineSmartPtr = std::unique_ptr<PlatformComputePipeline, HandleDeleter<PlatformComputePipeline>>;
using PlatformDeviceSmartPtr = std::unique_ptr<PlatformDevice, HandleDeleter<PlatformDevice>>;
using PlatformDrawableSmartPtr = std::unique_ptr<PlatformDrawable, HandleDeleter<PlatformDrawable>>;
using PlatformProgrammablePassEncoderSmartPtr = std::unique_ptr<PlatformProgrammablePassEncoder, HandleDeleter<PlatformProgrammablePassEncoder>>;
using PlatformQueueSmartPtr = std::unique_ptr<PlatformQueue, HandleDeleter<PlatformQueue>>;
using PlatformRenderPassEncoderSmartPtr = std::unique_ptr<PlatformRenderPassEncoder, HandleDeleter<PlatformRenderPassEncoder>>;
using PlatformRenderPipelineSmartPtr = std::unique_ptr<PlatformRenderPipeline, HandleDeleter<PlatformRenderPipeline>>;
using PlatformSamplerSmartPtr = std::unique_ptr<PlatformSampler, HandleDeleter<PlatformSampler>>;
using PlatformShaderModuleSmartPtr = std::unique_ptr<PlatformShaderModule, HandleDeleter<PlatformShaderModule>>;
using PlatformSwapLayerSmartPtr = std::unique_ptr<PlatformSwapLayer, HandleDeleter<PlatformSwapLayer>>;
using PlatformTextureSmartPtr = std::unique_ptr<PlatformTexture, HandleDeleter<PlatformTexture>>;

} // namespace WebCore
