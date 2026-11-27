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
#import "ASTExpression.h"
#import "ASTFunction.h"
#import "ASTParameter.h"
#import "ASTStructure.h"
#import "ASTStructureMember.h"
#import "BindGroupLayout.h"
#import "Device.h"
#import "Instance.h"
#import "IsValidToUseWith.h"
#import "Pipeline.h"
#import "PipelineLayout.h"
#import "ShaderModule.h"
#import "Types.h"
#import "WGSL.h"
#import "WGSLShaderModule.h"
#import <wtf/BlockPtr.h>
#import <wtf/Scope.h>
#import <wtf/threads/BinarySemaphore.h>

namespace WebGPU::Metal {

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

static std::pair<Ref<ComputePipeline>, NSString*> returnInvalidComputePipeline(WebGPU::Metal::Device &object, bool isAsync, NSString* error = nil)
{
    if (!isAsync)
        object.generateAValidationError(error ?: @"createComputePipeline failed");
    return std::make_pair(ComputePipeline::createInvalid(object), error);
}

RefPtr<WebGPU::ComputePipeline> Device::createComputePipeline(const WebGPU::ComputePipelineDescriptor& descriptor)
{
    std::optional<std::pair<Ref<ComputePipeline>, NSString*>> result;
    createComputePipeline(descriptor, false, nullptr, LibraryCompilation::Synchronous, [&](std::pair<Ref<ComputePipeline>, NSString*>&& pipelineAndError) {
        result = WTF::move(pipelineAndError);
    });
    // LibraryCompilation::Synchronous never defers the completion handler.
    RELEASE_ASSERT(result);
    return WTF::move(result->first);
}

void Device::createComputePipeline(const WebGPU::ComputePipelineDescriptor& descriptor, bool isAsync, const ComputePipeline* pipelineToReplace, LibraryCompilation libraryCompilation, CompletionHandler<void(std::pair<Ref<ComputePipeline>, NSString*>&&)>&& callback)
{
    Ref shaderModule = metal(descriptor.compute.module.get());
    RefPtr<PipelineLayout> pipelineLayout;
    if (pipelineToReplace)
        pipelineLayout = &pipelineToReplace->pipelineLayout();
    else if (descriptor.layout)
        pipelineLayout = &metal(*descriptor.layout);

    if (!shaderModule->isValid() || &shaderModule->device() != this || !pipelineLayout)
        return callback(returnInvalidComputePipeline(*this, isAsync));
    auto ast = shaderModule->ast();
    if (!ast)
        return callback(returnInvalidComputePipeline(*this, isAsync));

    if (!isValidToUseWithDevice(*pipelineLayout, *this))
        return callback(returnInvalidComputePipeline(*this, isAsync, @"GPUDevice.createComputePipeline: Pipeline layout is invalid"));

    auto& deviceLimits = limits();
    auto label = descriptor.label.createNSString();
    auto entryPointName = descriptor.compute.entryPoint.isNull() ? shaderModule->defaultComputeEntryPoint() : descriptor.compute.entryPoint;
    NSError *error;
    BufferBindingSizesForPipeline minimumBufferSizes;
    auto preparedLibrary = prepareLibrary(shaderModule.get(), pipelineLayout.get(), entryPointName, label.get(), descriptor.compute.constants, minimumBufferSizes, &error);
    if (!preparedLibrary || &pipelineLayout->device() != this)
        return callback(returnInvalidComputePipeline(*this, isAsync, error.localizedDescription ?: @"Compute library failed creation"));

    const auto& wgslConstantValues = preparedLibrary->wgslConstantValues;
    const auto& entryPointInformation = preparedLibrary->entryPointInformation;

    if (!std::holds_alternative<WGSL::Reflection::Compute>(entryPointInformation.typedEntryPoint))
        return callback(returnInvalidComputePipeline(*this, isAsync));
    WGSL::Reflection::Compute computeInformation = std::get<WGSL::Reflection::Compute>(entryPointInformation.typedEntryPoint);

    if (entryPointInformation.specializationConstants.size() != wgslConstantValues.size())
        return callback(returnInvalidComputePipeline(*this, isAsync));

    auto evaluatedSize = metalSize(*ast, computeInformation.workgroupSize, wgslConstantValues);
    if (!evaluatedSize)
        return callback(returnInvalidComputePipeline(*this, isAsync, @"Failed to evaluate overrides"));
    auto size = *evaluatedSize;
    if (entryPointInformation.sizeForWorkgroupVariables > deviceLimits.maxComputeWorkgroupStorageSize)
        return callback(returnInvalidComputePipeline(*this, isAsync));

    if (!size.width || size.width > deviceLimits.maxComputeWorkgroupSizeX || !size.height || size.height > deviceLimits.maxComputeWorkgroupSizeY || !size.depth || size.depth > deviceLimits.maxComputeWorkgroupSizeZ || size.width * size.height * size.depth > deviceLimits.maxComputeInvocationsPerWorkgroup)
        return callback(returnInvalidComputePipeline(*this, isAsync));

    uint32_t allowedImmediateSize = (!pipelineToReplace && pipelineLayout->isAutoImmediateSize()) ? deviceLimits.maxImmediateSize : pipelineLayout->immediateSize();
    if (entryPointInformation.immediateSize > allowedImmediateSize)
        return callback(returnInvalidComputePipeline(*this, isAsync, @"Compute shader immediate data size exceeds layout or device limit"));

    uint64_t requiredImmediateSlots = entryPointInformation.requiredImmediateSlots;
    bool usesImmediates = entryPointInformation.immediateSize > 0;

    uint64_t maxLinearIndexingWorkgroups = std::numeric_limits<uint64_t>::max();
    for (auto& entryPoint : ast->callGraph().entrypoints()) {
        if (entryPoint.originalName != entryPointName)
            continue;
        bool usesWorkgroupIndex = false;
        bool usesGlobalInvocationIndex = false;
        auto checkBuiltin = [&](std::optional<WGSL::Builtin> builtin) {
            if (builtin == WGSL::Builtin::WorkgroupIndex)
                usesWorkgroupIndex = true;
            else if (builtin == WGSL::Builtin::GlobalInvocationIndex)
                usesGlobalInvocationIndex = true;
        };
        for (auto& parameter : entryPoint.function.parameters()) {
            checkBuiltin(parameter.builtin());
            if (auto* structType = std::get_if<WGSL::Types::Struct>(parameter.typeName().inferredType())) {
                for (auto& member : structType->structure.members())
                    checkBuiltin(member.builtin());
            }
        }
        if (usesWorkgroupIndex)
            maxLinearIndexingWorkgroups = std::min<uint64_t>(maxLinearIndexingWorkgroups, std::numeric_limits<uint32_t>::max());
        if (usesGlobalInvocationIndex) {
            uint64_t invocationsPerWorkgroup = static_cast<uint64_t>(size.width) * size.height * size.depth;
            if (invocationsPerWorkgroup)
                maxLinearIndexingWorkgroups = std::min<uint64_t>(maxLinearIndexingWorkgroups, std::numeric_limits<uint32_t>::max() / invocationsPerWorkgroup);
        }
        break;
    }

    if (m_pipelineId == Device::maxPipelines) {
        loseTheDevice(WebGPU::DeviceLostReason::Unknown);
        return callback(returnInvalidComputePipeline(*this, isAsync, @"too many compute pipelines"));
    }

    // Resolve the bind group layout now, while the descriptor is still guaranteed to be alive.
    Ref finalPipelineLayout = *pipelineLayout;
    if (!pipelineToReplace && pipelineLayout->isAutoLayout() && entryPointInformation.defaultLayout) {
        Vector<Vector<ResolvedBindGroupLayoutEntry>> bindGroupEntries;
        if (NSString *layoutError = addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout))
            return callback(returnInvalidComputePipeline(*this, isAsync, layoutError));

        auto generatedPipelineLayout = generatePipelineLayout(bindGroupEntries, WTF::roundUpToMultipleOf<uint32_t>(4, entryPointInformation.immediateSize));
        if (!generatedPipelineLayout->isValid())
            return callback(returnInvalidComputePipeline(*this, isAsync));
        finalPipelineLayout = WTF::move(generatedPipelineLayout);
    }

