/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "GPURenderPipeline.h"

#if ENABLE(WEBGPU)

#import "GPULimits.h"
#import "GPUUtils.h"
#import "Logging.h"
#import "WHLSLPrepare.h"
#import "WHLSLVertexBufferIndexCalculator.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Optional.h>

namespace WebCore {

static RetainPtr<MTLDepthStencilState> tryCreateMtlDepthStencilState(const char* const functionName, const GPUDepthStencilStateDescriptor& descriptor, const GPUDevice& device)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    RetainPtr<MTLDepthStencilDescriptor> mtlDescriptor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlDescriptor = adoptNS([MTLDepthStencilDescriptor new]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlDescriptor) {
        LOG(WebGPU, "%s: Unable to create MTLDepthStencilDescriptor!", functionName);
        return nullptr;
    }

    auto mtlDepthCompare = static_cast<MTLCompareFunction>(platformCompareFunctionForGPUCompareFunction(descriptor.depthCompare));
    [mtlDescriptor setDepthCompareFunction:mtlDepthCompare];
    [mtlDescriptor setDepthWriteEnabled:descriptor.depthWriteEnabled];

    // FIXME: Implement back/frontFaceStencil.

    RetainPtr<MTLDepthStencilState> state;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    state = adoptNS([device.platformDevice() newDepthStencilStateWithDescriptor:mtlDescriptor.get()]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!state) {
        LOG(WebGPU, "%s: Error creating MTLDepthStencilState!", functionName);
        return nullptr;
    }

    return state;
}

static WHLSL::VertexFormat convertVertexFormat(GPUVertexFormat vertexFormat)
{
    switch (vertexFormat) {
    case GPUVertexFormat::Float4:
        return WHLSL::VertexFormat::FloatR32G32B32A32;
    case GPUVertexFormat::Float3:
        return WHLSL::VertexFormat::FloatR32G32B32;
    case GPUVertexFormat::Float2:
        return WHLSL::VertexFormat::FloatR32G32;
    default:
        ASSERT(vertexFormat == GPUVertexFormat::Float);
        return WHLSL::VertexFormat::FloatR32;
    }
}

static OptionSet<WHLSL::ShaderStage> convertShaderStageFlags(GPUShaderStageFlags flags)
{
    OptionSet<WHLSL::ShaderStage> result;
    if (flags & GPUShaderStageBit::Flags::Vertex)
        result.add(WHLSL::ShaderStage::Vertex);
    if (flags & GPUShaderStageBit::Flags::Fragment)
        result.add(WHLSL::ShaderStage::Fragment);
    if (flags & GPUShaderStageBit::Flags::Compute)
        result.add(WHLSL::ShaderStage::Compute);
    return result;
}

static Optional<WHLSL::BindingType> convertBindingType(GPUBindingType type)
{
    switch (type) {
    case GPUBindingType::UniformBuffer:
        return WHLSL::BindingType::UniformBuffer;
    case GPUBindingType::Sampler:
        return WHLSL::BindingType::Sampler;
    case GPUBindingType::SampledTexture:
        return WHLSL::BindingType::Texture;
    case GPUBindingType::StorageBuffer:
        return WHLSL::BindingType::StorageBuffer;
    default:
        return WTF::nullopt;
    }
}

static Optional<WHLSL::TextureFormat> convertTextureFormat(GPUTextureFormat format)
{
    switch (format) {
    case GPUTextureFormat::Rgba8unorm:
        return WHLSL::TextureFormat::RGBA8Unorm;
    case GPUTextureFormat::Rgba8uint:
        return WHLSL::TextureFormat::RGBA8Uint;
    case GPUTextureFormat::Bgra8unorm:
        return WHLSL::TextureFormat::BGRA8Unorm;
    case GPUTextureFormat::Depth32floatStencil8:
        return WTF::nullopt; // FIXME: Figure out what to do with this.
    case GPUTextureFormat::Bgra8unormSRGB:
        return WHLSL::TextureFormat::BGRA8UnormSrgb;
    case GPUTextureFormat::Rgba16float:
        return WHLSL::TextureFormat::RGBA16Float;
    default:
        return WTF::nullopt;
    }
}

