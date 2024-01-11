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
#import "ShaderModule.h"

#import "APIConversions.h"
#import "ASTFunction.h"
#import "ASTStructure.h"
#import "ASTStructureMember.h"
#import "Device.h"
#import "PipelineLayout.h"
#import "Types.h"
#import "WGSLShaderModule.h"

#import <WebGPU/WebGPU.h>
#import <wtf/DataLog.h>
#import <wtf/StringPrintStream.h>

namespace WebGPU {

struct ShaderModuleParameters {
    const WGPUShaderModuleWGSLDescriptor& wgsl;
    const WGPUShaderModuleCompilationHint* hints;
};

static std::optional<ShaderModuleParameters> findShaderModuleParameters(const WGPUShaderModuleDescriptor& descriptor)
{
    const WGPUShaderModuleWGSLDescriptor* wgsl = nullptr;
    const WGPUShaderModuleCompilationHint* hints = descriptor.hints;

    for (const WGPUChainedStruct* ptr = descriptor.nextInChain; ptr; ptr = ptr->next) {
        auto type = ptr->sType;

        switch (static_cast<int>(type)) {
        case WGPUSType_ShaderModuleWGSLDescriptor:
            if (wgsl)
                return std::nullopt;
            wgsl = reinterpret_cast<const WGPUShaderModuleWGSLDescriptor*>(ptr);
            break;
        default:
            return std::nullopt;
        }
    }

    if (!wgsl)
        return std::nullopt;

    return { { *wgsl, hints } };
}

id<MTLLibrary> ShaderModule::createLibrary(id<MTLDevice> device, const String& msl, String&& label)
{
    auto options = [MTLCompileOptions new];
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    options.fastMathEnabled = YES;
ALLOW_DEPRECATED_DECLARATIONS_END
    NSError *error = nil;
    // FIXME(PERFORMANCE): Run the asynchronous version of this
    id<MTLLibrary> library = [device newLibraryWithSource:msl options:options error:&error];
    if (error) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=250442
        WTFLogAlways("MSL compilation error: %@", error);
        WGPU_FUZZER_ASSERT_NOT_REACHED("Failed metal compilation");
    }
    library.label = label;
    return library;
}

static RefPtr<ShaderModule> earlyCompileShaderModule(Device& device, std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult, const WGPUShaderModuleDescriptor& suppliedHints, String&& label)
{
    HashMap<String, Ref<PipelineLayout>> hints;
    HashMap<String, std::optional<WGSL::PipelineLayout>> wgslHints;
    for (uint32_t i = 0; i < suppliedHints.hintCount; ++i) {
        const auto& hint = suppliedHints.hints[i];
        if (hint.nextInChain)
            return nullptr;
        auto hintKey = fromAPI(hint.entryPoint);
        auto& layout = WebGPU::fromAPI(hint.layout);
        hints.add(hintKey, layout);
        std::optional<WGSL::PipelineLayout> convertedPipelineLayout { std::nullopt };
        if (layout.numberOfBindGroupLayouts())
            convertedPipelineLayout = ShaderModule::convertPipelineLayout(layout);
        wgslHints.add(hintKey, WTFMove(convertedPipelineLayout));
    }

    auto prepareResult = WGSL::prepare(std::get<WGSL::SuccessfulCheck>(checkResult).ast, wgslHints);
    auto library = ShaderModule::createLibrary(device.device(), prepareResult.msl, WTFMove(label));
    if (!library)
        return nullptr;
    return ShaderModule::create(WTFMove(checkResult), WTFMove(hints), WTFMove(prepareResult.entryPoints), library, nil, { }, device);
}

static const HashSet<String> buildFeatureSet(const Vector<WGPUFeatureName>& features)
{
    HashSet<String> result;
    for (auto feature : features) {
        switch (feature) {
        case WGPUFeatureName_Undefined:
            continue;
        case WGPUFeatureName_DepthClipControl:
            result.add("depth-clip-control"_s);
            break;
        case WGPUFeatureName_Depth32FloatStencil8:
            result.add("depth32float-stencil8"_s);
            break;
        case WGPUFeatureName_TimestampQuery:
            result.add("timestamp-query"_s);
            break;
        case WGPUFeatureName_TextureCompressionBC:
            result.add("texture-compression-bc"_s);
            break;
        case WGPUFeatureName_TextureCompressionETC2:
            result.add("texture-compression-etc2"_s);
            break;
        case WGPUFeatureName_TextureCompressionASTC:
            result.add("texture-compression-astc"_s);
            break;
        case WGPUFeatureName_IndirectFirstInstance:
            result.add("indirect-first-instance"_s);
            break;
        case WGPUFeatureName_ShaderF16:
            result.add("shader-f16"_s);
            break;
        case WGPUFeatureName_RG11B10UfloatRenderable:
            result.add("rg11b10ufloat-renderable"_s);
            break;
        case WGPUFeatureName_BGRA8UnormStorage:
            result.add("bgra8unorm-storage"_s);
            break;
        case WGPUFeatureName_Float32Filterable:
            result.add("float32-filterable"_s);
            break;
        case WGPUFeatureName_Force32:
            ASSERT_NOT_REACHED();
            continue;
        }
    }

    return result;
}

