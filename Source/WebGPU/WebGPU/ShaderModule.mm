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
#import "ShaderModule.h"

#import "Device.h"
#import "PipelineLayout.h"

namespace WebGPU {

struct ShaderModuleParameters {
    const WGPUShaderModuleWGSLDescriptor& wgsl;
    const WGPUShaderModuleDescriptorHints* hints;
};

static std::optional<ShaderModuleParameters> findShaderModuleParameters(const WGPUShaderModuleDescriptor& descriptor)
{
    const WGPUShaderModuleWGSLDescriptor* wgsl = nullptr;
    const WGPUShaderModuleDescriptorHints* hints = nullptr;

    for (const WGPUChainedStruct* ptr = descriptor.nextInChain; ptr; ptr = ptr->next) {
        auto type = ptr->sType;

        switch (static_cast<int>(type)) {
        case WGPUSType_ShaderModuleWGSLDescriptor:
            if (wgsl)
                return std::nullopt;
            wgsl = reinterpret_cast<const WGPUShaderModuleWGSLDescriptor*>(ptr);
            break;
        case WGPUSTypeExtended_ShaderModuleDescriptorHints:
            if (hints)
                return std::nullopt;
            hints = reinterpret_cast<const WGPUShaderModuleDescriptorHints*>(ptr);
            break;
        default:
            return std::nullopt;
        }
    }

    if (!wgsl)
        return std::nullopt;

    return { { *wgsl, hints } };
}

id<MTLLibrary> ShaderModule::createLibrary(id<MTLDevice> device, const String& msl, NSString *label)
{
    auto options = [MTLCompileOptions new];
    options.fastMathEnabled = YES;
    NSError *error = nil;
    // FIXME: Run the asynchronous version of this
    id<MTLLibrary> library = [device newLibraryWithSource:msl options:options error:&error];
    if (error)
        WTFLogAlways("MSL compilation error: %@", error);
    library.label = label;
    return library;
}

static RefPtr<ShaderModule> earlyCompileShaderModule(id<MTLDevice> device, std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult, const WGPUShaderModuleDescriptorHints& suppliedHints, NSString *label)
{
    HashMap<String, Ref<PipelineLayout>> hints;
    HashMap<String, WGSL::PipelineLayout> wgslHints;
    for (uint32_t i = 0; i < suppliedHints.hintsCount; ++i) {
        const auto& hint = suppliedHints.hints[i];
        if (hint.nextInChain)
            return nullptr;
        hints.add(hint.key, hint.hint.layout->pipelineLayout);
        auto convertedPipelineLayout = ShaderModule::convertPipelineLayout(hint.hint.layout->pipelineLayout);
        wgslHints.add(hint.key, WTFMove(convertedPipelineLayout));
    }
    auto prepareResult = WGSL::prepare(std::get<WGSL::SuccessfulCheck>(checkResult).ast, wgslHints);
    auto library = ShaderModule::createLibrary(device, prepareResult.msl, label);
    if (!library)
        return nullptr;
    return ShaderModule::create(WTFMove(checkResult), WTFMove(hints), WTFMove(prepareResult.entryPoints), library);
}

RefPtr<ShaderModule> Device::createShaderModule(const WGPUShaderModuleDescriptor& descriptor)
{
    if (!descriptor.nextInChain)
        return nullptr;

    auto shaderModuleParameters = findShaderModuleParameters(descriptor);
    if (!shaderModuleParameters)
        return nullptr;

    auto checkResult = WGSL::staticCheck(String(shaderModuleParameters->wgsl.code), std::nullopt);

    if (std::holds_alternative<WGSL::SuccessfulCheck>(checkResult) && shaderModuleParameters->hints && shaderModuleParameters->hints->hintsCount) {
        if (auto result = earlyCompileShaderModule(m_device, WTFMove(checkResult), *shaderModuleParameters->hints, [NSString stringWithCString:descriptor.label encoding:NSUTF8StringEncoding]))
            return result;
    }

    return ShaderModule::create(WTFMove(checkResult), { }, { }, nil);
}

ShaderModule::ShaderModule(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult, HashMap<String, Ref<PipelineLayout>>&& pipelineLayoutHints, HashMap<String, WGSL::Reflection::EntryPointInformation>&& entryPointInformation, id<MTLLibrary> library)
    : m_checkResult(WTFMove(checkResult))
    , m_pipelineLayoutHints(WTFMove(pipelineLayoutHints))
    , m_entryPointInformation(WTFMove(entryPointInformation))
    , m_library(library)
{
}

ShaderModule::~ShaderModule() = default;

struct Messages {
    const Vector<WGSL::CompilationMessage>& messages;
    WGPUCompilationMessageType type;
};

struct CompilationMessageData {
    CompilationMessageData(Vector<WGPUCompilationMessage>&& compilationMessages, Vector<CString>&& messages)
        : compilationMessages(WTFMove(compilationMessages))
        , messages(WTFMove(messages))
    {
    }

