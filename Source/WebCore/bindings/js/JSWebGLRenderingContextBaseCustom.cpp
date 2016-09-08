/*
 * Copyright (C) 2015-2016 Apple Inc. All rights reserved.
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
#include "JSWebGLRenderingContextBase.h"

#include "ANGLEInstancedArrays.h"
#include "EXTBlendMinMax.h"
#include "EXTFragDepth.h"
#include "EXTShaderTextureLOD.h"
#include "EXTTextureFilterAnisotropic.h"
#include "EXTsRGB.h"
#include "ExceptionCode.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
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
#include "NotImplemented.h"
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
#include "WebGLGetInfo.h"
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
#include "HTMLVideoElement.h"
#include "JSHTMLVideoElement.h"
#endif

#if ENABLE(WEBGL2)
#include "JSWebGL2RenderingContext.h"
#endif

using namespace JSC;

namespace WebCore {

JSC::JSValue toJSNewlyCreated(JSC::ExecState*, JSDOMGlobalObject* globalObject, Ref<WebGLRenderingContextBase>&& object)
{
#if ENABLE(WEBGL2)
    if (is<WebGL2RenderingContext>(object))
        return CREATE_DOM_WRAPPER(globalObject, WebGL2RenderingContext, WTFMove(object));
#endif
    return CREATE_DOM_WRAPPER(globalObject, WebGLRenderingContext, WTFMove(object));
}

JSValue toJS(ExecState* state, JSDOMGlobalObject* globalObject, WebGLRenderingContextBase& object)
{
    return wrap(state, globalObject, object);
}
    
static JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, const WebGLGetInfo& info)
{
    switch (info.getType()) {
    case WebGLGetInfo::kTypeBool:
        return jsBoolean(info.getBool());
    case WebGLGetInfo::kTypeBoolArray: {
        MarkedArgumentBuffer list;
        const Vector<bool>& value = info.getBoolArray();
        for (size_t ii = 0; ii < value.size(); ++ii)
            list.append(jsBoolean(value[ii]));
        return constructArray(exec, 0, globalObject, list);
    }
    case WebGLGetInfo::kTypeFloat:
        return jsNumber(info.getFloat());
    case WebGLGetInfo::kTypeInt:
        return jsNumber(info.getInt());
    case WebGLGetInfo::kTypeNull:
        return jsNull();
    case WebGLGetInfo::kTypeString:
        return jsStringWithCache(exec, info.getString());
    case WebGLGetInfo::kTypeUnsignedInt:
        return jsNumber(info.getUnsignedInt());
    case WebGLGetInfo::kTypeWebGLBuffer:
        return toJS(exec, globalObject, info.getWebGLBuffer());
    case WebGLGetInfo::kTypeWebGLFloatArray:
        return toJS(exec, globalObject, info.getWebGLFloatArray());
    case WebGLGetInfo::kTypeWebGLFramebuffer:
        return toJS(exec, globalObject, info.getWebGLFramebuffer());
    case WebGLGetInfo::kTypeWebGLIntArray:
        return toJS(exec, globalObject, info.getWebGLIntArray());
        // FIXME: implement WebGLObjectArray
        // case WebGLGetInfo::kTypeWebGLObjectArray:
    case WebGLGetInfo::kTypeWebGLProgram:
        return toJS(exec, globalObject, info.getWebGLProgram());
    case WebGLGetInfo::kTypeWebGLRenderbuffer:
        return toJS(exec, globalObject, info.getWebGLRenderbuffer());
    case WebGLGetInfo::kTypeWebGLTexture:
        return toJS(exec, globalObject, info.getWebGLTexture());
    case WebGLGetInfo::kTypeWebGLUnsignedByteArray:
        return toJS(exec, globalObject, info.getWebGLUnsignedByteArray());
    case WebGLGetInfo::kTypeWebGLUnsignedIntArray:
        return toJS(exec, globalObject, info.getWebGLUnsignedIntArray());
#if ENABLE(WEBGL2)
    case WebGLGetInfo::kTypeWebGLVertexArrayObject:
        return toJS(exec, globalObject, info.getWebGLVertexArrayObject());
#endif
    case WebGLGetInfo::kTypeWebGLVertexArrayObjectOES:
        return toJS(exec, globalObject, info.getWebGLVertexArrayObjectOES());
    default:
        notImplemented();
        return jsUndefined();
    }
}

enum ObjectType {
    kBuffer, kRenderbuffer, kTexture, kVertexAttrib
};

static JSValue getObjectParameter(JSWebGLRenderingContextBase* obj, ExecState& state, ObjectType objectType)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    ExceptionCode ec = 0;
    WebGLRenderingContextBase& context = obj->wrapped();
    unsigned target = state.uncheckedArgument(0).toInt32(&state);
    if (state.hadException())
        return jsUndefined();
    unsigned pname = state.uncheckedArgument(1).toInt32(&state);
    if (state.hadException())
        return jsUndefined();
    WebGLGetInfo info;
    switch (objectType) {
    case kBuffer:
        info = context.getBufferParameter(target, pname, ec);
        break;
    case kRenderbuffer:
        info = context.getRenderbufferParameter(target, pname, ec);
        break;
    case kTexture:
        info = context.getTexParameter(target, pname, ec);
        break;
    case kVertexAttrib:
        // target => index
        info = context.getVertexAttrib(target, pname, ec);
        break;
    default:
        notImplemented();
        break;
    }
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }
    return toJS(&state, obj->globalObject(), info);
}

enum WhichProgramCall {
    kProgramParameter, kUniform
};

static JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, WebGLExtension* extension)
{
    if (!extension)
        return jsNull();
    switch (extension->getName()) {
    case WebGLExtension::WebGLLoseContextName:
        return toJS(exec, globalObject, static_cast<WebGLLoseContext*>(extension));
    case WebGLExtension::EXTShaderTextureLODName:
        return toJS(exec, globalObject, static_cast<EXTShaderTextureLOD*>(extension));
    case WebGLExtension::EXTTextureFilterAnisotropicName:
        return toJS(exec, globalObject, static_cast<EXTTextureFilterAnisotropic*>(extension));
    case WebGLExtension::EXTsRGBName:
        return toJS(exec, globalObject, static_cast<EXTsRGB*>(extension));
    case WebGLExtension::EXTFragDepthName:
        return toJS(exec, globalObject, static_cast<EXTFragDepth*>(extension));
    case WebGLExtension::EXTBlendMinMaxName:
        return toJS(exec, globalObject, static_cast<EXTBlendMinMax*>(extension));
    case WebGLExtension::OESStandardDerivativesName:
        return toJS(exec, globalObject, static_cast<OESStandardDerivatives*>(extension));
    case WebGLExtension::OESTextureFloatName:
        return toJS(exec, globalObject, static_cast<OESTextureFloat*>(extension));
    case WebGLExtension::OESTextureFloatLinearName:
        return toJS(exec, globalObject, static_cast<OESTextureFloatLinear*>(extension));
    case WebGLExtension::OESTextureHalfFloatName:
        return toJS(exec, globalObject, static_cast<OESTextureHalfFloat*>(extension));
    case WebGLExtension::OESTextureHalfFloatLinearName:
        return toJS(exec, globalObject, static_cast<OESTextureHalfFloatLinear*>(extension));
    case WebGLExtension::OESVertexArrayObjectName:
        return toJS(exec, globalObject, static_cast<OESVertexArrayObject*>(extension));
    case WebGLExtension::OESElementIndexUintName:
        return toJS(exec, globalObject, static_cast<OESElementIndexUint*>(extension));
    case WebGLExtension::WebGLDebugRendererInfoName:
        return toJS(exec, globalObject, static_cast<WebGLDebugRendererInfo*>(extension));
    case WebGLExtension::WebGLDebugShadersName:
        return toJS(exec, globalObject, static_cast<WebGLDebugShaders*>(extension));
    case WebGLExtension::WebGLCompressedTextureATCName:
        return toJS(exec, globalObject, static_cast<WebGLCompressedTextureATC*>(extension));
    case WebGLExtension::WebGLCompressedTexturePVRTCName:
        return toJS(exec, globalObject, static_cast<WebGLCompressedTexturePVRTC*>(extension));
    case WebGLExtension::WebGLCompressedTextureS3TCName:
        return toJS(exec, globalObject, static_cast<WebGLCompressedTextureS3TC*>(extension));
    case WebGLExtension::WebGLDepthTextureName:
        return toJS(exec, globalObject, static_cast<WebGLDepthTexture*>(extension));
    case WebGLExtension::WebGLDrawBuffersName:
        return toJS(exec, globalObject, static_cast<WebGLDrawBuffers*>(extension));
    case WebGLExtension::ANGLEInstancedArraysName:
        return toJS(exec, globalObject, static_cast<ANGLEInstancedArrays*>(extension));
    }
    ASSERT_NOT_REACHED();
    return jsNull();
}

bool JSWebGLRenderingContextBaseOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
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
    ExceptionCode ec = 0;
    WebGLRenderingContextBase& context = wrapped();
    WebGLProgram* program = JSWebGLProgram::toWrapped(state.uncheckedArgument(0));
    if (!program && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    Vector<RefPtr<WebGLShader>> shaders;
    bool succeed = context.getAttachedShaders(program, shaders, ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsNull();
    }
    if (!succeed)
        return jsNull();
    JSC::MarkedArgumentBuffer list;
    for (size_t ii = 0; ii < shaders.size(); ++ii)
        list.append(toJS(&state, globalObject(), shaders[ii].get()));
    return constructArray(&state, 0, globalObject(), list);
}

JSValue JSWebGLRenderingContextBase::getExtension(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() < 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    WebGLRenderingContextBase& context = wrapped();
    const String name = state.uncheckedArgument(0).toString(&state)->value(&state);
    if (state.hadException())
        return jsUndefined();
    WebGLExtension* extension = context.getExtension(name);
    return toJS(&state, globalObject(), extension);
}

JSValue JSWebGLRenderingContextBase::getBufferParameter(ExecState& state)
{
    return getObjectParameter(this, state, kBuffer);
}

JSValue JSWebGLRenderingContextBase::getFramebufferAttachmentParameter(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    ExceptionCode ec = 0;
    WebGLRenderingContextBase& context = wrapped();
    unsigned target = state.uncheckedArgument(0).toInt32(&state);
    if (state.hadException())
        return jsUndefined();
    unsigned attachment = state.uncheckedArgument(1).toInt32(&state);
    if (state.hadException())
        return jsUndefined();
    unsigned pname = state.uncheckedArgument(2).toInt32(&state);
    if (state.hadException())
        return jsUndefined();
    WebGLGetInfo info = context.getFramebufferAttachmentParameter(target, attachment, pname, ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }
    return toJS(&state, globalObject(), info);
}

JSValue JSWebGLRenderingContextBase::getParameter(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 1)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    ExceptionCode ec = 0;
    WebGLRenderingContextBase& context = wrapped();
    unsigned pname = state.uncheckedArgument(0).toInt32(&state);
    if (state.hadException())
        return jsUndefined();
    WebGLGetInfo info = context.getParameter(pname, ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }
    return toJS(&state, globalObject(), info);
}

JSValue JSWebGLRenderingContextBase::getProgramParameter(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    ExceptionCode ec = 0;
    WebGLRenderingContextBase& context = wrapped();
    WebGLProgram* program = JSWebGLProgram::toWrapped(state.uncheckedArgument(0));
    if (!program && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    unsigned pname = state.uncheckedArgument(1).toInt32(&state);
    if (state.hadException())
        return jsUndefined();
    WebGLGetInfo info = context.getProgramParameter(program, pname, ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }
    return toJS(&state, globalObject(), info);
}

JSValue JSWebGLRenderingContextBase::getRenderbufferParameter(ExecState& state)
{
    return getObjectParameter(this, state, kRenderbuffer);
}

JSValue JSWebGLRenderingContextBase::getShaderParameter(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    ExceptionCode ec = 0;
    WebGLRenderingContextBase& context = wrapped();
    if (!state.uncheckedArgument(0).isUndefinedOrNull() && !state.uncheckedArgument(0).inherits(JSWebGLShader::info()))
        return throwTypeError(&state, scope);
    WebGLShader* shader = JSWebGLShader::toWrapped(state.uncheckedArgument(0));
    unsigned pname = state.uncheckedArgument(1).toInt32(&state);
    if (state.hadException())
        return jsUndefined();
    WebGLGetInfo info = context.getShaderParameter(shader, pname, ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }
    return toJS(&state, globalObject(), info);
}

JSValue JSWebGLRenderingContextBase::getSupportedExtensions(ExecState& state)
{
    WebGLRenderingContextBase& context = wrapped();
    if (context.isContextLost())
        return jsNull();
    Vector<String> value = context.getSupportedExtensions();
    MarkedArgumentBuffer list;
    for (size_t ii = 0; ii < value.size(); ++ii)
        list.append(jsStringWithCache(&state, value[ii]));
    return constructArray(&state, 0, globalObject(), list);
}

JSValue JSWebGLRenderingContextBase::getTexParameter(ExecState& state)
{
    return getObjectParameter(this, state, kTexture);
}

JSValue JSWebGLRenderingContextBase::getUniform(ExecState& state)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    ExceptionCode ec = 0;
    WebGLRenderingContextBase& context = wrapped();
    WebGLProgram* program = JSWebGLProgram::toWrapped(state.uncheckedArgument(0));
    if (!program && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    WebGLUniformLocation* location = JSWebGLUniformLocation::toWrapped(state.uncheckedArgument(1));
    if (!location && !state.uncheckedArgument(1).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    WebGLGetInfo info = context.getUniform(program, location, ec);
    if (ec) {
        setDOMException(&state, ec);
        return jsUndefined();
    }
    return toJS(&state, globalObject(), info);
}

JSValue JSWebGLRenderingContextBase::getVertexAttrib(ExecState& state)
{
    return getObjectParameter(this, state, kVertexAttrib);
}

template<typename T, size_t inlineCapacity>
bool toVector(JSC::ExecState& state, JSC::JSValue value, Vector<T, inlineCapacity>& vector)
{
    if (!value.isObject())
        return false;
    
    JSC::JSObject* object = asObject(value);
    int32_t length = object->get(&state, state.vm().propertyNames->length).toInt32(&state);
    
    if (!vector.tryReserveCapacity(length))
        return false;
    vector.resize(length);
    
    for (int32_t i = 0; i < length; ++i) {
        JSC::JSValue v = object->get(&state, i);
        if (state.hadException())
            return false;
        vector[i] = static_cast<T>(v.toNumber(&state));
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

static bool functionForUniform(DataFunctionToCall f)
{
    switch (f) {
    case f_uniform1v:
    case f_uniform2v:
    case f_uniform3v:
    case f_uniform4v:
        return true;
    default: break;
    }
    return false;
}

static JSC::JSValue dataFunctionf(DataFunctionToCall f, JSC::ExecState& state, WebGLRenderingContextBase& context)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    WebGLUniformLocation* location = 0;
    long index = -1;
    
    if (functionForUniform(f)) {
        location = JSWebGLUniformLocation::toWrapped(state.uncheckedArgument(0));
        if (!location && !state.uncheckedArgument(0).isUndefinedOrNull())
            return throwTypeError(&state, scope);
    } else
        index = state.uncheckedArgument(0).toInt32(&state);
    
    if (state.hadException())
        return jsUndefined();
    
    RefPtr<Float32Array> webGLArray = toFloat32Array(state.uncheckedArgument(1));
    if (state.hadException())
        return jsUndefined();
    
    ExceptionCode ec = 0;
    if (webGLArray) {
        switch (f) {
        case f_uniform1v:
            context.uniform1fv(location, *webGLArray, ec);
            break;
        case f_uniform2v:
            context.uniform2fv(location, *webGLArray, ec);
            break;
        case f_uniform3v:
            context.uniform3fv(location, *webGLArray, ec);
            break;
        case f_uniform4v:
            context.uniform4fv(location, *webGLArray, ec);
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
        
        setDOMException(&state, ec);
        return jsUndefined();
    }
    
    Vector<float, 64> array;
    if (!toVector(state, state.uncheckedArgument(1), array))
        return throwTypeError(&state, scope);
    
    switch (f) {
    case f_uniform1v:
        context.uniform1fv(location, array.data(), array.size(), ec);
        break;
    case f_uniform2v:
        context.uniform2fv(location, array.data(), array.size(), ec);
        break;
    case f_uniform3v:
        context.uniform3fv(location, array.data(), array.size(), ec);
        break;
    case f_uniform4v:
        context.uniform4fv(location, array.data(), array.size(), ec);
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
    
    setDOMException(&state, ec);
    return jsUndefined();
}

static JSC::JSValue dataFunctioni(DataFunctionToCall f, JSC::ExecState& state, WebGLRenderingContextBase& context)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 2)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    WebGLUniformLocation* location = JSWebGLUniformLocation::toWrapped(state.uncheckedArgument(0));
    if (!location && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    
    RefPtr<Int32Array> webGLArray = toInt32Array(state.uncheckedArgument(1));
    
    ExceptionCode ec = 0;
    if (webGLArray) {
        switch (f) {
        case f_uniform1v:
            context.uniform1iv(location, *webGLArray, ec);
            break;
        case f_uniform2v:
            context.uniform2iv(location, *webGLArray, ec);
            break;
        case f_uniform3v:
            context.uniform3iv(location, *webGLArray, ec);
            break;
        case f_uniform4v:
            context.uniform4iv(location, *webGLArray, ec);
            break;
        default:
            break;
        }
        
        setDOMException(&state, ec);
        return jsUndefined();
    }
    
    
    Vector<int, 64> array;
    if (!toVector(state, state.uncheckedArgument(1), array))
        return throwTypeError(&state, scope);
    
    switch (f) {
    case f_uniform1v:
        context.uniform1iv(location, array.data(), array.size(), ec);
        break;
    case f_uniform2v:
        context.uniform2iv(location, array.data(), array.size(), ec);
        break;
    case f_uniform3v:
        context.uniform3iv(location, array.data(), array.size(), ec);
        break;
    case f_uniform4v:
        context.uniform4iv(location, array.data(), array.size(), ec);
        break;
    default:
        break;
    }
    
    setDOMException(&state, ec);
    return jsUndefined();
}

static JSC::JSValue dataFunctionMatrix(DataFunctionMatrixToCall f, JSC::ExecState& state, WebGLRenderingContextBase& context)
{
    VM& vm = state.vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    if (state.argumentCount() != 3)
        return throwException(&state, scope, createNotEnoughArgumentsError(&state));
    
    WebGLUniformLocation* location = JSWebGLUniformLocation::toWrapped(state.uncheckedArgument(0));
    if (!location && !state.uncheckedArgument(0).isUndefinedOrNull())
        return throwTypeError(&state, scope);
    
    bool transpose = state.uncheckedArgument(1).toBoolean(&state);
    if (state.hadException())
        return jsUndefined();
    
    RefPtr<Float32Array> webGLArray = toFloat32Array(state.uncheckedArgument(2));
    
    ExceptionCode ec = 0;
    if (webGLArray) {
        switch (f) {
        case f_uniformMatrix2fv:
            context.uniformMatrix2fv(location, transpose, *webGLArray, ec);
            break;
        case f_uniformMatrix3fv:
            context.uniformMatrix3fv(location, transpose, *webGLArray, ec);
            break;
        case f_uniformMatrix4fv:
            context.uniformMatrix4fv(location, transpose, *webGLArray, ec);
            break;
        }
        
        setDOMException(&state, ec);
        return jsUndefined();
    }
    
    Vector<float, 64> array;
    if (!toVector(state, state.uncheckedArgument(2), array))
        return throwTypeError(&state, scope);
    
    switch (f) {
    case f_uniformMatrix2fv:
        context.uniformMatrix2fv(location, transpose, array.data(), array.size(), ec);
        break;
    case f_uniformMatrix3fv:
        context.uniformMatrix3fv(location, transpose, array.data(), array.size(), ec);
        break;
    case f_uniformMatrix4fv:
        context.uniformMatrix4fv(location, transpose, array.data(), array.size(), ec);
        break;
    }
    
    setDOMException(&state, ec);
    return jsUndefined();
}

JSC::JSValue JSWebGLRenderingContextBase::uniform1fv(JSC::ExecState& state)
{
    return dataFunctionf(f_uniform1v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniform1iv(JSC::ExecState& state)
{
    return dataFunctioni(f_uniform1v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniform2fv(JSC::ExecState& state)
{
    return dataFunctionf(f_uniform2v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniform2iv(JSC::ExecState& state)
{
    return dataFunctioni(f_uniform2v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniform3fv(JSC::ExecState& state)
{
    return dataFunctionf(f_uniform3v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniform3iv(JSC::ExecState& state)
{
    return dataFunctioni(f_uniform3v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniform4fv(JSC::ExecState& state)
{
    return dataFunctionf(f_uniform4v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniform4iv(JSC::ExecState& state)
{
    return dataFunctioni(f_uniform4v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniformMatrix2fv(JSC::ExecState& state)
{
    return dataFunctionMatrix(f_uniformMatrix2fv, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniformMatrix3fv(JSC::ExecState& state)
{
    return dataFunctionMatrix(f_uniformMatrix3fv, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::uniformMatrix4fv(JSC::ExecState& state)
{
    return dataFunctionMatrix(f_uniformMatrix4fv, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::vertexAttrib1fv(JSC::ExecState& state)
{
    return dataFunctionf(f_vertexAttrib1v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::vertexAttrib2fv(JSC::ExecState& state)
{
    return dataFunctionf(f_vertexAttrib2v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::vertexAttrib3fv(JSC::ExecState& state)
{
    return dataFunctionf(f_vertexAttrib3v, state, wrapped());
}

JSC::JSValue JSWebGLRenderingContextBase::vertexAttrib4fv(JSC::ExecState& state)
{
    return dataFunctionf(f_vertexAttrib4v, state, wrapped());
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