static Ref<ShaderModule> handleShaderSuccessOrFailure(WebGPU::Device &object, std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck> &checkResult, const WGPUShaderModuleDescriptor &descriptor, std::optional<ShaderModuleParameters> &shaderModuleParameters, NSMutableSet<NSString *> * originalOverrideNames, HashMap<String, String>&& originalFunctionNames)
{
    if (std::holds_alternative<WGSL::SuccessfulCheck>(checkResult)) {
        if (shaderModuleParameters->hints && descriptor.hintCount) {
            // FIXME: re-enable early compilation later on once deferred compilation is fully implemented
            // https://bugs.webkit.org/show_bug.cgi?id=254258
            UNUSED_PARAM(earlyCompileShaderModule);
        }
        dataLogLn(fromAPI(shaderModuleParameters->wgsl.code));
        return ShaderModule::create(WTFMove(checkResult), { }, { }, nil, originalOverrideNames, WTFMove(originalFunctionNames), object);
    }

    auto& failedCheck = std::get<WGSL::FailedCheck>(checkResult);
    StringPrintStream message;
    message.print(String::number(failedCheck.errors.size()), " error", failedCheck.errors.size() != 1 ? "s" : "", " generated while compiling the shader:"_s);
    for (const auto& error : failedCheck.errors)
        message.print("\n"_s, error);

    dataLogLn(message.toString());
    dataLogLn(fromAPI(shaderModuleParameters->wgsl.code));
    object.generateAValidationError(message.toString());
    return ShaderModule::createInvalid(object, failedCheck);
}

Ref<ShaderModule> Device::createShaderModule(const WGPUShaderModuleDescriptor& descriptor)
{
    if (!descriptor.nextInChain || !isValid())
        return ShaderModule::createInvalid(*this);

    auto shaderModuleParameters = findShaderModuleParameters(descriptor);
    if (!shaderModuleParameters)
        return ShaderModule::createInvalid(*this);

    HashMap<String, String> functionNames;
    auto supportedFeatures = buildFeatureSet(m_capabilities.features);
    auto checkResult = WGSL::staticCheck(fromAPI(shaderModuleParameters->wgsl.code), std::nullopt, WGSL::Configuration {
        .maxBuffersPlusVertexBuffersForVertexStage = maxBuffersPlusVertexBuffersForVertexStage(),
        .maxBuffersForFragmentStage = maxBuffersForFragmentStage(),
        .maxBuffersForComputeStage = maxBuffersForComputeStage(),
        .supportedFeatures = WTFMove(supportedFeatures)
    });

    // FIXME: Remove when https://bugs.webkit.org/show_bug.cgi?id=266774 is completed
    if (!std::holds_alternative<WGSL::SuccessfulCheck>(checkResult)) {
        NSString *nsWgsl = [NSString stringWithUTF8String:shaderModuleParameters->wgsl.code];
        NSMutableSet<NSString *> *overrideNames = [NSMutableSet set];
        NSRange currentRange = NSMakeRange(0, nsWgsl.length);
        for (;;) {
            NSRange newRange = [nsWgsl rangeOfString:@"override " options:NSLiteralSearch range:currentRange];
            if (newRange.location == NSNotFound)
                break;
            NSRange endRange = [nsWgsl rangeOfString:@":" options:NSLiteralSearch range:NSMakeRange(newRange.location, nsWgsl.length - newRange.location)];
            auto startIndex = newRange.location + newRange.length;
            NSString* overrideName = [nsWgsl substringWithRange:NSMakeRange(startIndex, endRange.location - startIndex)];
            [overrideNames addObject:overrideName];
            currentRange = NSMakeRange(endRange.location + 1, nsWgsl.length - endRange.location - 1);
        }

        NSString* stageNames[] = { @"@vertex ", @"@fragment ", @"@compute " };
        for (NSString* moduleName : stageNames) {
            currentRange = NSMakeRange(0, nsWgsl.length);
            for (;;) {
                NSRange newRange = [nsWgsl rangeOfString:moduleName options:NSLiteralSearch range:currentRange];
                if (newRange.location == NSNotFound)
                    break;

                newRange = [nsWgsl rangeOfString:@" fn " options:NSLiteralSearch range:NSMakeRange(newRange.location, nsWgsl.length - newRange.location - 1)];
                if (newRange.location == NSNotFound)
                    break;

                NSRange endRange = [nsWgsl rangeOfString:@"(" options:NSLiteralSearch range:NSMakeRange(newRange.location, nsWgsl.length - newRange.location)];
                auto startIndex = newRange.location + newRange.length;
                NSString* functionName = [nsWgsl substringWithRange:NSMakeRange(startIndex, endRange.location - startIndex)];
                currentRange = NSMakeRange(endRange.location + 1, nsWgsl.length - endRange.location - 1);
                NSString *transformedName = [functionName stringByApplyingTransform:NSStringTransformToLatin reverse:NO];
                transformedName = [transformedName stringByFoldingWithOptions:NSDiacriticInsensitiveSearch locale:NSLocale.currentLocale];
                functionNames.set(functionName, transformedName);
            }
        }

        nsWgsl = [nsWgsl stringByApplyingTransform:NSStringTransformToLatin reverse:NO];
        nsWgsl = [nsWgsl stringByFoldingWithOptions:NSDiacriticInsensitiveSearch locale:NSLocale.currentLocale];
        auto checkResult = WGSL::staticCheck(nsWgsl, std::nullopt, { maxBuffersPlusVertexBuffersForVertexStage(), maxBuffersForFragmentStage(), maxBuffersForComputeStage() });
        dataLogLn(fromAPI(shaderModuleParameters->wgsl.code));
        dataLogLn(String(nsWgsl));
        return handleShaderSuccessOrFailure(*this, checkResult, descriptor, shaderModuleParameters, overrideNames, WTFMove(functionNames));
    }

    return handleShaderSuccessOrFailure(*this, checkResult, descriptor, shaderModuleParameters, nil, WTFMove(functionNames));
}

