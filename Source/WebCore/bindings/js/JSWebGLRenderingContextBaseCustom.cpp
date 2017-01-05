/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#if ENABLE(WEBGL)

#include "EXTBlendMinMax.h"
#include "EXTFragDepth.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTsRGB.h"
#include "ExceptionCode.h"
#include "JSANGLEInstancedArrays.h"
#include "JSEXTBlendMinMax.h"
#include "JSEXTFragDepth.h"
#include "JSEXTShaderTextureLOD.h"
#include "JSEXTTextureFilterAnisotropic.h"
#include "JSEXTsRGB.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLImageElement.h"
#include "JSImageData.h"
#include "JSOESElementIndexUint.h"
#include "JSOESStandardDerivatives.h"
#include "JSOESTextureFloat.h"
#include "JSOESTextureFloatLinear.h"
#include "JSOESTextureHalfFloat.h"
#include "JSOESTextureHalfFloatLinear.h"
#include "JSOESVertexArrayObject.h"
#include "JSWebGLBuffer.h"
#include "JSWebGLCompressedTextureATC.h"
#include "JSWebGLCompressedTexturePVRTC.h"
#include "JSWebGLCompressedTextureS3TC.h"
#include "JSWebGLDebugRendererInfo.h"
#include "JSWebGLDebugShaders.h"
#include "JSWebGLDepthTexture.h"
#include "JSWebGLDrawBuffers.h"
#include "JSWebGLFramebuffer.h"
#include "JSWebGLLoseContext.h"
#include "JSWebGLProgram.h"
#include "JSWebGLRenderbuffer.h"
#include "JSWebGLRenderingContext.h"
#include "JSWebGLShader.h"
#include "JSWebGLTexture.h"
#include "JSWebGLUniformLocation.h"
#include "JSWebGLVertexArrayObject.h"
#include "JSWebGLVertexArrayObjectOES.h"
#include "JSWebKitCSSMatrix.h"
#include "OESElementIndexUint.h"
#include "OESStandardDerivatives.h"
#include "OESTextureFloat.h"
#include "OESTextureFloatLinear.h"
#include "OESTextureHalfFloat.h"
#include "OESTextureHalfFloatLinear.h"
#include "OESVertexArrayObject.h"
#include "WebGLBuffer.h"
#include "WebGLCompressedTextureATC.h"
#include "WebGLCompressedTexturePVRTC.h"
#include "WebGLCompressedTextureS3TC.h"
#include "WebGLDebugRendererInfo.h"
#include "WebGLDebugShaders.h"
#include "WebGLDepthTexture.h"
#include "WebGLDrawBuffers.h"
#include "WebGLExtension.h"
#include "WebGLFramebuffer.h"
#include "WebGLLoseContext.h"
#include "WebGLProgram.h"
#include "WebGLRenderingContextBase.h"
#include "WebGLVertexArrayObject.h"
#include "WebGLVertexArrayObjectOES.h"
#include <runtime/Error.h>
#include <runtime/JSObjectInlines.h>
#include <runtime/JSTypedArrays.h>
#include <runtime/TypedArrayInlines.h>
#include <runtime/TypedArrays.h>
#include <wtf/FastMalloc.h>

#if ENABLE(VIDEO)
#include "JSHTMLVideoElement.h"
#endif

#if ENABLE(WEBGL2)
#include "JSWebGL2RenderingContext.h"
#endif

using namespace JSC;

namespace WebCore {

JSValue toJSNewlyCreated(ExecState*, JSDOMGlobalObject* globalObject, Ref<WebGLRenderingContextBase>&& object)
{
#if ENABLE(WEBGL2)
    if (is<WebGL2RenderingContext>(object))
        return createWrapper<WebGL2RenderingContext>(globalObject, WTFMove(object));
#endif
    return createWrapper<WebGLRenderingContext>(globalObject, WTFMove(object));
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, WebGLRenderingContextBase& object)
{
    return wrap(state, globalObject, object);
}
    
static JSValue objectParameter(JSWebGLRenderingContextBase& context, ExecState& state, WebGLAny (WebGLRenderingContextBase::*getter)(GC3Denum target, GC3Denum pname))
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    auto target = state.uncheckedArgument(0).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, { });
    auto pname = state.uncheckedArgument(1).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, { });
    return toJS(state, *context.globalObject(), (context.wrapped().*getter)(target, pname));
}

