/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ComputePipeline.h"

#import "APIConversions.h"
#import "BindGroupLayout.h"
#import "Device.h"
#import "Instance.h"
#import "IsValidToUseWith.h"
#import "Pipeline.h"
#import "PipelineLayout.h"
#import "ShaderModule.h"
#import "WGSL.h"
#import <wtf/Scope.h>

namespace WebGPU {

static MTLComputePipelineDescriptor *createComputePipelineDescriptor(id<MTLFunction> function, const PipelineLayout& pipelineLayout, NSString *label, GPUShaderValidation validationState)
{
    auto computePipelineDescriptor = [MTLComputePipelineDescriptor new];
#if ENABLE(WEBGPU_BY_DEFAULT)
    computePipelineDescriptor.shaderValidation = validationState;
#else
    UNUSED_PARAM(validationState);
#endif

    computePipelineDescriptor.computeFunction = function;
    for (size_t i = 0; i < pipelineLayout.numberOfBindGroupLayouts(); ++i)
        computePipelineDescriptor.buffers[i].mutability = MTLMutabilityImmutable; // Argument buffers are always immutable in WebGPU.
    computePipelineDescriptor.label = label;
    return computePipelineDescriptor;
}

static std::optional<MTLSize> metalSize(WGSL::ShaderModule& shaderModule, auto workgroupSize, const HashMap<String, WGSL::ConstantValue>& wgslConstantValues)
{
    auto width = WGSL::evaluate(shaderModule, *workgroupSize.width, wgslConstantValues);
    auto height = workgroupSize.height ? WGSL::evaluate(shaderModule, *workgroupSize.height, wgslConstantValues) : 1;
    auto depth = workgroupSize.depth ? WGSL::evaluate(shaderModule, *workgroupSize.depth, wgslConstantValues) : 1;
    if (!width.has_value() || !height.has_value() || !depth.has_value())
        return std::nullopt;

    return MTLSizeMake(width->integerValue(), height->integerValue(), depth->integerValue());
}

static std::pair<Ref<ComputePipeline>, NSString*> returnInvalidComputePipeline(WebGPU::Device &object, bool isAsync, NSString* error = nil)
{
    if (!isAsync)
        object.generateAValidationError(error ?: @"createComputePipeline failed");
    return std::make_pair(ComputePipeline::createInvalid(object), error);
}

std::pair<Ref<ComputePipeline>, NSString*> Device::createComputePipeline(const WGPUComputePipelineDescriptor& descriptor, bool isAsync, const ComputePipeline* pipelineToReplace)
{
    std::optional<std::pair<Ref<ComputePipeline>, NSString*>> result;
    createComputePipeline(descriptor, isAsync, pipelineToReplace, LibraryCompilation::Synchronous, [&](std::pair<Ref<ComputePipeline>, NSString*>&& pipelineAndError) {
        result = WTF::move(pipelineAndError);
    });
    // LibraryCompilation::Synchronous never defers the completion handler.
    RELEASE_ASSERT(result);
    return WTF::move(*result);
}

void Device::createComputePipeline(const WGPUComputePipelineDescriptor& descriptor, bool isAsync, const ComputePipeline* pipelineToReplace, LibraryCompilation libraryCompilation, CompletionHandler<void(std::pair<Ref<ComputePipeline>, NSString*>&&)>&& callback)
{
    Ref shaderModule = WebGPU::fromAPI(descriptor.compute.module);
    RefPtr<PipelineLayout> pipelineLayout;
    if (pipelineToReplace)
        pipelineLayout = &pipelineToReplace->pipelineLayout();
    else if (descriptor.layout)
        pipelineLayout = &WebGPU::fromAPI(descriptor.layout);

    if (!shaderModule->isValid() || &shaderModule->device() != this || !pipelineLayout)
        return callback(returnInvalidComputePipeline(*this, isAsync));

    if (!isValidToUseWithDevice(*pipelineLayout, *this))
        return callback(returnInvalidComputePipeline(*this, isAsync, @"GPUDevice.createComputePipeline: Pipeline layout is invalid"));

    auto& deviceLimits = limits();
    auto label = fromAPI(descriptor.label).createNSString();
    auto entryPointName = descriptor.compute.entryPoint ? fromAPI(descriptor.compute.entryPoint) : shaderModule->defaultComputeEntryPoint();
    NSError *error;
    BufferBindingSizesForPipeline minimumBufferSizes;
    auto preparedLibrary = prepareLibrary(shaderModule.get(), pipelineLayout.get(), entryPointName, label.get(), constantsSpan(descriptor.compute), minimumBufferSizes, &error);
    if (!preparedLibrary || &pipelineLayout->device() != this)
        return callback(returnInvalidComputePipeline(*this, isAsync, error.localizedDescription ?: @"Compute library failed creation"));

    const auto& wgslConstantValues = preparedLibrary->wgslConstantValues;
    const auto& entryPointInformation = preparedLibrary->entryPointInformation;

    if (!std::holds_alternative<WGSL::Reflection::Compute>(entryPointInformation.typedEntryPoint))
        return callback(returnInvalidComputePipeline(*this, isAsync));
    WGSL::Reflection::Compute computeInformation = std::get<WGSL::Reflection::Compute>(entryPointInformation.typedEntryPoint);

    if (entryPointInformation.specializationConstants.size() != wgslConstantValues.size())
        return callback(returnInvalidComputePipeline(*this, isAsync));

    auto evaluatedSize = metalSize(*shaderModule->ast(), computeInformation.workgroupSize, wgslConstantValues);
    if (!evaluatedSize)
        return callback(returnInvalidComputePipeline(*this, isAsync, @"Failed to evaluate overrides"));
    auto size = *evaluatedSize;
    if (entryPointInformation.sizeForWorkgroupVariables > deviceLimits.maxComputeWorkgroupStorageSize)
        return callback(returnInvalidComputePipeline(*this, isAsync));

    if (!size.width || size.width > deviceLimits.maxComputeWorkgroupSizeX || !size.height || size.height > deviceLimits.maxComputeWorkgroupSizeY || !size.depth || size.depth > deviceLimits.maxComputeWorkgroupSizeZ || size.width * size.height * size.depth > deviceLimits.maxComputeInvocationsPerWorkgroup)
        return callback(returnInvalidComputePipeline(*this, isAsync));

    if (m_pipelineId == Device::maxPipelines) {
        loseTheDevice(WGPUDeviceLostReason_Undefined);
        return callback(returnInvalidComputePipeline(*this, isAsync, @"too many compute pipelines"));
    }

    // Resolve the bind group layout now, while the descriptor is still guaranteed to be alive.
    Ref finalPipelineLayout = *pipelineLayout;
    if (!pipelineToReplace && pipelineLayout->isAutoLayout() && entryPointInformation.defaultLayout) {
        Vector<Vector<WGPUBindGroupLayoutEntry>> bindGroupEntries;
        if (NSString *layoutError = addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout))
            return callback(returnInvalidComputePipeline(*this, isAsync, layoutError));

        auto generatedPipelineLayout = generatePipelineLayout(bindGroupEntries);
        if (!generatedPipelineLayout->isValid())
            return callback(returnInvalidComputePipeline(*this, isAsync));
        finalPipelineLayout = WTF::move(generatedPipelineLayout);
    }