auto ShaderModule::convertCheckResult(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult) -> CheckResult
{
    return WTF::switchOn(WTFMove(checkResult), [](auto&& check) -> CheckResult {
        return std::forward<decltype(check)>(check);
    });
}

static MTLDataType metalDataTypeFromPrimitive(const WGSL::Types::Primitive *primitiveType, int vectorSize = 1)
{
    switch (vectorSize) {
    case 1:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return MTLDataTypeInt;
        case WGSL::Types::Primitive::U32:
            return MTLDataTypeUInt;
        case WGSL::Types::Primitive::F16:
            return MTLDataTypeHalf;
        case WGSL::Types::Primitive::F32:
            return MTLDataTypeFloat;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 2:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return MTLDataTypeInt2;
        case WGSL::Types::Primitive::U32:
            return MTLDataTypeUInt2;
        case WGSL::Types::Primitive::F16:
            return MTLDataTypeHalf2;
        case WGSL::Types::Primitive::F32:
            return MTLDataTypeFloat2;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 3:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return MTLDataTypeInt3;
        case WGSL::Types::Primitive::U32:
            return MTLDataTypeUInt3;
        case WGSL::Types::Primitive::F16:
            return MTLDataTypeHalf3;
        case WGSL::Types::Primitive::F32:
            return MTLDataTypeFloat3;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 4:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return MTLDataTypeInt4;
        case WGSL::Types::Primitive::U32:
            return MTLDataTypeUInt4;
        case WGSL::Types::Primitive::F16:
            return MTLDataTypeHalf4;
        case WGSL::Types::Primitive::F32:
            return MTLDataTypeFloat4;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static WGPUVertexFormat vertexFormatTypeFromPrimitive(const WGSL::Types::Primitive *primitiveType, int vectorSize)
{
    switch (vectorSize) {
    case 1:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return WGPUVertexFormat_Sint32;
        case WGSL::Types::Primitive::U32:
            return WGPUVertexFormat_Uint32;
        case WGSL::Types::Primitive::F16:
            return WGPUVertexFormat_Float32;
        case WGSL::Types::Primitive::F32:
            return WGPUVertexFormat_Float32;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 2:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return WGPUVertexFormat_Sint32x2;
        case WGSL::Types::Primitive::U32:
            return WGPUVertexFormat_Uint32x2;
        case WGSL::Types::Primitive::F16:
            return WGPUVertexFormat_Float16x2;
        case WGSL::Types::Primitive::F32:
            return WGPUVertexFormat_Float32x2;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 3:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return WGPUVertexFormat_Sint32x3;
        case WGSL::Types::Primitive::U32:
            return WGPUVertexFormat_Uint32x3;
        case WGSL::Types::Primitive::F16:
            return WGPUVertexFormat_Float16x4;
        case WGSL::Types::Primitive::F32:
            return WGPUVertexFormat_Float32x3;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    case 4:
        switch (primitiveType->kind) {
        case WGSL::Types::Primitive::I32:
            return WGPUVertexFormat_Sint32x4;
        case WGSL::Types::Primitive::U32:
            return WGPUVertexFormat_Uint32x4;
        case WGSL::Types::Primitive::F16:
            return WGPUVertexFormat_Float16x4;
        case WGSL::Types::Primitive::F32:
            return WGPUVertexFormat_Float32x4;
        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    default:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static MTLDataType metalDataTypeForStructMember(const WGSL::Type* type)
{
    if (!type)
        return MTLDataTypeNone;

    auto* vectorType = std::get_if<WGSL::Types::Vector>(type);
    auto* primitiveType = std::get_if<WGSL::Types::Primitive>(vectorType ? vectorType->element : type);
    if (!primitiveType)
        return MTLDataTypeNone;

    auto vectorSize = vectorType ? vectorType->size : 1;
    return metalDataTypeFromPrimitive(primitiveType, vectorSize);
}

static WGPUVertexFormat vertexFormatTypeForStructMember(const WGSL::Type* type)
{
    if (!type) {
        RELEASE_ASSERT_NOT_REACHED();
        return WGPUVertexFormat_Undefined;
    }

    auto* vectorType = std::get_if<WGSL::Types::Vector>(type);
    auto* primitiveType = std::get_if<WGSL::Types::Primitive>(vectorType ? vectorType->element : type);
    if (!primitiveType) {
        RELEASE_ASSERT_NOT_REACHED();
        return WGPUVertexFormat_Undefined;
    }

    auto vectorSize = vectorType ? vectorType->size : 1;
    return vertexFormatTypeFromPrimitive(primitiveType, vectorSize);
}

static ShaderModule::FragmentOutputs parseFragmentReturnType(const WGSL::Type& type)
{
    ShaderModule::FragmentOutputs fragmentOutputs;
    if (auto* returnPrimitive = std::get_if<WGSL::Types::Primitive>(&type)) {
        fragmentOutputs.add(0, metalDataTypeFromPrimitive(returnPrimitive));
        return fragmentOutputs;
    }
    if (std::get_if<WGSL::Types::Vector>(&type)) {
        fragmentOutputs.add(0, metalDataTypeForStructMember(&type));
        return fragmentOutputs;
    }
    auto* returnStruct = std::get_if<WGSL::Types::Struct>(&type);
    if (!returnStruct)
        return fragmentOutputs;

    for (auto& member : returnStruct->structure.members()) {
        if (!member.location() || member.builtin())
            continue;

        auto location = *member.location();
        fragmentOutputs.add(location, metalDataTypeForStructMember(member.type().inferredType()));
    }

    return fragmentOutputs;
}

static ShaderModule::VertexOutputs parseVertexReturnType(const WGSL::Type& type)
{
    ShaderModule::VertexOutputs vertexOutputs;
    if (auto* returnPrimitive = std::get_if<WGSL::Types::Primitive>(&type)) {
        vertexOutputs.add(0, ShaderModule::VertexOutputFragmentInput {
            .dataType = metalDataTypeFromPrimitive(returnPrimitive),
            .interpolation = std::nullopt
        });
        return vertexOutputs;
    }
    if (std::get_if<WGSL::Types::Vector>(&type)) {
        vertexOutputs.add(0, ShaderModule::VertexOutputFragmentInput {
            .dataType = metalDataTypeForStructMember(&type),
            .interpolation = std::nullopt
        });
        return vertexOutputs;
    }
    auto* returnStruct = std::get_if<WGSL::Types::Struct>(&type);
    if (!returnStruct)
        return vertexOutputs;

    for (auto& member : returnStruct->structure.members()) {
        if (!member.location() || member.builtin())
            continue;

        auto location = *member.location();
        vertexOutputs.add(location, ShaderModule::VertexOutputFragmentInput {
            .dataType = metalDataTypeForStructMember(member.type().inferredType()),
            .interpolation = member.interpolation()
        });
    }

    return vertexOutputs;
}

static void populateStageInMap(const WGSL::Type& type, ShaderModule::VertexStageIn& vertexStageIn)
{
    auto* inputStruct = std::get_if<WGSL::Types::Struct>(&type);
    if (!inputStruct)
        return;

    for (auto& member : inputStruct->structure.members()) {
        if (!member.location())
            continue;
        auto location = *member.location();
        auto dataType = vertexFormatTypeForStructMember(member.type().inferredType());
        vertexStageIn.add(location, dataType);
    }
}

static void populateFragmentInputs(const WGSL::Type& type, ShaderModule::FragmentInputs& fragmentInputs)
{
    auto* inputStruct = std::get_if<WGSL::Types::Struct>(&type);
    if (!inputStruct)
        return;

    for (auto& member : inputStruct->structure.members()) {
        if (!member.location())
            continue;
        auto location = *member.location();
        auto dataType = metalDataTypeForStructMember(member.type().inferredType());
        fragmentInputs.add(location, ShaderModule::VertexOutputFragmentInput {
            .dataType = dataType,
            .interpolation = member.interpolation()
        });
    }
}

static ShaderModule::VertexStageIn parseStageIn(const WGSL::AST::Function& function)
{
    ShaderModule::VertexStageIn result;
    for (auto& parameter : function.parameters()) {
        if (parameter.role() != WGSL::AST::ParameterRole::UserDefined)
            continue;

        if (auto* inferredType = parameter.typeName().inferredType())
            populateStageInMap(*inferredType, result);
    }

    return result;
}

static ShaderModule::FragmentInputs parseFragmentInputs(const WGSL::AST::Function& function)
{
    ShaderModule::FragmentInputs result;
    for (auto& parameter : function.parameters()) {
        if (parameter.role() != WGSL::AST::ParameterRole::UserDefined)
            continue;

        if (auto* inferredType = parameter.typeName().inferredType())
            populateFragmentInputs(*inferredType, result);
    }

    return result;
}

ShaderModule::ShaderModule(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult, HashMap<String, Ref<PipelineLayout>>&& pipelineLayoutHints, HashMap<String, WGSL::Reflection::EntryPointInformation>&& entryPointInformation, id<MTLLibrary> library, NSMutableSet<NSString* >* originalOverrideNames, HashMap<String, String>&& originalFunctionNames, Device& device)
    : m_checkResult(convertCheckResult(WTFMove(checkResult)))
    , m_pipelineLayoutHints(WTFMove(pipelineLayoutHints))
    , m_entryPointInformation(WTFMove(entryPointInformation))
    , m_library(library)
    , m_device(device)
    , m_originalOverrideNames(originalOverrideNames)
    , m_originalFunctionNames(WTFMove(originalFunctionNames))
{
    bool allowVertexDefault = true, allowFragmentDefault = true, allowComputeDefault = true;
    if (std::holds_alternative<WGSL::SuccessfulCheck>(m_checkResult)) {
        auto& check = std::get<WGSL::SuccessfulCheck>(m_checkResult);
        for (auto& declaration : check.ast->declarations()) {
            if (!is<WGSL::AST::Function>(declaration))
                continue;
            auto& function = downcast<WGSL::AST::Function>(declaration);
            if (!function.stage())
                continue;
            switch (*function.stage()) {
            case WGSL::ShaderStage::Vertex: {
                m_stageInTypesForEntryPoint.add(function.name(), parseStageIn(function));
                if (auto expression = function.maybeReturnType()) {
                    if (auto* inferredType = expression->inferredType())
                        m_vertexReturnTypeForEntryPoint.add(function.name(), parseVertexReturnType(*inferredType));
                }
                if (!allowVertexDefault || m_defaultVertexEntryPoint.length()) {
                    allowVertexDefault = false;
                    m_defaultVertexEntryPoint = emptyString();
                    continue;
                }
                m_defaultVertexEntryPoint = function.name();
            } break;
            case WGSL::ShaderStage::Fragment: {
                m_fragmentInputsForEntryPoint.add(function.name(), parseFragmentInputs(function));
                if (auto expression = function.maybeReturnType()) {
                    if (auto* inferredType = expression->inferredType())
                        m_fragmentReturnTypeForEntryPoint.add(function.name(), parseFragmentReturnType(*inferredType));
                }
                if (!allowFragmentDefault || m_defaultFragmentEntryPoint.length()) {
                    allowFragmentDefault = false;
                    m_defaultFragmentEntryPoint = emptyString();
                    continue;
                }
                m_defaultFragmentEntryPoint = function.name();
            } break;
            case WGSL::ShaderStage::Compute: {
                if (!allowComputeDefault || m_defaultComputeEntryPoint.length()) {
                    allowComputeDefault = false;
                    m_defaultComputeEntryPoint = emptyString();
                    continue;
                }
                m_defaultComputeEntryPoint = function.name();
            } break;
            default:
                ASSERT_NOT_REACHED();
                break;
            }
        }
    }
}

const ShaderModule::FragmentInputs* ShaderModule::fragmentInputsForEntryPoint(const String& entryPoint) const
{
    if (auto it = m_fragmentInputsForEntryPoint.find(entryPoint); it != m_fragmentInputsForEntryPoint.end())
        return &it->value;

    auto transformed = m_originalFunctionNames.find(entryPoint);
    if (transformed == m_originalFunctionNames.end())
        return nullptr;
    if (auto it = m_fragmentInputsForEntryPoint.find(transformed->value); it != m_fragmentInputsForEntryPoint.end())
        return &it->value;

    return nullptr;
}

const ShaderModule::FragmentOutputs* ShaderModule::fragmentReturnTypeForEntryPoint(const String& entryPoint) const
{
    if (auto it = m_fragmentReturnTypeForEntryPoint.find(entryPoint); it != m_fragmentReturnTypeForEntryPoint.end())
        return &it->value;

    auto transformed = m_originalFunctionNames.find(entryPoint);
    if (transformed == m_originalFunctionNames.end())
        return nullptr;
    if (auto it = m_fragmentReturnTypeForEntryPoint.find(transformed->value); it != m_fragmentReturnTypeForEntryPoint.end())
        return &it->value;

    return nullptr;
}

const ShaderModule::VertexOutputs* ShaderModule::vertexReturnTypeForEntryPoint(const String& entryPoint) const
{
    if (auto it = m_vertexReturnTypeForEntryPoint.find(entryPoint); it != m_vertexReturnTypeForEntryPoint.end())
        return &it->value;

    auto transformed = m_originalFunctionNames.find(entryPoint);
    if (transformed == m_originalFunctionNames.end())
        return nullptr;
    if (auto it = m_vertexReturnTypeForEntryPoint.find(transformed->value); it != m_vertexReturnTypeForEntryPoint.end())
        return &it->value;

    return nullptr;
}

bool ShaderModule::hasOverride(const String& name) const
{
    return [m_originalOverrideNames containsObject:name];
}

const ShaderModule::VertexStageIn* ShaderModule::stageInTypesForEntryPoint(const String& entryPoint) const
{
    if (auto it = m_stageInTypesForEntryPoint.find(entryPoint); it != m_stageInTypesForEntryPoint.end())
        return &it->value;

    auto transformed = m_originalFunctionNames.find(entryPoint);
    if (transformed == m_originalFunctionNames.end())
        return nullptr;
    if (auto it = m_stageInTypesForEntryPoint.find(transformed->value); it != m_stageInTypesForEntryPoint.end())
        return &it->value;

    return nullptr;
}

ShaderModule::ShaderModule(Device& device, CheckResult&& checkResult)
    : m_checkResult(WTFMove(checkResult))
    , m_device(device)
{
}

ShaderModule::~ShaderModule() = default;

struct Messages {
    const Vector<WGSL::CompilationMessage>& messages;
    WGPUCompilationMessageType type;
};

struct CompilationMessageData {
    CompilationMessageData(Vector<WGPUCompilationMessage>&& compilationMessages, Vector<String>&& messages)
        : compilationMessages(WTFMove(compilationMessages))
        , messages(WTFMove(messages))
    {
    }

    CompilationMessageData(const CompilationMessageData&) = delete;
    CompilationMessageData(CompilationMessageData&&) = default;

    Vector<WGPUCompilationMessage> compilationMessages;
    Vector<String> messages;
};

static CompilationMessageData convertMessages(const Messages& messages1, const std::optional<Messages>& messages2 = std::nullopt)
{
    Vector<WGPUCompilationMessage> flattenedCompilationMessages;
    Vector<String> flattenedMessages;

    auto populateMessages = [&](const Messages& compilationMessages) {
        for (const auto& compilationMessage : compilationMessages.messages)
            flattenedMessages.append(compilationMessage.message());
    };

    populateMessages(messages1);
    if (messages2)
        populateMessages(*messages2);

    auto populateCompilationMessages = [&](const Messages& compilationMessages, size_t base) {
        for (size_t i = 0; i < compilationMessages.messages.size(); ++i) {
            const auto& compilationMessage = compilationMessages.messages[i];
            flattenedCompilationMessages.append({
                .nextInChain = nullptr,
                .message = flattenedMessages[i + base],
                .type = compilationMessages.type,
                .lineNum = compilationMessage.lineNumber(),
                .linePos = compilationMessage.lineOffset(),
                .offset = compilationMessage.offset(),
                .length = compilationMessage.length(),
                .utf16LinePos = compilationMessage.lineOffset(),
                .utf16Offset = compilationMessage.offset(),
                .utf16Length = compilationMessage.length(),
            });
        }
    };

    populateCompilationMessages(messages1, 0);
    if (messages2)
        populateCompilationMessages(*messages2, messages1.messages.size());

    return { WTFMove(flattenedCompilationMessages), WTFMove(flattenedMessages) };
}

void ShaderModule::getCompilationInfo(CompletionHandler<void(WGPUCompilationInfoRequestStatus, const WGPUCompilationInfo&)>&& callback)
{
    WTF::switchOn(m_checkResult, [&](const WGSL::SuccessfulCheck& successfulCheck) {
        auto compilationMessageData(convertMessages({ successfulCheck.warnings, WGPUCompilationMessageType_Warning }));
        WGPUCompilationInfo compilationInfo {
            nullptr,
            static_cast<uint32_t>(compilationMessageData.compilationMessages.size()),
            compilationMessageData.compilationMessages.data(),
        };
        callback(WGPUCompilationInfoRequestStatus_Success, compilationInfo);
    }, [&](const WGSL::FailedCheck& failedCheck) {
        auto compilationMessageData(convertMessages(
            { failedCheck.errors, WGPUCompilationMessageType_Error },
            { { failedCheck.warnings, WGPUCompilationMessageType_Warning } }));
        WGPUCompilationInfo compilationInfo {
            nullptr,
            static_cast<uint32_t>(compilationMessageData.compilationMessages.size()),
            compilationMessageData.compilationMessages.data(),
        };
        callback(WGPUCompilationInfoRequestStatus_Error, compilationInfo);
    }, [](std::monostate) {
        ASSERT_NOT_REACHED();
    });
}

void ShaderModule::setLabel(String&& label)
{
    if (m_library)
        m_library.label = label;
}

static auto wgslBindingType(WGPUBufferBindingType bindingType)
{
    switch (bindingType) {
    case WGPUBufferBindingType_Uniform:
        return WGSL::BufferBindingType::Uniform;
    case WGPUBufferBindingType_Storage:
        return WGSL::BufferBindingType::Storage;
    case WGPUBufferBindingType_ReadOnlyStorage:
        return WGSL::BufferBindingType::ReadOnlyStorage;
    case WGPUBufferBindingType_Undefined:
    case WGPUBufferBindingType_Force32:
        ASSERT_NOT_REACHED("Unexpected buffer bindingType");
        return WGSL::BufferBindingType::Uniform;
    }
}

static auto wgslSamplerType(WGPUSamplerBindingType bindingType)
{
    switch (bindingType) {
    case WGPUSamplerBindingType_Filtering:
        return WGSL::SamplerBindingType::Filtering;
    case WGPUSamplerBindingType_Comparison:
        return WGSL::SamplerBindingType::Comparison;
    case WGPUSamplerBindingType_NonFiltering:
        return WGSL::SamplerBindingType::NonFiltering;
    case WGPUSamplerBindingType_Force32:
    case WGPUSamplerBindingType_Undefined:
        ASSERT_NOT_REACHED("Unexpected sampler bindingType");
        return WGSL::SamplerBindingType::Filtering;
    }
}

static auto wgslSampleType(WGPUTextureSampleType sampleType)
{
    switch (sampleType) {
    case WGPUTextureSampleType_Sint:
        return WGSL::TextureSampleType::SignedInt;
    case WGPUTextureSampleType_Uint:
        return WGSL::TextureSampleType::UnsignedInt;
    case WGPUTextureSampleType_Depth:
        return WGSL::TextureSampleType::Depth;
    case WGPUTextureSampleType_Float:
        return WGSL::TextureSampleType::Float;
    case WGPUTextureSampleType_UnfilterableFloat:
        return WGSL::TextureSampleType::UnfilterableFloat;
    case WGPUTextureSampleType_Force32:
    case WGPUTextureSampleType_Undefined:
        ASSERT_NOT_REACHED("Unexpected sampleType");
        return WGSL::TextureSampleType::Float;
    }
}

static auto wgslViewDimension(WGPUTextureViewDimension viewDimension)
{
    switch (viewDimension) {
    case WGPUTextureViewDimension_Cube:
    case WGPUTextureViewDimension_1D:
        return WGSL::TextureViewDimension::OneDimensional;
    case WGPUTextureViewDimension_2D:
        return WGSL::TextureViewDimension::TwoDimensional;
    case WGPUTextureViewDimension_3D:
        return WGSL::TextureViewDimension::ThreeDimensional;
    case WGPUTextureViewDimension_CubeArray:
        return WGSL::TextureViewDimension::CubeArray;
    case WGPUTextureViewDimension_2DArray:
        return WGSL::TextureViewDimension::TwoDimensionalArray;
    case WGPUTextureViewDimension_Force32:
    case WGPUTextureViewDimension_Undefined:
        ASSERT_NOT_REACHED("Unexpected viewDimension");
        return WGSL::TextureViewDimension::TwoDimensional;
    }
}

static WGSL::StorageTextureAccess wgslAccess(WGPUStorageTextureAccess access)
{
    switch (access) {
    case WGPUStorageTextureAccess_WriteOnly:
        return WGSL::StorageTextureAccess::WriteOnly;
    case WGPUStorageTextureAccess_ReadOnly:
        return WGSL::StorageTextureAccess::ReadOnly;
    case WGPUStorageTextureAccess_ReadWrite:
        return WGSL::StorageTextureAccess::ReadWrite;
    case WGPUStorageTextureAccess_Undefined:
    case WGPUStorageTextureAccess_Force32:
        RELEASE_ASSERT_NOT_REACHED();
    }
}

static WGSL::TexelFormat wgslFormat(WGPUTextureFormat format)
{
    switch (format) {
    case WGPUTextureFormat_BGRA8Unorm:
        return WGSL::TexelFormat::BGRA8unorm;
    case WGPUTextureFormat_R32Float:
        return WGSL::TexelFormat::R32float;
    case WGPUTextureFormat_R32Sint:
        return WGSL::TexelFormat::R32sint;
    case WGPUTextureFormat_R32Uint:
        return WGSL::TexelFormat::R32uint;
    case WGPUTextureFormat_RG32Float:
        return WGSL::TexelFormat::RG32float;
    case WGPUTextureFormat_RG32Sint:
        return WGSL::TexelFormat::RG32sint;
    case WGPUTextureFormat_RG32Uint:
        return WGSL::TexelFormat::RG32uint;
    case WGPUTextureFormat_RGBA16Float:
        return WGSL::TexelFormat::RGBA16float;
    case WGPUTextureFormat_RGBA16Sint:
        return WGSL::TexelFormat::RGBA16sint;
    case WGPUTextureFormat_RGBA16Uint:
        return WGSL::TexelFormat::RGBA16uint;
    case WGPUTextureFormat_RGBA32Float:
        return WGSL::TexelFormat::RGBA32float;
    case WGPUTextureFormat_RGBA32Sint:
        return WGSL::TexelFormat::RGBA32sint;
    case WGPUTextureFormat_RGBA32Uint:
        return WGSL::TexelFormat::RGBA32uint;
    case WGPUTextureFormat_RGBA8Sint:
        return WGSL::TexelFormat::RGBA8sint;
    case WGPUTextureFormat_RGBA8Snorm:
        return WGSL::TexelFormat::RGBA8snorm;
    case WGPUTextureFormat_RGBA8Uint:
        return WGSL::TexelFormat::RGBA8uint;
    case WGPUTextureFormat_RGBA8Unorm:
        return WGSL::TexelFormat::RGBA8unorm;
    default:
        ASSERT_NOT_REACHED();
        return WGSL::TexelFormat::BGRA8unorm;
    }
}

static WGSL::BindGroupLayoutEntry::BindingMember convertBindingLayout(const BindGroupLayout::Entry::BindingLayout& bindingLayout)
{
    return WTF::switchOn(bindingLayout, [](const WGPUBufferBindingLayout& bindingLayout) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::BufferBindingLayout {
            .type = wgslBindingType(bindingLayout.type),
            .hasDynamicOffset = !!bindingLayout.hasDynamicOffset,
            .minBindingSize = bindingLayout.minBindingSize
        };
    }, [](const WGPUSamplerBindingLayout& bindingLayout) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::SamplerBindingLayout {
            .type = wgslSamplerType(bindingLayout.type)
        };
    }, [](const WGPUTextureBindingLayout& bindingLayout) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::TextureBindingLayout {
            .sampleType = wgslSampleType(bindingLayout.sampleType),
            .viewDimension = wgslViewDimension(bindingLayout.viewDimension),
            .multisampled = !!bindingLayout.multisampled
        };
    }, [](const WGPUStorageTextureBindingLayout& bindingLayout) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::StorageTextureBindingLayout {
            .access = wgslAccess(bindingLayout.access),
            .format = wgslFormat(bindingLayout.format),
            .viewDimension = wgslViewDimension(bindingLayout.viewDimension)
        };
    }, [](const WGPUExternalTextureBindingLayout&) -> WGSL::BindGroupLayoutEntry::BindingMember {
        return WGSL::ExternalTextureBindingLayout {
        };
    });
}

WGSL::PipelineLayout ShaderModule::convertPipelineLayout(const PipelineLayout& pipelineLayout)
{
    Vector<WGSL::BindGroupLayout> bindGroupLayouts;

    for (size_t i = 0; i < pipelineLayout.numberOfBindGroupLayouts(); ++i) {
        auto& bindGroupLayout = pipelineLayout.bindGroupLayout(i);
        WGSL::BindGroupLayout wgslBindGroupLayout;
        for (auto& entry : bindGroupLayout.entries()) {
            WGSL::BindGroupLayoutEntry wgslEntry;
            wgslEntry.binding = entry.value.binding;
            wgslEntry.visibility = wgslEntry.visibility.fromRaw(entry.value.visibility);
            wgslEntry.bindingMember = convertBindingLayout(entry.value.bindingLayout);
            wgslEntry.vertexArgumentBufferIndex = entry.value.argumentBufferIndices[WebGPU::ShaderStage::Vertex];
            wgslEntry.vertexArgumentBufferSizeIndex = entry.value.bufferSizeArgumentBufferIndices[WebGPU::ShaderStage::Vertex];
            if (entry.value.vertexDynamicOffset) {
                RELEASE_ASSERT(!(entry.value.vertexDynamicOffset.value() % sizeof(uint32_t)));
                wgslEntry.vertexBufferDynamicOffset = *entry.value.vertexDynamicOffset / sizeof(uint32_t);
            }
            wgslEntry.fragmentArgumentBufferIndex = entry.value.argumentBufferIndices[WebGPU::ShaderStage::Fragment];
            wgslEntry.fragmentArgumentBufferSizeIndex = entry.value.bufferSizeArgumentBufferIndices[WebGPU::ShaderStage::Fragment];
            if (entry.value.fragmentDynamicOffset) {
                RELEASE_ASSERT(!(entry.value.fragmentDynamicOffset.value() % sizeof(uint32_t)));
                wgslEntry.fragmentBufferDynamicOffset = *entry.value.fragmentDynamicOffset / sizeof(uint32_t) + RenderBundleEncoder::startIndexForFragmentDynamicOffsets;
            }
            wgslEntry.computeArgumentBufferIndex = entry.value.argumentBufferIndices[WebGPU::ShaderStage::Compute];
            wgslEntry.computeArgumentBufferSizeIndex = entry.value.bufferSizeArgumentBufferIndices[WebGPU::ShaderStage::Compute];
            if (entry.value.computeDynamicOffset) {
                RELEASE_ASSERT(!(entry.value.computeDynamicOffset.value() % sizeof(uint32_t)));
                wgslEntry.computeBufferDynamicOffset = *entry.value.computeDynamicOffset / sizeof(uint32_t);
            }
            wgslBindGroupLayout.entries.append(wgslEntry);
        }

        bindGroupLayouts.append(wgslBindGroupLayout);
    }

    return { .bindGroupLayouts = bindGroupLayouts };
}

WGSL::ShaderModule* ShaderModule::ast() const
{
    return WTF::switchOn(m_checkResult, [&](const WGSL::SuccessfulCheck& successfulCheck) -> WGSL::ShaderModule* {
        return successfulCheck.ast.ptr();
    }, [&](const WGSL::FailedCheck&) -> WGSL::ShaderModule* {
        return nullptr;
    }, [](std::monostate) -> WGSL::ShaderModule* {
        ASSERT_NOT_REACHED();
        return nullptr;
    });
}

const PipelineLayout* ShaderModule::pipelineLayoutHint(const String& name) const
{
    auto iterator = m_pipelineLayoutHints.find(name);
    if (iterator == m_pipelineLayoutHints.end())
        return nullptr;
    return iterator->value.ptr();
}

const WGSL::Reflection::EntryPointInformation* ShaderModule::entryPointInformation(const String& name) const
{
    auto iterator = m_entryPointInformation.find(name);
    if (iterator != m_entryPointInformation.end())
        return &iterator->value;

    auto transformed = m_originalFunctionNames.find(name);
    if (transformed == m_originalFunctionNames.end())
        return nullptr;
    if (auto it = m_entryPointInformation.find(transformed->value); it != m_entryPointInformation.end())
        return &it->value;

    return nullptr;
}

const String& ShaderModule::defaultVertexEntryPoint() const
{
    return m_defaultVertexEntryPoint;
}

const String& ShaderModule::defaultFragmentEntryPoint() const
{
    return m_defaultFragmentEntryPoint;
}

const String& ShaderModule::defaultComputeEntryPoint() const
{
    return m_defaultComputeEntryPoint;
}

const String& ShaderModule::transformedEntryPoint(const String& entryPoint) const
{
    if (!entryPoint.length())
        return entryPoint;

    auto transformed = m_originalFunctionNames.find(entryPoint);
    if (transformed != m_originalFunctionNames.end())
        return transformed->value;

    return entryPoint;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

void wgpuShaderModuleReference(WGPUShaderModule shaderModule)
{
    WebGPU::fromAPI(shaderModule).ref();
}

void wgpuShaderModuleRelease(WGPUShaderModule shaderModule)
{
    WebGPU::fromAPI(shaderModule).deref();
}

void wgpuShaderModuleGetCompilationInfo(WGPUShaderModule shaderModule, WGPUCompilationInfoCallback callback, void * userdata)
{
    WebGPU::fromAPI(shaderModule).getCompilationInfo([callback, userdata](WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo& compilationInfo) {
        callback(status, &compilationInfo, userdata);
    });
}

void wgpuShaderModuleGetCompilationInfoWithBlock(WGPUShaderModule shaderModule, WGPUCompilationInfoBlockCallback callback)
{
    WebGPU::fromAPI(shaderModule).getCompilationInfo([callback = WebGPU::fromAPI(WTFMove(callback))](WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo& compilationInfo) {
        callback(status, &compilationInfo);
    });
}

void wgpuShaderModuleSetLabel(WGPUShaderModule shaderModule, const char* label)
{
    WebGPU::fromAPI(shaderModule).setLabel(WebGPU::fromAPI(label));
}