static JSValue toJS(ExecState& state, JSDOMGlobalObject& globalObject, WebGLExtension* extension)
{
    if (!extension)
        return jsNull();
    switch (extension->getName()) {
    case WebGLExtension::WebGLLoseContextName:
        return toJS(&state, &globalObject, static_cast<WebGLLoseContext*>(extension));
    case WebGLExtension::EXTShaderTextureLODName:
        return toJS(&state, &globalObject, static_cast<EXTShaderTextureLOD*>(extension));
    case WebGLExtension::EXTTextureFilterAnisotropicName:
        return toJS(&state, &globalObject, static_cast<EXTTextureFilterAnisotropic*>(extension));
    case WebGLExtension::EXTsRGBName:
        return toJS(&state, &globalObject, static_cast<EXTsRGB*>(extension));
    case WebGLExtension::EXTFragDepthName:
        return toJS(&state, &globalObject, static_cast<EXTFragDepth*>(extension));
    case WebGLExtension::EXTBlendMinMaxName:
        return toJS(&state, &globalObject, static_cast<EXTBlendMinMax*>(extension));
    case WebGLExtension::OESStandardDerivativesName:
        return toJS(&state, &globalObject, static_cast<OESStandardDerivatives*>(extension));
    case WebGLExtension::OESTextureFloatName:
        return toJS(&state, &globalObject, static_cast<OESTextureFloat*>(extension));
    case WebGLExtension::OESTextureFloatLinearName:
        return toJS(&state, &globalObject, static_cast<OESTextureFloatLinear*>(extension));
    case WebGLExtension::OESTextureHalfFloatName:
        return toJS(&state, &globalObject, static_cast<OESTextureHalfFloat*>(extension));
    case WebGLExtension::OESTextureHalfFloatLinearName:
        return toJS(&state, &globalObject, static_cast<OESTextureHalfFloatLinear*>(extension));
    case WebGLExtension::OESVertexArrayObjectName:
        return toJS(&state, &globalObject, static_cast<OESVertexArrayObject*>(extension));
    case WebGLExtension::OESElementIndexUintName:
        return toJS(&state, &globalObject, static_cast<OESElementIndexUint*>(extension));
    case WebGLExtension::WebGLDebugRendererInfoName:
        return toJS(&state, &globalObject, static_cast<WebGLDebugRendererInfo*>(extension));
    case WebGLExtension::WebGLDebugShadersName:
        return toJS(&state, &globalObject, static_cast<WebGLDebugShaders*>(extension));
    case WebGLExtension::WebGLCompressedTextureATCName:
        return toJS(&state, &globalObject, static_cast<WebGLCompressedTextureATC*>(extension));
    case WebGLExtension::WebGLCompressedTexturePVRTCName:
        return toJS(&state, &globalObject, static_cast<WebGLCompressedTexturePVRTC*>(extension));
    case WebGLExtension::WebGLCompressedTextureS3TCName:
        return toJS(&state, &globalObject, static_cast<WebGLCompressedTextureS3TC*>(extension));
    case WebGLExtension::WebGLDepthTextureName:
        return toJS(&state, &globalObject, static_cast<WebGLDepthTexture*>(extension));
    case WebGLExtension::WebGLDrawBuffersName:
        return toJS(&state, &globalObject, static_cast<WebGLDrawBuffers*>(extension));
    case WebGLExtension::ANGLEInstancedArraysName:
        return toJS(&state, &globalObject, static_cast<ANGLEInstancedArrays*>(extension));
    }
    ASSERT_NOT_REACHED();
    return jsNull();
}

bool JSWebGLRenderingContextBaseOwner::isReachableFromOpaqueRoots(Handle<Unknown> handle, void*, SlotVisitor& visitor)
{
    JSWebGLRenderingContextBase* jsWebGLRenderingContext = jsCast<JSWebGLRenderingContextBase*>(handle.slot()->asCell());
    void* root = WebCore::root(jsWebGLRenderingContext->wrapped().canvas());
    return visitor.containsOpaqueRoot(root);
}

