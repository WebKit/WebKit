/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
#import "Pipeline.h"

#import "APIConversions.h"
#import "ShaderModule.h"

namespace WebGPU {

std::optional<LibraryCreationResult> createLibrary(id<MTLDevice> device, const ShaderModule& shaderModule, PipelineLayout* pipelineLayout, const String& entryPoint, NSString *label, std::span<const WGPUConstantEntry> constants, BufferBindingSizesForPipeline& mininumBufferSizes, NSError **error)
{
    HashMap<String, WGSL::ConstantValue> wgslConstantValues;

    if (!entryPoint.length() || !shaderModule.isValid())
        return std::nullopt;

    if (shaderModule.library() && pipelineLayout) {
        if (const RefPtr pipelineLayoutHint = shaderModule.pipelineLayoutHint(entryPoint)) {
            if (*pipelineLayoutHint == *pipelineLayout) {
                if (const auto* entryPointInformation = shaderModule.entryPointInformation(entryPoint))
                    return { { shaderModule.library(), *entryPointInformation,  wgslConstantValues } };
            }
        }
    }

    auto* ast = shaderModule.ast();
    RELEASE_ASSERT(ast);

    std::optional<WGSL::PipelineLayout> wgslPipelineLayout { std::nullopt };
    if (pipelineLayout && pipelineLayout->numberOfBindGroupLayouts())
        wgslPipelineLayout = ShaderModule::convertPipelineLayout(*pipelineLayout);

    auto prepareResult = WGSL::prepare(*ast, entryPoint, wgslPipelineLayout ? &*wgslPipelineLayout : nullptr);
    if (std::holds_alternative<WGSL::Error>(prepareResult)) {
        auto wgslError = std::get<WGSL::Error>(prepareResult);
        *error = [NSError errorWithDomain:@"WebGPU" code:1 userInfo:@{ NSLocalizedDescriptionKey: wgslError.message() }];
        return std::nullopt;
    }

    auto& result = std::get<WGSL::PrepareResult>(prepareResult);
    auto iterator = result.entryPoints.find(entryPoint);
    if (iterator == result.entryPoints.end())
        return std::nullopt;

    const auto& entryPointInformation = iterator->value;

    for (const auto entry : constants) {
        auto keyEntry = fromAPI(entry.key);
        auto indexIterator = entryPointInformation.specializationConstants.find(keyEntry);
        if (indexIterator == entryPointInformation.specializationConstants.end())
            return { };

        const auto& specializationConstant = indexIterator->value;
        keyEntry = specializationConstant.mangledName;
        switch (specializationConstant.type) {
        case WGSL::Reflection::SpecializationConstantType::Boolean: {
            bool value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Float: {
            if (entry.value < std::numeric_limits<float>::lowest() || entry.value > std::numeric_limits<float>::max())
                return std::nullopt;
            float value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Int: {
            if (entry.value < std::numeric_limits<int32_t>::min() || entry.value > std::numeric_limits<int32_t>::max())
                return std::nullopt;
            int value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Unsigned: {
            if (entry.value < 0 || entry.value > std::numeric_limits<uint32_t>::max())
                return std::nullopt;
            unsigned value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Half: {
            constexpr double halfMax = 0x1.ffcp15;
            constexpr double halfLowest = -halfMax;
            if (entry.value < halfLowest || entry.value > halfMax)
                return std::nullopt;
            WGSL::half value = entry.value;
            wgslConstantValues.set(keyEntry, value);
            break;
        }
        }
    }

    for (auto& kvp : entryPointInformation.specializationConstants) {
        auto& specializationConstant = kvp.value;
        if (!specializationConstant.defaultValue || wgslConstantValues.contains(kvp.value.mangledName))
            continue;

        auto constantValue = WGSL::evaluate(*kvp.value.defaultValue, wgslConstantValues);
        if (!constantValue) {
            if (error)
                *error = [NSError errorWithDomain:@"WebGPU" code:1 userInfo:@{ NSLocalizedDescriptionKey: @"Failed to evaluate override value" }];
            return std::nullopt;
        }
        auto addResult = wgslConstantValues.add(kvp.value.mangledName, *constantValue);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    if (pipelineLayout) {
        for (unsigned i = 0; i < pipelineLayout->numberOfBindGroupLayouts(); ++i) {
            auto& wgslBindGroupLayout = wgslPipelineLayout->bindGroupLayouts[i];
            auto it = mininumBufferSizes.add(i, BufferBindingSizesForBindGroup()).iterator;
            BufferBindingSizesForBindGroup& shaderBindingSizeForBuffer = it->value;
            for (unsigned i = 0; i < wgslBindGroupLayout.entries.size(); ++i) {
                auto& wgslBindGroupLayoutEntry = wgslBindGroupLayout.entries[i];
                auto* wgslBufferBinding = std::get_if<WGSL::BufferBindingLayout>(&wgslBindGroupLayoutEntry.bindingMember);
                if (wgslBufferBinding && wgslBufferBinding->minBindingSize) {
                    auto newValue = wgslBufferBinding->minBindingSize;
                    if (auto existingValueIt = shaderBindingSizeForBuffer.find(wgslBindGroupLayoutEntry.binding); existingValueIt != shaderBindingSizeForBuffer.end())
                        newValue = std::max(newValue, existingValueIt->value);
                    shaderBindingSizeForBuffer.set(wgslBindGroupLayoutEntry.binding, newValue);
                }
            }
        }
    }

    auto generationResult = WGSL::generate(*ast, result, wgslConstantValues);
    if (auto* generationError = std::get_if<WGSL::Error>(&generationResult)) {
        *error = [NSError errorWithDomain:@"WebGPU" code:1 userInfo:@{ NSLocalizedDescriptionKey: generationError->message() }];
        return std::nullopt;
    }
    auto& msl = std::get<String>(generationResult);

    auto library = ShaderModule::createLibrary(device, msl, label, error);
    if (error && *error)
        return { };

    return { { library, entryPointInformation, wgslConstantValues } };
}

id<MTLFunction> createFunction(id<MTLLibrary> library, const WGSL::Reflection::EntryPointInformation& entryPointInformation, NSString *label)
{
    auto functionDescriptor = [MTLFunctionDescriptor new];
    functionDescriptor.name = entryPointInformation.mangledName;
    NSError *error = nil;
    id<MTLFunction> function = [library newFunctionWithDescriptor:functionDescriptor error:&error];
    if (error)
        WTFLogAlways("Function creation error: %@", error);
    function.label = label;
    return function;
}

NSString* errorValidatingBindGroup(const BindGroup& bindGroup, const BufferBindingSizesForBindGroup* mininumBufferSizes, const Vector<uint32_t>* dynamicOffsets)
{
    RefPtr bindGroupLayout = bindGroup.bindGroupLayout();
    if (!bindGroupLayout)
        return nil;

    auto& bindGroupLayoutEntries = bindGroupLayout->entries();
    for (const auto& resourceVector : bindGroup.resources()) {
        for (const auto& resource : resourceVector.resourceUsages) {
            auto bindingIndex = resource.binding;
            auto* buffer = get_if<RefPtr<Buffer>>(&resource.resource);
            if (!buffer)
                continue;

            auto it = bindGroupLayoutEntries.find(bindingIndex);
            if (it == bindGroupLayoutEntries.end())
                return [NSString stringWithFormat:@"Buffer size is missing for binding at index %u bind group", bindingIndex];

            uint64_t bufferSize = 0;
            if (auto* bufferBinding = get_if<WGPUBufferBindingLayout>(&it->value.bindingLayout))
                bufferSize = bufferBinding->minBindingSize;
            if (mininumBufferSizes) {
                if (auto bufferSizeIt = mininumBufferSizes->find(it->value.binding); bufferSizeIt != mininumBufferSizes->end()) {
                    if (bufferSize && bufferSizeIt->value > bufferSize)
                        return [NSString stringWithFormat:@"buffer size from WGSL shader(%llu) is less than the binding buffer size(%llu)", bufferSizeIt->value, bufferSize];
                    bufferSize = std::max(bufferSize, bufferSizeIt->value);
                }
            }

            if (bufferSize && buffer->get()) {
                auto dynamicOffset = bindGroup.dynamicOffset(bindingIndex, dynamicOffsets);
                auto checkedTotalOffset = checkedSum<uint64_t>(resource.entryOffset, dynamicOffset);
                if (checkedTotalOffset.hasOverflowed())
                    return [NSString stringWithFormat:@"resourceOffset(%llu) + dynamicOffset(%u) overflows uint64_t", resource.entryOffset, dynamicOffset];
                auto totalOffset = checkedTotalOffset.value();
                auto mtlBufferLength = buffer->get()->buffer().length;
                if (totalOffset > mtlBufferLength || (mtlBufferLength - totalOffset) < bufferSize || bufferSize > resource.entrySize)
                    return [NSString stringWithFormat:@"buffer length(%zu) minus offset(%llu), (resourceOffset(%llu) + dynamicOffset(%u)), is less than required bufferSize(%llu)", mtlBufferLength, totalOffset, resource.entryOffset, dynamicOffset, bufferSize];
            }
        }
    }
    return nil;
}

} // namespace WebGPU
