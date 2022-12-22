/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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
#import "PipelineLayout.h"
#import "ShaderModule.h"

namespace WebGPU {

struct LibraryCreationResult {
    id<MTLLibrary> library;
    WGSL::Reflection::EntryPointInformation entryPointInformation; // FIXME(PERFORMANCE): This is big. Don't copy this around.
};

static std::optional<LibraryCreationResult> createLibrary(id<MTLDevice> device, const ShaderModule& shaderModule, const PipelineLayout& pipelineLayout, const String& entryPoint, NSString *label)
{
    if (shaderModule.library()) {
        if (const auto* pipelineLayoutHint = shaderModule.pipelineLayoutHint(entryPoint)) {
            if (*pipelineLayoutHint == pipelineLayout) {
                if (const auto* entryPointInformation = shaderModule.entryPointInformation(entryPoint))
                    return { { shaderModule.library(), *entryPointInformation } };
            }
        }
    }

    const auto* ast = shaderModule.ast();
    if (!ast)
        return std::nullopt;

    auto prepareResult = WGSL::prepare(*ast, entryPoint, ShaderModule::convertPipelineLayout(pipelineLayout));

    auto library = ShaderModule::createLibrary(device, prepareResult.msl, label);

    auto iterator = prepareResult.entryPoints.find(entryPoint);
    if (iterator == prepareResult.entryPoints.end())
        return std::nullopt;
    const auto& entryPointInformation = iterator->value;

    return { { library, entryPointInformation } };
}

static MTLFunctionConstantValues *createConstantValues(uint32_t constantCount, const WGPUConstantEntry* constants, const WGSL::Reflection::EntryPointInformation& entryPointInformation)
{
    auto constantValues = [MTLFunctionConstantValues new];
    for (uint32_t i = 0; i < constantCount; ++i) {
        const auto& entry = constants[i];
        auto nameIterator = entryPointInformation.specializationConstantIndices.find(fromAPI(entry.key));
        if (nameIterator == entryPointInformation.specializationConstantIndices.end())
            return nullptr;
        auto specializationConstantIndex = nameIterator->value;
        auto indexIterator = entryPointInformation.specializationConstants.find(specializationConstantIndex);
        if (indexIterator == entryPointInformation.specializationConstants.end())
            return nullptr;
        const auto& specializationConstant = indexIterator->value;
        switch (specializationConstant.type) {
        case WGSL::Reflection::SpecializationConstantType::Boolean: {
            bool value = entry.value;
            [constantValues setConstantValue:&value type:MTLDataTypeBool withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Float: {
            float value = entry.value;
            [constantValues setConstantValue:&value type:MTLDataTypeFloat withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Int: {
            int value = entry.value;
            [constantValues setConstantValue:&value type:MTLDataTypeInt withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Unsigned: {
            unsigned value = entry.value;
            [constantValues setConstantValue:&value type:MTLDataTypeUInt withName:specializationConstant.mangledName];
            break;
        }
        }
    }
    return constantValues;
}

static id<MTLFunction> createFunction(id<MTLLibrary> library, const WGSL::Reflection::EntryPointInformation& entryPointInformation, const WGPUProgrammableStageDescriptor& compute, NSString *label)
{
    auto functionDescriptor = [MTLFunctionDescriptor new];
    functionDescriptor.name = entryPointInformation.mangledName;
    if (compute.constantCount) {
        auto constantValues = createConstantValues(compute.constantCount, compute.constants, entryPointInformation);
        if (!constantValues)
            return nullptr;
        functionDescriptor.constantValues = constantValues;
    }
    NSError *error = nil;
    id<MTLFunction> function = [library newFunctionWithDescriptor:functionDescriptor error:&error];
    if (error)
        WTFLogAlways("Function creation error: %@", error);
    function.label = label;
    return function;
}

static id<MTLComputePipelineState> createComputePipelineState(id<MTLDevice> device, id<MTLFunction> function, const PipelineLayout& pipelineLayout, const WGSL::Reflection::Compute& computeInformation, NSString *label)
{
    auto computePipelineDescriptor = [MTLComputePipelineDescriptor new];
    computePipelineDescriptor.computeFunction = function;
    // FIXME: check this calculation for overflow
    computePipelineDescriptor.maxTotalThreadsPerThreadgroup = computeInformation.workgroupSize.width * computeInformation.workgroupSize.height * computeInformation.workgroupSize.depth;
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

Ref<ComputePipeline> Device::createComputePipeline(const WGPUComputePipelineDescriptor& descriptor)
{
    if (descriptor.nextInChain || descriptor.compute.nextInChain)
        return ComputePipeline::createInvalid(*this);

    const ShaderModule& shaderModule = WebGPU::fromAPI(descriptor.compute.module);
    const PipelineLayout& pipelineLayout = WebGPU::fromAPI(descriptor.layout);
    auto label = fromAPI(descriptor.label);

    auto libraryCreationResult = createLibrary(m_device, shaderModule, pipelineLayout, fromAPI(descriptor.compute.entryPoint), label);
    if (!libraryCreationResult)
        return ComputePipeline::createInvalid(*this);

    auto library = libraryCreationResult->library;
    const auto& entryPointInformation = libraryCreationResult->entryPointInformation;

    if (!std::holds_alternative<WGSL::Reflection::Compute>(entryPointInformation.typedEntryPoint))
        return ComputePipeline::createInvalid(*this);
    const auto& computeInformation = std::get<WGSL::Reflection::Compute>(entryPointInformation.typedEntryPoint);

    auto function = createFunction(library, entryPointInformation, descriptor.compute, label);

    auto computePipelineState = createComputePipelineState(m_device, function, pipelineLayout, computeInformation, label);

    return ComputePipeline::create(computePipelineState, *this);
}

void Device::createComputePipelineAsync(const WGPUComputePipelineDescriptor& descriptor, CompletionHandler<void(WGPUCreatePipelineAsyncStatus, Ref<ComputePipeline>&&, String&& message)>&& callback)
{
    // FIXME: Implement this
    UNUSED_PARAM(descriptor);
    instance().scheduleWork([strongThis = Ref { *this }, callback = WTFMove(callback)]() mutable {
        callback(WGPUCreatePipelineAsyncStatus_Error, ComputePipeline::createInvalid(strongThis), { });
    });
}

ComputePipeline::ComputePipeline(id<MTLComputePipelineState> computePipelineState, Device& device)
    : m_computePipelineState(computePipelineState)
    , m_device(device)
{
}

ComputePipeline::ComputePipeline(Device& device)
    : m_device(device)
{
}

ComputePipeline::~ComputePipeline() = default;

BindGroupLayout* ComputePipeline::getBindGroupLayout(uint32_t groupIndex)
{
    UNUSED_PARAM(groupIndex);
    return nullptr;
}

void ComputePipeline::setLabel(String&&)
{
    // MTLComputePipelineState's labels are read-only.
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuComputePipelineRelease(WGPUComputePipeline computePipeline)
{
    WebGPU::fromAPI(computePipeline).deref();
}

WGPUBindGroupLayout wgpuComputePipelineGetBindGroupLayout(WGPUComputePipeline computePipeline, uint32_t groupIndex)
{
    return WebGPU::fromAPI(computePipeline).getBindGroupLayout(groupIndex);
}

void wgpuComputePipelineSetLabel(WGPUComputePipeline computePipeline, const char* label)
{
    WebGPU::fromAPI(computePipeline).setLabel(WebGPU::fromAPI(label));
}