void JSWebGLRenderingContextBase::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(&wrapped());
    visitor.addOpaqueRoot(root(wrapped().canvas()));
}

JSValue JSWebGLRenderingContextBase::getAttachedShaders(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    auto& context = wrapped();
    auto* program = JSWebGLProgram::toWrapped(state.uncheckedArgument(0));
    if (!program && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    Vector<RefPtr<WebGLShader>> shaders;
    if (!context.getAttachedShaders(program, shaders))
        return jsNull();
    MarkedArgumentBuffer list;
    for (auto& shader : shaders)
        list.append(toJS(&state, globalObject(), shader.get()));
    return constructArray(&state, 0, globalObject(), list);
}

JSValue JSWebGLRenderingContextBase::getExtension(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    auto name = state.uncheckedArgument(0).toWTFString(&state);
    RETURN_IF_EXCEPTION(scope, { });
    return toJS(state, *globalObject(), wrapped().getExtension(name));
}

JSValue JSWebGLRenderingContextBase::getBufferParameter(ExecState& state)
{
    return objectParameter(*this, state, &WebGLRenderingContextBase::getBufferParameter);
}

JSValue JSWebGLRenderingContextBase::getFramebufferAttachmentParameter(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    auto target = state.uncheckedArgument(0).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, { });
    auto attachment = state.uncheckedArgument(1).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, { });
    auto pname = state.uncheckedArgument(2).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, { });
    return toJS(state, *globalObject(), wrapped().getFramebufferAttachmentParameter(target, attachment, pname));
}

JSValue JSWebGLRenderingContextBase::getParameter(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    auto pname = state.uncheckedArgument(0).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, { });
    return toJS(state, *globalObject(), wrapped().getParameter(pname));
}

JSValue JSWebGLRenderingContextBase::getProgramParameter(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    auto* program = JSWebGLProgram::toWrapped(state.uncheckedArgument(0));
    if (!program && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    auto pname = state.uncheckedArgument(1).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, { });
    return toJS(state, *globalObject(), wrapped().getProgramParameter(program, pname));
}

JSValue JSWebGLRenderingContextBase::getRenderbufferParameter(ExecState& state)
{
    return objectParameter(*this, state, &WebGLRenderingContextBase::getRenderbufferParameter);
}

JSValue JSWebGLRenderingContextBase::getShaderParameter(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    if (!state.uncheckedArgument(0).isUndefinedOrNull() && !state.uncheckedArgument(0).inherits(JSWebGLShader::info()))
        return throwTypeError(&state, scope);
    WebGLShader* shader = JSWebGLShader::toWrapped(state.uncheckedArgument(0));
    unsigned pname = state.uncheckedArgument(1).toInt32(&state);
    RETURN_IF_EXCEPTION(scope, { });
    return toJS(state, *globalObject(), wrapped().getShaderParameter(shader, pname));
}

JSValue JSWebGLRenderingContextBase::getSupportedExtensions(ExecState& state)
{
    WebGLRenderingContextBase& context = wrapped();
    if (context.isContextLost())
        return jsNull();
    MarkedArgumentBuffer list;
    for (auto& extension : context.getSupportedExtensions())
        list.append(jsStringWithCache(&state, extension));
    return constructArray(&state, 0, globalObject(), list);
}

JSValue JSWebGLRenderingContextBase::getTexParameter(ExecState& state)
{
    return objectParameter(*this, state, &WebGLRenderingContextBase::getTexParameter);
}

