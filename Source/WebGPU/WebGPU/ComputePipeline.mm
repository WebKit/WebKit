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
#import "Pipeline.h"
#import "PipelineLayout.h"
#import "ShaderModule.h"

namespace WebGPU {

static id<MTLComputePipelineState> createComputePipelineState(id<MTLDevice> device, id<MTLFunction> function, const PipelineLayout& pipelineLayout, const MTLSize& size, NSString *label)
{
    auto computePipelineDescriptor = [MTLComputePipelineDescriptor new];
#if ENABLE(WEBGPU_BY_DEFAULT)
    computePipelineDescriptor.shaderValidation = MTLShaderValidationEnabled;
#endif

    computePipelineDescriptor.computeFunction = function;
    computePipelineDescriptor.maxTotalThreadsPerThreadgroup = size.width * size.height * size.depth;
    for (size_t i = 0; i < pipelineLayout.numberOfBindGroupLayouts(); ++i)
        computePipelineDescriptor.buffers[i].mutability = MTLMutabilityImmutable; // Argument buffers are always immutable in WebGPU.
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=249345 don't unconditionally set this to YES
    computePipelineDescriptor.supportIndirectCommandBuffers = YES;
    computePipelineDescriptor.label = label;
    NSError *error = nil;
    // FIXME: Run the asynchronous version of this
    id<MTLComputePipelineState> computePipelineState = [device newComputePipelineStateWithDescriptor:computePipelineDescriptor options:MTLPipelineOptionNone reflection:nil error:&error];

    if (error)
        WTFLogAlways("Pipeline state creation error: %@", error);
    return computePipelineState;
}

static std::optional<MTLSize> metalSize(auto workgroupSize, const HashMap<String, WGSL::ConstantValue>& wgslConstantValues)
{
    auto width = WGSL::evaluate(*workgroupSize.width, wgslConstantValues);
    auto height = workgroupSize.height ? WGSL::evaluate(*workgroupSize.height, wgslConstantValues) : 1;
    auto depth = workgroupSize.depth ? WGSL::evaluate(*workgroupSize.depth, wgslConstantValues) : 1;
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

std::pair<Ref<ComputePipeline>, NSString*> Device::createComputePipeline(const WGPUComputePipelineDescriptor& descriptor, bool isAsync)
{
    if (descriptor.nextInChain || descriptor.compute.nextInChain)
        return returnInvalidComputePipeline(*this, isAsync);

    ShaderModule& shaderModule = WebGPU::protectedFromAPI(descriptor.compute.module);
    if (!shaderModule.isValid() || &shaderModule.device() != this || !descriptor.layout)
        return returnInvalidComputePipeline(*this, isAsync);

    PipelineLayout& pipelineLayout = WebGPU::protectedFromAPI(descriptor.layout);
    auto& deviceLimits = limits();
    auto label = fromAPI(descriptor.label);
    auto entryPointName = descriptor.compute.entryPoint ? fromAPI(descriptor.compute.entryPoint) : shaderModule.defaultComputeEntryPoint();
    NSError *error;
    BufferBindingSizesForPipeline minimumBufferSizes;
    auto libraryCreationResult = createLibrary(m_device, shaderModule, &pipelineLayout, entryPointName, label, descriptor.compute.constantsSpan(), minimumBufferSizes, &error);
    if (!libraryCreationResult || &pipelineLayout.device() != this)
        return returnInvalidComputePipeline(*this, isAsync, error.localizedDescription ?: @"Compute library failed creation");

    auto library = libraryCreationResult->library;
    const auto& wgslConstantValues = libraryCreationResult->wgslConstantValues;
    const auto& entryPointInformation = libraryCreationResult->entryPointInformation;

    if (!std::holds_alternative<WGSL::Reflection::Compute>(entryPointInformation.typedEntryPoint))
        return returnInvalidComputePipeline(*this, isAsync);
    WGSL::Reflection::Compute computeInformation = std::get<WGSL::Reflection::Compute>(entryPointInformation.typedEntryPoint);

    auto function = createFunction(library, entryPointInformation, label);
    if (!function || function.functionType != MTLFunctionTypeKernel || entryPointInformation.specializationConstants.size() != wgslConstantValues.size())
        return returnInvalidComputePipeline(*this, isAsync);

    auto evaluatedSize = metalSize(computeInformation.workgroupSize, wgslConstantValues);
    if (!evaluatedSize)
        return returnInvalidComputePipeline(*this, isAsync, @"Failed to evaluate overrides");
    auto size = *evaluatedSize;
    if (entryPointInformation.sizeForWorkgroupVariables > deviceLimits.maxComputeWorkgroupStorageSize)
        return returnInvalidComputePipeline(*this, isAsync);

    if (!size.width || size.width > deviceLimits.maxComputeWorkgroupSizeX || !size.height || size.height > deviceLimits.maxComputeWorkgroupSizeY || !size.depth || size.depth > deviceLimits.maxComputeWorkgroupSizeZ || size.width * size.height * size.depth > deviceLimits.maxComputeInvocationsPerWorkgroup)
        return returnInvalidComputePipeline(*this, isAsync);

    if (pipelineLayout.isAutoLayout() && entryPointInformation.defaultLayout) {
        Vector<Vector<WGPUBindGroupLayoutEntry>> bindGroupEntries;
        if (NSString* error = addPipelineLayouts(bindGroupEntries, entryPointInformation.defaultLayout))
            return returnInvalidComputePipeline(*this, isAsync, error);

        auto generatedPipelineLayout = generatePipelineLayout(bindGroupEntries);
        if (!generatedPipelineLayout->isValid())
            return returnInvalidComputePipeline(*this, isAsync);
        auto computePipelineState = createComputePipelineState(m_device, function, generatedPipelineLayout, size, label);
        return std::make_pair(ComputePipeline::create(computePipelineState, WTFMove(generatedPipelineLayout), size, WTFMove(minimumBufferSizes), *this), nil);
    }

    auto computePipelineState = createComputePipelineState(m_device, function, pipelineLayout, size, label);
    return std::make_pair(ComputePipeline::create(computePipelineState, pipelineLayout, size, WTFMove(minimumBufferSizes), *this), nil);
}

void Device::createComputePipelineAsync(const WGPUComputePipelineDescriptor& descriptor, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<ComputePipeline>&&, String&& message)>&& callback)
{
    auto pipelineAndError = createComputePipeline(descriptor, true);
    if (auto inst = instance(); inst.get()) {
        inst->scheduleWork([pipeline = WTFMove(pipelineAndError.first), callback = WTFMove(callback), protectedThis = Ref { *this }, error = WTFMove(pipelineAndError.second)]() mutable {
            callback((pipeline->isValid() || protectedThis->isDestroyed()) ? WGPUCreatePipelineAsyncStatus_Success : WGPUCreatePipelineAsyncStatus_ValidationError, WTFMove(pipeline), WTFMove(error));
        });
    } else
        callback(WGPUCreatePipelineAsyncStatus_ValidationError, WTFMove(pipelineAndError.first), WTFMove(pipelineAndError.second));
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(ComputePipeline);

ComputePipeline::ComputePipeline(id<MTLComputePipelineState> computePipelineState, Ref<PipelineLayout>&& pipelineLayout, MTLSize threadsPerThreadgroup, BufferBindingSizesForPipeline&& minimumBufferSizes, Device& device)
    : m_computePipelineState(computePipelineState)
    , m_device(device)
    , m_threadsPerThreadgroup(threadsPerThreadgroup)
    , m_pipelineLayout(WTFMove(pipelineLayout))
    , m_minimumBufferSizes(WTFMove(minimumBufferSizes))
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
    if (!isValid()) {
        m_device->generateAValidationError("getBindGroupLayout: ComputePipeline is invalid"_s);
        m_pipelineLayout->makeInvalid();
        return BindGroupLayout::createInvalid(m_device);
    }

    if (groupIndex >= m_pipelineLayout->numberOfBindGroupLayouts()) {
        m_device->generateAValidationError("getBindGroupLayout: groupIndex is out of range"_s);
        m_pipelineLayout->makeInvalid();
        return BindGroupLayout::createInvalid(m_device);
    }

    return m_pipelineLayout->bindGroupLayout(groupIndex);
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

void wgpuComputePipelineReference(WGPUComputePipeline computePipeline)
{
    WebGPU::fromAPI(computePipeline).ref();
}

void wgpuComputePipelineRelease(WGPUComputePipeline computePipeline)
{
    WebGPU::fromAPI(computePipeline).deref();
}

WGPUBindGroupLayout wgpuComputePipelineGetBindGroupLayout(WGPUComputePipeline computePipeline, uint32_t groupIndex)
{
    return WebGPU::releaseToAPI(WebGPU::protectedFromAPI(computePipeline)->getBindGroupLayout(groupIndex));
}

void wgpuComputePipelineSetLabel(WGPUComputePipeline computePipeline, const char* label)
{
    WebGPU::protectedFromAPI(computePipeline)->setLabel(WebGPU::fromAPI(label));
}
