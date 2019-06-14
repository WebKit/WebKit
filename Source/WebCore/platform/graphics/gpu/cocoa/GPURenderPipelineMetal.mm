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

#import "GPUDevice.h"
#import "GPULimits.h"
#import "GPUPipelineMetalConvertLayout.h"
#import "GPUUtils.h"
#import "Logging.h"
#import "WHLSLPrepare.h"
#import "WHLSLVertexBufferIndexCalculator.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/CheckedArithmetic.h>
#import <wtf/HashSet.h>
#import <wtf/OptionSet.h>
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

// FIXME: Move this into GPULimits when that is implemented properly.
constexpr unsigned maxVertexAttributes = 16;

static bool trySetVertexInput(const char* const functionName, const GPUVertexInputDescriptor& descriptor, MTLRenderPipelineDescriptor *mtlDescriptor, Optional<WHLSL::RenderPipelineDescriptor>& whlslDescriptor)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    const auto& buffers = descriptor.vertexBuffers;

    if (buffers.size() > maxVertexBuffers) {
        LOG(WebGPU, "%s: Too many vertex input buffers!", functionName);
        return false;
    }

    auto mtlVertexDescriptor = adoptNS([MTLVertexDescriptor new]);

    auto layoutArray = retainPtr(mtlVertexDescriptor.get().layouts);
    auto attributeArray = retainPtr(mtlVertexDescriptor.get().attributes);

    // Attribute shaderLocations must be uniquely flat-mapped to [0, {max number of vertex attributes}].
    unsigned attributeIndex = 0;
    HashSet<unsigned, IntHash<unsigned>, WTF::UnsignedWithZeroKeyHashTraits<unsigned>> locations;

    for (size_t index = 0; index < buffers.size(); ++index) {
        if (!buffers[index])
            continue;

        const auto& attributes = buffers[index]->attributeSet;

        if (attributes.size() + attributeIndex > maxVertexAttributes) {
            LOG(WebGPU, "%s: Too many vertex attributes!", functionName);
            return false;
        }

        NSUInteger inputStride = 0;
        if (!WTF::convertSafely(buffers[index]->stride, inputStride)) {
            LOG(WebGPU, "%s: Stride for vertex input buffer %u is too large!", functionName, index);
            return false;
        }

        auto convertedBufferIndex = WHLSL::Metal::calculateVertexBufferIndex(index);

        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        auto mtlLayoutDesc = retainPtr([layoutArray objectAtIndexedSubscript:convertedBufferIndex]);
        [mtlLayoutDesc setStepFunction:mtlStepFunctionForGPUInputStepMode(buffers[index]->stepMode)];
        [mtlLayoutDesc setStride:inputStride];
        END_BLOCK_OBJC_EXCEPTIONS;

        for (const auto& attribute : attributes) {
            if (!locations.add(attribute.shaderLocation).isNewEntry) {
                LOG(WebGPU, "%s: Duplicate shaderLocation %u for vertex attribute!", functionName, attribute.shaderLocation);
                return false;
            }

            NSUInteger offset = 0;
            if (!WTF::convertSafely(attribute.offset, offset)) {
                LOG(WebGPU, "%s: Buffer offset for vertex attribute %u is too large!", functionName, attribute.shaderLocation);
                return false;
            }

            BEGIN_BLOCK_OBJC_EXCEPTIONS;
            auto mtlAttributeDesc = retainPtr([attributeArray objectAtIndexedSubscript:attributeIndex]);
            [mtlAttributeDesc setFormat:mtlVertexFormatForGPUVertexFormat(attribute.format)];
            [mtlAttributeDesc setOffset:offset];
            [mtlAttributeDesc setBufferIndex:convertedBufferIndex];
            END_BLOCK_OBJC_EXCEPTIONS;

            if (whlslDescriptor)
                whlslDescriptor->vertexAttributes.append({ convertVertexFormat(attribute.format), attribute.shaderLocation, attributeIndex });

            ++attributeIndex;
        }
    }

    [mtlDescriptor setVertexDescriptor:mtlVertexDescriptor.get()];

    return true;
}

