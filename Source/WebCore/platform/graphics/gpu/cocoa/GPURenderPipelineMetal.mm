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
#import "Logging.h"
#import "WHLSLVertexBufferIndexCalculator.h"
#import <Metal/Metal.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/Optional.h>

namespace WebCore {

static Optional<MTLCompareFunction> validateAndConvertDepthCompareFunctionToMtl(GPUCompareFunction func)
{
    switch (func) {
    case GPUCompareFunction::Never:
        return MTLCompareFunctionNever;
    case GPUCompareFunction::Less:
        return MTLCompareFunctionLess;
    case GPUCompareFunction::Equal:
        return MTLCompareFunctionEqual;
    case GPUCompareFunction::LessEqual:
        return MTLCompareFunctionLessEqual;
    case GPUCompareFunction::Greater:
        return MTLCompareFunctionGreater;
    case GPUCompareFunction::NotEqual:
        return MTLCompareFunctionNotEqual;
    case GPUCompareFunction::GreaterEqual:
        return MTLCompareFunctionGreaterEqual;
    case GPUCompareFunction::Always:
        return MTLCompareFunctionAlways;
    default:
        return WTF::nullopt;
    }
}

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

    auto mtlDepthCompare = validateAndConvertDepthCompareFunctionToMtl(descriptor.depthCompare);
    if (!mtlDepthCompare) {
        LOG(WebGPU, "%s: Invalid GPUCompareFunction in GPUDepthStencilStateDescriptor!", functionName);
        return nullptr;
    }

    [mtlDescriptor setDepthCompareFunction:*mtlDepthCompare];
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

static bool setFunctionsForPipelineDescriptor(const char* const functionName, MTLRenderPipelineDescriptor *mtlDescriptor, const GPURenderPipelineDescriptor& descriptor)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    // Metal requires a vertex shader in all render pipelines.
    const auto& vertexStage = descriptor.vertexStage;
    auto mtlLibrary = vertexStage.module->platformShaderModule();

    if (!mtlLibrary) {
        LOG(WebGPU, "%s: MTLLibrary for vertex stage does not exist!", functionName);
        return false;
    }

    auto function = adoptNS([mtlLibrary newFunctionWithName:vertexStage.entryPoint]);

    if (!function) {
        LOG(WebGPU, "%s: Vertex MTLFunction \"%s\" not found!", functionName, vertexStage.entryPoint.utf8().data());
        return false;
    }

    [mtlDescriptor setVertexFunction:function.get()];

    // However, fragment shaders are optional.
    const auto fragmentStage = descriptor.fragmentStage;
    if (!fragmentStage.module || !fragmentStage.entryPoint)
        return true;

    mtlLibrary = fragmentStage.module->platformShaderModule();

    if (!mtlLibrary) {
        LOG(WebGPU, "%s: MTLLibrary for fragment stage does not exist!", functionName);
        return false;
    }

    function = adoptNS([mtlLibrary newFunctionWithName:fragmentStage.entryPoint]);

    if (!function) {
        LOG(WebGPU, "%s: Fragment MTLFunction \"%s\" not found!", functionName, fragmentStage.entryPoint.utf8().data());
        return false;
    }

    [mtlDescriptor setFragmentFunction:function.get()];

    return true;
}

static Optional<MTLVertexFormat> validateAndConvertVertexFormatToMTLVertexFormat(GPUVertexFormatEnum format)
{
    switch (format) {
    case GPUVertexFormat::FloatR32G32B32A32:
        return MTLVertexFormatFloat4;
    case GPUVertexFormat::FloatR32G32B32:
        return MTLVertexFormatFloat3;
    case GPUVertexFormat::FloatR32G32:
        return MTLVertexFormatFloat2;
    case GPUVertexFormat::FloatR32:
        return MTLVertexFormatFloat;
    default:
        return WTF::nullopt;
    }
}

static Optional<MTLVertexStepFunction> validateAndConvertStepModeToMTLStepFunction(GPUInputStepModeEnum mode)
{
    switch (mode) {
    case GPUInputStepMode::Vertex:
        return MTLVertexStepFunctionPerVertex;
    case GPUInputStepMode::Instance:
        return MTLVertexStepFunctionPerInstance;
    default:
        return WTF::nullopt;
    }
}

