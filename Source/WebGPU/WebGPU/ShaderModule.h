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

#import "WGSL.h"
#import <wtf/FastMalloc.h>
#import <wtf/Ref.h>
#import <wtf/RefCounted.h>
#import <wtf/text/StringHash.h>
#import <wtf/text/WTFString.h>

struct WGPUShaderModuleImpl {
};

namespace WebGPU {

class Device;
class PipelineLayout;

// https://gpuweb.github.io/gpuweb/#gpushadermodule
class ShaderModule : public WGPUShaderModuleImpl, public RefCounted<ShaderModule> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static Ref<ShaderModule> create(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult, HashMap<String, Ref<PipelineLayout>>&& pipelineLayoutHints, HashMap<String, WGSL::Reflection::EntryPointInformation>&& entryPointInformation, id<MTLLibrary> library, Device& device)
    {
        return adoptRef(*new ShaderModule(WTFMove(checkResult), WTFMove(pipelineLayoutHints), WTFMove(entryPointInformation), library, device));
    }
    static Ref<ShaderModule> createInvalid(Device& device)
    {
        return adoptRef(*new ShaderModule(device));
    }

    ~ShaderModule();

    void getCompilationInfo(CompletionHandler<void(WGPUCompilationInfoRequestStatus, const WGPUCompilationInfo&)>&& callback);
    void setLabel(String&&);

    id<MTLFunction> getNamedFunction(const String& name) const;

    bool isValid() const { return !std::holds_alternative<std::monostate>(m_checkResult); }

    static WGSL::PipelineLayout convertPipelineLayout(const PipelineLayout&);
    static id<MTLLibrary> createLibrary(id<MTLDevice>, const String& msl, String&& label);

    const WGSL::AST::ShaderModule* ast() const;

    const PipelineLayout* pipelineLayoutHint(const String&) const;
    const WGSL::Reflection::EntryPointInformation* entryPointInformation(const String&) const;
    id<MTLLibrary> library() const { return m_library; }

    Device& device() const { return m_device; }

private:
    ShaderModule(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&&, HashMap<String, Ref<PipelineLayout>>&&, HashMap<String, WGSL::Reflection::EntryPointInformation>&&, id<MTLLibrary>, Device&);
    ShaderModule(Device&);

    using CheckResult = std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck, std::monostate>;
    CheckResult convertCheckResult(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&&);

    const CheckResult m_checkResult;
    const HashMap<String, Ref<PipelineLayout>> m_pipelineLayoutHints;
    const HashMap<String, WGSL::Reflection::EntryPointInformation> m_entryPointInformation;
    const id<MTLLibrary> m_library { nil }; // This is only non-null if we could compile the module early.

    const Ref<Device> m_device;
};

} // namespace WebGPU