    // Claim the id up front so that concurrently compiling pipelines can't be handed the same one.
    auto pipelineId = ++m_pipelineId;
    auto compileRequest = libraryCompileRequest(*preparedLibrary);
    uint32_t bindGroupCount = static_cast<uint32_t>(finalPipelineLayout->numberOfBindGroupLayouts());
    uint32_t validationStateRaw = static_cast<uint32_t>(shaderValidationState());

    if (id<MTLLibrary> cachedLibrary = lookupCachedLibrary(m_device, compileRequest)) {
        if (id<MTLComputePipelineState> cachedPSO = lookupCachedComputePipelineState(cachedLibrary, preparedLibrary->entryPointInformation.mangledName, bindGroupCount, validationStateRaw)) {
            Ref pipeline = ComputePipeline::create(cachedPSO, WTF::move(finalPipelineLayout), size, WTF::move(minimumBufferSizes), pipelineId, maxLinearIndexingWorkgroups, *this);
            pipeline->setImmediateInfo(requiredImmediateSlots, usesImmediates);
            return callback(std::make_pair(WTF::move(pipeline), nil));
        }
    }

    if (!isAsync && libraryCompilation == LibraryCompilation::Synchronous && asynchronousIfPossible() == LibraryCompilation::Asynchronous && m_device) {
        auto pending = std::make_shared<PendingComputePipelineCompilation>();
        RetainPtr<id<MTLDevice>> retainedDevice = m_device;
        RetainPtr<NSString> retainedLabel = label;
        String mangledName = preparedLibrary->entryPointInformation.mangledName.isolatedCopy();

        compileOrCoalesceLibraryAsync(m_device, compileRequest, [pending, retainedDevice = WTF::move(retainedDevice), retainedLabel = WTF::move(retainedLabel), mangledName = WTF::move(mangledName), bindGroupCount, validationStateRaw](id<MTLLibrary> library, NSError *libraryError) mutable {
            if (!library) {
                pending->hadLibraryError = true;
                pending->libraryErrorMessage = libraryError.localizedDescription ?: @"Compute library failed creation";
                pending->complete();
                return;
            }
            compileOrCoalesceComputePSOAsync(retainedDevice.get(), library, mangledName, retainedLabel.get(), bindGroupCount, validationStateRaw, [pending](id<MTLComputePipelineState> pso, NSString *errMsg, bool isCompileFailure) mutable {
                if (!pso && !isCompileFailure) {
                    pending->hadLibraryError = true;
                    pending->libraryErrorMessage = errMsg ?: @"createComputePipeline failed";
                }
                pending->computePipelineState = pso;
                pending->complete();
            });
        });

        Ref pendingPipeline = ComputePipeline::createPending(WTF::move(pending), WTF::move(finalPipelineLayout), size, WTF::move(minimumBufferSizes), pipelineId, maxLinearIndexingWorkgroups, *this);
        pendingPipeline->setImmediateInfo(requiredImmediateSlots, usesImmediates);
        return callback(std::make_pair(WTF::move(pendingPipeline), nil));
    }

