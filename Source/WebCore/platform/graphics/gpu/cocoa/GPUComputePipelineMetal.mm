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
#import "GPUErrorScopes.h"
#import "GPUPipelineLayout.h"
#import "GPUPipelineMetalConvertLayout.h"
#import "WHLSLPrepare.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/DataLog.h>
#import <wtf/MonotonicTime.h>
#import <wtf/text/StringConcatenate.h>

namespace WebCore {

static bool trySetMetalFunctions(MTLLibrary *computeMetalLibrary, MTLComputePipelineDescriptor *mtlDescriptor, const String& computeEntryPointName, GPUErrorScopes& errorScopes)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    if (!computeMetalLibrary) {
        errorScopes.generatePrefixedError("MTLLibrary for compute stage does not exist!");
        return false;
    }

    auto function = adoptNS([computeMetalLibrary newFunctionWithName:computeEntryPointName]);
    if (!function) {
        errorScopes.generatePrefixedError(makeString("Cannot create compute MTLFunction '", computeEntryPointName, "'!"));
        return false;
    }

    [mtlDescriptor setComputeFunction:function.get()];

    END_BLOCK_OBJC_EXCEPTIONS

    return true;
}

static Optional<WHLSL::ComputeDimensions> trySetFunctions(const GPUProgrammableStageDescriptor& computeStage, const GPUDevice& device, MTLComputePipelineDescriptor* mtlDescriptor, Optional<WHLSL::ComputePipelineDescriptor>& whlslDescriptor, GPUErrorScopes& errorScopes)
{
    RetainPtr<MTLLibrary> computeLibrary;
    String computeEntryPoint;

    WHLSL::ComputeDimensions computeDimensions { 1, 1, 1 };

    if (whlslDescriptor) {
        ASSERT(computeStage.module->whlslModule());

        whlslDescriptor->entryPointName = computeStage.entryPoint;

        auto whlslCompileResult = WHLSL::prepare(*computeStage.module->whlslModule(), *whlslDescriptor);
        if (!whlslCompileResult) {
            errorScopes.generatePrefixedError(makeString("WHLSL compile error: ", whlslCompileResult.error()));
            return WTF::nullopt;
        }

        computeDimensions = whlslCompileResult->computeDimensions;

        NSError *error = nil;

        BEGIN_BLOCK_OBJC_EXCEPTIONS
        MonotonicTime startTime;
        if (WHLSL::dumpMetalCompileTimes)
            startTime = MonotonicTime::now();
        // FIXME: https://webkit.org/b/200474 Add direct StringBuilder -> NSString conversion to avoid extra copy into a WTF::String
        computeLibrary = adoptNS([device.platformDevice() newLibraryWithSource:whlslCompileResult->metalSource.toString() options:nil error:&error]);
        if (WHLSL::dumpMetalCompileTimes)
            dataLogLn("Metal compile times: ", (MonotonicTime::now() - startTime).milliseconds(), " ms");
        END_BLOCK_OBJC_EXCEPTIONS
#ifndef NDEBUG
        if (!computeLibrary)
            NSLog(@"%@", error);
#endif
        ASSERT(computeLibrary);
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195771 Once we zero-fill variables, there should be no warnings, so we should be able to ASSERT(!error) here.

        computeEntryPoint = whlslCompileResult->mangledEntryPointName.toString();
    } else {
        computeLibrary = computeStage.module->platformShaderModule();
        computeEntryPoint = computeStage.entryPoint;
    }

    if (trySetMetalFunctions(computeLibrary.get(), mtlDescriptor, computeEntryPoint, errorScopes))
        return computeDimensions;

    return WTF::nullopt;
}

struct ConvertResult {
    RetainPtr<MTLComputePipelineDescriptor> pipelineDescriptor;
    WHLSL::ComputeDimensions computeDimensions;
};

