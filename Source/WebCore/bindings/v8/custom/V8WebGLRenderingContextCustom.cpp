/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 * 
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#if ENABLE(WEBGL)

#include "V8WebGLRenderingContext.h"

#include "ExceptionCode.h"
#include "NotImplemented.h"
#include "V8ArrayBufferView.h"
#include "V8Binding.h"
#include "V8EXTTextureFilterAnisotropic.h"
#include "V8Float32Array.h"
#include "V8HTMLCanvasElement.h"
#include "V8HTMLImageElement.h"
#include "V8HTMLVideoElement.h"
#include "V8ImageData.h"
#include "V8Int16Array.h"
#include "V8Int32Array.h"
#include "V8Int8Array.h"
#include "V8OESElementIndexUint.h"
#include "V8OESStandardDerivatives.h"
#include "V8OESTextureFloat.h"
#include "V8OESVertexArrayObject.h"
#include "V8Uint16Array.h"
#include "V8Uint32Array.h"
#include "V8Uint8Array.h"
#include "V8WebGLBuffer.h"
#include "V8WebGLCompressedTextureS3TC.h"
#include "V8WebGLDebugRendererInfo.h"
#include "V8WebGLDebugShaders.h"
#include "V8WebGLDepthTexture.h"
#include "V8WebGLFramebuffer.h"
#include "V8WebGLLoseContext.h"
#include "V8WebGLProgram.h"
#include "V8WebGLRenderbuffer.h"
#include "V8WebGLShader.h"
#include "V8WebGLTexture.h"
#include "V8WebGLUniformLocation.h"
#include "V8WebGLVertexArrayObjectOES.h"
#include "WebGLRenderingContext.h"
#include <limits>
#include <wtf/FastMalloc.h>