    if (isAsync && libraryCompilation == LibraryCompilation::Asynchronous && m_device) {
        if (RefPtr protectedInstance = weakInstance()) {
            using HopCallback = CompletionHandler<void(id<MTLComputePipelineState>, NSString *, bool)>;
            auto* hopToWebGPU = new HopCallback([protectedThis = protect(*this), suppressErrors = m_supressAllErrors, pipelineLayout = WTF::move(finalPipelineLayout), minimumBufferSizes = WTF::move(minimumBufferSizes), size, pipelineId, maxLinearIndexingWorkgroups, requiredImmediateSlots, usesImmediates, callback = WTF::move(callback)](id<MTLComputePipelineState> pso, NSString *errMsg, bool isOOM) mutable {
                auto errorReporting = scopedErrorReporting(protectedThis, suppressErrors);
                if (!pso) {
                    if (isOOM)
                        protectedThis->generateAnOutOfMemoryError("Compute pipeline failed compilation likely due to being too complex, please reduce its size"_s);
                    return callback(returnInvalidComputePipeline(protectedThis, true, errMsg ?: @"GPUCompuePipeline could not compile"));
                }
                Ref pipeline = ComputePipeline::create(pso, WTF::move(pipelineLayout), size, WTF::move(minimumBufferSizes), pipelineId, maxLinearIndexingWorkgroups, protectedThis);
                pipeline->setImmediateInfo(requiredImmediateSlots, usesImmediates);
                callback(std::make_pair(WTF::move(pipeline), nil));
            });

            auto finishHop = [asyncCompileInstance = AsyncCompileInstance { *protectedInstance }, hopToWebGPU](id<MTLComputePipelineState> pso, NSString *errMsg, bool isOOM) mutable {
                RetainPtr<id<MTLComputePipelineState>> retainedPSO = pso;
                RetainPtr<NSString> retainedErr = errMsg;
                auto localInstance = WTF::move(asyncCompileInstance);
                if (RefPtr instance = localInstance.take()) {
                    instance->scheduleWork([hopToWebGPU, retainedPSO = WTF::move(retainedPSO), retainedErr = WTF::move(retainedErr), isOOM] mutable {
                        std::unique_ptr<HopCallback> owned(hopToWebGPU);
                        (*owned)(retainedPSO.get(), retainedErr.get(), isOOM);
                    });
                }
            };

            RetainPtr<id<MTLDevice>> retainedDevice = m_device;
            RetainPtr<NSString> retainedLabel = label;
            String mangledName = preparedLibrary->entryPointInformation.mangledName.isolatedCopy();

            compileOrCoalesceLibraryAsync(m_device, compileRequest, [retainedDevice = WTF::move(retainedDevice), retainedLabel = WTF::move(retainedLabel), mangledName = WTF::move(mangledName), bindGroupCount, validationStateRaw, finishHop = WTF::move(finishHop)](id<MTLLibrary> library, NSError *libraryError) mutable {
                if (!library) {
                    finishHop(nil, libraryError.localizedDescription ?: @"Compute library failed creation", false);
                    return;
                }
                compileOrCoalesceComputePSOAsync(retainedDevice.get(), library, mangledName, retainedLabel.get(), bindGroupCount, validationStateRaw, [finishHop = WTF::move(finishHop)](id<MTLComputePipelineState> pso, NSString *errMsg, bool isCompileFailure) mutable {
                    if (pso)
                        finishHop(pso, nil, false);
                    else
                        finishHop(nil, errMsg ?: @"GPUCompuePipeline could not compile", isCompileFailure);
                });
            });
            return;
        }
    }

