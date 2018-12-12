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

#import "config.h"
#import "GPURenderPipeline.h"

#if ENABLE(WEBGPU)

#import "Logging.h"

#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

static bool setFunctionsForPipelineDescriptor(const char* const functionName, MTLRenderPipelineDescriptor *mtlDescriptor, const GPURenderPipelineDescriptor& descriptor)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    // Metal requires a vertex shader in all render pipelines.
    const auto& vertexStage = descriptor.vertexStage;
    auto mtlLibrary = vertexStage.module->platformShaderModule();

    if (!mtlLibrary) {
        LOG(WebGPU, "%s: MTLLibrary for vertex stage does not exist!", functionName);
        return false;
    }

    auto function = adoptNS([mtlLibrary newFunctionWithName:vertexStage.entryPoint]);

    if (!function) {
        LOG(WebGPU, "%s: Vertex MTLFunction \"%s\" not found!", functionName, vertexStage.entryPoint.utf8().data());
        return false;
    }

    [mtlDescriptor setVertexFunction:function.get()];

    // However, fragment shaders are optional.
    const auto fragmentStage = descriptor.fragmentStage;
    if (!fragmentStage.module || !fragmentStage.entryPoint)
        return true;

    mtlLibrary = fragmentStage.module->platformShaderModule();

    if (!mtlLibrary) {
        LOG(WebGPU, "%s: MTLLibrary for fragment stage does not exist!", functionName);
        return false;
    }

    function = adoptNS([mtlLibrary newFunctionWithName:fragmentStage.entryPoint]);

    if (!function) {
        LOG(WebGPU, "%s: Fragment MTLFunction \"%s\" not found!", functionName, fragmentStage.entryPoint.utf8().data());
        return false;
    }

    [mtlDescriptor setFragmentFunction:function.get()];

    return true;
}

RefPtr<GPURenderPipeline> GPURenderPipeline::create(const GPUDevice& device, GPURenderPipelineDescriptor&& descriptor)
{
    const char* const functionName = "GPURenderPipeline::create()";

    if (!device.platformDevice()) {
        LOG(WebGPU, "%s: Invalid GPUDevice!", functionName);
        return nullptr;
    }

    RetainPtr<MTLRenderPipelineDescriptor> mtlDescriptor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlDescriptor = adoptNS([MTLRenderPipelineDescriptor new]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlDescriptor) {
        LOG(WebGPU, "%s: Error creating MTLDescriptor!", functionName);
        return nullptr;
    }

    if (!setFunctionsForPipelineDescriptor(functionName, mtlDescriptor.get(), descriptor))
        return nullptr;

    // FIXME: Get the pixelFormat as configured for the context/CAMetalLayer.
    mtlDescriptor.get().colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    PlatformRenderPipelineSmartPtr pipeline;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    pipeline = adoptNS([device.platformDevice() newRenderPipelineStateWithDescriptor:mtlDescriptor.get() error:nil]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!pipeline) {
        LOG(WebGPU, "%s: Error creating MTLRenderPipelineState!", functionName);
        return nullptr;
    }

    return adoptRef(new GPURenderPipeline(WTFMove(pipeline), WTFMove(descriptor)));
}

GPURenderPipeline::GPURenderPipeline(PlatformRenderPipelineSmartPtr&& pipeline, GPURenderPipelineDescriptor&& descriptor)
    : m_platformRenderPipeline(WTFMove(pipeline))
    , m_descriptor(WTFMove(descriptor))
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
