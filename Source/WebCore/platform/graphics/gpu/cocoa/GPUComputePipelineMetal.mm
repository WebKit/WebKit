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

#import "config.h"
#import "GPUComputePipeline.h"

#if ENABLE(WEBGPU)

#import "GPUComputePipelineDescriptor.h"
#import "GPUDevice.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

OBJC_PROTOCOL(MTLFunction);

namespace WebCore {

static RetainPtr<MTLFunction> tryCreateMtlComputeFunction(const GPUPipelineStageDescriptor& stage)
{
    if (!stage.module->platformShaderModule() || stage.entryPoint.isNull()) {
        LOG(WebGPU, "GPUComputePipeline::tryCreate(): Invalid GPUShaderModule!");
        return nullptr;
    }

    RetainPtr<MTLFunction> function;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    function = adoptNS([stage.module->platformShaderModule() newFunctionWithName:stage.entryPoint]);
    END_BLOCK_OBJC_EXCEPTIONS;

    if (!function)
        LOG(WebGPU, "GPUComputePipeline::tryCreate(): Cannot create compute MTLFunction \"%s\"!", stage.entryPoint.utf8().data());

    return function;
}

static RetainPtr<MTLComputePipelineState> tryCreateMTLComputePipelineState(const GPUDevice& device, const GPUComputePipelineDescriptor& descriptor)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUComputePipeline::tryCreate(): Invalid GPUDevice!");
        return nullptr;
    }

    auto computeFunction = tryCreateMtlComputeFunction(descriptor.computeStage);
    if (!computeFunction)
        return nullptr;

    RetainPtr<MTLComputePipelineState> pipelineState;
    NSError *error = nil;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    pipelineState = adoptNS([device.platformDevice() newComputePipelineStateWithFunction:computeFunction.get() error:&error]);
    END_BLOCK_OBJC_EXCEPTIONS;

    if (!pipelineState)
        LOG(WebGPU, "GPUComputePipeline::tryCreate(): %s!", error ? error.localizedDescription.UTF8String : "Unable to create MTLComputePipelineState!");

    return pipelineState;
}

RefPtr<GPUComputePipeline> GPUComputePipeline::tryCreate(const GPUDevice& device, const GPUComputePipelineDescriptor& descriptor)
{
    auto mtlPipeline = tryCreateMTLComputePipelineState(device, descriptor);
    if (!mtlPipeline)
        return nullptr;

    return adoptRef(new GPUComputePipeline(WTFMove(mtlPipeline)));
}

GPUComputePipeline::GPUComputePipeline(RetainPtr<MTLComputePipelineState>&& pipeline)
    : m_platformComputePipeline(WTFMove(pipeline))
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
