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

#import "APIConversions.h"
#import "Device.h"
#import "PipelineLayout.h"

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
    options.fastMathEnabled = YES;
    NSError *error = nil;
    // FIXME(PERFORMANCE): Run the asynchronous version of this
    id<MTLLibrary> library = [device newLibraryWithSource:msl options:options error:&error];
    if (error) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=250442
        WTFLogAlways("MSL compilation error: %@", error);
    }
    library.label = label;
    return library;
}

static RefPtr<ShaderModule> earlyCompileShaderModule(Device& device, std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult, const WGPUShaderModuleDescriptor& suppliedHints, String&& label)
{
    HashMap<String, Ref<PipelineLayout>> hints;
    HashMap<String, WGSL::PipelineLayout> wgslHints;
    for (uint32_t i = 0; i < suppliedHints.hintCount; ++i) {
        const auto& hint = suppliedHints.hints[i];
        if (hint.nextInChain)
            return nullptr;
        auto hintKey = fromAPI(hint.entryPoint);
        hints.add(hintKey, WebGPU::fromAPI(hint.layout));
        auto convertedPipelineLayout = ShaderModule::convertPipelineLayout(WebGPU::fromAPI(hint.layout));
        wgslHints.add(hintKey, WTFMove(convertedPipelineLayout));
    }

    auto prepareResult = WGSL::prepare(std::get<WGSL::SuccessfulCheck>(checkResult).ast, wgslHints);
    auto library = ShaderModule::createLibrary(device.device(), prepareResult.msl, WTFMove(label));
    if (!library)
        return nullptr;
    return ShaderModule::create(WTFMove(checkResult), WTFMove(hints), WTFMove(prepareResult.entryPoints), library, device);
}

Ref<ShaderModule> Device::createShaderModule(const WGPUShaderModuleDescriptor& descriptor)
{
    if (!descriptor.nextInChain)
        return ShaderModule::createInvalid(*this);

    auto shaderModuleParameters = findShaderModuleParameters(descriptor);
    if (!shaderModuleParameters)
        return ShaderModule::createInvalid(*this);

    auto checkResult = WGSL::staticCheck(fromAPI(shaderModuleParameters->wgsl.code), std::nullopt, { maxBuffersPlusVertexBuffersForVertexStage() });

    // FIXME: we shouldn't compile early unless hints were passed in. Remove this
    // once we have wired up the deferred compilation.
    if (std::holds_alternative<WGSL::SuccessfulCheck>(checkResult)) {
        if (auto result = earlyCompileShaderModule(*this, WTFMove(checkResult), descriptor, fromAPI(descriptor.label)))
            return result.releaseNonNull();
    } else {
        // FIXME: remove shader library generation from MSL after compiler bringup
        auto library = ShaderModule::createLibrary(device(), String::fromUTF8(shaderModuleParameters->wgsl.code), fromAPI(descriptor.label));
        if (library)
            return ShaderModule::create(WTFMove(checkResult), { }, { }, library, *this);
    }

    return ShaderModule::create(WTFMove(checkResult), { }, { }, nil, *this);
}

auto ShaderModule::convertCheckResult(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult) -> CheckResult
{
    return WTF::switchOn(WTFMove(checkResult), [](auto&& check) -> CheckResult {
        return std::forward<decltype(check)>(check);
    });
}

ShaderModule::ShaderModule(std::variant<WGSL::SuccessfulCheck, WGSL::FailedCheck>&& checkResult, HashMap<String, Ref<PipelineLayout>>&& pipelineLayoutHints, HashMap<String, WGSL::Reflection::EntryPointInformation>&& entryPointInformation, id<MTLLibrary> library, Device& device)
    : m_checkResult(convertCheckResult(WTFMove(checkResult)))
    , m_pipelineLayoutHints(WTFMove(pipelineLayoutHints))
    , m_entryPointInformation(WTFMove(entryPointInformation))
    , m_library(library)
    , m_device(device)
{
}