static MTLColorWriteMask mtlColorWriteMaskForGPUColorWriteFlags(GPUColorWriteFlags flags)
{
    if (flags == static_cast<GPUColorWriteFlags>(GPUColorWriteBits::Flags::All))
        return MTLColorWriteMaskAll;

    auto options = OptionSet<GPUColorWriteBits::Flags>::fromRaw(flags);

    MTLColorWriteMask mask = MTLColorWriteMaskNone;
    if (options & GPUColorWriteBits::Flags::Red)
        mask |= MTLColorWriteMaskRed;
    if (options & GPUColorWriteBits::Flags::Green)
        mask |= MTLColorWriteMaskGreen;
    if (options & GPUColorWriteBits::Flags::Blue)
        mask |= MTLColorWriteMaskBlue;
    if (options & GPUColorWriteBits::Flags::Alpha)
        mask |= MTLColorWriteMaskAlpha;

    return mask;
}

static MTLBlendOperation mtlBlendOperationForGPUBlendOperation(GPUBlendOperation op)
{
    switch (op) {
    case GPUBlendOperation::Add:
        return MTLBlendOperationAdd;
    case GPUBlendOperation::Subtract:
        return MTLBlendOperationSubtract;
    case GPUBlendOperation::ReverseSubtract:
        return MTLBlendOperationReverseSubtract;
    case GPUBlendOperation::Min:
        return MTLBlendOperationMin;
    case GPUBlendOperation::Max:
        return MTLBlendOperationMax;
    }

    ASSERT_NOT_REACHED();
}

static MTLBlendFactor mtlBlendFactorForGPUBlendFactor(GPUBlendFactor factor)
{
    switch (factor) {
    case GPUBlendFactor::Zero:
        return MTLBlendFactorZero;
    case GPUBlendFactor::One:
        return MTLBlendFactorOne;
    case GPUBlendFactor::SrcColor:
        return MTLBlendFactorSourceColor;
    case GPUBlendFactor::OneMinusSrcColor:
        return MTLBlendFactorOneMinusSourceColor;
    case GPUBlendFactor::SrcAlpha:
        return MTLBlendFactorSourceAlpha;
    case GPUBlendFactor::OneMinusSrcAlpha:
        return MTLBlendFactorOneMinusSourceAlpha;
    case GPUBlendFactor::DstColor:
        return MTLBlendFactorDestinationColor;
    case GPUBlendFactor::OneMinusDstColor:
        return MTLBlendFactorOneMinusDestinationColor;
    case GPUBlendFactor::DstAlpha:
        return MTLBlendFactorDestinationAlpha;
    case GPUBlendFactor::OneMinusDstAlpha:
        return MTLBlendFactorOneMinusDestinationAlpha;
    case GPUBlendFactor::SrcAlphaSaturated:
        return MTLBlendFactorSourceAlpha;
    case GPUBlendFactor::BlendColor:
        return MTLBlendFactorBlendColor;
    case GPUBlendFactor::OneMinusBlendColor:
        return MTLBlendFactorOneMinusBlendColor;
    }

    ASSERT_NOT_REACHED();
}

