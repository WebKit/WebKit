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
#import "GPUPipelineMetalConvertLayout.h"
#import "Logging.h"
#import "WHLSLPrepare.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>

namespace WebCore {

static bool trySetMetalFunctions(const char* const functionName, MTLLibrary *computeMetalLibrary, MTLComputePipelineDescriptor *mtlDescriptor, const String& computeEntryPointName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    if (!computeMetalLibrary) {
        LOG(WebGPU, "%s: MTLLibrary for compute stage does not exist!", functionName);
        return false;
    }

    auto function = adoptNS([computeMetalLibrary newFunctionWithName:computeEntryPointName]);
    if (!function) {
        LOG(WebGPU, "%s: Cannot create compute MTLFunction \"%s\"!", functionName, computeEntryPointName.utf8().data());
        return false;
    }

    [mtlDescriptor setComputeFunction:function.get()];
    return true;

    END_BLOCK_OBJC_EXCEPTIONS;

    return false;
}

static Optional<WHLSL::ComputeDimensions> trySetFunctions(const char* const functionName, const GPUPipelineStageDescriptor& computeStage, const GPUDevice& device, MTLComputePipelineDescriptor* mtlDescriptor, Optional<WHLSL::ComputePipelineDescriptor>& whlslDescriptor)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    RetainPtr<MTLLibrary> computeLibrary;
    String computeEntryPoint;

    WHLSL::ComputeDimensions computeDimensions { 1, 1, 1 };

    if (whlslDescriptor) {
        // WHLSL functions are compiled to MSL first.
        String whlslSource = computeStage.module->whlslSource();
        ASSERT(!whlslSource.isNull());

        whlslDescriptor->entryPointName = computeStage.entryPoint;

        auto whlslCompileResult = WHLSL::prepare(whlslSource, *whlslDescriptor);
        if (!whlslCompileResult)
            return WTF::nullopt;
        computeDimensions = whlslCompileResult->computeDimensions;

        NSError *error = nil;

        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        computeLibrary = adoptNS([device.platformDevice() newLibraryWithSource:whlslCompileResult->metalSource options:nil error:&error]);
        END_BLOCK_OBJC_EXCEPTIONS;
#ifndef NDEBUG
        if (!computeLibrary)
            NSLog(@"%@", error);
#endif
        ASSERT(computeLibrary);
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195771 Once we zero-fill variables, there should be no warnings, so we should be able to ASSERT(!error) here.

        computeEntryPoint = whlslCompileResult->mangledEntryPointName;
    } else {
        computeLibrary = computeStage.module->platformShaderModule();
        computeEntryPoint = computeStage.entryPoint;
    }

    if (trySetMetalFunctions(functionName, computeLibrary.get(), mtlDescriptor, computeEntryPoint))
        return computeDimensions;
    return WTF::nullopt;
}

struct ConvertResult {
    RetainPtr<MTLComputePipelineDescriptor> pipelineDescriptor;
    WHLSL::ComputeDimensions computeDimensions;
};
static Optional<ConvertResult> convertComputePipelineDescriptor(const char* const functionName, const GPUComputePipelineDescriptor& descriptor, const GPUDevice& device)
{
    RetainPtr<MTLComputePipelineDescriptor> mtlDescriptor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlDescriptor = adoptNS([MTLComputePipelineDescriptor new]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlDescriptor) {
        LOG(WebGPU, "%s: Error creating MTLDescriptor!", functionName);
        return WTF::nullopt;
    }

    const auto& computeStage = descriptor.computeStage;

    bool isWhlsl = !computeStage.module->whlslSource().isNull();

    Optional<WHLSL::ComputePipelineDescriptor> whlslDescriptor;
    if (isWhlsl)
        whlslDescriptor = WHLSL::ComputePipelineDescriptor();

    if (descriptor.layout && whlslDescriptor) {
        if (auto layout = convertLayout(*descriptor.layout))
            whlslDescriptor->layout = WTFMove(*layout);
        else {
            LOG(WebGPU, "%s: Error converting GPUPipelineLayout!", functionName);
            return WTF::nullopt;
        }
    }

    if (auto computeDimensions = trySetFunctions(functionName, computeStage, device, mtlDescriptor.get(), whlslDescriptor))
        return {{ mtlDescriptor, *computeDimensions }};

    return WTF::nullopt;
}

struct CreateResult {
    RetainPtr<MTLComputePipelineState> pipelineState;
    WHLSL::ComputeDimensions computeDimensions;
};
static Optional<CreateResult> tryCreateMTLComputePipelineState(const char* const functionName, const GPUDevice& device, const GPUComputePipelineDescriptor& descriptor)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUComputePipeline::tryCreate(): Invalid GPUDevice!");
        return WTF::nullopt;
    }

    auto convertResult = convertComputePipelineDescriptor(functionName, descriptor, device);
    if (!convertResult)
        return WTF::nullopt;
    ASSERT(convertResult->pipelineDescriptor);
    auto mtlDescriptor = convertResult->pipelineDescriptor;

    RetainPtr<MTLComputePipelineState> pipeline;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSError *error = nil;
    pipeline = adoptNS([device.platformDevice() newComputePipelineStateWithDescriptor:mtlDescriptor.get() options:MTLPipelineOptionNone reflection:nil error:&error]);
    if (!pipeline) {
        LOG(WebGPU, "GPUComputePipeline::tryCreate(): %s!", error ? error.localizedDescription.UTF8String : "Unable to create MTLComputePipelineState!");
        return WTF::nullopt;
    }

    END_BLOCK_OBJC_EXCEPTIONS;

    return {{ pipeline, convertResult->computeDimensions }};
}

RefPtr<GPUComputePipeline> GPUComputePipeline::tryCreate(const GPUDevice& device, const GPUComputePipelineDescriptor& descriptor)
{
    const char* const functionName = "GPURenderPipeline::create()";

    auto createResult = tryCreateMTLComputePipelineState(functionName, device, descriptor);
    if (!createResult)
        return nullptr;

    return adoptRef(new GPUComputePipeline(WTFMove(createResult->pipelineState), createResult->computeDimensions));
}

GPUComputePipeline::GPUComputePipeline(RetainPtr<MTLComputePipelineState>&& pipeline, WHLSL::ComputeDimensions computeDimensions)
    : m_platformComputePipeline(WTFMove(pipeline))
    , m_computeDimensions(computeDimensions)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