ShaderModule::ShaderModule(Device& device)
    : m_checkResult(std::monostate { })
    , m_device(device)
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

    auto populateMessages = [&](const Messages& compilationMessages) {
        for (const auto& compilationMessage : compilationMessages.messages)
            flattenedMessages.append(compilationMessage.message().utf8());
    };

    populateMessages(messages1);
    if (messages2)
        populateMessages(*messages2);

    auto populateCompilationMessages = [&](const Messages& compilationMessages, size_t base) {
        for (size_t i = 0; i < compilationMessages.messages.size(); ++i) {
            const auto& compilationMessage = compilationMessages.messages[i];
            flattenedCompilationMessages.append({
                nullptr,
                flattenedMessages[i + base].data(),
                compilationMessages.type,
                compilationMessage.lineNumber(),
                compilationMessage.lineOffset(),
                compilationMessage.offset(),
                compilationMessage.length(),
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
        m_device->instance().scheduleWork([compilationInfo = WTFMove(compilationInfo), callback = WTFMove(callback)]() mutable {
            callback(WGPUCompilationInfoRequestStatus_Success, compilationInfo);
        });
    }, [&](const WGSL::FailedCheck& failedCheck) {
        auto compilationMessageData(convertMessages(
            { failedCheck.errors, WGPUCompilationMessageType_Error },
            { { failedCheck.warnings, WGPUCompilationMessageType_Warning } }));
        WGPUCompilationInfo compilationInfo {
            nullptr,
            static_cast<uint32_t>(compilationMessageData.compilationMessages.size()),
            compilationMessageData.compilationMessages.data(),
        };
        m_device->instance().scheduleWork([compilationInfo = WTFMove(compilationInfo), callback = WTFMove(callback)]() mutable {
            callback(WGPUCompilationInfoRequestStatus_Error, compilationInfo);
        });
    }, [](std::monostate) {
        ASSERT_NOT_REACHED();
    });
}

void ShaderModule::setLabel(String&& label)
{
    if (m_library)
        m_library.label = label;
}

id<MTLFunction> ShaderModule::getNamedFunction(const String& originalName, const HashMap<String, double>& keyValueReplacements) const
{
    const auto* information = entryPointInformation(originalName);
    const String& name = information ? information->mangledName : originalName;
    auto originalFunction = [m_library newFunctionWithName:name];

    if (!keyValueReplacements.size())
        return originalFunction;

    NSDictionary<NSString *, MTLFunctionConstant *> *originalFunctionConstants = [originalFunction functionConstantsDictionary];
    MTLFunctionConstantValues *constantValues = [MTLFunctionConstantValues new];
    for (auto& kvp : keyValueReplacements) {
        auto it = m_constantIdentifiersToNames.find(kvp.key);
        auto& constantName = it != m_constantIdentifiersToNames.end() ? it->value : kvp.key;

        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=250444 - it would be preferable
        // to get the type information from the WGSL compiler so we don't have to call
        // -[MTLLibrary newFunctionWithName:] twice
        MTLDataType dataType = [originalFunctionConstants objectForKey:constantName].type;
        union {
            bool b;
            int32_t i;
            uint32_t u;
            float f;
            __fp16 h;
        } v;
        if (dataType == MTLDataTypeFloat)
            v.f = static_cast<decltype(v.f)>(kvp.value);
        else if (dataType == MTLDataTypeHalf)
            v.h = static_cast<decltype(v.h)>(kvp.value);
        else if (dataType == MTLDataTypeInt)
            v.i = static_cast<decltype(v.i)>(kvp.value);
        else if (dataType == MTLDataTypeUInt)
            v.u = static_cast<decltype(v.u)>(kvp.value);
        else if (dataType == MTLDataTypeBool)
            v.b = static_cast<decltype(v.b)>(kvp.value);
        else {
            ASSERT_NOT_REACHED("Unsupported MTLFunctionConstant data type");
            return nil;
        }

        [constantValues setConstantValue:&v type:dataType withName:constantName];
    }

    NSError *error;
    id<MTLFunction> result = [m_library newFunctionWithName:name constantValues:constantValues error:&error];

    if (error) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=250442
        WTFLogAlways("MSL compilation error: %@", error);
    }

    return result;
}

WGSL::PipelineLayout ShaderModule::convertPipelineLayout(const PipelineLayout& pipelineLayout)
{
    UNUSED_PARAM(pipelineLayout);
    // FIXME: Implement this
    return { { } };
}

WGSL::AST::ShaderModule* ShaderModule::ast() const
{
    return WTF::switchOn(m_checkResult, [&](const WGSL::SuccessfulCheck& successfulCheck) -> WGSL::AST::ShaderModule* {
        return successfulCheck.ast.ptr();
    }, [&](const WGSL::FailedCheck&) -> WGSL::AST::ShaderModule* {
        return nullptr;
    }, [](std::monostate) -> WGSL::AST::ShaderModule* {
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
    if (iterator == m_entryPointInformation.end())
        return nullptr;
    return &iterator->value;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

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
