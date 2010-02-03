/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions areV8ClassIndex::WEBGL
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

#if ENABLE(3D_CANVAS)

#include "V8WebGLRenderingContext.h"

#include "ExceptionCode.h"

#include "NotImplemented.h"

#include <wtf/FastMalloc.h>

#include "V8Binding.h"
#include "V8WebGLArray.h"
#include "V8WebGLBuffer.h"
#include "V8WebGLByteArray.h"
#include "V8WebGLFloatArray.h"
#include "V8WebGLFramebuffer.h"
#include "V8WebGLIntArray.h"
#include "V8WebGLProgram.h"
#include "V8WebGLRenderbuffer.h"
#include "V8WebGLShader.h"
#include "V8WebGLShortArray.h"
#include "V8WebGLTexture.h"
#include "V8WebGLUniformLocation.h"
#include "V8WebGLUnsignedByteArray.h"
#include "V8WebGLUnsignedIntArray.h"
#include "V8WebGLUnsignedShortArray.h"
#include "V8HTMLCanvasElement.h"
#include "V8HTMLImageElement.h"
#include "V8HTMLVideoElement.h"
#include "V8ImageData.h"
#include "V8Proxy.h"
#include "WebGLRenderingContext.h"

namespace WebCore {

// Allocates new storage via tryFastMalloc.
// Returns NULL if array failed to convert for any reason.
static float* jsArrayToFloatArray(v8::Handle<v8::Array> array, uint32_t len)
{
    // Convert the data element-by-element.
    float* data;
    if (!tryFastMalloc(len * sizeof(float)).getValue(data))
        return 0;
    for (uint32_t i = 0; i < len; i++) {
        v8::Local<v8::Value> val = array->Get(v8::Integer::New(i));
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
    int* data;
    if (!tryFastMalloc(len * sizeof(int)).getValue(data))
        return 0;
    for (uint32_t i = 0; i < len; i++) {
        v8::Local<v8::Value> val = array->Get(v8::Integer::New(i));
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

v8::Handle<v8::Value> V8WebGLRenderingContext::bufferDataCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.bufferData()");

    // Forms:
    // * bufferData(GLenum target, WebGLArray data, GLenum usage);
    //   - Sets the buffer's data from the given WebGLArray
    // * bufferData(GLenum target, GLsizeiptr size, GLenum usage);
    //   - Sets the size of the buffer to the given size in bytes
    if (args.Length() != 3) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    bool ok;
    int target = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    int usage = toInt32(args[2], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    if (args[1]->IsInt32()) {
        int size = toInt32(args[1]);
        ExceptionCode exceptionCode;
        context->bufferData(target, size, usage, exceptionCode);
    } else if (V8WebGLArray::HasInstance(args[1])) {
        WebGLArray* array = V8WebGLArray::toNative(args[1]->ToObject());
        ExceptionCode exceptionCode;
        context->bufferData(target, array, usage, exceptionCode);
    } else {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    return v8::Undefined();
}

v8::Handle<v8::Value> V8WebGLRenderingContext::bufferSubDataCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.bufferSubData()");

    // Forms:
    // * bufferSubData(GLenum target, GLintptr offset, WebGLArray data);
    if (args.Length() != 3) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    bool ok;
    int target = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    int offset = toInt32(args[1], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    if (!V8WebGLArray::HasInstance(args[2])) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    WebGLArray* array = V8WebGLArray::toNative(args[2]->ToObject());
    ExceptionCode exceptionCode;
    context->bufferSubData(target, offset, array, exceptionCode);
    return v8::Undefined();
}

static v8::Handle<v8::Value> toV8Object(const WebGLGetInfo& info)
{
    switch (info.getType()) {
    case WebGLGetInfo::kTypeBool:
        return v8::Boolean::New(info.getBool());
    case WebGLGetInfo::kTypeFloat:
        return v8::Number::New(info.getFloat());
    case WebGLGetInfo::kTypeLong:
        return v8::Integer::New(info.getLong());
    case WebGLGetInfo::kTypeNull:
        return v8::Null();
    case WebGLGetInfo::kTypeString:
        return v8::String::New(fromWebCoreString(info.getString()), info.getString().length());
    case WebGLGetInfo::kTypeUnsignedLong:
        return v8::Integer::NewFromUnsigned(info.getUnsignedLong());
    case WebGLGetInfo::kTypeWebGLBuffer:
        return toV8(info.getWebGLBuffer());
    case WebGLGetInfo::kTypeWebGLFloatArray:
        return toV8(info.getWebGLFloatArray());
    case WebGLGetInfo::kTypeWebGLFramebuffer:
        return toV8(info.getWebGLFramebuffer());
    case WebGLGetInfo::kTypeWebGLIntArray:
        return toV8(info.getWebGLIntArray());
    // FIXME: implement WebGLObjectArray
    // case WebGLGetInfo::kTypeWebGLObjectArray:
    case WebGLGetInfo::kTypeWebGLProgram:
        return toV8(info.getWebGLProgram());
    case WebGLGetInfo::kTypeWebGLRenderbuffer:
        return toV8(info.getWebGLRenderbuffer());
    case WebGLGetInfo::kTypeWebGLTexture:
        return toV8(info.getWebGLTexture());
    case WebGLGetInfo::kTypeWebGLUnsignedByteArray:
        return toV8(info.getWebGLUnsignedByteArray());
    default:
        notImplemented();
        return v8::Undefined();
    }
}

enum ObjectType {
    kBuffer, kRenderbuffer, kTexture, kVertexAttrib
};

static v8::Handle<v8::Value> getObjectParameter(const v8::Arguments& args, ObjectType objectType)
{
    if (args.Length() != 2) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    bool ok;
    unsigned target = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    unsigned pname = toInt32(args[1], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
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
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }
    return toV8Object(info);
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

v8::Handle<v8::Value> V8WebGLRenderingContext::getBufferParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getBufferParameter()");
    return getObjectParameter(args, kBuffer);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getFramebufferAttachmentParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getFramebufferAttachmentParameter()");

    if (args.Length() != 3) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    bool ok;
    unsigned target = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    unsigned attachment = toInt32(args[1], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    unsigned pname = toInt32(args[2], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    WebGLGetInfo info = context->getFramebufferAttachmentParameter(target, attachment, pname, ec);
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }
    return toV8Object(info);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getParameter()");

    if (args.Length() != 1) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    bool ok;
    unsigned pname = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    WebGLGetInfo info = context->getParameter(pname, ec);
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }
    return toV8Object(info);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getProgramParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getProgramParameter()");

    if (args.Length() != 2) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    WebGLProgram* program = V8WebGLProgram::HasInstance(args[0]) ? V8WebGLProgram::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    bool ok;
    unsigned pname = toInt32(args[1], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    WebGLGetInfo info = context->getProgramParameter(program, pname, ec);
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }
    return toV8Object(info);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getRenderbufferParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getRenderbufferParameter()");
    return getObjectParameter(args, kRenderbuffer);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getShaderParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getShaderParameter()");

    if (args.Length() != 2) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    WebGLShader* shader = V8WebGLShader::HasInstance(args[0]) ? V8WebGLShader::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;
    bool ok;
    unsigned pname = toInt32(args[1], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    WebGLGetInfo info = context->getShaderParameter(shader, pname, ec);
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }
    return toV8Object(info);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getTexParameterCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getTexParameter()");
    return getObjectParameter(args, kTexture);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getUniformCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getUniform()");

    if (args.Length() != 2) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    WebGLProgram* program = V8WebGLProgram::HasInstance(args[0]) ? V8WebGLProgram::toNative(v8::Handle<v8::Object>::Cast(args[0])) : 0;

    bool ok = false;
    WebGLUniformLocation* location = toWebGLUniformLocation(args[1], ok);

    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    WebGLGetInfo info = context->getUniform(program, location, ec);
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }
    return toV8Object(info);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::getVertexAttribCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.getVertexAttrib()");
    return getObjectParameter(args, kVertexAttrib);
}

v8::Handle<v8::Value> V8WebGLRenderingContext::texImage2DCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.texImage2D()");

    // Currently supported forms:
    // * void texImage2D(in GLenum target, in GLint level,
    //                   in GLint internalformat,
    //                   in GLsizei width, in GLsizei height, in GLint border,
    //                   in GLenum format, in GLenum type, in WebGLArray pixels);
    // * void texImage2D(in GLenum target, in GLint level, in ImageData pixels,
    //                   [Optional] in GLboolean flipY, [Optional] in GLboolean premulitplyAlpha);
    // * void texImage2D(in GLenum target, in GLint level, in HTMLImageElement image,
    //                   [Optional] in GLboolean flipY, [Optional] in GLboolean premultiplyAlpha);
    // * void texImage2D(in GLenum target, in GLint level, in HTMLCanvasElement image,
    //                   [Optional] in GLboolean flipY, [Optional] in GLboolean premultiplyAlpha);
    // * void texImage2D(in GLenum target, in GLint level, in HTMLVideoElement image,
    //                   [Optional] in GLboolean flipY, [Optional] in GLboolean premultiplyAlpha);
    if (args.Length() != 3 &&
        args.Length() != 4 &&
        args.Length() != 5 &&
        args.Length() != 9) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    bool ok;
    int target = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    int level = toInt32(args[1], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    ExceptionCode ec = 0;
    if (args.Length() == 3 ||
        args.Length() == 4 ||
        args.Length() == 5) {
        bool flipY = false;
        bool premultiplyAlpha = false;
        if (args.Length() >= 4)
            flipY = args[3]->BooleanValue();
        if (args.Length() >= 5)
            premultiplyAlpha = args[4]->BooleanValue();

        v8::Handle<v8::Value> arg = args[2];
        if (V8HTMLImageElement::HasInstance(arg)) {
            HTMLImageElement* element = V8HTMLImageElement::toNative(v8::Handle<v8::Object>::Cast(arg));
            context->texImage2D(target, level, element, flipY, premultiplyAlpha, ec);
        } else if (V8HTMLCanvasElement::HasInstance(arg)) {
            HTMLCanvasElement* element = V8HTMLCanvasElement::toNative(v8::Handle<v8::Object>::Cast(arg));
            context->texImage2D(target, level, element, flipY, premultiplyAlpha, ec);
        } else if(V8ImageData::HasInstance(arg)) {
            ImageData* imageElement = V8ImageData::toNative(v8::Handle<v8::Object>::Cast(arg));
            context->texImage2D(target, level, imageElement, flipY, premultiplyAlpha, ec);
        } else if (V8HTMLVideoElement::HasInstance(arg)) {
            HTMLVideoElement* element = V8HTMLVideoElement::toNative(v8::Handle<v8::Object>::Cast(arg));
            context->texImage2D(target, level, element, flipY, premultiplyAlpha, ec);
        }
        else {
            // FIXME: consider different / better exception type.
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        // Fall through
    } else if (args.Length() == 9) {
        int internalformat = toInt32(args[2], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        int width = toInt32(args[3], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        int height = toInt32(args[4], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        int border = toInt32(args[5], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        int format = toInt32(args[6], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        int type = toInt32(args[7], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        v8::Handle<v8::Value> arg = args[8];
        if (!arg->IsObject())
        // Assume that the user is passing null for texture
            context->texImage2D(target,
                                level,
                                internalformat,
                                width,
                                height,
                                border,
                                format,
                                type,
                                0,
                                ec);
     else if (V8WebGLArray::HasInstance(arg)) {
            WebGLArray* array = V8WebGLArray::toNative(arg->ToObject());
            context->texImage2D(target,
                                level,
                                internalformat,
                                width,
                                height,
                                border,
                                format,
                                type,
                                array,
                                ec);
            // Fall through
        } else {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
    } else {
        ASSERT_NOT_REACHED();
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    return v8::Undefined();
}

v8::Handle<v8::Value> V8WebGLRenderingContext::texSubImage2DCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.WebGLRenderingContext.texSubImage2D()");

    // Currently supported forms:
    // * void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset, 
    //                      in GLsizei width, in GLsizei height, 
    //                      in GLenum format, in GLenum type, in WebGLArray pixels);
    // * void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset,
    //                      in ImageData pixels, [Optional] GLboolean flipY, [Optional] in premultiplyAlpha);
    // * void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset, 
    //                      in HTMLImageElement image, [Optional] GLboolean flipY, [Optional] in premultiplyAlpha);
    // * void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset, 
    //                      in HTMLCanvasElement canvas, [Optional] GLboolean flipY, [Optional] in premultiplyAlpha);
    // * void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset, 
    //                      in HTMLVideoElement video, [Optional] GLboolean flipY, [Optional] in premultiplyAlpha);

    if (args.Length() != 5 &&
        args.Length() != 6 &&
        args.Length() != 7 &&
        args.Length() != 9) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    bool ok;
    int target = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    int level = toInt32(args[1], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    int xoff = toInt32(args[2], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    int yoff = toInt32(args[3], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    ExceptionCode ec = 0;
    if (args.Length() == 5 ||
        args.Length() == 6 ||
        args.Length() == 7) {
        bool flipY = false;
        bool premultiplyAlpha = false;
        if (args.Length() >= 6)
            flipY = args[5]->BooleanValue();
        if (args.Length() >= 7)
            premultiplyAlpha = args[6]->BooleanValue();

        v8::Handle<v8::Value> arg = args[4];
        if (V8HTMLImageElement::HasInstance(arg)) {
            HTMLImageElement* element = V8HTMLImageElement::toNative(v8::Handle<v8::Object>::Cast(arg));
            context->texSubImage2D(target, level, xoff, yoff, element, flipY, premultiplyAlpha, ec);
        } else if (V8HTMLCanvasElement::HasInstance(arg)) {
            HTMLCanvasElement* element = V8HTMLCanvasElement::toNative(v8::Handle<v8::Object>::Cast(arg));
            context->texSubImage2D(target, level, xoff, yoff, element, flipY, premultiplyAlpha, ec);
        } else if(V8ImageData::HasInstance(arg)) {
            ImageData* imageElement = V8ImageData::toNative(v8::Handle<v8::Object>::Cast(arg));
            context->texSubImage2D(target, level, xoff, yoff, imageElement, flipY, premultiplyAlpha, ec);
        } else if (V8HTMLVideoElement::HasInstance(arg)) {
            HTMLVideoElement* element = V8HTMLVideoElement::toNative(v8::Handle<v8::Object>::Cast(arg));
            context->texSubImage2D(target, level, xoff, yoff, element, flipY, premultiplyAlpha, ec);
        }
        else {
            // FIXME: consider different / better exception type.
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        // Fall through
    } else if (args.Length() == 9) {
        int width = toInt32(args[4], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        int height = toInt32(args[5], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        int format = toInt32(args[6], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        int type = toInt32(args[7], ok);
        if (!ok) {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
        v8::Handle<v8::Value> arg = args[8];
        if (!arg->IsObject())
        // Assume that the user is passing null for texture
            context->texSubImage2D(target,
                                   level,
                                   xoff,
                                   yoff,
                                   width,
                                   height,
                                   format,
                                   type,
                                   0,
                                   ec);
     else if (V8WebGLArray::HasInstance(arg)) {
            WebGLArray* array = V8WebGLArray::toNative(arg->ToObject());
            context->texSubImage2D(target,
                                   level,
                                   xoff,
                                   yoff,
                                   width,
                                   height,
                                   format,
                                   type,
                                   array,
                                   ec);
            // Fall through
        } else {
            V8Proxy::setDOMException(SYNTAX_ERR);
            return notHandledByInterceptor();
        }
    } else {
        ASSERT_NOT_REACHED();
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    if (ec) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }
    return v8::Undefined();
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
    // * glUniform1fv(WebGLUniformLocation location, WebGLFloatArray data);
    // * glUniform2fv(WebGLUniformLocation location, Array data);
    // * glUniform2fv(WebGLUniformLocation location, WebGLFloatArray data);
    // * glUniform3fv(WebGLUniformLocation location, Array data);
    // * glUniform3fv(WebGLUniformLocation location, WebGLFloatArray data);
    // * glUniform4fv(WebGLUniformLocation location, Array data);
    // * glUniform4fv(WebGLUniformLocation location, WebGLFloatArray data);
    // * glVertexAttrib1fv(GLint index, Array data);
    // * glVertexAttrib1fv(GLint index, WebGLFloatArray data);
    // * glVertexAttrib2fv(GLint index, Array data);
    // * glVertexAttrib2fv(GLint index, WebGLFloatArray data);
    // * glVertexAttrib3fv(GLint index, Array data);
    // * glVertexAttrib3fv(GLint index, WebGLFloatArray data);
    // * glVertexAttrib4fv(GLint index, Array data);
    // * glVertexAttrib4fv(GLint index, WebGLFloatArray data);

    if (args.Length() != 2) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    bool ok = false;
    int index = -1;
    WebGLUniformLocation* location = 0;

    if (isFunctionToCallForAttribute(functionToCall))
        index = toInt32(args[0], ok);
    else
        location = toWebGLUniformLocation(args[0], ok);

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());

    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    if (V8WebGLFloatArray::HasInstance(args[1])) {
        WebGLFloatArray* array = V8WebGLFloatArray::toNative(args[1]->ToObject());
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
            V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }

    v8::Handle<v8::Array> array =
      v8::Local<v8::Array>::Cast(args[1]);
    if (array.IsEmpty()) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    uint32_t len = array->Length();
    float* data = jsArrayToFloatArray(array, len);
    if (!data) {
        // FIXME: consider different / better exception type.
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
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
        V8Proxy::setDOMException(ec);
    return v8::Undefined();
}

static v8::Handle<v8::Value> uniformHelperi(const v8::Arguments& args,
                                            FunctionToCall functionToCall) {
    // Forms:
    // * glUniform1iv(GLUniformLocation location, Array data);
    // * glUniform1iv(GLUniformLocation location, WebGLIntArray data);
    // * glUniform2iv(GLUniformLocation location, Array data);
    // * glUniform2iv(GLUniformLocation location, WebGLIntArray data);
    // * glUniform3iv(GLUniformLocation location, Array data);
    // * glUniform3iv(GLUniformLocation location, WebGLIntArray data);
    // * glUniform4iv(GLUniformLocation location, Array data);
    // * glUniform4iv(GLUniformLocation location, WebGLIntArray data);

    if (args.Length() != 2) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());
    bool ok = false;
    WebGLUniformLocation* location = toWebGLUniformLocation(args[0], ok);

    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    if (V8WebGLIntArray::HasInstance(args[1])) {
        WebGLIntArray* array = V8WebGLIntArray::toNative(args[1]->ToObject());
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
            V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }

    v8::Handle<v8::Array> array =
      v8::Local<v8::Array>::Cast(args[1]);
    if (array.IsEmpty()) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    uint32_t len = array->Length();
    int* data = jsArrayToIntArray(array, len);
    if (!data) {
        // FIXME: consider different / better exception type.
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
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
        V8Proxy::setDOMException(ec);
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
    // * glUniformMatrix2fv(GLint location, GLboolean transpose, WebGLFloatArray data);
    // * glUniformMatrix3fv(GLint location, GLboolean transpose, Array data);
    // * glUniformMatrix3fv(GLint location, GLboolean transpose, WebGLFloatArray data);
    // * glUniformMatrix4fv(GLint location, GLboolean transpose, Array data);
    // * glUniformMatrix4fv(GLint location, GLboolean transpose, WebGLFloatArray data);
    //
    // FIXME: need to change to accept WebGLFloatArray as well.
    if (args.Length() != 3) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context = V8WebGLRenderingContext::toNative(args.Holder());

    bool ok = false;
    WebGLUniformLocation* location = toWebGLUniformLocation(args[0], ok);
    
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    bool transpose = args[1]->BooleanValue();
    if (V8WebGLFloatArray::HasInstance(args[2])) {
        WebGLFloatArray* array = V8WebGLFloatArray::toNative(args[2]->ToObject());
        ASSERT(array != NULL);
        ExceptionCode ec = 0;
        switch (matrixSize) {
            case 2: context->uniformMatrix2fv(location, transpose, array, ec); break;
            case 3: context->uniformMatrix3fv(location, transpose, array, ec); break;
            case 4: context->uniformMatrix4fv(location, transpose, array, ec); break;
            default: ASSERT_NOT_REACHED(); break;
        }
        if (ec)
            V8Proxy::setDOMException(ec);
        return v8::Undefined();
    }

    v8::Handle<v8::Array> array =
      v8::Local<v8::Array>::Cast(args[2]);
    if (array.IsEmpty()) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    uint32_t len = array->Length();
    float* data = jsArrayToFloatArray(array, len);
    if (!data) {
        // FIXME: consider different / better exception type.
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
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
        V8Proxy::setDOMException(ec); 
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

#endif // ENABLE(3D_CANVAS)