    // Claim the id up front so that concurrently compiling pipelines can't be handed the same one.
    auto pipelineId = ++m_pipelineId;
    auto compileRequest = libraryCompileRequest(*preparedLibrary);

    // Nothing below this point reads `descriptor`, so it may run after the MSL compile finishes.
    CompletionHandler<void(id<MTLLibrary>, NSError *)> finishCreation = [protectedThis = protect(*this), isAsync, suppressErrors = m_supressAllErrors, libraryCompilation, label = WTF::move(label), pipelineLayout = WTF::move(finalPipelineLayout), libraryInfo = WTF::move(*preparedLibrary), minimumBufferSizes = WTF::move(minimumBufferSizes), size, pipelineId, callback = WTF::move(callback)](id<MTLLibrary> library, NSError *libraryError) mutable {
        auto errorReporting = scopedErrorReporting(protectedThis, suppressErrors);

        if (!library)
            return callback(returnInvalidComputePipeline(protectedThis, isAsync, libraryError.localizedDescription ?: @"Compute library failed creation"));

        auto function = createFunction(library, libraryInfo.entryPointInformation, label.get());
        if (!function || function.functionType != MTLFunctionTypeKernel)
            return callback(returnInvalidComputePipeline(protectedThis, isAsync));

        // The pipeline state is the second half of the compile, and it is what a dispatch actually needs,
        // so the pipeline isn't handed back until Metal is done with it.
        auto computePipelineDescriptor = createComputePipelineDescriptor(function, pipelineLayout, label.get(), protectedThis->shaderValidationState());
        protectedThis->createComputePipelineState(computePipelineDescriptor, WTF::move(libraryInfo.msl), libraryCompilation, [protectedThis, isAsync, suppressErrors, pipelineLayout = WTF::move(pipelineLayout), minimumBufferSizes = WTF::move(minimumBufferSizes), size, pipelineId, callback = WTF::move(callback)](id<MTLComputePipelineState> computePipelineState) mutable {
            auto errorReporting = scopedErrorReporting(protectedThis, suppressErrors);

            if (!computePipelineState) {
                protectedThis->generateAnOutOfMemoryError("Compute pipeline failed compilation likely due to being too complex, please reduce its size"_s);
                return callback(returnInvalidComputePipeline(protectedThis, isAsync, @"GPUCompuePipeline could not compile"));
            }

            callback(std::make_pair(ComputePipeline::create(computePipelineState, WTF::move(pipelineLayout), size, WTF::move(minimumBufferSizes), pipelineId, protectedThis), nil));
        });
    };

    compileLibrary(compileRequest, libraryCompilation, WTF::move(finishCreation));
}