    // Nothing below this point reads `descriptor`, so it may run after the MSL compile finishes.
    String asyncMangledName = preparedLibrary->entryPointInformation.mangledName.isolatedCopy();
    RetainPtr<NSString> asyncFunctionName = asyncMangledName.createNSString();
    String asyncMSL = preparedLibrary->msl.isolatedCopy();
    CompletionHandler<void(id<MTLLibrary>, NSError *)> finishCreation = [protectedThis = protect(*this), isAsync, suppressErrors = m_supressAllErrors, libraryCompilation, label = WTF::move(label), pipelineLayout = WTF::move(finalPipelineLayout), mangledName = WTF::move(asyncMangledName), functionName = WTF::move(asyncFunctionName), msl = WTF::move(asyncMSL), minimumBufferSizes = WTF::move(minimumBufferSizes), size, pipelineId, maxLinearIndexingWorkgroups, requiredImmediateSlots, usesImmediates, bindGroupCount, validationStateRaw, callback = WTF::move(callback)](id<MTLLibrary> library, NSError *libraryError) mutable {
        auto errorReporting = scopedErrorReporting(protectedThis, suppressErrors);

        if (!library)
            return callback(returnInvalidComputePipeline(protectedThis, isAsync, libraryError.localizedDescription ?: @"Compute library failed creation"));

        if (id<MTLComputePipelineState> cachedPSO = lookupCachedComputePipelineState(library, mangledName, bindGroupCount, validationStateRaw)) {
            Ref pipeline = ComputePipeline::create(cachedPSO, WTF::move(pipelineLayout), size, WTF::move(minimumBufferSizes), pipelineId, maxLinearIndexingWorkgroups, protectedThis);
            pipeline->setImmediateInfo(requiredImmediateSlots, usesImmediates);
            return callback(std::make_pair(WTF::move(pipeline), nil));
        }

        auto function = createFunction(library, functionName.get(), label.get());
        if (!function || function.functionType != MTLFunctionTypeKernel)
            return callback(returnInvalidComputePipeline(protectedThis, isAsync));

        // The pipeline state is the second half of the compile, and it is what a dispatch actually needs,
        // so the pipeline isn't handed back until Metal is done with it.
        RetainPtr<id<MTLLibrary>> retainedLibrary = library;
        auto computePipelineDescriptor = createComputePipelineDescriptor(function, pipelineLayout, label.get(), protectedThis->shaderValidationState());
        protectedThis->createComputePipelineState(computePipelineDescriptor, WTF::move(msl), libraryCompilation, [protectedThis, isAsync, suppressErrors, retainedLibrary = WTF::move(retainedLibrary), mangledName = WTF::move(mangledName), bindGroupCount, validationStateRaw, pipelineLayout = WTF::move(pipelineLayout), minimumBufferSizes = WTF::move(minimumBufferSizes), size, pipelineId, maxLinearIndexingWorkgroups, requiredImmediateSlots, usesImmediates, callback = WTF::move(callback)](id<MTLComputePipelineState> computePipelineState) mutable {
            auto errorReporting = scopedErrorReporting(protectedThis, suppressErrors);

            if (!computePipelineState) {
                protectedThis->generateAnOutOfMemoryError("Compute pipeline failed compilation likely due to being too complex, please reduce its size"_s);
                return callback(returnInvalidComputePipeline(protectedThis, isAsync, @"GPUCompuePipeline could not compile"));
            }

            cacheComputePipelineState(retainedLibrary.get(), mangledName, bindGroupCount, validationStateRaw, computePipelineState);
            Ref pipeline = ComputePipeline::create(computePipelineState, WTF::move(pipelineLayout), size, WTF::move(minimumBufferSizes), pipelineId, maxLinearIndexingWorkgroups, protectedThis);
            pipeline->setImmediateInfo(requiredImmediateSlots, usesImmediates);
            callback(std::make_pair(WTF::move(pipeline), nil));
        });
    };

