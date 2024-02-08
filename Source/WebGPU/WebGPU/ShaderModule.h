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

#pragma once

#import "ASTInterpolateAttribute.h"
#import "WGSL.h"
#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

struct WGPUShaderModuleImpl {
};

namespace WGSL {
namespace AST {
class Function;
}
struct Type;
}

namespace WebGPU {

class Device;
class PipelineLayout;

// https://gpuweb.github.io/gpuweb/#gpushadermodule
class ShaderModule : public WGPUShaderModuleImpl, public RefCounted<ShaderModule> {
    WTF_MAKE_FAST_ALLOCATED;

    using CheckResult = std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck, std::monostate>;
public:
    static Ref<ShaderModule> create(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult, HashMap<String, Ref<PipelineLayout>>&& pipelineLayoutHints, HashMap<String, WGSL::Reflection::EntryPointInformation>&& entryPointInformation, id<MTLLibrary> library, NSMutableSet<NSString *> *originalOverrideNames, HashMap<String, String>&& originalFunctionNames, Device& device)
    {
        return adoptRef(*new ShaderModule(WTFMove(checkResult), WTFMove(pipelineLayoutHints), WTFMove(entryPointInformation), library, originalOverrideNames, WTFMove(originalFunctionNames), device));
    }
    static Ref<ShaderModule> createInvalid(Device& device, CheckResult&& checkResult = std::monostate { })
    {
        return adoptRef(*new ShaderModule(device, WTFMove(checkResult)));
    }

    ~ShaderModule();

    void getCompilationInfo(CompletionHandler<void(WGPUCompilationInfoRequestStatus, const WGPUCompilationInfo&)>&& callback);
    void setLabel(String&&);

    bool isValid() const { return std::holds_alternative<WGSL::SuccessfulCheck>(m_checkResult); }

    static WGSL::PipelineLayout convertPipelineLayout(const PipelineLayout&);
    static id<MTLLibrary> createLibrary(id<MTLDevice>, const String& msl, String&& label);

    WGSL::ShaderModule* ast() const;

    const PipelineLayout* pipelineLayoutHint(const String&) const;
    const WGSL::Reflection::EntryPointInformation* entryPointInformation(const String&) const;
    id<MTLLibrary> library() const { return m_library; }
    const String& transformedEntryPoint(const String&) const;

    Device& device() const { return m_device; }
    const String& defaultVertexEntryPoint() const;
    const String& defaultFragmentEntryPoint() const;
    const String& defaultComputeEntryPoint() const;

    using VertexStageIn = HashMap<uint32_t, WGPUVertexFormat, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using FragmentOutputs = HashMap<uint32_t, MTLDataType, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    struct VertexOutputFragmentInput {
        MTLDataType dataType { MTLDataTypeNone };
        std::optional<WGSL::AST::Interpolation> interpolation { std::nullopt };
    };
    using VertexOutputs = HashMap<uint32_t, VertexOutputFragmentInput, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using FragmentInputs = VertexOutputs;
    const FragmentOutputs* fragmentReturnTypeForEntryPoint(const String&) const;
    const FragmentInputs* fragmentInputsForEntryPoint(const String&) const;
    bool hasOverride(const String&) const;
    const VertexStageIn* stageInTypesForEntryPoint(const String&) const;
    const VertexOutputs* vertexReturnTypeForEntryPoint(const String&) const;
    bool usesFrontFacingInInput() const;
    bool usesSampleIndexInInput() const;
    bool usesSampleMaskInInput() const;

private:
    ShaderModule(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&&, HashMap<String, Ref<PipelineLayout>>&&, HashMap<String, WGSL::Reflection::EntryPointInformation>&&, id<MTLLibrary>, NSMutableSet<NSString *> *, HashMap<String, String>&&, Device&);
    ShaderModule(Device&, CheckResult&&);

    CheckResult convertCheckResult(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&&);

    const CheckResult m_checkResult;
    const HashMap<String, Ref<PipelineLayout>> m_pipelineLayoutHints;
    const HashMap<String, WGSL::Reflection::EntryPointInformation> m_entryPointInformation;
    const id<MTLLibrary> m_library { nil }; // This is only non-null if we could compile the module early.
    void populateFragmentInputs(const WGSL::Type&, ShaderModule::FragmentInputs&);
    FragmentInputs parseFragmentInputs(const WGSL::AST::Function&);

    const Ref<Device> m_device;
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=250441 - this needs to be populated from the compiler
    HashMap<String, String> m_constantIdentifiersToNames;
    HashMap<String, FragmentOutputs> m_fragmentReturnTypeForEntryPoint;
    HashMap<String, FragmentInputs> m_fragmentInputsForEntryPoint;
    HashMap<String, VertexOutputs> m_vertexReturnTypeForEntryPoint;
    HashMap<String, VertexStageIn> m_stageInTypesForEntryPoint;

    String m_defaultVertexEntryPoint;
    String m_defaultFragmentEntryPoint;
    String m_defaultComputeEntryPoint;

    NSMutableSet<NSString *> *m_originalOverrideNames { nil };
    const HashMap<String, String> m_originalFunctionNames;
    bool m_usesFrontFacingInInput { false };
    bool m_usesSampleIndexInInput { false };
    bool m_usesSampleMaskInInput { false };
};

} // namespace WebGPU