static Optional<WHLSL::Layout> convertLayout(const GPUPipelineLayout& layout)
{
    WHLSL::Layout result;
    if (layout.bindGroupLayouts().size() > std::numeric_limits<unsigned>::max())
        return WTF::nullopt;
    for (size_t i = 0; i < layout.bindGroupLayouts().size(); ++i) {
        const auto& bindGroupLayout = layout.bindGroupLayouts()[i];
        WHLSL::BindGroup bindGroup;
        bindGroup.name = static_cast<unsigned>(i);
        for (const auto& keyValuePair : bindGroupLayout->bindingsMap()) {
            const auto& gpuBindGroupLayoutBinding = keyValuePair.value;
            WHLSL::Binding binding;
            binding.visibility = convertShaderStageFlags(gpuBindGroupLayoutBinding.visibility);
            if (auto bindingType = convertBindingType(gpuBindGroupLayoutBinding.type))
                binding.bindingType = *bindingType;
            else
                return WTF::nullopt;
            if (gpuBindGroupLayoutBinding.binding > std::numeric_limits<unsigned>::max())
                return WTF::nullopt;
            binding.name = static_cast<unsigned>(gpuBindGroupLayoutBinding.binding);
            bindGroup.bindings.append(WTFMove(binding));
        }
        result.append(WTFMove(bindGroup));
    }
    return result;
}

static Optional<WHLSL::RenderPipelineDescriptor> convertRenderPipelineDescriptor(const GPURenderPipelineDescriptor& descriptor)
{
    WHLSL::RenderPipelineDescriptor whlslDescriptor;
    if (descriptor.inputState.attributes.size() > std::numeric_limits<unsigned>::max())
        return WTF::nullopt;
    if (descriptor.colorStates.size() > std::numeric_limits<unsigned>::max())
        return WTF::nullopt;

    for (size_t i = 0; i < descriptor.inputState.attributes.size(); ++i)
        whlslDescriptor.vertexAttributes.append({ convertVertexFormat(descriptor.inputState.attributes[i].format), static_cast<unsigned>(i) });

    for (size_t i = 0; i < descriptor.colorStates.size(); ++i) {
        if (auto format = convertTextureFormat(descriptor.colorStates[i].format))
            whlslDescriptor.attachmentsStateDescriptor.attachmentDescriptors.append({*format, static_cast<unsigned>(i)});
        else
            return WTF::nullopt;
    }

    // FIXME: depthStencilAttachmentDescriptor isn't implemented yet.

    if (descriptor.layout) {
        if (auto layout = convertLayout(*descriptor.layout))
            whlslDescriptor.layout = WTFMove(*layout);
        else
            return WTF::nullopt;
    }
    whlslDescriptor.vertexEntryPointName = descriptor.vertexStage.entryPoint;
    whlslDescriptor.fragmentEntryPointName = descriptor.fragmentStage.entryPoint;
    return whlslDescriptor;
}

static bool trySetMetalFunctionsForPipelineDescriptor(const char* const functionName, MTLLibrary *vertexMetalLibrary, MTLLibrary *fragmentMetalLibrary, MTLRenderPipelineDescriptor *mtlDescriptor, const String& vertexEntryPointName, const String& fragmentEntryPointName)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif

    {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;

        // Metal requires a vertex shader in all render pipelines.
        if (!vertexMetalLibrary) {
            LOG(WebGPU, "%s: MTLLibrary for vertex stage does not exist!", functionName);
            return false;
        }

        auto function = adoptNS([vertexMetalLibrary newFunctionWithName:vertexEntryPointName]);
        if (!function) {
            LOG(WebGPU, "%s: Cannot create vertex MTLFunction \"%s\"!", functionName, vertexEntryPointName.utf8().data());
            return false;
        }

        [mtlDescriptor setVertexFunction:function.get()];

        END_BLOCK_OBJC_EXCEPTIONS;
    }

    {
        BEGIN_BLOCK_OBJC_EXCEPTIONS;

        // However, fragment shaders are optional.
        if (!fragmentMetalLibrary)
            return true;

        auto function = adoptNS([fragmentMetalLibrary newFunctionWithName:fragmentEntryPointName]);

        if (!function) {
            LOG(WebGPU, "%s: Cannot create fragment MTLFunction \"%s\"!", functionName, fragmentEntryPointName.utf8().data());
            return false;
        }

        [mtlDescriptor setFragmentFunction:function.get()];
        return true;

        END_BLOCK_OBJC_EXCEPTIONS;
    }

    return false;
}