static bool setInputStateForPipelineDescriptor(const char* const functionName, MTLRenderPipelineDescriptor *mtlDescriptor, const GPURenderPipelineDescriptor& descriptor)
{
#if LOG_DISABLED
    UNUSED_PARAM(functionName);
#endif
    auto mtlVertexDescriptor = adoptNS([MTLVertexDescriptor new]);

    const auto& attributes = descriptor.inputState.attributes;

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
        auto mtlFormat = validateAndConvertVertexFormatToMTLVertexFormat(attributes[i].format);
        if (!mtlFormat) {
            LOG(WebGPU, "%s: Invalid WebGPUVertexFormatEnum for vertex attribute %lu!", functionName, location);
            return false;
        }

        auto mtlAttributeDesc = retainPtr([attributeArray objectAtIndexedSubscript:location]);
        [mtlAttributeDesc setFormat:*mtlFormat];
        [mtlAttributeDesc setOffset:attributes[i].offset]; // FIXME: After adding more vertex formats, ensure offset < buffer's stride + format's data size.
        [mtlAttributeDesc setBufferIndex:WHLSL::Metal::calculateVertexBufferIndex(attributes[i].inputSlot)];
    }

    const auto& inputs = descriptor.inputState.inputs;

    auto layoutArray = retainPtr(mtlVertexDescriptor.get().layouts);

    for (size_t j = 0; j < inputs.size(); ++j) {
        auto slot = inputs[j].inputSlot;
        if (slot >= maxVertexBuffers) {
            LOG(WebGPU, "%s: Invalid inputSlot %d for vertex buffer!", functionName, slot);
            return false;
        }

        auto mtlStepFunction = validateAndConvertStepModeToMTLStepFunction(inputs[j].stepMode);
        if (!mtlStepFunction) {
            LOG(WebGPU, "%s: Invalid WebGPUInputStepMode for vertex buffer at slot %lu!", functionName, slot);
            return false;
        }

        auto convertedSlot = WHLSL::Metal::calculateVertexBufferIndex(slot);
        auto mtlLayoutDesc = retainPtr([layoutArray objectAtIndexedSubscript:convertedSlot]);
        [mtlLayoutDesc setStepFunction:*mtlStepFunction];
        [mtlLayoutDesc setStride:inputs[j].stride];
    }

    [mtlDescriptor setVertexDescriptor:mtlVertexDescriptor.get()];

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

    bool didSetFunctions = false, didSetInputState = false;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    didSetFunctions = setFunctionsForPipelineDescriptor(functionName, mtlDescriptor.get(), descriptor);
    didSetInputState = setInputStateForPipelineDescriptor(functionName, mtlDescriptor.get(), descriptor);

    END_BLOCK_OBJC_EXCEPTIONS;

    if (!didSetFunctions || !didSetInputState)
        return nullptr;

    // FIXME: Get the pixelFormat as configured for the context/CAMetalLayer.
    mtlDescriptor.get().colorAttachments[0].pixelFormat = MTLPixelFormatBGRA8Unorm;

    RetainPtr<MTLRenderPipelineState> pipeline;

    BEGIN_BLOCK_OBJC_EXCEPTIONS;

    NSError *error = [NSError errorWithDomain:@"com.apple.WebKit.GPU" code:1 userInfo:nil];
    pipeline = adoptNS([device.platformDevice() newRenderPipelineStateWithDescriptor:mtlDescriptor.get() error:&error]);
    if (!pipeline)
        LOG(WebGPU, "%s: %s!", functionName, error.localizedDescription.UTF8String);

    END_BLOCK_OBJC_EXCEPTIONS;

    return pipeline;
}

RefPtr<GPURenderPipeline> GPURenderPipeline::create(const GPUDevice& device, GPURenderPipelineDescriptor&& descriptor)
{
    const char* const functionName = "GPURenderPipeline::create()";

    if (!device.platformDevice()) {
        LOG(WebGPU, "%s: Invalid GPUDevice!", functionName);
        return nullptr;
    }

    // Depth Stencil state is optional and separate from the render pipeline state in Metal.
    RetainPtr<MTLDepthStencilState> depthStencil;

    if (descriptor.depthStencilState)
        depthStencil = tryCreateMtlDepthStencilState(functionName, *descriptor.depthStencilState, device);

    auto pipeline = tryCreateMtlRenderPipelineState(functionName, descriptor, device);
    if (!pipeline)
        return nullptr;

    return adoptRef(new GPURenderPipeline(WTFMove(depthStencil), WTFMove(pipeline), WTFMove(descriptor)));
}

GPURenderPipeline::GPURenderPipeline(RetainPtr<MTLDepthStencilState>&& depthStencil, RetainPtr<MTLRenderPipelineState>&& pipeline, GPURenderPipelineDescriptor&& descriptor)
    : m_depthStencilState(WTFMove(depthStencil))
    , m_platformRenderPipeline(WTFMove(pipeline))
    , m_layout(WTFMove(descriptor.layout))
    , m_primitiveTopology(descriptor.primitiveTopology)
{
}

} // namespace WebCore

#endif // ENABLE(WEBGPU)