    compileLibrary(compileRequest, libraryCompilation, WTF::move(finishCreation));
}

static CompletionHandler<void(std::pair<Ref<ComputePipeline>, NSString*>&&)> asyncComputePipelineCompletion(Device& device, CompletionHandler<void(std::expected<Ref<WebGPU::ComputePipeline>, WebGPU::PipelineError>&&)>&& callback)
{
    return [protectedDevice = protect(device), callback = WTF::move(callback)](std::pair<Ref<ComputePipeline>, NSString*>&& pipelineAndError) mutable {
        auto reportResult = [protectedDevice, callback = WTF::move(callback), pipeline = WTF::move(pipelineAndError.first), message = String { pipelineAndError.second }]() mutable {
            // A lost device makes invalid objects without errors.
            if (pipeline->isValid() || protectedDevice->isDestroyed())
                return callback(Ref<WebGPU::ComputePipeline> { WTF::move(pipeline) });
            callback(makeUnexpected(WebGPU::PipelineError { .reason = WebGPU::PipelineErrorReason::Validation, .message = WTF::move(message) }));
        };

        // Resolve on a later turn of the WebGPU thread, never re-entrantly from the caller.
        if (RefPtr protectedInstance = protectedDevice->weakInstance()) {
            protectedInstance->scheduleWork(WTF::move(reportResult));
            return;
        }
        reportResult();
    };
}