static bool trySetWHLSLFunctionsForPipelineDescriptor(const char* const functionName, MTLRenderPipelineDescriptor *mtlDescriptor, const GPURenderPipelineDescriptor& descriptor, String whlslSource, const GPUDevice& device)
{
    auto whlslDescriptor = convertRenderPipelineDescriptor(descriptor);
    if (!whlslDescriptor)
        return false;

    auto result = WHLSL::prepare(whlslSource, *whlslDescriptor);
    if (!result)
        return false;

    NSError *error = nil;
    auto library = adoptNS([device.platformDevice() newLibraryWithSource:result->metalSource options:nil error:&error]);
    ASSERT(library);
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195771 Once we zero-fill variables, there should be no warnings, so we should be able to ASSERT(!error) here.

    return trySetMetalFunctionsForPipelineDescriptor(functionName, library.get(), library.get(), mtlDescriptor, result->mangledVertexEntryPointName, result->mangledFragmentEntryPointName);
}

static bool trySetFunctionsForPipelineDescriptor(const char* const functionName, MTLRenderPipelineDescriptor *mtlDescriptor, const GPURenderPipelineDescriptor& descriptor, const GPUDevice& device)
{
    const auto& vertexStage = descriptor.vertexStage;
    const auto& fragmentStage = descriptor.fragmentStage;

    if (vertexStage.module.ptr() == fragmentStage.module.ptr()) {
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195446 Allow WHLSL shaders to come from different programs.
        const auto& whlslSource = vertexStage.module->whlslSource();
        if (!whlslSource.isNull())
            return trySetWHLSLFunctionsForPipelineDescriptor(functionName, mtlDescriptor, descriptor, whlslSource, device);
    }

    auto vertexLibrary = vertexStage.module->platformShaderModule();
    MTLLibrary *fragmentLibrary = nil;
    if (!fragmentStage.entryPoint.isNull())
        fragmentLibrary = fragmentStage.module->platformShaderModule();

    return trySetMetalFunctionsForPipelineDescriptor(functionName, vertexLibrary, fragmentLibrary, mtlDescriptor, vertexStage.entryPoint, fragmentStage.entryPoint);
}

static MTLVertexFormat mtlVertexFormatForGPUVertexFormat(GPUVertexFormat format)
{
    switch (format) {
    case GPUVertexFormat::Float:
        return MTLVertexFormatFloat;
    case GPUVertexFormat::Float2:
        return MTLVertexFormatFloat2;
    case GPUVertexFormat::Float3:
        return MTLVertexFormatFloat3;
    case GPUVertexFormat::Float4:
        return MTLVertexFormatFloat4;
    }

    ASSERT_NOT_REACHED();
}

static MTLVertexStepFunction mtlStepFunctionForGPUInputStepMode(GPUInputStepMode mode)
{
    switch (mode) {
    case GPUInputStepMode::Vertex:
        return MTLVertexStepFunctionPerVertex;
    case GPUInputStepMode::Instance:
        return MTLVertexStepFunctionPerInstance;
    }

    ASSERT_NOT_REACHED();
}

static bool trySetInputStateForPipelineDescriptor(const char* const functionName, MTLRenderPipelineDescriptor *mtlDescriptor, const GPUInputStateDescriptor& descriptor)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    auto mtlVertexDescriptor = adoptNS([MTLVertexDescriptor new]);

    const auto& attributes = descriptor.attributes;

    auto attributeArray = retainPtr(mtlVertexDescriptor.get().attributes);

    for (size_t i = 0; i < attributes.size(); ++i) {
        auto location = attributes[i].shaderLocation;
        // Maximum number of vertex attributes to be supported by Web GPU.
        if (location >= 16) {
            LOG(WebGPU, "%s: Invalid shaderLocation %lu for vertex attribute!", functionName, location);
            return false;
        }
        if (attributes[i].inputSlot >= maxVertexBuffers) {
            LOG(WebGPU, "%s: Invalid inputSlot %lu for vertex attribute %lu!", functionName, attributes[i].inputSlot, location);
            return false;
        }

        auto mtlAttributeDesc = retainPtr([attributeArray objectAtIndexedSubscript:location]);
        [mtlAttributeDesc setFormat:mtlVertexFormatForGPUVertexFormat(attributes[i].format)];
        [mtlAttributeDesc setOffset:attributes[i].offset]; // FIXME: After adding more vertex formats, ensure offset < buffer's stride + format's data size.
        [mtlAttributeDesc setBufferIndex:WHLSL::Metal::calculateVertexBufferIndex(attributes[i].inputSlot)];
    }

    const auto& inputs = descriptor.inputs;

    auto layoutArray = retainPtr(mtlVertexDescriptor.get().layouts);

    for (size_t j = 0; j < inputs.size(); ++j) {
        auto slot = inputs[j].inputSlot;
        if (slot >= maxVertexBuffers) {
            LOG(WebGPU, "%s: Invalid inputSlot %d for vertex buffer!", functionName, slot);
            return false;
        }

        auto convertedSlot = WHLSL::Metal::calculateVertexBufferIndex(slot);
        auto mtlLayoutDesc = retainPtr([layoutArray objectAtIndexedSubscript:convertedSlot]);
        [mtlLayoutDesc setStepFunction:mtlStepFunctionForGPUInputStepMode(inputs[j].stepMode)];
        [mtlLayoutDesc setStride:inputs[j].stride];
    }

    [mtlDescriptor setVertexDescriptor:mtlVertexDescriptor.get()];

    return true;
}