JSValue JSWebGLRenderingContextBase::getUniform(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    WebGLProgram* program = JSWebGLProgram::toWrapped(state.uncheckedArgument(0));
    if (!program && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    WebGLUniformLocation* location = JSWebGLUniformLocation::toWrapped(state.uncheckedArgument(1));
    if (!location && !state.uncheckedArgument(1).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    return toJS(state, *globalObject(), wrapped().getUniform(program, location));
}

JSValue JSWebGLRenderingContextBase::getVertexAttrib(ExecState& state)
{
    return objectParameter(*this, state, &WebGLRenderingContextBase::getVertexAttrib);
}

template<typename VectorType> bool toNumberVector(ExecState& state, JSValue value, VectorType& vector)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (!value.isObject())
        return false;

    auto& object = *asObject(value);
    int32_t length = object.get(&state, state.vm().propertyNames->length).toInt32(&state);

    if (!vector.tryReserveCapacity(length))
        return false;

    for (int32_t i = 0; i < length; ++i) {
        auto value = object.get(&state, i);
        RETURN_IF_EXCEPTION(scope, false);
        vector.uncheckedAppend(value.toNumber(&state));
        RETURN_IF_EXCEPTION(scope, false);
    }

    return true;
}

enum DataFunctionToCall {
    f_uniform1v, f_uniform2v, f_uniform3v, f_uniform4v,
    f_vertexAttrib1v, f_vertexAttrib2v, f_vertexAttrib3v, f_vertexAttrib4v
};

enum DataFunctionMatrixToCall {
    f_uniformMatrix2fv, f_uniformMatrix3fv, f_uniformMatrix4fv
};

static inline bool functionForUniform(DataFunctionToCall f)
{
    switch (f) {
    case f_uniform1v:
    case f_uniform2v:
    case f_uniform3v:
    case f_uniform4v:
        return true;
    default:
        return false;
    }
}

static JSValue dataFunctionf(DataFunctionToCall f, ExecState& state, WebGLRenderingContextBase& context)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    WebGLUniformLocation* location = nullptr;
    GLuint index = -1;

    if (functionForUniform(f)) {
        location = JSWebGLUniformLocation::toWrapped(state.uncheckedArgument(0));
        if (!location && !state.uncheckedArgument(0).isUndefinedOrNull())
            return throwTypeError(&state, scope);
    } else {
        index = state.uncheckedArgument(0).toInt32(&state);
        RETURN_IF_EXCEPTION(scope, { });
    }

    if (auto webGLArray = toUnsharedFloat32Array(state.uncheckedArgument(1))) {
        switch (f) {
        case f_uniform1v:
            context.uniform1fv(location, *webGLArray);
            break;
        case f_uniform2v:
            context.uniform2fv(location, *webGLArray);
            break;
        case f_uniform3v:
            context.uniform3fv(location, *webGLArray);
            break;
        case f_uniform4v:
            context.uniform4fv(location, *webGLArray);
            break;
        case f_vertexAttrib1v:
            context.vertexAttrib1fv(index, *webGLArray);
            break;
        case f_vertexAttrib2v:
            context.vertexAttrib2fv(index, *webGLArray);
            break;
        case f_vertexAttrib3v:
            context.vertexAttrib3fv(index, *webGLArray);
            break;
        case f_vertexAttrib4v:
            context.vertexAttrib4fv(index, *webGLArray);
            break;
        }
        return jsUndefined();
    }

    Vector<float, 64> array;
    if (!toNumberVector(state, state.uncheckedArgument(1), array))
        return throwTypeError(&state, scope);

    switch (f) {
    case f_uniform1v:
        context.uniform1fv(location, array.data(), array.size());
        break;
    case f_uniform2v:
        context.uniform2fv(location, array.data(), array.size());
        break;
    case f_uniform3v:
        context.uniform3fv(location, array.data(), array.size());
        break;
    case f_uniform4v:
        context.uniform4fv(location, array.data(), array.size());
        break;
    case f_vertexAttrib1v:
        context.vertexAttrib1fv(index, array.data(), array.size());
        break;
    case f_vertexAttrib2v:
        context.vertexAttrib2fv(index, array.data(), array.size());
        break;
    case f_vertexAttrib3v:
        context.vertexAttrib3fv(index, array.data(), array.size());
        break;
    case f_vertexAttrib4v:
        context.vertexAttrib4fv(index, array.data(), array.size());
        break;
    }
    return jsUndefined();
}