void Device::createComputePipelineAsync(const WebGPU::ComputePipelineDescriptor& descriptor, CompletionHandler<void(std::expected<Ref<WebGPU::ComputePipeline>, WebGPU::PipelineError>&&)>&& callback)
{
    createComputePipeline(descriptor, true, nullptr, asynchronousIfPossible(), asyncComputePipelineCompletion(*this, WTF::move(callback)));
}

void Device::createComputePipelineWithPipelineLayoutFromPipelineAsync(const WebGPU::ComputePipelineDescriptor& descriptor, const WebGPU::ComputePipeline& pipelineToReplace, CompletionHandler<void(std::expected<Ref<WebGPU::ComputePipeline>, WebGPU::PipelineError>&&)>&& callback)
{
    bool wasErrorReportingPaused = pauseErrorReporting(true);
    createComputePipeline(descriptor, true, &downcast<ComputePipeline>(pipelineToReplace), asynchronousIfPossible(), asyncComputePipelineCompletion(*this, WTF::move(callback)));
    pauseErrorReporting(wasErrorReportingPaused);
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ComputePipeline);

ComputePipeline::ComputePipeline(id<MTLComputePipelineState> computePipelineState, std::shared_ptr<PendingComputePipelineCompilation>&& pendingCompilation, Ref<PipelineLayout>&& pipelineLayout, MTLSize threadsPerThreadgroup, BufferBindingSizesForPipeline&& minimumBufferSizes, uint64_t uniqueId, uint64_t maxLinearIndexingWorkgroups, Device& device)
    : m_computePipelineState(computePipelineState)
    , m_pendingCompilation(WTF::move(pendingCompilation))
    , m_isValid(computePipelineState || m_pendingCompilation)
    , m_device(device)
    , m_threadsPerThreadgroup(threadsPerThreadgroup)
    , m_pipelineLayout(WTF::move(pipelineLayout))
    , m_minimumBufferSizes(WTF::move(minimumBufferSizes))
    , m_uniqueId(uniqueId)
    , m_maxLinearIndexingWorkgroups(maxLinearIndexingWorkgroups)
{
}

id<MTLComputePipelineState> ComputePipeline::computePipelineState() const
{
    if (m_pendingCompilation) {
        auto pending = std::exchange(m_pendingCompilation, nullptr);
        m_computePipelineState = pending->wait();
        if (!m_computePipelineState) {
            m_isValid = false;
            if (pending->hadLibraryError)
                m_device->generateAValidationError(pending->libraryErrorMessage.get() ?: @"Compute library failed creation");
            else
                m_device->generateAnOutOfMemoryError("Compute pipeline failed compilation likely due to being too complex, please reduce its size"_s);
        }
    }
    return m_computePipelineState;
}

ComputePipeline::ComputePipeline(Device& device)
    : m_device(device)
    , m_threadsPerThreadgroup(MTLSizeMake(0, 0, 0))
    , m_pipelineLayout(PipelineLayout::createInvalid(device))
    , m_minimumBufferSizes({ })
{
}

ComputePipeline::~ComputePipeline() = default;

Ref<WebGPU::BindGroupLayout> ComputePipeline::getBindGroupLayout(uint32_t groupIndex)
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

} // namespace WebGPU::Metal