static bool trySetColorStatesForColorAttachmentArray(MTLRenderPipelineColorAttachmentDescriptorArray* array, const Vector<GPUColorStateDescriptor>& colorStates)
{
    // FIXME: Implement and validate the rest of GPUColorStateDescriptor.
    for (unsigned i = 0; i < colorStates.size(); ++i)
        [array[i] setPixelFormat:static_cast<MTLPixelFormat>(platformTextureFormatForGPUTextureFormat(colorStates[i].format))];

    return true;
}

static RetainPtr<MTLRenderPipelineState> tryCreateMtlRenderPipelineState(const char* const functionName, const GPURenderPipelineDescriptor& descriptor, const GPUDevice& device)
{
    RetainPtr<MTLRenderPipelineDescriptor> mtlDescriptor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlDescriptor = adoptNS([MTLRenderPipelineDescriptor new]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlDescriptor) {
        LOG(WebGPU, "%s: Error creating MTLDescriptor!", functionName);
        return nullptr;
    }

    bool didSetFunctions = false, didSetInputState = false, didSetColorStates = false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    didSetFunctions = trySetFunctionsForPipelineDescriptor(functionName, mtlDescriptor.get(), descriptor, device);
    didSetInputState = trySetInputStateForPipelineDescriptor(functionName, mtlDescriptor.get(), descriptor.inputState);
    didSetColorStates = trySetColorStatesForColorAttachmentArray(mtlDescriptor.get().colorAttachments, descriptor.colorStates);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!didSetFunctions || !didSetInputState || !didSetColorStates)
        return nullptr;

    RetainPtr<MTLRenderPipelineState> pipeline;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSError *error = [NSError errorWithDomain:@"com.apple.WebKit.GPU" code:1 userInfo:nil];
    pipeline = adoptNS([device.platformDevice() newRenderPipelineStateWithDescriptor:mtlDescriptor.get() error:&error]);
    if (!pipeline)
        LOG(WebGPU, "%s: %s!", functionName, error.localizedDescription.UTF8String);

    END_BLOCK_OBJC_EXCEPTIONS;

    return pipeline;
}

RefPtr<GPURenderPipeline> GPURenderPipeline::tryCreate(const GPUDevice& device, const GPURenderPipelineDescriptor& descriptor)
{
    const char* const functionName = "GPURenderPipeline::create()";

    if (!device.platformDevice()) {
        LOG(WebGPU, "%s: Invalid GPUDevice!", functionName);
        return nullptr;
    }

    RetainPtr<MTLDepthStencilState> depthStencil;

    if (descriptor.depthStencilState && !(depthStencil = tryCreateMtlDepthStencilState(functionName, *descriptor.depthStencilState, device)))
        return nullptr;

    auto pipeline = tryCreateMtlRenderPipelineState(functionName, descriptor, device);
    if (!pipeline)
        return nullptr;

    return adoptRef(new GPURenderPipeline(WTFMove(depthStencil), WTFMove(pipeline), descriptor.primitiveTopology));
}

GPURenderPipeline::GPURenderPipeline(RetainPtr<MTLDepthStencilState>&& depthStencil, RetainPtr<MTLRenderPipelineState>&& pipeline, GPUPrimitiveTopology topology)
    : m_depthStencilState(WTFMove(depthStencil))
    , m_platformRenderPipeline(WTFMove(pipeline))
    , m_primitiveTopology(topology)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
