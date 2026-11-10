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
#import "CallGraph.h"
#import "WGSL.h"
#import <WebGPU/WebGPUCpp.h>
#import <variant>
#import <wtf/CompletionHandler.h>
#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefPtr.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>


namespace WGSL {
namespace AST {
class Function;
}
struct Type;
}

namespace WebGPU::Metal {

class Device;
class PipelineLayout;

// https://gpuweb.github.io/gpuweb/#gpushadermodule
class ShaderModule final : public WebGPU::ShaderModule {
    WTF_MAKE_TZONE_ALLOCATED(ShaderModule);

    // The AST of a valid module lives in m_ast, which can be released and rebuilt from m_source.
    struct ValidCheck {
        Vector<WGSL::Warning> warnings;
    };
    using CheckResult = Variant<ValidCheck, WGSL::FailedCheck, std::monostate>;
public:
    static Ref<ShaderModule> create(WGSL::SuccessfulCheck&& successfulCheck, RefPtr<StringImpl>&& source, WGSL::Configuration&& configuration, HashMap<RefPtr<StringImpl>, Ref<PipelineLayout>>&& pipelineLayoutHints, HashMap<RefPtr<StringImpl>, WGSL::Reflection::EntryPointInformation>&& entryPointInformation, id<MTLLibrary> library, Device& device)
    {
        Ref shaderModule = adoptRef(*new ShaderModule(WTF::move(successfulCheck), WTF::move(source), WTF::move(configuration), WTF::move(pipelineLayoutHints), WTF::move(entryPointInformation), library, device));
        shaderModule->releaseASTIfUnused();
        return shaderModule;
    }
    static Ref<ShaderModule> createInvalid(Device& device, CheckResult&& checkResult = std::monostate { })
    {
        return adoptRef(*new ShaderModule(device, WTF::move(checkResult)));
    }

    ~ShaderModule();

    void compilationInfo(CompletionHandler<void(WebGPU::CompilationInfo&&)>&&) final;
    void setLabel(RefPtr<StringImpl>&&) final;

    bool isValid() const final { return std::holds_alternative<ValidCheck>(m_checkResult); }

    static WGSL::PipelineLayout convertPipelineLayout(const PipelineLayout&);
    static id<MTLLibrary> createLibrary(id<MTLDevice>, StringImpl* msl, RefPtr<StringImpl>&& label, NSError **, WGSL::DeviceState&&);
    static id<MTLLibrary> createLibrary(id<MTLDevice>, NSString *msl, NSString *label, NSError **, const WGSL::DeviceState&);
    static void createLibraryAsync(id<MTLDevice>, NSString *msl, NSString *label, const WGSL::DeviceState&, CompletionHandler<void(id<MTLLibrary>, NSError *)>&&);

    class ASTHandle {
        WTF_MAKE_NONCOPYABLE(ASTHandle);
    public:
        ASTHandle() = default;
        ASTHandle(ASTHandle&&);
        ASTHandle& operator=(ASTHandle&&);
        ~ASTHandle();

        WGSL::ShaderModule* get() const { return m_ast; }
        WGSL::ShaderModule& operator*() const { ASSERT(m_ast); return *m_ast; }
        WGSL::ShaderModule* operator->() const { ASSERT(m_ast); return m_ast; }
        explicit operator bool() const { return !!m_ast; }

    private:
        friend class ShaderModule;
        ASTHandle(const ShaderModule&, WGSL::ShaderModule&);

        RefPtr<const ShaderModule> m_shaderModule;
        WGSL::ShaderModule* m_ast { nullptr };
    };

    ASTHandle ast() const;

    const PipelineLayout* pipelineLayoutHint(StringImpl*) const;
    const WGSL::Reflection::EntryPointInformation* entryPointInformation(StringImpl*) const;
    id<MTLLibrary> library() const { return m_library; }

    Device& device() const { return m_device; }
    NODELETE const RefPtr<StringImpl>& defaultVertexEntryPoint() const;
    NODELETE const RefPtr<StringImpl>& defaultFragmentEntryPoint() const;
    NODELETE const RefPtr<StringImpl>& defaultComputeEntryPoint() const;