static bool trySetColorStates(const char* const functionName, const Vector<GPUColorStateDescriptor>& colorStates, MTLRenderPipelineColorAttachmentDescriptorArray* array, Optional<WHLSL::RenderPipelineDescriptor>& whlslDescriptor)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    // FIXME: Replace with maximum number of color attachments per render pass from GPULimits.
    if (colorStates.size() > 4) {
        LOG(WebGPU, "%s: Invalid number of GPUColorStateDescriptors!", functionName);
        return false;
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    for (unsigned i = 0; i < colorStates.size(); ++i) {
        auto& state = colorStates[i];
        auto descriptor = retainPtr([array objectAtIndexedSubscript:i]);
        [descriptor setPixelFormat:static_cast<MTLPixelFormat>(platformTextureFormatForGPUTextureFormat(state.format))];
        [descriptor setWriteMask:mtlColorWriteMaskForGPUColorWriteFlags(state.writeMask)];
        [descriptor setBlendingEnabled:YES];
        [descriptor setAlphaBlendOperation:mtlBlendOperationForGPUBlendOperation(state.alphaBlend.operation)];
        [descriptor setRgbBlendOperation:mtlBlendOperationForGPUBlendOperation(state.colorBlend.operation)];
        [descriptor setDestinationAlphaBlendFactor:mtlBlendFactorForGPUBlendFactor(state.alphaBlend.dstFactor)];
        [descriptor setDestinationRGBBlendFactor:mtlBlendFactorForGPUBlendFactor(state.colorBlend.dstFactor)];
        [descriptor setSourceAlphaBlendFactor:mtlBlendFactorForGPUBlendFactor(state.alphaBlend.srcFactor)];
        [descriptor setSourceRGBBlendFactor:mtlBlendFactorForGPUBlendFactor(state.colorBlend.srcFactor)];

        if (whlslDescriptor) {
            if (auto format = convertTextureFormat(state.format))
                whlslDescriptor->attachmentsStateDescriptor.attachmentDescriptors.append({*format, i});
            else {
                LOG(WebGPU, "%s: Invalid texture format for color attachment %u!", functionName, i);
                return false;
            }
        }
    }

    END_BLOCK_OBJC_EXCEPTIONS;

    return true;
}

static bool trySetMetalFunctions(const char* const functionName, MTLLibrary *vertexMetalLibrary, MTLLibrary *fragmentMetalLibrary, MTLRenderPipelineDescriptor *mtlDescriptor, const String& vertexEntryPointName, const String& fragmentEntryPointName)
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

static bool trySetFunctions(const char* const functionName, const GPUPipelineStageDescriptor& vertexStage, const Optional<GPUPipelineStageDescriptor>& fragmentStage, const GPUDevice& device, MTLRenderPipelineDescriptor* mtlDescriptor, Optional<WHLSL::RenderPipelineDescriptor>& whlslDescriptor)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    RetainPtr<MTLLibrary> vertexLibrary, fragmentLibrary;
    String vertexEntryPoint, fragmentEntryPoint;

    if (whlslDescriptor) {
        // WHLSL functions are compiled to MSL first.
        String whlslSource = vertexStage.module->whlslSource();
        ASSERT(!whlslSource.isNull());

        whlslDescriptor->vertexEntryPointName = vertexStage.entryPoint;
        if (fragmentStage)
            whlslDescriptor->fragmentEntryPointName = fragmentStage->entryPoint;

        auto whlslCompileResult = WHLSL::prepare(whlslSource, *whlslDescriptor);
        if (!whlslCompileResult)
            return false;

        NSError *error = nil;

        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        vertexLibrary = adoptNS([device.platformDevice() newLibraryWithSource:whlslCompileResult->metalSource options:nil error:&error]);
        END_BLOCK_OBJC_EXCEPTIONS;

#ifndef NDEBUG
        if (!vertexLibrary)
            NSLog(@"%@", error);
#endif
        ASSERT(vertexLibrary);
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195771 Once we zero-fill variables, there should be no warnings, so we should be able to ASSERT(!error) here.

        fragmentLibrary = vertexLibrary;
        vertexEntryPoint = whlslCompileResult->mangledVertexEntryPointName;
        fragmentEntryPoint = whlslCompileResult->mangledFragmentEntryPointName;
    } else {
        vertexLibrary = vertexStage.module->platformShaderModule();
        vertexEntryPoint = vertexStage.entryPoint;
        if (fragmentStage) {
            fragmentLibrary = fragmentStage->module->platformShaderModule();
            fragmentEntryPoint = fragmentStage->entryPoint;
        }
    }

    return trySetMetalFunctions(functionName, vertexLibrary.get(), fragmentLibrary.get(), mtlDescriptor, vertexEntryPoint, fragmentEntryPoint);
}

