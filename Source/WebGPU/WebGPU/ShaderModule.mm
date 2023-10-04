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
#import "Device.h"
#import "PipelineLayout.h"

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
    return ShaderModule::create(WTFMove(checkResult), WTFMove(hints), WTFMove(prepareResult.entryPoints), library, device);
}

Ref<ShaderModule> Device::createShaderModule(const WGPUShaderModuleDescriptor& descriptor)
{
    if (!descriptor.nextInChain)
        return ShaderModule::createInvalid(*this);

    auto shaderModuleParameters = findShaderModuleParameters(descriptor);
    if (!shaderModuleParameters)
        return ShaderModule::createInvalid(*this);

    auto checkResult = WGSL::staticCheck(fromAPI(shaderModuleParameters->wgsl.code), std::nullopt, { maxBuffersPlusVertexBuffersForVertexStage(), maxBuffersForFragmentStage(), maxBuffersForComputeStage() });

    if (std::holds_alternative<WGSL::SuccessfulCheck>(checkResult)) {
        if (shaderModuleParameters->hints && descriptor.hintCount) {
            // FIXME: re-enable early compilation later on once deferred compilation is fully implemented
            // https://bugs.webkit.org/show_bug.cgi?id=254258
            UNUSED_PARAM(earlyCompileShaderModule);
        }
    } else {
        auto& failedCheck = std::get<WGSL::FailedCheck>(checkResult);
        StringPrintStream message;
        message.print(String::number(failedCheck.errors.size()), " error", failedCheck.errors.size() != 1 ? "s" : "", " generated while compiling the shader:"_s);
        for (const auto& error : failedCheck.errors) {
            message.print("\n"_s, error);
        }
        dataLogLn(message.toString());
        generateAValidationError(message.toString());
        return ShaderModule::createInvalid(*this);
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
                .nextInChain = nullptr,
                .message = flattenedMessages[i + base].data(),
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
            wgslEntry.visibility.fromRaw(entry.value.visibility);
            wgslEntry.bindingMember = convertBindingLayout(entry.value.bindingLayout);
            wgslEntry.vertexArgumentBufferIndex = entry.value.argumentBufferIndices[WebGPU::ShaderStage::Vertex];
            wgslEntry.vertexBufferDynamicOffset = entry.value.vertexDynamicOffset;
            wgslEntry.fragmentArgumentBufferIndex = entry.value.argumentBufferIndices[WebGPU::ShaderStage::Fragment];
            wgslEntry.fragmentBufferDynamicOffset = entry.value.fragmentDynamicOffset;
            wgslEntry.computeArgumentBufferIndex = entry.value.argumentBufferIndices[WebGPU::ShaderStage::Compute];
            wgslEntry.computeBufferDynamicOffset = entry.value.computeDynamicOffset;
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
    if (iterator == m_entryPointInformation.end())
        return nullptr;
    return &iterator->value;
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
