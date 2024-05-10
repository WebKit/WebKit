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

std::optional<LibraryCreationResult> createLibrary(id<MTLDevice> device, const ShaderModule& shaderModule, PipelineLayout* pipelineLayout, const String& untransformedEntryPoint, NSString *label, uint32_t constantCount, const WGPUConstantEntry* constants, NSError **error)
{
    // FIXME: Remove below line when https://bugs.webkit.org/show_bug.cgi?id=266774 is completed
    HashMap<String, WGSL::ConstantValue> wgslConstantValues;

    auto entryPoint = shaderModule.transformedEntryPoint(untransformedEntryPoint);
    if (!entryPoint.length() || !shaderModule.isValid())
        return std::nullopt;

    if (shaderModule.library() && pipelineLayout) {
        if (const auto* pipelineLayoutHint = shaderModule.pipelineLayoutHint(entryPoint)) {
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
    // FIXME: return the actual error
    if (std::holds_alternative<WGSL::Error>(prepareResult))
        return std::nullopt;

    auto& result = std::get<WGSL::PrepareResult>(prepareResult);
    auto iterator = result.entryPoints.find(entryPoint);
    if (iterator == result.entryPoints.end())
        return std::nullopt;

    const auto& entryPointInformation = iterator->value;

    for (auto& kvp : entryPointInformation.specializationConstants) {
        auto& specializationConstant = kvp.value;
        if (!specializationConstant.defaultValue)
            continue;

        auto constantValue = WGSL::evaluate(*kvp.value.defaultValue, wgslConstantValues);
        auto addResult = wgslConstantValues.add(kvp.key, constantValue);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);
    }

    for (uint32_t i = 0; i < constantCount; ++i) {
        const auto& entry = constants[i];
        auto keyEntry = fromAPI(entry.key);
        auto indexIterator = entryPointInformation.specializationConstants.find(keyEntry);
        // FIXME: Remove code inside the following conditional statement when https://bugs.webkit.org/show_bug.cgi?id=266774 is completed
        if (indexIterator == entryPointInformation.specializationConstants.end()) {
            if (!shaderModule.hasOverride(keyEntry))
                return { };

            NSString *nsConstant = [NSString stringWithUTF8String:keyEntry.utf8().data()];
            nsConstant = [nsConstant stringByApplyingTransform:NSStringTransformToLatin reverse:NO];
            nsConstant = [nsConstant stringByFoldingWithOptions:NSDiacriticInsensitiveSearch locale:NSLocale.currentLocale];
            keyEntry = nsConstant;
            indexIterator = entryPointInformation.specializationConstants.find(keyEntry);
            if (indexIterator == entryPointInformation.specializationConstants.end())
                return { };
        }
        const auto& specializationConstant = indexIterator->value;
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

    if (pipelineLayout) {
        for (unsigned i = 0; i < pipelineLayout->numberOfBindGroupLayouts(); ++i) {
            auto& bindGroupLayout = pipelineLayout->bindGroupLayout(i);
            auto& wgslBindGroupLayout = wgslPipelineLayout->bindGroupLayouts[i];

            for (unsigned i = 0; i < wgslBindGroupLayout.entries.size(); ++i) {
                auto& wgslBindGroupLayoutEntry = wgslBindGroupLayout.entries[i];
                auto it = bindGroupLayout.entries().find(wgslBindGroupLayoutEntry.binding);
                RELEASE_ASSERT(it != bindGroupLayout.entries().end());
                auto& bindGroupLayoutEntry = it->value;
                if (auto* bufferBinding = std::get_if<WGPUBufferBindingLayout>(&bindGroupLayoutEntry.bindingLayout)) {
                    auto& wgslBufferBinding = std::get<WGSL::BufferBindingLayout>(wgslBindGroupLayoutEntry.bindingMember);
                    const_cast<WGPUBufferBindingLayout*>(bufferBinding)->minBindingSize = wgslBufferBinding.minBindingSize;
                }
            }
        }
    }

    auto msl = WGSL::generate(*ast, result, wgslConstantValues);
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

bool validateBindGroup(BindGroup& bindGroup)
{
    auto& bindGroupLayoutEntries = bindGroup.bindGroupLayout()->entries();
    for (const auto& resourceVector : bindGroup.resources()) {
        for (const auto& resource : resourceVector.resourceUsages) {
            auto bindingIndex = resource.binding;
            auto* buffer = get_if<RefPtr<const Buffer>>(&resource.resource);
            if (!buffer)
                continue;

            auto it = bindGroupLayoutEntries.find(bindingIndex);
            RELEASE_ASSERT(it != bindGroupLayoutEntries.end());

            auto* bufferBinding = get_if<WGPUBufferBindingLayout>(&it->value.bindingLayout);
            if (!bufferBinding)
                continue;

            auto bufferSize = bufferBinding->minBindingSize;
            if (bufferSize && buffer->get()) {
                if (buffer->get()->buffer().length < bufferSize)
                    return false;
            }
        }
    }
    return true;
}

} // namespace WebGPU