    using VertexStageIn = HashMap<uint32_t, WebGPU::VertexFormat, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using FragmentOutputs = HashMap<uint32_t, MTLDataType, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    struct VertexOutputFragmentInput {
        MTLDataType dataType { MTLDataTypeNone };
        std::optional<WGSL::AST::Interpolation> interpolation { std::nullopt };
    };
    using VertexOutputs = HashMap<uint32_t, VertexOutputFragmentInput, DefaultHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>>;
    using FragmentInputs = VertexOutputs;
    const FragmentOutputs* fragmentReturnTypeForEntryPoint(StringImpl*) const;
    const FragmentInputs* fragmentInputsForEntryPoint(StringImpl*) const;
    const VertexStageIn* stageInTypesForEntryPoint(StringImpl*) const;
    const VertexOutputs* vertexReturnTypeForEntryPoint(StringImpl*) const;
    bool usesFrontFacingInInput(StringImpl*) const;
    bool usesSampleIndexInInput(StringImpl*) const;
    bool usesSampleMaskInInput(StringImpl*) const;
    bool usesSampleMaskInOutput(StringImpl*) const;
    bool usesFragDepth(StringImpl*) const;
    bool usesDualSourceBlending(StringImpl*) const;
    bool usesPrimitiveIndexInInput(StringImpl*) const;
    bool usesSubgroupInvocationIdInInput(StringImpl*) const;
    bool usesSubgroupSizeInInput(StringImpl*) const;
    uint32_t clipDistancesCount(StringImpl*) const;

private:
    ShaderModule(WGSL::SuccessfulCheck&&, RefPtr<StringImpl>&& source, WGSL::Configuration&&, HashMap<RefPtr<StringImpl>, Ref<PipelineLayout>>&&, HashMap<RefPtr<StringImpl>, WGSL::Reflection::EntryPointInformation>&&, id<MTLLibrary>, Device&);
    ShaderModule(Device&, CheckResult&&);

    void releaseASTIfUnused() const;

    const CheckResult m_checkResult;
    // A module can be kept alive for a long time before it is used to create a pipeline, and its
    // AST is many times the size of its source, so the AST only exists while the module is being
    // created or a pipeline is being created from it.
    const RefPtr<StringImpl> m_source;
    const std::optional<WGSL::Configuration> m_configuration;
    mutable std::unique_ptr<WGSL::ShaderModule> m_ast;
    mutable unsigned m_astHandleCount { 0 };
    const HashMap<RefPtr<StringImpl>, Ref<PipelineLayout>> m_pipelineLayoutHints;
    const HashMap<RefPtr<StringImpl>, WGSL::Reflection::EntryPointInformation> m_entryPointInformation;
    const id<MTLLibrary> m_library { nil }; // This is only non-null if we could compile the module early.
    void populateFragmentInputs(const WGSL::Type&, ShaderModule::FragmentInputs&, StringImpl*);
    FragmentInputs parseFragmentInputs(const WGSL::AST::Function&, StringImpl* entryPointName);
    void populateOutputState(StringImpl*, WGSL::Builtin);

    ShaderModule::FragmentOutputs parseFragmentReturnType(const WGSL::Type&, const WGSL::CallGraph::EntryPoint&);

    const Ref<Device> m_device;
    HashMap<RefPtr<StringImpl>, FragmentOutputs> m_fragmentReturnTypeForEntryPoint;
    HashMap<RefPtr<StringImpl>, FragmentInputs> m_fragmentInputsForEntryPoint;
    HashMap<RefPtr<StringImpl>, VertexOutputs> m_vertexReturnTypeForEntryPoint;
    HashMap<RefPtr<StringImpl>, VertexStageIn> m_stageInTypesForEntryPoint;

    RefPtr<StringImpl> m_defaultVertexEntryPoint;
    RefPtr<StringImpl> m_defaultFragmentEntryPoint;
    RefPtr<StringImpl> m_defaultComputeEntryPoint;

    struct ShaderModuleState {
        bool usesFrontFacingInInput { false };
        bool usesSampleIndexInInput { false };
        bool usesSampleMaskInInput { false };
        bool usesSampleMaskInOutput { false };
        bool usesFragDepth { false };
        bool usesDualSourceBlending { false };
        bool usesPrimitiveIndexInInput { false };
        bool usesSubgroupInvocationIdInInput { false };
        bool usesSubgroupSizeInInput { false };
        uint32_t clipDistancesCount { 0 }; // Number of clip distances (0 if not used)
    };
    const ShaderModuleState* shaderModuleState(StringImpl*) const;
    ShaderModuleState& populateShaderModuleState(StringImpl*);
    HashMap<RefPtr<StringImpl>, ShaderModuleState> m_usageInformationPerEntryPoint;
};

} // namespace WebGPU::Metal