namespace WebCore {

// Allocates new storage via tryFastMalloc.
// Returns NULL if array failed to convert for any reason.
static float* jsArrayToFloatArray(v8::Handle<v8::Array> array, uint32_t len)
{
    // Convert the data element-by-element.
    float* data = 0;
    if (len > std::numeric_limits<uint32_t>::max() / sizeof(float)
        || !tryFastMalloc(len * sizeof(float)).getValue(data))
        return 0;
    for (uint32_t i = 0; i < len; i++) {
        v8::Local<v8::Value> val = array->Get(i);
        if (!val->IsNumber()) {
            fastFree(data);
            return 0;
        }
        data[i] = toFloat(val);
    }
    return data;
}

// Allocates new storage via tryFastMalloc.
// Returns NULL if array failed to convert for any reason.
static int* jsArrayToIntArray(v8::Handle<v8::Array> array, uint32_t len)
{
    // Convert the data element-by-element.
    int* data = 0;
    if (len > std::numeric_limits<uint32_t>::max() / sizeof(int)
        || !tryFastMalloc(len * sizeof(int)).getValue(data))
        return 0;
    for (uint32_t i = 0; i < len; i++) {
        v8::Local<v8::Value> val = array->Get(i);
        bool ok;
        int ival = toInt32(val, ok);
        if (!ok) {
            fastFree(data);
            return 0;
        }
        data[i] = ival;
    }
    return data;
}

static v8::Handle<v8::Value> toV8Object(const WebGLGetInfo& info, v8::Handle<v8::Object> creationContext, v8::Isolate* isolate)
{
    switch (info.getType()) {
    case WebGLGetInfo::kTypeBool:
        return v8::Boolean::New(info.getBool());
    case WebGLGetInfo::kTypeBoolArray: {
        const Vector<bool>& value = info.getBoolArray();
        v8::Local<v8::Array> array = v8::Array::New(value.size());
        for (size_t ii = 0; ii < value.size(); ++ii)
            array->Set(v8Integer(ii, isolate), v8::Boolean::New(value[ii]));
        return array;
    }
    case WebGLGetInfo::kTypeFloat:
        return v8::Number::New(info.getFloat());
    case WebGLGetInfo::kTypeInt:
        return v8Integer(info.getInt(), isolate);
    case WebGLGetInfo::kTypeNull:
        return v8Null(isolate);
    case WebGLGetInfo::kTypeString:
        return v8String(info.getString(), isolate);
    case WebGLGetInfo::kTypeUnsignedInt:
        return v8UnsignedInteger(info.getUnsignedInt(), isolate);
    case WebGLGetInfo::kTypeWebGLBuffer:
        return toV8(info.getWebGLBuffer(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLFloatArray:
        return toV8(info.getWebGLFloatArray(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLFramebuffer:
        return toV8(info.getWebGLFramebuffer(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLIntArray:
        return toV8(info.getWebGLIntArray(), creationContext, isolate);
    // FIXME: implement WebGLObjectArray
    // case WebGLGetInfo::kTypeWebGLObjectArray:
    case WebGLGetInfo::kTypeWebGLProgram:
        return toV8(info.getWebGLProgram(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLRenderbuffer:
        return toV8(info.getWebGLRenderbuffer(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLTexture:
        return toV8(info.getWebGLTexture(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLUnsignedByteArray:
        return toV8(info.getWebGLUnsignedByteArray(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLUnsignedIntArray:
        return toV8(info.getWebGLUnsignedIntArray(), creationContext, isolate);
    case WebGLGetInfo::kTypeWebGLVertexArrayObjectOES:
        return toV8(info.getWebGLVertexArrayObjectOES(), creationContext, isolate);
    default:
        notImplemented();
        return v8::Undefined();
    }
}

static v8::Handle<v8::Value> toV8Object(WebGLExtension* extension, v8::Handle<v8::Object> contextObject, v8::Isolate* isolate)
{
    if (!extension)
        return v8Null(isolate);
    v8::Handle<v8::Value> extensionObject;
    const char* referenceName = 0;
    switch (extension->getName()) {
    case WebGLExtension::WebKitWebGLLoseContextName:
        extensionObject = toV8(static_cast<WebGLLoseContext*>(extension), contextObject, isolate);
        referenceName = "webKitWebGLLoseContextName";
        break;
    case WebGLExtension::EXTTextureFilterAnisotropicName:
        extensionObject = toV8(static_cast<EXTTextureFilterAnisotropic*>(extension), contextObject, isolate);
        referenceName = "extTextureFilterAnisotropicName";
        break;
    case WebGLExtension::OESStandardDerivativesName:
        extensionObject = toV8(static_cast<OESStandardDerivatives*>(extension), contextObject, isolate);
        referenceName = "oesStandardDerivativesName";
        break;
    case WebGLExtension::OESTextureFloatName:
        extensionObject = toV8(static_cast<OESTextureFloat*>(extension), contextObject, isolate);
        referenceName = "oesTextureFloatName";
        break;
    case WebGLExtension::OESVertexArrayObjectName:
        extensionObject = toV8(static_cast<OESVertexArrayObject*>(extension), contextObject, isolate);
        referenceName = "oesVertexArrayObjectName";
        break;
    case WebGLExtension::OESElementIndexUintName:
        extensionObject = toV8(static_cast<OESElementIndexUint*>(extension), contextObject, isolate);
        referenceName = "oesElementIndexUintName";
        break;
    case WebGLExtension::WebGLDebugRendererInfoName:
        extensionObject = toV8(static_cast<WebGLDebugRendererInfo*>(extension), contextObject, isolate);
        referenceName = "webGLDebugRendererInfoName";
        break;
    case WebGLExtension::WebGLDebugShadersName:
        extensionObject = toV8(static_cast<WebGLDebugShaders*>(extension), contextObject, isolate);
        referenceName = "webGLDebugShadersName";
        break;
    case WebGLExtension::WebKitWebGLCompressedTextureS3TCName:
        extensionObject = toV8(static_cast<WebGLCompressedTextureS3TC*>(extension), contextObject, isolate);
        referenceName = "webKitWebGLCompressedTextureS3TCName";
        break;
    case WebGLExtension::WebKitWebGLDepthTextureName:
        extensionObject = toV8(static_cast<WebGLDepthTexture*>(extension), contextObject, isolate);
        referenceName = "webKitWebGLDepthTextureName";
        break;
    }
    ASSERT(!extensionObject.IsEmpty());
    V8DOMWrapper::setNamedHiddenReference(contextObject, referenceName, extensionObject);
    return extensionObject;
}

enum ObjectType {
    kBuffer, kRenderbuffer, kTexture, kVertexAttrib
};

static v8::Handle<v8::Value> getObjectParameter(const v8::Arguments& args, ObjectType objectType)
{
    if (args.Length() != 2)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    unsigned target = toInt32(args[0]);
    unsigned pname = toInt32(args[1]);
    WebGLGetInfo info;
    switch (objectType) {
    case kBuffer:
        info = context->getBufferParameter(target, pname, ec);
        break;
    case kRenderbuffer:
        info = context->getRenderbufferParameter(target, pname, ec);
        break;
    case kTexture:
        info = context->getTexParameter(target, pname, ec);
        break;
    case kVertexAttrib:
        // target => index
        info = context->getVertexAttrib(target, pname, ec);
        break;
    default:
        notImplemented();
        break;
    }
    if (ec)
        return setDOMException(ec, args.GetIsolate());
    return toV8Object(info, args.Holder(), args.GetIsolate());
}

static WebGLUniformLocation* toWebGLUniformLocation(v8::Handle<v8::Value> value, bool& ok)
{
    ok = false;
    WebGLUniformLocation* location = 0;
    if (V8WebGLUniformLocation::HasInstance(value)) {
        location = V8WebGLUniformLocation::toNative(value->ToObject());
        ok = true;
    }
    return location;
}

enum WhichProgramCall {
    kProgramParameter, kUniform
};

v8::Handle<v8::Value> V8WebGLRenderingContext::getAttachedShadersCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getAttachedShaders()");

    if (args.Length() < 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLProgram::HasInstance(args[0]))
        return throwTypeError(0, args.GetIsolate());
    WebGLProgram* program = V8WebGLProgram::HasInstance(args[0]) ? V8WebGLProgram::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    Vector<RefPtr<WebGLShader> > shaders;
    bool succeed = context->getAttachedShaders(program, shaders, ec);
    if (ec) {
        setDOMException(ec, args.GetIsolate());
        return v8Null(args.GetIsolate());
    }
    if (!succeed)
        return v8Null(args.GetIsolate());
    v8::Local<v8::Array> array = v8::Array::New(shaders.size());
    for (size_t ii = 0; ii < shaders.size(); ++ii)
        array->Set(v8Integer(ii, args.GetIsolate()), toV8(shaders[ii].get(), args.Holder(), args.GetIsolate()));
    return array;
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getBufferParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getBufferParameter()");
    return getObjectParameter(args, kBuffer);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getExtensionCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getExtensionCallback()");
    WebGLRenderingContext* imp = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() < 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());
    V8TRYCATCH_FOR_V8STRINGRESOURCE(V8StringResource<>, name, args[0]);
    WebGLExtension* extension = imp->getExtension(name);
    return toV8Object(extension, args.Holder(), args.GetIsolate());
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getFramebufferAttachmentParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getFramebufferAttachmentParameter()");

    if (args.Length() != 3)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    unsigned target = toInt32(args[0]);
    unsigned attachment = toInt32(args[1]);
    unsigned pname = toInt32(args[2]);
    WebGLGetInfo info = context->getFramebufferAttachmentParameter(target, attachment, pname, ec);
    if (ec)
        return setDOMException(ec, args.GetIsolate());
    return toV8Object(info, args.Holder(), args.GetIsolate());
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getParameter()");

    if (args.Length() != 1)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    unsigned pname = toInt32(args[0]);
    WebGLGetInfo info = context->getParameter(pname, ec);
    if (ec)
        return setDOMException(ec, args.GetIsolate());
    return toV8Object(info, args.Holder(), args.GetIsolate());
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getProgramParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getProgramParameter()");

    if (args.Length() != 2)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLProgram::HasInstance(args[0]))
        return throwTypeError(0, args.GetIsolate());
    WebGLProgram* program = V8WebGLProgram::HasInstance(args[0]) ? V8WebGLProgram::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    unsigned pname = toInt32(args[1]);
    WebGLGetInfo info = context->getProgramParameter(program, pname, ec);
    if (ec)
        return setDOMException(ec, args.GetIsolate());
    return toV8Object(info, args.Holder(), args.GetIsolate());
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getRenderbufferParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getRenderbufferParameter()");
    return getObjectParameter(args, kRenderbuffer);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getShaderParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getShaderParameter()");

    if (args.Length() != 2)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLShader::HasInstance(args[0]))
        return throwTypeError(0, args.GetIsolate());
    WebGLShader* shader = V8WebGLShader::HasInstance(args[0]) ? V8WebGLShader::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    unsigned pname = toInt32(args[1]);
    WebGLGetInfo info = context->getShaderParameter(shader, pname, ec);
    if (ec)
        return setDOMException(ec, args.GetIsolate());
    return toV8Object(info, args.Holder(), args.GetIsolate());
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getSupportedExtensionsCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getSupportedExtensionsCallback()");
    WebGLRenderingContext* imp = V8WebGLRenderingContext::toNative(args.Holder());
    if (imp->isContextLost())
        return v8Null(args.GetIsolate());

    Vector<String> value = imp->getSupportedExtensions();
    v8::Local<v8::Array> array = v8::Array::New(value.size());
    for (size_t ii = 0; ii < value.size(); ++ii)
        array->Set(v8Integer(ii, args.GetIsolate()), v8String(value[ii], args.GetIsolate()));
    return array;
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getTexParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getTexParameter()");
    return getObjectParameter(args, kTexture);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getUniformCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getUniform()");

    if (args.Length() != 2)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLProgram::HasInstance(args[0]))
        return throwTypeError(0, args.GetIsolate());
    WebGLProgram* program = V8WebGLProgram::HasInstance(args[0]) ? V8WebGLProgram::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;

    if (args.Length() > 1 && !isUndefinedOrNull(args[1]) && !V8WebGLUniformLocation::HasInstance(args[1]))
        return throwTypeError(0, args.GetIsolate());
    bool ok = false;
    WebGLUniformLocation* location = toWebGLUniformLocation(args[1], ok);

    WebGLGetInfo info = context->getUniform(program, location, ec);
    if (ec)
        return setDOMException(ec, args.GetIsolate());
    return toV8Object(info, args.Holder(), args.GetIsolate());
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getVertexAttribCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getVertexAttrib()");
    return getObjectParameter(args, kVertexAttrib);
}

enum FunctionToCall {
    kUniform1v, kUniform2v, kUniform3v, kUniform4v,
    kVertexAttrib1v, kVertexAttrib2v, kVertexAttrib3v, kVertexAttrib4v
};

bool isFunctionToCallForAttribute(FunctionToCall functionToCall)
{
    switch (functionToCall) {
    case kVertexAttrib1v:
    case kVertexAttrib2v:
    case kVertexAttrib3v:
    case kVertexAttrib4v:
        return true;
    default:
        break;
    }
    return false;
}

static v8::Handle<v8::Value> vertexAttribAndUniformHelperf(const v8::Arguments& args,
                                                           FunctionToCall functionToCall) {
    // Forms:
    // * glUniform1fv(WebGLUniformLocation location, Array data);
    // * glUniform1fv(WebGLUniformLocation location, Float32Array data);
    // * glUniform2fv(WebGLUniformLocation location, Array data);
    // * glUniform2fv(WebGLUniformLocation location, Float32Array data);
    // * glUniform3fv(WebGLUniformLocation location, Array data);
    // * glUniform3fv(WebGLUniformLocation location, Float32Array data);
    // * glUniform4fv(WebGLUniformLocation location, Array data);
    // * glUniform4fv(WebGLUniformLocation location, Float32Array data);
    // * glVertexAttrib1fv(GLint index, Array data);
    // * glVertexAttrib1fv(GLint index, Float32Array data);
    // * glVertexAttrib2fv(GLint index, Array data);
    // * glVertexAttrib2fv(GLint index, Float32Array data);
    // * glVertexAttrib3fv(GLint index, Array data);
    // * glVertexAttrib3fv(GLint index, Float32Array data);
    // * glVertexAttrib4fv(GLint index, Array data);
    // * glVertexAttrib4fv(GLint index, Float32Array data);

    if (args.Length() != 2)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    bool ok = false;
    int index = -1;
    WebGLUniformLocation* location = 0;

    if (isFunctionToCallForAttribute(functionToCall))
        index = toInt32(args[0]);
    else {
        if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLUniformLocation::HasInstance(args[0]))
            return throwTypeError(0, args.GetIsolate());
        location = toWebGLUniformLocation(args[0], ok);
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());

    if (V8Float32Array::HasInstance(args[1])) {
        Float32Array* array = V8Float32Array::toNative(args[1]->ToObject());
        ASSERT(array != NULL);
        ExceptionCode ec = 0;
        switch (functionToCall) {
            case kUniform1v: context->uniform1fv(location, array, ec); break;
            case kUniform2v: context->uniform2fv(location, array, ec); break;
            case kUniform3v: context->uniform3fv(location, array, ec); break;
            case kUniform4v: context->uniform4fv(location, array, ec); break;
            case kVertexAttrib1v: context->vertexAttrib1fv(index, array); break;
            case kVertexAttrib2v: context->vertexAttrib2fv(index, array); break;
            case kVertexAttrib3v: context->vertexAttrib3fv(index, array); break;
            case kVertexAttrib4v: context->vertexAttrib4fv(index, array); break;
            default: ASSERT_NOT_REACHED(); break;
        }
        if (ec)
            return setDOMException(ec, args.GetIsolate());
        return v8::Undefined();
    }

    if (args[1].IsEmpty() || !args[1]->IsArray())
        return throwTypeError(0, args.GetIsolate());
    v8::Handle<v8::Array> array =
      v8::Local<v8::Array>::Cast(args[1]);
    uint32_t len = array->Length();
    float* data = jsArrayToFloatArray(array, len);
    if (!data) {
        // FIXME: consider different / better exception type.
        return setDOMException(SYNTAX_ERR, args.GetIsolate());
    }
    ExceptionCode ec = 0;
    switch (functionToCall) {
        case kUniform1v: context->uniform1fv(location, data, len, ec); break;
        case kUniform2v: context->uniform2fv(location, data, len, ec); break;
        case kUniform3v: context->uniform3fv(location, data, len, ec); break;
        case kUniform4v: context->uniform4fv(location, data, len, ec); break;
        case kVertexAttrib1v: context->vertexAttrib1fv(index, data, len); break;
        case kVertexAttrib2v: context->vertexAttrib2fv(index, data, len); break;
        case kVertexAttrib3v: context->vertexAttrib3fv(index, data, len); break;
        case kVertexAttrib4v: context->vertexAttrib4fv(index, data, len); break;
        default: ASSERT_NOT_REACHED(); break;
    }
    fastFree(data);
    if (ec)
        return setDOMException(ec, args.GetIsolate());
    return v8::Undefined();
}

static v8::Handle<v8::Value> uniformHelperi(const v8::Arguments& args,
                                            FunctionToCall functionToCall) {
    // Forms:
    // * glUniform1iv(GLUniformLocation location, Array data);
    // * glUniform1iv(GLUniformLocation location, Int32Array data);
    // * glUniform2iv(GLUniformLocation location, Array data);
    // * glUniform2iv(GLUniformLocation location, Int32Array data);
    // * glUniform3iv(GLUniformLocation location, Array data);
    // * glUniform3iv(GLUniformLocation location, Int32Array data);
    // * glUniform4iv(GLUniformLocation location, Array data);
    // * glUniform4iv(GLUniformLocation location, Int32Array data);

    if (args.Length() != 2)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLUniformLocation::HasInstance(args[0]))
        return throwTypeError(0, args.GetIsolate());
    bool ok = false;
    WebGLUniformLocation* location = toWebGLUniformLocation(args[0], ok);

    if (V8Int32Array::HasInstance(args[1])) {
        Int32Array* array = V8Int32Array::toNative(args[1]->ToObject());
        ASSERT(array != NULL);
        ExceptionCode ec = 0;
        switch (functionToCall) {
            case kUniform1v: context->uniform1iv(location, array, ec); break;
            case kUniform2v: context->uniform2iv(location, array, ec); break;
            case kUniform3v: context->uniform3iv(location, array, ec); break;
            case kUniform4v: context->uniform4iv(location, array, ec); break;
            default: ASSERT_NOT_REACHED(); break;
        }
        if (ec)
            return setDOMException(ec, args.GetIsolate());
        return v8::Undefined();
    }

    if (args[1].IsEmpty() || !args[1]->IsArray())
        return throwTypeError(0, args.GetIsolate());
    v8::Handle<v8::Array> array =
      v8::Local<v8::Array>::Cast(args[1]);
    uint32_t len = array->Length();
    int* data = jsArrayToIntArray(array, len);
    if (!data) {
        // FIXME: consider different / better exception type.
        return setDOMException(SYNTAX_ERR, args.GetIsolate());
    }
    ExceptionCode ec = 0;
    switch (functionToCall) {
        case kUniform1v: context->uniform1iv(location, data, len, ec); break;
        case kUniform2v: context->uniform2iv(location, data, len, ec); break;
        case kUniform3v: context->uniform3iv(location, data, len, ec); break;
        case kUniform4v: context->uniform4iv(location, data, len, ec); break;
        default: ASSERT_NOT_REACHED(); break;
    }
    fastFree(data);
    if (ec)
        return setDOMException(ec, args.GetIsolate());
    return v8::Undefined();
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniform1fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform1fv()");
    return vertexAttribAndUniformHelperf(args, kUniform1v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniform1ivCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform1iv()");
    return uniformHelperi(args, kUniform1v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniform2fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform2fv()");
    return vertexAttribAndUniformHelperf(args, kUniform2v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniform2ivCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform2iv()");
    return uniformHelperi(args, kUniform2v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniform3fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform3fv()");
    return vertexAttribAndUniformHelperf(args, kUniform3v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniform3ivCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform3iv()");
    return uniformHelperi(args, kUniform3v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniform4fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform4fv()");
    return vertexAttribAndUniformHelperf(args, kUniform4v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniform4ivCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform4iv()");
    return uniformHelperi(args, kUniform4v);
}

static v8::Handle<v8::Value> uniformMatrixHelper(const v8::Arguments& args,
                                                 int matrixSize)
{
    // Forms:
    // * glUniformMatrix2fv(GLint location, GLboolean transpose, Array data);
    // * glUniformMatrix2fv(GLint location, GLboolean transpose, Float32Array data);
    // * glUniformMatrix3fv(GLint location, GLboolean transpose, Array data);
    // * glUniformMatrix3fv(GLint location, GLboolean transpose, Float32Array data);
    // * glUniformMatrix4fv(GLint location, GLboolean transpose, Array data);
    // * glUniformMatrix4fv(GLint location, GLboolean transpose, Float32Array data);
    //
    // FIXME: need to change to accept Float32Array as well.
    if (args.Length() != 3)
        return throwNotEnoughArgumentsError(args.GetIsolate());

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());

    if (args.Length() > 0 && !isUndefinedOrNull(args[0]) && !V8WebGLUniformLocation::HasInstance(args[0]))
        return throwTypeError(0, args.GetIsolate());
    bool ok = false;
    WebGLUniformLocation* location = toWebGLUniformLocation(args[0], ok);
    
    bool transpose = args[1]->BooleanValue();
    if (V8Float32Array::HasInstance(args[2])) {
        Float32Array* array = V8Float32Array::toNative(args[2]->ToObject());
        ASSERT(array != NULL);
        ExceptionCode ec = 0;
        switch (matrixSize) {
            case 2: context->uniformMatrix2fv(location, transpose, array, ec); break;
            case 3: context->uniformMatrix3fv(location, transpose, array, ec); break;
            case 4: context->uniformMatrix4fv(location, transpose, array, ec); break;
            default: ASSERT_NOT_REACHED(); break;
        }
        if (ec)
            return setDOMException(ec, args.GetIsolate());
        return v8::Undefined();
    }

    if (args[2].IsEmpty() || !args[2]->IsArray())
        return throwTypeError(0, args.GetIsolate());
    v8::Handle<v8::Array> array =
      v8::Local<v8::Array>::Cast(args[2]);
    uint32_t len = array->Length();
    float* data = jsArrayToFloatArray(array, len);
    if (!data) {
        // FIXME: consider different / better exception type.
        return setDOMException(SYNTAX_ERR, args.GetIsolate());
    }
    ExceptionCode ec = 0;
    switch (matrixSize) {
        case 2: context->uniformMatrix2fv(location, transpose, data, len, ec); break;
        case 3: context->uniformMatrix3fv(location, transpose, data, len, ec); break;
        case 4: context->uniformMatrix4fv(location, transpose, data, len, ec); break;
        default: ASSERT_NOT_REACHED(); break;
    }
    fastFree(data);
    if (ec)
        return setDOMException(ec, args.GetIsolate()); 
    return v8::Undefined();
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniformMatrix2fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniformMatrix2fv()");
    return uniformMatrixHelper(args, 2);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniformMatrix3fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniformMatrix3fv()");
    return uniformMatrixHelper(args, 3);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::uniformMatrix4fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.uniformMatrix4fv()");
    return uniformMatrixHelper(args, 4);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::vertexAttrib1fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.vertexAttrib1fv()");
    return vertexAttribAndUniformHelperf(args, kVertexAttrib1v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::vertexAttrib2fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.vertexAttrib2fv()");
    return vertexAttribAndUniformHelperf(args, kVertexAttrib2v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::vertexAttrib3fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.vertexAttrib3fv()");
    return vertexAttribAndUniformHelperf(args, kVertexAttrib3v);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::vertexAttrib4fvCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.vertexAttrib4fv()");
    return vertexAttribAndUniformHelperf(args, kVertexAttrib4v);
}

} // namespace WebCore

#endif // ENABLE(WEBGL)
