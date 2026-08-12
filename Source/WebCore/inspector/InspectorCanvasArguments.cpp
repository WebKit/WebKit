/*
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "InspectorCanvasArguments.h"

#include "GPUBindGroupDescriptor.h"
#include "GPUComputePassDescriptor.h"
#include "GPUComputePipelineDescriptor.h"
#include "GPUCopyElementImageDestination.h"
#include "GPUCopyElementImageSource.h"
#include "GPUExternalTextureDescriptor.h"
#include "GPUImageCopyBuffer.h"
#include "GPUImageCopyExternalImage.h"
#include "GPUImageCopyTexture.h"
#include "GPUImageCopyTextureTagged.h"
#include "GPUPipelineLayoutDescriptor.h"
#include "GPURenderPassDescriptor.h"
#include "GPURenderPipelineDescriptor.h"
#include "GPUShaderModuleDescriptor.h"
#include "Path2D.h"
#include "WebGLBuffer.h"
#include "WebGLFramebuffer.h"
#include "WebGLProgram.h"
#include "WebGLQuery.h"
#include "WebGLRenderbuffer.h"
#include "WebGLRenderingContext.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLSampler.h"
#include "WebGLShader.h"
#include "WebGLSync.h"
#include "WebGLTexture.h"
#include "WebGLTimerQueryEXT.h"
#include "WebGLTransformFeedback.h"
#include "WebGLUniformLocation.h"
#include "WebGLVertexArrayObject.h"
#include "WebGLVertexArrayObjectOES.h"

namespace WebCore {

// MARK: - Helpers

static Ref<JSON::ArrayOf<JSON::Value>> process(const InspectorCanvasProcessedArgument& processed)
{
    auto reference = JSON::ArrayOf<JSON::Value>::create();
    reference->addItem(processed.value.copyRef());
    reference->addItem(static_cast<int>(processed.swizzleType));
    return reference;
}

static Ref<JSON::ArrayOf<JSON::Value>> process(InspectorCanvas& inspectorCanvas, const auto& object)
{
    using ObjectType = std::remove_cvref_t<decltype(object.get())>;
    auto processed = InspectorCanvasArgumentProcessor<IDLInterface<ObjectType>>{}(inspectorCanvas, object);
    RELEASE_ASSERT(processed);
    return process(*processed);
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUFragmentState& state)
{
    Ref object = state.toJSON();
    object->setArray("module"_s, process(inspectorCanvas, state.module));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUVertexState& state)
{
    Ref object = state.toJSON();
    object->setArray("module"_s, process(inspectorCanvas, state.module));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUBindGroupDescriptor& descriptor)
{
    Ref object = descriptor.toJSON();
    object->setArray("layout"_s, process(inspectorCanvas, descriptor.layout));

    auto entries = JSON::Array::create();
    for (auto& entry : descriptor.entries) {
        auto entryObject = JSON::Object::create();
        entryObject->setDouble("binding"_s, entry.binding);
        WTF::switchOn(entry.resource, [&](const auto& resource) {
            using Resource = std::remove_cvref_t<decltype(resource)>;
            if constexpr (std::is_same_v<Resource, GPUBufferBinding>) {
                auto bufferBinding = JSON::Object::create();
                bufferBinding->setArray("buffer"_s, process(inspectorCanvas, resource.buffer));
                bufferBinding->setDouble("offset"_s, resource.offset);
                if (resource.size)
                    bufferBinding->setDouble("size"_s, *resource.size);
                entryObject->setValue("resource"_s, WTF::move(bufferBinding));
            } else
                entryObject->setArray("resource"_s, process(inspectorCanvas, resource));
        });
        entries->pushValue(WTF::move(entryObject));
    }
    object->setValue("entries"_s, WTF::move(entries));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUComputePassTimestampWrites& timestampWrites)
{
    Ref object = timestampWrites.toJSON();
    object->setArray("querySet"_s, process(inspectorCanvas, timestampWrites.querySet));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUComputePassDescriptor& descriptor)
{
    Ref object = descriptor.toJSON();
    if (descriptor.timestampWrites)
        object->setObject("timestampWrites"_s, process(inspectorCanvas, *descriptor.timestampWrites));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUImageCopyTexture& imageCopyTexture)
{
    Ref object = imageCopyTexture.toJSON();
    object->setArray("texture"_s, process(inspectorCanvas, imageCopyTexture.texture));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUImageCopyTextureTagged& imageCopyTexture)
{
    Ref object = imageCopyTexture.toJSON();
    object->setArray("texture"_s, process(inspectorCanvas, imageCopyTexture.texture));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUCopyElementImageDestination& descriptor)
{
    Ref object = descriptor.toJSON();
    object->setObject("destination"_s, process(inspectorCanvas, descriptor.destination));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUCopyElementImageSource& descriptor)
{
    Ref object = descriptor.toJSON();
    WTF::switchOn(descriptor.source, [&](const auto& source) {
        object->setArray("source"_s, process(inspectorCanvas, source));
    });
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUExternalTextureDescriptor& descriptor)
{
    Ref object = descriptor.toJSON();
#if ENABLE(VIDEO)
#if ENABLE(WEB_CODECS)
    WTF::switchOn(descriptor.source, [&](const auto& source) {
        object->setArray("source"_s, process(inspectorCanvas, source));
    });
#else // ENABLE(WEB_CODECS)
    object->setArray("source"_s, process(inspectorCanvas, descriptor.source));
#endif // !ENABLE(WEB_CODECS)
#endif // ENABLE(VIDEO)
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUImageCopyBuffer& imageCopyBuffer)
{
    Ref object = imageCopyBuffer.toJSON();
    object->setArray("buffer"_s, process(inspectorCanvas, imageCopyBuffer.buffer));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUImageCopyExternalImage& imageCopyExternalImage)
{
    Ref object = imageCopyExternalImage.toJSON();
    WTF::switchOn(imageCopyExternalImage.source, [&](const auto& source) {
        object->setArray("source"_s, process(inspectorCanvas, source));
    });
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUPipelineLayoutDescriptor& descriptor)
{
    Ref object = descriptor.toJSON();
    auto bindGroupLayouts = JSON::Array::create();
    for (auto& bindGroupLayout : descriptor.bindGroupLayouts) {
        if (bindGroupLayout)
            bindGroupLayouts->pushArray(process(inspectorCanvas, Ref { *bindGroupLayout }));
        else
            bindGroupLayouts->pushValue(JSON::Value::null());
    }
    object->setArray("bindGroupLayouts"_s, WTF::move(bindGroupLayouts));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPURenderPassColorAttachment& colorAttachment)
{
    Ref object = colorAttachment.toJSON();
    WTF::switchOn(colorAttachment.view, [&](const auto& view) {
        object->setArray("view"_s, process(inspectorCanvas, view));
    });
    if (colorAttachment.resolveTarget) {
        WTF::switchOn(*colorAttachment.resolveTarget, [&](const auto& resolveTarget) {
            object->setArray("resolveTarget"_s, process(inspectorCanvas, resolveTarget));
        });
    }
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPURenderPassDepthStencilAttachment& depthStencilAttachment)
{
    Ref object = depthStencilAttachment.toJSON();
    WTF::switchOn(depthStencilAttachment.view, [&](const auto& view) {
        object->setArray("view"_s, process(inspectorCanvas, view));
    });
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPURenderPassTimestampWrites& timestampWrites)
{
    Ref object = timestampWrites.toJSON();
    object->setArray("querySet"_s, process(inspectorCanvas, timestampWrites.querySet));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPURenderPassDescriptor& descriptor)
{
    Ref object = descriptor.toJSON();

    auto colorAttachments = JSON::Array::create();
    for (auto& colorAttachment : descriptor.colorAttachments) {
        if (colorAttachment)
            colorAttachments->pushObject(process(inspectorCanvas, *colorAttachment));
        else
            colorAttachments->pushValue(JSON::Value::null());
    }
    object->setArray("colorAttachments"_s, WTF::move(colorAttachments));

    if (descriptor.depthStencilAttachment)
        object->setObject("depthStencilAttachment"_s, process(inspectorCanvas, *descriptor.depthStencilAttachment));
    if (descriptor.occlusionQuerySet)
        object->setArray("occlusionQuerySet"_s, process(inspectorCanvas, Ref { *descriptor.occlusionQuerySet }));
    if (descriptor.timestampWrites)
        object->setObject("timestampWrites"_s, process(inspectorCanvas, *descriptor.timestampWrites));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUShaderModuleCompilationHint& hint)
{
    Ref object = hint.toJSON();
    if (auto* layout = std::get_if<Ref<GPUPipelineLayout>>(&hint.layout))
        object->setArray("layout"_s, process(inspectorCanvas, *layout));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUShaderModuleDescriptor& descriptor)
{
    Ref object = descriptor.toJSON();
    auto hints = JSON::Object::create();
    for (auto& hint : descriptor.hints)
        hints->setObject(hint.key, process(inspectorCanvas, hint.value));
    object->setObject("hints"_s, WTF::move(hints));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPUComputePipelineDescriptor& descriptor)
{
    Ref object = descriptor.toJSON();
    if (auto* layout = std::get_if<Ref<GPUPipelineLayout>>(&descriptor.layout))
        object->setArray("layout"_s, process(inspectorCanvas, *layout));

    Ref compute = descriptor.compute.toJSON();
    compute->setArray("module"_s, process(inspectorCanvas, descriptor.compute.module));
    object->setObject("compute"_s, WTF::move(compute));
    return object;
}

static Ref<JSON::Object> process(InspectorCanvas& inspectorCanvas, const GPURenderPipelineDescriptor& descriptor)
{
    Ref object = descriptor.toJSON();
    if (auto* layout = std::get_if<Ref<GPUPipelineLayout>>(&descriptor.layout))
        object->setArray("layout"_s, process(inspectorCanvas, *layout));
    object->setObject("vertex"_s, process(inspectorCanvas, descriptor.vertex));
    if (descriptor.fragment)
        object->setObject("fragment"_s, process(inspectorCanvas, *descriptor.fragment));
    return object;
}

static std::optional<InspectorCanvasProcessedArgument> processJSON(Ref<JSON::Value>&& valueIndex, RecordingSwizzleType swizzleType = RecordingSwizzleType::JSON)
{
    return { { WTF::move(valueIndex), swizzleType } };
}

// MARK: - Dictionaries

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUComputePassDescriptor>>::operator()(InspectorCanvas& context, const GPUComputePassDescriptor& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUComputePassDescriptor);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUComputePipelineDescriptor>>::operator()(InspectorCanvas& context, const GPUComputePipelineDescriptor& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUComputePipelineDescriptor);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUCopyElementImageDestination>>::operator()(InspectorCanvas& context, const GPUCopyElementImageDestination& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUCopyElementImageDestination);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUCopyElementImageSource>>::operator()(InspectorCanvas& context, const GPUCopyElementImageSource& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUCopyElementImageSource);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUExternalTextureDescriptor>>::operator()(InspectorCanvas& context, const GPUExternalTextureDescriptor& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUExternalTextureDescriptor);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUImageCopyBuffer>>::operator()(InspectorCanvas& context, const GPUImageCopyBuffer& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUImageCopyBuffer);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUImageCopyExternalImage>>::operator()(InspectorCanvas& context, const GPUImageCopyExternalImage& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUImageCopyExternalImage);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUImageCopyTexture>>::operator()(InspectorCanvas& context, const GPUImageCopyTexture& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUImageCopyTexture);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUImageCopyTextureTagged>>::operator()(InspectorCanvas& context, const GPUImageCopyTextureTagged& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUImageCopyTextureTagged);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUPipelineLayoutDescriptor>>::operator()(InspectorCanvas& context, const GPUPipelineLayoutDescriptor& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUPipelineLayoutDescriptor);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPURenderPassDescriptor>>::operator()(InspectorCanvas& context, const GPURenderPassDescriptor& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPURenderPassDescriptor);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUBindGroupDescriptor>>::operator()(InspectorCanvas& context, const GPUBindGroupDescriptor& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUBindGroupDescriptor);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPURenderPipelineDescriptor>>::operator()(InspectorCanvas& context, const GPURenderPipelineDescriptor& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPURenderPipelineDescriptor);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<GPUShaderModuleDescriptor>>::operator()(InspectorCanvas& context, const GPUShaderModuleDescriptor& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return processJSON(context.valueIndexForData(process(context, argument)->toJSONString()), RecordingSwizzleType::GPUShaderModuleDescriptor);
}

auto InspectorCanvasArgumentProcessor<IDLDictionary<DOMMatrix2DInit>>::operator()(InspectorCanvas&, const DOMMatrix2DInit& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    auto array = JSON::ArrayOf<double>::create();
    array->addItem(argument.a.value_or(1));
    array->addItem(argument.b.value_or(0));
    array->addItem(argument.c.value_or(0));
    array->addItem(argument.d.value_or(1));
    array->addItem(argument.e.value_or(0));
    array->addItem(argument.f.value_or(0));
    return {{ WTF::move(array), RecordingSwizzleType::DOMMatrix }};
}

// MARK: - Strings

auto InspectorCanvasArgumentProcessor<IDLDOMString>::operator()(InspectorCanvas& context, const String& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::String }};
}

// MARK: - Numerics

auto InspectorCanvasArgumentProcessor<IDLBoolean>::operator()(InspectorCanvas&, bool argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(argument), RecordingSwizzleType::Boolean }};
}

// MARK: - Typed Arrays

auto InspectorCanvasArgumentProcessor<IDLArrayBuffer>::operator()(InspectorCanvas&, const Ref<JSC::ArrayBuffer>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

auto InspectorCanvasArgumentProcessor<IDLArrayBufferView>::operator()(InspectorCanvas&, const Ref<JSC::ArrayBufferView>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

auto InspectorCanvasArgumentProcessor<IDLFloat32Array>::operator()(InspectorCanvas&, const Ref<JSC::Float32Array>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

auto InspectorCanvasArgumentProcessor<IDLInt32Array>::operator()(InspectorCanvas&, const Ref<JSC::Int32Array>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

auto InspectorCanvasArgumentProcessor<IDLUint32Array>::operator()(InspectorCanvas&, const Ref<JSC::Uint32Array>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::TypedArray }};
}

// MARK: - Interfaces

auto InspectorCanvasArgumentProcessor<IDLInterface<Element>>::operator()(InspectorCanvas& context, const Ref<Element>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    // Elements are not serializable, so add a string as a placeholder since the actual
    // element cannot be reconstructed in the frontend.
    return {{ context.valueIndexForData("Element"_s), RecordingSwizzleType::None }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<HTMLImageElement>>::operator()(InspectorCanvas& context, const Ref<HTMLImageElement>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<SVGImageElement>>::operator()(InspectorCanvas& context, const Ref<SVGImageElement>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    // FIXME: To maintain existing behavior for initial specialization adoption, we pretend SVGImageElement goes down the Element path.
    return {{ context.valueIndexForData("Element"_s), RecordingSwizzleType::None }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<HTMLCanvasElement>>::operator()(InspectorCanvas& context, const Ref<HTMLCanvasElement>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<CSSStyleImageValue>>::operator()(InspectorCanvas& context, const Ref<CSSStyleImageValue>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<CanvasGradient>>::operator()(InspectorCanvas& context, const Ref<CanvasGradient>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(const_cast<Ref<CanvasGradient>&>(argument)), RecordingSwizzleType::CanvasGradient }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<CanvasPattern>>::operator()(InspectorCanvas& context, const Ref<CanvasPattern>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::CanvasPattern }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<Path2D>>::operator()(InspectorCanvas& context, const Ref<Path2D>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(buildStringFromPath(argument->path())), RecordingSwizzleType::Path2D }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<ImageBitmap>>::operator()(InspectorCanvas& context, const Ref<ImageBitmap>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::ImageBitmap }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<ImageData>>::operator()(InspectorCanvas& context, const Ref<ImageData>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::ImageData }};
}

auto InspectorCanvasArgumentProcessor<IDLInterface<CanvasElementImage>>::operator()(InspectorCanvas& context, const Ref<CanvasElementImage>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData("CanvasElementImage"_s), RecordingSwizzleType::None }};
}

#if ENABLE(OFFSCREEN_CANVAS)

auto InspectorCanvasArgumentProcessor<IDLInterface<OffscreenCanvas>>::operator()(InspectorCanvas& context, const Ref<OffscreenCanvas>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

#endif // ENABLE(OFFSCREEN_CANVAS)

#if ENABLE(VIDEO)

auto InspectorCanvasArgumentProcessor<IDLInterface<HTMLVideoElement>>::operator()(InspectorCanvas& context, const Ref<HTMLVideoElement>& argument) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ context.valueIndexForData(argument), RecordingSwizzleType::Image }};
}

#endif // ENABLE(VIDEO)

#if ENABLE(WEB_CODECS)

auto InspectorCanvasArgumentProcessor<IDLInterface<WebCodecsVideoFrame>>::operator()(InspectorCanvas&, const Ref<WebCodecsVideoFrame>&) -> std::optional<InspectorCanvasProcessedArgument>
{
    return {{ JSON::Value::create(0), RecordingSwizzleType::Image }};
}

#endif // ENABLE(WEB_CODECS)

#if ENABLE(WEBGL)

RecordingSwizzleType recordingSwizzleTypeForWebGLExtension(WebGLExtensionName name)
{
    switch (name) {
    case WebGLExtensionName::ANGLEInstancedArrays:
        return RecordingSwizzleType::ANGLEInstancedArrays;
    case WebGLExtensionName::EXTBlendMinMax:
        return RecordingSwizzleType::EXTBlendMinMax;
    case WebGLExtensionName::EXTClipControl:
        return RecordingSwizzleType::EXTClipControl;
    case WebGLExtensionName::EXTColorBufferFloat:
        return RecordingSwizzleType::EXTColorBufferFloat;
    case WebGLExtensionName::EXTColorBufferHalfFloat:
        return RecordingSwizzleType::EXTColorBufferHalfFloat;
    case WebGLExtensionName::EXTConservativeDepth:
        return RecordingSwizzleType::EXTConservativeDepth;
    case WebGLExtensionName::EXTDepthClamp:
        return RecordingSwizzleType::EXTDepthClamp;
    case WebGLExtensionName::EXTDisjointTimerQuery:
        return RecordingSwizzleType::EXTDisjointTimerQuery;
    case WebGLExtensionName::EXTDisjointTimerQueryWebGL2:
        return RecordingSwizzleType::EXTDisjointTimerQueryWebGL2;
    case WebGLExtensionName::EXTFloatBlend:
        return RecordingSwizzleType::EXTFloatBlend;
    case WebGLExtensionName::EXTFragDepth:
        return RecordingSwizzleType::EXTFragDepth;
    case WebGLExtensionName::EXTPolygonOffsetClamp:
        return RecordingSwizzleType::EXTPolygonOffsetClamp;
    case WebGLExtensionName::EXTRenderSnorm:
        return RecordingSwizzleType::EXTRenderSnorm;
    case WebGLExtensionName::EXTShaderTextureLOD:
        return RecordingSwizzleType::EXTShaderTextureLOD;
    case WebGLExtensionName::EXTTextureCompressionBPTC:
        return RecordingSwizzleType::EXTTextureCompressionBPTC;
    case WebGLExtensionName::EXTTextureCompressionRGTC:
        return RecordingSwizzleType::EXTTextureCompressionRGTC;
    case WebGLExtensionName::EXTTextureFilterAnisotropic:
        return RecordingSwizzleType::EXTTextureFilterAnisotropic;
    case WebGLExtensionName::EXTTextureMirrorClampToEdge:
        return RecordingSwizzleType::EXTTextureMirrorClampToEdge;
    case WebGLExtensionName::EXTTextureNorm16:
        return RecordingSwizzleType::EXTTextureNorm16;
    case WebGLExtensionName::EXTsRGB:
        return RecordingSwizzleType::EXTsRGB;
    case WebGLExtensionName::KHRParallelShaderCompile:
        return RecordingSwizzleType::KHRParallelShaderCompile;
    case WebGLExtensionName::NVShaderNoperspectiveInterpolation:
        return RecordingSwizzleType::NVShaderNoperspectiveInterpolation;
    case WebGLExtensionName::OESDrawBuffersIndexed:
        return RecordingSwizzleType::OESDrawBuffersIndexed;
    case WebGLExtensionName::OESElementIndexUint:
        return RecordingSwizzleType::OESElementIndexUint;
    case WebGLExtensionName::OESFBORenderMipmap:
        return RecordingSwizzleType::OESFBORenderMipmap;
    case WebGLExtensionName::OESSampleVariables:
        return RecordingSwizzleType::OESSampleVariables;
    case WebGLExtensionName::OESShaderMultisampleInterpolation:
        return RecordingSwizzleType::OESShaderMultisampleInterpolation;
    case WebGLExtensionName::OESStandardDerivatives:
        return RecordingSwizzleType::OESStandardDerivatives;
    case WebGLExtensionName::OESTextureFloat:
        return RecordingSwizzleType::OESTextureFloat;
    case WebGLExtensionName::OESTextureFloatLinear:
        return RecordingSwizzleType::OESTextureFloatLinear;
    case WebGLExtensionName::OESTextureHalfFloat:
        return RecordingSwizzleType::OESTextureHalfFloat;
    case WebGLExtensionName::OESTextureHalfFloatLinear:
        return RecordingSwizzleType::OESTextureHalfFloatLinear;
    case WebGLExtensionName::OESVertexArrayObject:
        return RecordingSwizzleType::OESVertexArrayObject;
    case WebGLExtensionName::WebGLBlendFuncExtended:
        return RecordingSwizzleType::WebGLBlendFuncExtended;
    case WebGLExtensionName::WebGLClipCullDistance:
        return RecordingSwizzleType::WebGLClipCullDistance;
    case WebGLExtensionName::WebGLColorBufferFloat:
        return RecordingSwizzleType::WebGLColorBufferFloat;
    case WebGLExtensionName::WebGLCompressedTextureASTC:
        return RecordingSwizzleType::WebGLCompressedTextureASTC;
    case WebGLExtensionName::WebGLCompressedTextureETC:
        return RecordingSwizzleType::WebGLCompressedTextureETC;
    case WebGLExtensionName::WebGLCompressedTextureETC1:
        return RecordingSwizzleType::WebGLCompressedTextureETC1;
    case WebGLExtensionName::WebGLCompressedTexturePVRTC:
        return RecordingSwizzleType::WebGLCompressedTexturePVRTC;
    case WebGLExtensionName::WebGLCompressedTextureS3TC:
        return RecordingSwizzleType::WebGLCompressedTextureS3TC;
    case WebGLExtensionName::WebGLCompressedTextureS3TCsRGB:
        return RecordingSwizzleType::WebGLCompressedTextureS3TCsRGB;
    case WebGLExtensionName::WebGLDebugRendererInfo:
        return RecordingSwizzleType::WebGLDebugRendererInfo;
    case WebGLExtensionName::WebGLDebugShaders:
        return RecordingSwizzleType::WebGLDebugShaders;
    case WebGLExtensionName::WebGLDepthTexture:
        return RecordingSwizzleType::WebGLDepthTexture;
    case WebGLExtensionName::WebGLDrawBuffers:
        return RecordingSwizzleType::WebGLDrawBuffers;
    case WebGLExtensionName::WebGLDrawInstancedBaseVertexBaseInstance:
        return RecordingSwizzleType::WebGLDrawInstancedBaseVertexBaseInstance;
    case WebGLExtensionName::WebGLLoseContext:
        return RecordingSwizzleType::WebGLLoseContext;
    case WebGLExtensionName::WebGLMultiDraw:
        return RecordingSwizzleType::WebGLMultiDraw;
    case WebGLExtensionName::WebGLMultiDrawInstancedBaseVertexBaseInstance:
        return RecordingSwizzleType::WebGLMultiDrawInstancedBaseVertexBaseInstance;
    case WebGLExtensionName::WebGLPolygonMode:
        return RecordingSwizzleType::WebGLPolygonMode;
    case WebGLExtensionName::WebGLProvokingVertex:
        return RecordingSwizzleType::WebGLProvokingVertex;
    case WebGLExtensionName::WebGLRenderSharedExponent:
        return RecordingSwizzleType::WebGLRenderSharedExponent;
    case WebGLExtensionName::WebGLStencilTexturing:
        return RecordingSwizzleType::WebGLStencilTexturing;
    }

    WTF_UNREACHABLE();
}

#endif // ENABLE(WEBGL)

} // namespace WebCore
