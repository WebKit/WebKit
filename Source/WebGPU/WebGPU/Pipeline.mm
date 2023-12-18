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

std::optional<LibraryCreationResult> createLibrary(id<MTLDevice> device, const ShaderModule& shaderModule, const PipelineLayout* pipelineLayout, const String& entryPoint, NSString *label)
{
    if (!entryPoint.length() || !shaderModule.isValid())
        return std::nullopt;

    if (shaderModule.library() && pipelineLayout) {
        if (const auto* pipelineLayoutHint = shaderModule.pipelineLayoutHint(entryPoint)) {
            if (*pipelineLayoutHint == *pipelineLayout) {
                if (const auto* entryPointInformation = shaderModule.entryPointInformation(entryPoint))
                    return { { shaderModule.library(), *entryPointInformation } };
            }
        }
    }

    auto* ast = shaderModule.ast();
    RELEASE_ASSERT(ast);

    std::optional<WGSL::PipelineLayout> wgslPipelineLayout { std::nullopt };
    if (pipelineLayout && pipelineLayout->numberOfBindGroupLayouts())
        wgslPipelineLayout = ShaderModule::convertPipelineLayout(*pipelineLayout);

    auto prepareResult = WGSL::prepare(*ast, entryPoint, wgslPipelineLayout);

    auto library = ShaderModule::createLibrary(device, prepareResult.msl, label);

    auto iterator = prepareResult.entryPoints.find(entryPoint);
    if (iterator == prepareResult.entryPoints.end())
        return std::nullopt;
    const auto& entryPointInformation = iterator->value;

    return { { library, entryPointInformation } };
}

std::tuple<MTLFunctionConstantValues *, HashMap<String, WGSL::ConstantValue>> createConstantValues(uint32_t constantCount, const WGPUConstantEntry* constants, const WGSL::Reflection::EntryPointInformation& entryPointInformation)
{
    HashMap<String, WGSL::ConstantValue> wgslConstantValues;

    auto constantValues = [MTLFunctionConstantValues new];
    for (auto& kvp : entryPointInformation.specializationConstants) {
        auto& specializationConstant = kvp.value;
        if (!specializationConstant.defaultValue)
            continue;

        auto constantValue = WGSL::evaluate(*kvp.value.defaultValue, wgslConstantValues);
        auto addResult = wgslConstantValues.add(kvp.key, constantValue);
        ASSERT_UNUSED(addResult, addResult.isNewEntry);

        switch (specializationConstant.type) {
        case WGSL::Reflection::SpecializationConstantType::Boolean: {
            auto value = std::get<bool>(constantValue);
            [constantValues setConstantValue:&value type:MTLDataTypeBool withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Float: {
            auto value = std::get<float>(constantValue);
            [constantValues setConstantValue:&value type:MTLDataTypeFloat withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Int: {
            auto value = std::get<int32_t>(constantValue);
            [constantValues setConstantValue:&value type:MTLDataTypeInt withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Unsigned: {
            auto value = std::get<uint32_t>(constantValue);
            [constantValues setConstantValue:&value type:MTLDataTypeUInt withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Half: {
            auto value = std::get<WGSL::half>(constantValue);
            [constantValues setConstantValue:&value type:MTLDataTypeHalf withName:specializationConstant.mangledName];
            break;
        }
        }
    }

    for (uint32_t i = 0; i < constantCount; ++i) {
        const auto& entry = constants[i];
        auto indexIterator = entryPointInformation.specializationConstants.find(fromAPI(entry.key));
        if (indexIterator == entryPointInformation.specializationConstants.end())
            return { };
        const auto& specializationConstant = indexIterator->value;
        switch (specializationConstant.type) {
        case WGSL::Reflection::SpecializationConstantType::Boolean: {
            bool value = entry.value;
            wgslConstantValues.set(fromAPI(entry.key), value);
            [constantValues setConstantValue:&value type:MTLDataTypeBool withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Float: {
            float value = entry.value;
            wgslConstantValues.set(fromAPI(entry.key), value);
            [constantValues setConstantValue:&value type:MTLDataTypeFloat withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Int: {
            int value = entry.value;
            wgslConstantValues.set(fromAPI(entry.key), value);
            [constantValues setConstantValue:&value type:MTLDataTypeInt withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Unsigned: {
            unsigned value = entry.value;
            wgslConstantValues.set(fromAPI(entry.key), value);
            [constantValues setConstantValue:&value type:MTLDataTypeUInt withName:specializationConstant.mangledName];
            break;
        }
        case WGSL::Reflection::SpecializationConstantType::Half: {
            float value = entry.value;
            wgslConstantValues.set(fromAPI(entry.key), value);
            [constantValues setConstantValue:&value type:MTLDataTypeHalf withName:specializationConstant.mangledName];
            break;
        }
        }
    }
    return { constantValues, WTFMove(wgslConstantValues) };
}

id<MTLFunction> createFunction(id<MTLLibrary> library, const WGSL::Reflection::EntryPointInformation& entryPointInformation, MTLFunctionConstantValues *constantValues, NSString *label)
{
    auto functionDescriptor = [MTLFunctionDescriptor new];
    functionDescriptor.name = entryPointInformation.mangledName;
    if (constantValues)
        functionDescriptor.constantValues = constantValues;
    NSError *error = nil;
    id<MTLFunction> function = [library newFunctionWithDescriptor:functionDescriptor error:&error];
    if (error)
        WTFLogAlways("Function creation error: %@", error);
    function.label = label;
    return function;
}

} // namespace WebGPU