static JSValue dataFunctioni(DataFunctionToCall f, ExecState& state, WebGLRenderingContextBase& context)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto* location = JSWebGLUniformLocation::toWrapped(state.uncheckedArgument(0));
    if (!location && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);

    if (auto webGLArray = toUnsharedInt32Array(state.uncheckedArgument(1))) {
        switch (f) {
        case f_uniform1v:
            context.uniform1iv(location, *webGLArray);
            break;
        case f_uniform2v:
            context.uniform2iv(location, *webGLArray);
            break;
        case f_uniform3v:
            context.uniform3iv(location, *webGLArray);
            break;
        case f_uniform4v:
            context.uniform4iv(location, *webGLArray);
            break;
        default:
            break;
        }
        return jsUndefined();
    }

    Vector<int, 64> array;
    if (!toNumberVector(state, state.uncheckedArgument(1), array))
        return throwTypeError(&state, scope);

    switch (f) {
    case f_uniform1v:
        context.uniform1iv(location, array.data(), array.size());
        break;
    case f_uniform2v:
        context.uniform2iv(location, array.data(), array.size());
        break;
    case f_uniform3v:
        context.uniform3iv(location, array.data(), array.size());
        break;
    case f_uniform4v:
        context.uniform4iv(location, array.data(), array.size());
        break;
    default:
        break;
    }
    return jsUndefined();
}

static JSValue dataFunctionMatrix(DataFunctionMatrixToCall f, ExecState& state, WebGLRenderingContextBase& context)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));

    auto* location = JSWebGLUniformLocation::toWrapped(state.uncheckedArgument(0));
    if (!location && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);

    bool transpose = state.uncheckedArgument(1).toBoolean(&state);
    RETURN_IF_EXCEPTION(scope, { });

    if (auto webGLArray = toUnsharedFloat32Array(state.uncheckedArgument(2))) {
        switch (f) {
        case f_uniformMatrix2fv:
            context.uniformMatrix2fv(location, transpose, *webGLArray);
            break;
        case f_uniformMatrix3fv:
            context.uniformMatrix3fv(location, transpose, *webGLArray);
            break;
        case f_uniformMatrix4fv:
            context.uniformMatrix4fv(location, transpose, *webGLArray);
            break;
        }
        return jsUndefined();
    }

    Vector<float, 64> array;
    if (!toNumberVector(state, state.uncheckedArgument(2), array))
        return throwTypeError(&state, scope);

    switch (f) {
    case f_uniformMatrix2fv:
        context.uniformMatrix2fv(location, transpose, array.data(), array.size());
        break;
    case f_uniformMatrix3fv:
        context.uniformMatrix3fv(location, transpose, array.data(), array.size());
        break;
    case f_uniformMatrix4fv:
        context.uniformMatrix4fv(location, transpose, array.data(), array.size());
        break;
    }
    return jsUndefined();
}

JSValue JSWebGLRenderingContextBase::uniform1fv(ExecState& state)
{
    return dataFunctionf(f_uniform1v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniform1iv(ExecState& state)
{
    return dataFunctioni(f_uniform1v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniform2fv(ExecState& state)
{
    return dataFunctionf(f_uniform2v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniform2iv(ExecState& state)
{
    return dataFunctioni(f_uniform2v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniform3fv(ExecState& state)
{
    return dataFunctionf(f_uniform3v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniform3iv(ExecState& state)
{
    return dataFunctioni(f_uniform3v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniform4fv(ExecState& state)
{
    return dataFunctionf(f_uniform4v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniform4iv(ExecState& state)
{
    return dataFunctioni(f_uniform4v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniformMatrix2fv(ExecState& state)
{
    return dataFunctionMatrix(f_uniformMatrix2fv, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniformMatrix3fv(ExecState& state)
{
    return dataFunctionMatrix(f_uniformMatrix3fv, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::uniformMatrix4fv(ExecState& state)
{
    return dataFunctionMatrix(f_uniformMatrix4fv, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::vertexAttrib1fv(ExecState& state)
{
    return dataFunctionf(f_vertexAttrib1v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::vertexAttrib2fv(ExecState& state)
{
    return dataFunctionf(f_vertexAttrib2v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::vertexAttrib3fv(ExecState& state)
{
    return dataFunctionf(f_vertexAttrib3v, state, wrapped());
}

JSValue JSWebGLRenderingContextBase::vertexAttrib4fv(ExecState& state)
{
    return dataFunctionf(f_vertexAttrib4v, state, wrapped());
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