static RetainPtr<MTLRenderPipelineDescriptor> convertRenderPipelineDescriptor(const char* const functionName, const GPURenderPipelineDescriptor& descriptor, const GPUDevice& device)
{
    RetainPtr<MTLRenderPipelineDescriptor> mtlDescriptor;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    mtlDescriptor = adoptNS([MTLRenderPipelineDescriptor new]);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!mtlDescriptor) {
        LOG(WebGPU, "%s: Error creating MTLDescriptor!", functionName);
        return nullptr;
    }

    // Determine if shader source is in WHLSL or MSL.
    const auto& vertexStage = descriptor.vertexStage;
    const auto& fragmentStage = descriptor.fragmentStage;

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=195446 Allow WHLSL shaders to come from different programs.
    bool isWhlsl = !vertexStage.module->whlslSource().isNull() && (!fragmentStage || vertexStage.module.ptr() == fragmentStage->module.ptr());

    // Set data for the Metal pipeline descriptor (and WHLSL's, if needed).
    Optional<WHLSL::RenderPipelineDescriptor> whlslDescriptor;
    if (isWhlsl)
        whlslDescriptor = WHLSL::RenderPipelineDescriptor();

    if (!trySetVertexInput(functionName, descriptor.vertexInput, mtlDescriptor.get(), whlslDescriptor))
        return nullptr;

    if (!trySetColorStates(functionName, descriptor.colorStates, mtlDescriptor.get().colorAttachments, whlslDescriptor))
        return nullptr;

    if (descriptor.layout && whlslDescriptor) {
        if (auto layout = convertLayout(*descriptor.layout))
            whlslDescriptor->layout = WTFMove(*layout);
        else {
            LOG(WebGPU, "%s: Error converting GPUPipelineLayout!", functionName);
            return nullptr;
        }
    }

    if (!trySetFunctions(functionName, vertexStage, fragmentStage, device, mtlDescriptor.get(), whlslDescriptor))
        return nullptr;

    return mtlDescriptor;
}

static RetainPtr<MTLRenderPipelineState> tryCreateMtlRenderPipelineState(const char* const functionName, const GPURenderPipelineDescriptor& descriptor, const GPUDevice& device)
{
    if (!device.platformDevice()) {
        LOG(WebGPU, "GPUComputePipeline::tryCreate(): Invalid GPUDevice!");
        return nullptr;
    }

    auto mtlDescriptor = convertRenderPipelineDescriptor(functionName, descriptor, device);
    if (!mtlDescriptor)
        return nullptr;

    RetainPtr<MTLRenderPipelineState> pipeline;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSError *error = nil;
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

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=198387 depthStencilAttachmentDescriptor isn't implemented yet for WHLSL compiler.

    auto pipeline = tryCreateMtlRenderPipelineState(functionName, descriptor, device);
    if (!pipeline)
        return nullptr;

    return adoptRef(new GPURenderPipeline(WTFMove(depthStencil), WTFMove(pipeline), descriptor.primitiveTopology, descriptor.vertexInput.indexFormat));
}

GPURenderPipeline::GPURenderPipeline(RetainPtr<MTLDepthStencilState>&& depthStencil, RetainPtr<MTLRenderPipelineState>&& pipeline, GPUPrimitiveTopology topology, Optional<GPUIndexFormat> format)
    : m_depthStencilState(WTFMove(depthStencil))
    , m_platformRenderPipeline(WTFMove(pipeline))
    , m_primitiveTopology(topology)
    , m_indexFormat(format)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