    CompilationMessageData(const CompilationMessageData&) = delete;
    CompilationMessageData(CompilationMessageData&&) = default;

    Vector<WGPUCompilationMessage> compilationMessages;
    Vector<CString> messages;
};

static CompilationMessageData convertMessages(const Messages& messages1, const std::optional<Messages>& messages2 = std::nullopt)
{
    Vector<WGPUCompilationMessage> flattenedCompilationMessages;
    Vector<CString> flattenedMessages;

    auto populateMessages = [&] (const Messages& compilationMessages) {
        for (const auto& compilationMessage : compilationMessages.messages)
            flattenedMessages.append(compilationMessage.message.utf8());
    };

    populateMessages(messages1);
    if (messages2)
        populateMessages(*messages2);

    auto populateCompilationMessages = [&] (const Messages& compilationMessages, size_t base) {
        for (size_t i = 0; i < compilationMessages.messages.size(); ++i) {
            const auto& compilationMessage = compilationMessages.messages[i];
            flattenedCompilationMessages.append({
                nullptr,
                flattenedMessages[i + base].data(),
                compilationMessages.type,
                compilationMessage.lineNumber,
                compilationMessage.linePosition,
                compilationMessage.offset,
                compilationMessage.length,
            });
        }
    };

    populateCompilationMessages(messages1, 0);
    if (messages2)
        populateCompilationMessages(*messages2, messages1.messages.size());

    return { WTFMove(flattenedCompilationMessages), WTFMove(flattenedMessages) };
}

void ShaderModule::getCompilationInfo(WTF::Function<void(WGPUCompilationInfoRequestStatus, const WGPUCompilationInfo&)>&& callback)
{
    WTF::switchOn(m_checkResult, [&] (const WGSL::SuccessfulCheck& successfulCheck) {
        auto compilationMessageData(convertMessages({ successfulCheck.warnings, WGPUCompilationMessageType_Warning }));
        WGPUCompilationInfo compilationInfo {
            nullptr,
            static_cast<uint32_t>(compilationMessageData.compilationMessages.size()),
            compilationMessageData.compilationMessages.data(),
        };
        callback(WGPUCompilationInfoRequestStatus_Success, compilationInfo);
    }, [&] (const WGSL::FailedCheck& failedCheck) {
        auto compilationMessageData(convertMessages(
            { failedCheck.errors, WGPUCompilationMessageType_Error },
            { { failedCheck.warnings, WGPUCompilationMessageType_Warning } }));
        WGPUCompilationInfo compilationInfo {
            nullptr,
            static_cast<uint32_t>(compilationMessageData.compilationMessages.size()),
            compilationMessageData.compilationMessages.data(),
        };
        callback(WGPUCompilationInfoRequestStatus_Error, compilationInfo);
    });
}

void ShaderModule::setLabel(const char* label)
{
    if (m_library)
        m_library.label = [NSString stringWithCString:label encoding:NSUTF8StringEncoding];
}

WGSL::PipelineLayout ShaderModule::convertPipelineLayout(const PipelineLayout& pipelineLayout)
{
    UNUSED_PARAM(pipelineLayout);
    // FIXME: Implement this
    return { { } };
}

const WGSL::AST* ShaderModule::ast() const
{
    return WTF::switchOn(m_checkResult, [&] (const WGSL::SuccessfulCheck& successfulCheck) -> const WGSL::AST* {
        return successfulCheck.ast.ptr();
    }, [&] (const WGSL::FailedCheck&) -> const WGSL::AST* {
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
    if (iterator == m_entryPointInformation.end())
        return nullptr;
    return &iterator->value;
}

} // namespace WebGPU

void wgpuShaderModuleRelease(WGPUShaderModule shaderModule)
{
    delete shaderModule;
}

void wgpuShaderModuleGetCompilationInfo(WGPUShaderModule shaderModule, WGPUCompilationInfoCallback callback, void * userdata)
{
    shaderModule->shaderModule->getCompilationInfo([callback, userdata] (WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo& compilationInfo) {
        callback(status, &compilationInfo, userdata);
    });
}

void wgpuShaderModuleGetCompilationInfoWithBlock(WGPUShaderModule shaderModule, WGPUCompilationInfoBlockCallback callback)
{
    shaderModule->shaderModule->getCompilationInfo([callback] (WGPUCompilationInfoRequestStatus status, const WGPUCompilationInfo& compilationInfo) {
        callback(status, &compilationInfo);
    });
}

void wgpuShaderModuleSetLabel(WGPUShaderModule shaderModule, const char* label)
{
    shaderModule->shaderModule->setLabel(label);
}