static CompletionHandler<void(std::pair<Ref<ComputePipeline>, NSString*>&&)> asyncComputePipelineCompletion(Device& device, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<ComputePipeline>&&, String&& message)>&& callback)
{
    return [protectedDevice = protect(device), callback = WTF::move(callback)](std::pair<Ref<ComputePipeline>, NSString*>&& pipelineAndError) mutable {
        auto reportResult = [protectedDevice, callback = WTF::move(callback), pipeline = WTF::move(pipelineAndError.first), message = String { pipelineAndError.second }]() mutable {
            callback((pipeline->isValid() || protectedDevice->isDestroyed()) ? WGPUCreatePipelineAsyncStatus_Success : WGPUCreatePipelineAsyncStatus_ValidationError, WTF::move(pipeline), WTF::move(message));
        };

        // Resolve on a later turn of the WebGPU thread, never re-entrantly from the caller.
        if (RefPtr protectedInstance = protectedDevice->instance()) {
            protectedInstance->scheduleWork(WTF::move(reportResult));
            return;
        }
        reportResult();
    };
}

void Device::createComputePipelineAsync(const WGPUComputePipelineDescriptor& descriptor, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<ComputePipeline>&&, String&& message)>&& callback)
{
    createComputePipeline(descriptor, true, nullptr, asynchronousIfPossible(), asyncComputePipelineCompletion(*this, WTF::move(callback)));
}

void Device::createComputePipelineWithPipelineLayoutFromPipelineAsync(const WGPUComputePipelineDescriptor& descriptor, const ComputePipeline& pipelineToReplace, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<ComputePipeline>&&, String&& message)>&& callback)
{
    bool wasErrorReportingPaused = pauseErrorReporting(true);
    createComputePipeline(descriptor, true, &pipelineToReplace, asynchronousIfPossible(), asyncComputePipelineCompletion(*this, WTF::move(callback)));
    pauseErrorReporting(wasErrorReportingPaused);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ComputePipeline);

ComputePipeline::ComputePipeline(id<MTLComputePipelineState> computePipelineState, Ref<PipelineLayout>&& pipelineLayout, MTLSize threadsPerThreadgroup, BufferBindingSizesForPipeline&& minimumBufferSizes, uint64_t uniqueId, Device& device)
    : m_computePipelineState(computePipelineState)
    , m_device(device)
    , m_threadsPerThreadgroup(threadsPerThreadgroup)
    , m_pipelineLayout(WTF::move(pipelineLayout))
    , m_minimumBufferSizes(WTF::move(minimumBufferSizes))
    , m_uniqueId(uniqueId)
{
}

ComputePipeline::ComputePipeline(Device& device)
    : m_device(device)
    , m_threadsPerThreadgroup(MTLSizeMake(0, 0, 0))
    , m_pipelineLayout(PipelineLayout::createInvalid(device))
    , m_minimumBufferSizes({ })
{
}

ComputePipeline::~ComputePipeline() = default;

Ref<BindGroupLayout> ComputePipeline::getBindGroupLayout(uint32_t groupIndex)
{
    Ref device = m_device;
    Ref pipelineLayout = m_pipelineLayout;

    if (!isValid()) {
        device->generateAValidationError("getBindGroupLayout: ComputePipeline is invalid"_s);
        pipelineLayout->makeInvalid();
        return BindGroupLayout::createInvalid(device);
    }

    if (groupIndex >= pipelineLayout->numberOfBindGroupLayouts()) {
        if (groupIndex >= device->limits().maxBindGroups) {
            device->generateAValidationError("getBindGroupLayout: groupIndex is out of range"_s);
            pipelineLayout->makeInvalid();
        }
        return BindGroupLayout::createInvalid(device);
    }

    return pipelineLayout->bindGroupLayout(groupIndex);
}

void ComputePipeline::setLabel(String&&)
{
    // MTLComputePipelineState's labels are read-only.
}

const BufferBindingSizesForBindGroup* ComputePipeline::minimumBufferSizes(uint32_t index) const
{
    auto it = m_minimumBufferSizes.find(index);
    return it == m_minimumBufferSizes.end() ? nullptr : &it->value;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void NODELETE wgpuComputePipelineAddRef(WGPUComputePipeline computePipeline)
{
    WebGPU::fromAPI(computePipeline).ref();
}

void wgpuComputePipelineRelease(WGPUComputePipeline computePipeline)
{
    WebGPU::fromAPI(computePipeline).deref();
}

WGPUBindGroupLayout wgpuComputePipelineGetBindGroupLayout(WGPUComputePipeline computePipeline, uint32_t groupIndex)
{
    return WebGPU::releaseToAPI(protect(WebGPU::fromAPI(computePipeline))->getBindGroupLayout(groupIndex));
}

void wgpuComputePipelineSetLabel(WGPUComputePipeline computePipeline, WGPUStringView label)
{
    WebGPU::fromAPI(computePipeline).setLabel(WebGPU::fromAPI(label));
}