static Optional<ConvertResult> convertComputePipelineDescriptor(const GPUComputePipelineDescriptor& descriptor, const GPUDevice& device, GPUErrorScopes& errorScopes)
{
    RetainPtr<MTLComputePipelineDescriptor> mtlDescriptor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    mtlDescriptor = adoptNS([MTLComputePipelineDescriptor new]);
    END_BLOCK_OBJC_EXCEPTIONS

    if (!mtlDescriptor) {
        errorScopes.generatePrefixedError("Error creating MTLComputePipelineDescriptor!");
        return WTF::nullopt;
    }

    const auto& computeStage = descriptor.computeStage;

    bool isWhlsl = computeStage.module->whlslModule();

    Optional<WHLSL::ComputePipelineDescriptor> whlslDescriptor;
    if (isWhlsl)
        whlslDescriptor = WHLSL::ComputePipelineDescriptor();

    if (descriptor.layout && whlslDescriptor) {
        if (auto layout = convertLayout(*descriptor.layout))
            whlslDescriptor->layout = WTFMove(*layout);
        else {
            errorScopes.generatePrefixedError("Error converting GPUPipelineLayout!");
            return WTF::nullopt;
        }
    }

    if (auto computeDimensions = trySetFunctions(computeStage, device, mtlDescriptor.get(), whlslDescriptor, errorScopes))
        return {{ mtlDescriptor, *computeDimensions }};

    return WTF::nullopt;
}

struct CreateResult {
    RetainPtr<MTLComputePipelineState> pipelineState;
    WHLSL::ComputeDimensions computeDimensions;
};

static Optional<CreateResult> tryCreateMTLComputePipelineState(const GPUDevice& device, const GPUComputePipelineDescriptor& descriptor, GPUErrorScopes& errorScopes)
{
    if (!device.platformDevice()) {
        errorScopes.generatePrefixedError("Invalid GPUDevice!");
        return WTF::nullopt;
    }

    auto convertResult = convertComputePipelineDescriptor(descriptor, device, errorScopes);
    if (!convertResult)
        return WTF::nullopt;
    ASSERT(convertResult->pipelineDescriptor);
    auto mtlDescriptor = convertResult->pipelineDescriptor;

    RetainPtr<MTLComputePipelineState> pipeline;

    BEGIN_BLOCK_OBJC_EXCEPTIONS

    NSError *error = nil;
    pipeline = adoptNS([device.platformDevice() newComputePipelineStateWithDescriptor:mtlDescriptor.get() options:MTLPipelineOptionNone reflection:nil error:&error]);
    if (!pipeline) {
        errorScopes.generatePrefixedError(error ? error.localizedDescription.UTF8String : "Unable to create MTLComputePipelineState!");
        return WTF::nullopt;
    }

    END_BLOCK_OBJC_EXCEPTIONS

    return {{ pipeline, convertResult->computeDimensions }};
}

RefPtr<GPUComputePipeline> GPUComputePipeline::tryCreate(const GPUDevice& device, const GPUComputePipelineDescriptor& descriptor, GPUErrorScopes& errorScopes)
{
    auto createResult = tryCreateMTLComputePipelineState(device, descriptor, errorScopes);
    if (!createResult)
        return nullptr;

    return adoptRef(new GPUComputePipeline(WTFMove(createResult->pipelineState), createResult->computeDimensions, descriptor.layout));
}

GPUComputePipeline::GPUComputePipeline(RetainPtr<MTLComputePipelineState>&& pipeline, WHLSL::ComputeDimensions computeDimensions, const RefPtr<GPUPipelineLayout>& layout)
    : GPUPipeline()
    , m_platformComputePipeline(WTFMove(pipeline))
    , m_computeDimensions(computeDimensions)
    , m_layout(layout)
{
}

GPUComputePipeline::~GPUComputePipeline() = default;

bool GPUComputePipeline::recompile(const GPUDevice& device, GPUProgrammableStageDescriptor&& computeStage)
{
    GPUComputePipelineDescriptor descriptor(makeRefPtr(m_layout.get()), WTFMove(computeStage));
    auto errorScopes = GPUErrorScopes::create([] (GPUError&&) { });
    if (auto createResult = tryCreateMTLComputePipelineState(device, descriptor, errorScopes)) {
        m_platformComputePipeline = WTFMove(createResult->pipelineState);
        m_computeDimensions = createResult->computeDimensions;
        return true;
    }

    return false;
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
