/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(3D_CANVAS)

#include "JSWebGLRenderingContext.h"

#include "ExceptionCode.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLImageElement.h"
#include "JSImageData.h"
#include "JSWebGLBuffer.h"
#include "JSWebGLFloatArray.h"
#include "JSWebGLFramebuffer.h"
#include "JSWebGLIntArray.h"
#include "JSWebGLProgram.h"
#include "JSWebGLRenderbuffer.h"
#include "JSWebGLShader.h"
#include "JSWebGLTexture.h"
#include "JSWebGLUniformLocation.h"
#include "JSWebGLUnsignedByteArray.h"
#include "JSWebKitCSSMatrix.h"
#include "NotImplemented.h"
#include "WebGLBuffer.h"
#include "WebGLFloatArray.h"
#include "WebGLFramebuffer.h"
#include "WebGLGetInfo.h"
#include "WebGLIntArray.h"
#include "WebGLProgram.h"
#include "WebGLRenderingContext.h"
#include <runtime/Error.h>
#include <wtf/FastMalloc.h>
#include <wtf/OwnFastMallocPtr.h>

#if ENABLE(VIDEO)
#include "HTMLVideoElement.h"
#include "JSHTMLVideoElement.h"
#endif

using namespace JSC;

namespace WebCore {

JSValue JSWebGLRenderingContext::bufferData(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    unsigned target = args.at(0).toInt32(exec);
    unsigned usage = args.at(2).toInt32(exec);
    ExceptionCode ec = 0;

    // If argument 1 is a number, we are initializing this buffer to that size
    if (!args.at(1).isObject()) {
        unsigned int count = args.at(1).toInt32(exec);
        static_cast<WebGLRenderingContext*>(impl())->bufferData(target, count, usage, ec);
    } else {
        WebGLArray* array = toWebGLArray(args.at(1));
        static_cast<WebGLRenderingContext*>(impl())->bufferData(target, array, usage, ec);
    }

    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue JSWebGLRenderingContext::bufferSubData(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    unsigned target = args.at(0).toInt32(exec);
    unsigned offset = args.at(1).toInt32(exec);
    ExceptionCode ec = 0;
    
    WebGLArray* array = toWebGLArray(args.at(2));
    
    static_cast<WebGLRenderingContext*>(impl())->bufferSubData(target, offset, array, ec);

    setDOMException(exec, ec);
    return jsUndefined();
}

static JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, const WebGLGetInfo& info)
{
    switch (info.getType()) {
    case WebGLGetInfo::kTypeBool:
        return jsBoolean(info.getBool());
    case WebGLGetInfo::kTypeFloat:
        return jsNumber(exec, info.getFloat());
    case WebGLGetInfo::kTypeLong:
        return jsNumber(exec, info.getLong());
    case WebGLGetInfo::kTypeNull:
        return jsNull();
    case WebGLGetInfo::kTypeString:
        return jsString(exec, info.getString());
    case WebGLGetInfo::kTypeUnsignedLong:
        return jsNumber(exec, info.getUnsignedLong());
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
    default:
        notImplemented();
        return jsUndefined();
    }
}

enum ObjectType {
    kBuffer, kRenderbuffer, kTexture, kVertexAttrib
};

static JSValue getObjectParameter(JSWebGLRenderingContext* obj, ExecState* exec, const ArgList& args, ObjectType objectType)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(obj->impl());
    unsigned target = args.at(0).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    unsigned pname = args.at(1).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
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
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, obj->globalObject(), info);
}

enum WhichProgramCall {
    kProgramParameter, kUniform
};

JSValue JSWebGLRenderingContext::getBufferParameter(ExecState* exec, const ArgList& args)
{
    return getObjectParameter(this, exec, args, kBuffer);
}

JSValue JSWebGLRenderingContext::getFramebufferAttachmentParameter(ExecState* exec, const ArgList& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    unsigned target = args.at(0).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    unsigned attachment = args.at(1).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    unsigned pname = args.at(2).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getFramebufferAttachmentParameter(target, attachment, pname, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getParameter(ExecState* exec, const ArgList& args)
{
    if (args.size() != 1)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    unsigned pname = args.at(0).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getParameter(pname, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getProgramParameter(ExecState* exec, const ArgList& args)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    WebGLProgram* program = toWebGLProgram(args.at(0));
    unsigned pname = args.at(1).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getProgramParameter(program, pname, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getRenderbufferParameter(ExecState* exec, const ArgList& args)
{
    return getObjectParameter(this, exec, args, kRenderbuffer);
}

JSValue JSWebGLRenderingContext::getShaderParameter(ExecState* exec, const ArgList& args)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    WebGLShader* shader = toWebGLShader(args.at(0));
    unsigned pname = args.at(1).toInt32(exec);
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getShaderParameter(shader, pname, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getTexParameter(ExecState* exec, const ArgList& args)
{
    return getObjectParameter(this, exec, args, kTexture);
}

JSValue JSWebGLRenderingContext::getUniform(ExecState* exec, const ArgList& args)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());
    WebGLProgram* program = toWebGLProgram(args.at(0));
    WebGLUniformLocation* loc = toWebGLUniformLocation(args.at(1));
    if (exec->hadException())
        return jsUndefined();
    WebGLGetInfo info = context->getUniform(program, loc, ec);
    if (ec) {
        setDOMException(exec, ec);
        return jsUndefined();
    }
    return toJS(exec, globalObject(), info);
}

JSValue JSWebGLRenderingContext::getVertexAttrib(ExecState* exec, const ArgList& args)
{
    return getObjectParameter(this, exec, args, kVertexAttrib);
}

//   void texImage2D(in GLenum target, in GLint level, in GLenum internalformat, in GLsizei width, in GLsizei height, in GLint border, in GLenum format, in GLenum type, in WebGLArray pixels);
//   void texImage2D(in GLenum target, in GLint level, in ImageData pixels, [Optional] GLboolean flipY, [Optional] in premultiplyAlpha);
//   void texImage2D(in GLenum target, in GLint level, in HTMLImageElement image, [Optional] in GLboolean flipY, [Optional] in premultiplyAlpha);
//   void texImage2D(in GLenum target, in GLint level, in HTMLCanvasElement canvas, [Optional] in GLboolean flipY, [Optional] in premultiplyAlpha);
//   void texImage2D(in GLenum target, in GLint level, in HTMLVideoElement video, [Optional] in GLboolean flipY, [Optional] in premultiplyAlpha);
JSValue JSWebGLRenderingContext::texImage2D(ExecState* exec, const ArgList& args)
{ 
    if (args.size() < 3 || args.size() > 9)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    
    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());    
    unsigned target = args.at(0).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
    
    unsigned level = args.at(1).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();

    JSObject* o = 0;
    
    if (args.size() <= 5) {
        // This is one of the last 4 forms. Param 2 can be ImageData or <img>, <canvas> or <video> element.
        JSValue value = args.at(2);
    
        if (!value.isObject())
            return throwError(exec, TypeError);
        
        o = asObject(value);
        
        bool flipY = args.at(3).toBoolean(exec);
        bool premultiplyAlpha = args.at(4).toBoolean(exec);
        
        if (o->inherits(&JSImageData::s_info)) {
            ImageData* data = static_cast<ImageData*>(static_cast<JSImageData*>(o)->impl());
            context->texImage2D(target, level, data, flipY, premultiplyAlpha, ec);
        } else if (o->inherits(&JSHTMLImageElement::s_info)) {
            HTMLImageElement* element = static_cast<HTMLImageElement*>(static_cast<JSHTMLImageElement*>(o)->impl());
            context->texImage2D(target, level, element, flipY, premultiplyAlpha, ec);
        } else if (o->inherits(&JSHTMLCanvasElement::s_info)) {
            HTMLCanvasElement* element = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLCanvasElement*>(o)->impl());
            context->texImage2D(target, level, element, flipY, premultiplyAlpha, ec);
#if ENABLE(VIDEO)
        } else if (o->inherits(&JSHTMLVideoElement::s_info)) {
            HTMLVideoElement* element = static_cast<HTMLVideoElement*>(static_cast<JSHTMLVideoElement*>(o)->impl());
            context->texImage2D(target, level, element, flipY, premultiplyAlpha, ec);
#endif
        } else
            ec = TYPE_MISMATCH_ERR;
    } else {
        if (args.size() != 9)
            return throwError(exec, SyntaxError);

        // This must be the WebGLArray case
        unsigned internalformat = args.at(2).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned width = args.at(3).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned height = args.at(4).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned border = args.at(5).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned format = args.at(6).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        unsigned type = args.at(7).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();

        JSValue value = args.at(8);
            
        // For this case passing 0 (for a null array) is allowed
        if (value.isNull())
            context->texImage2D(target, level, internalformat, width, height, border, format, type, 0, ec);
        else if (value.isObject()) {
            o = asObject(value);
            
            if (o->inherits(&JSWebGLArray::s_info)) {
                // FIXME: Need to check to make sure WebGLArray is a WebGLByteArray or WebGLShortArray,
                // depending on the passed type parameter.
                WebGLArray* obj = static_cast<WebGLArray*>(static_cast<JSWebGLArray*>(o)->impl());
                context->texImage2D(target, level, internalformat, width, height, border, format, type, obj, ec);
            } else
                return throwError(exec, TypeError);
        } else 
            return throwError(exec, TypeError);
    }
    
    setDOMException(exec, ec);
    return jsUndefined();    
}

//   void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset, in GLsizei width, in GLsizei height, in GLenum format, in GLenum type, in WebGLArray pixels);
//   void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset, in ImageData pixels, [Optional] GLboolean flipY, [Optional] in premultiplyAlpha);
//   void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset, in HTMLImageElement image, [Optional] GLboolean flipY, [Optional] in premultiplyAlpha);
//   void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset, in HTMLCanvasElement canvas, [Optional] GLboolean flipY, [Optional] in premultiplyAlpha);
//   void texSubImage2D(in GLenum target, in GLint level, in GLint xoffset, in GLint yoffset, in HTMLVideoElement video, [Optional] GLboolean flipY, [Optional] in premultiplyAlpha);
JSValue JSWebGLRenderingContext::texSubImage2D(ExecState* exec, const ArgList& args)
{ 
    if (args.size() < 5 || args.size() > 9)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;

    WebGLRenderingContext* context = static_cast<WebGLRenderingContext*>(impl());    
    unsigned target = args.at(0).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();

    unsigned level = args.at(1).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
    
    unsigned xoff = args.at(2).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
    
    unsigned yoff = args.at(3).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
    
    JSObject* o = 0;
        
    if (args.size() <= 7) {
        // This is one of the last 4 forms. Param 4 can be <img>, <canvas> or <video> element, of the format param.
        JSValue value = args.at(4);

        if (!value.isObject())
            return throwError(exec, SyntaxError);

        o = asObject(value);

        bool flipY = args.at(5).toBoolean(exec);
        bool premultiplyAlpha = args.at(6).toBoolean(exec);
        
        if (o->inherits(&JSImageData::s_info)) {
            ImageData* data = static_cast<ImageData*>(static_cast<JSImageData*>(o)->impl());
            context->texSubImage2D(target, level, xoff, yoff, data, flipY, premultiplyAlpha, ec);
        } else if (o->inherits(&JSHTMLImageElement::s_info)) {
            HTMLImageElement* element = static_cast<HTMLImageElement*>(static_cast<JSHTMLImageElement*>(o)->impl());
            context->texSubImage2D(target, level, xoff, yoff, element, flipY, premultiplyAlpha, ec);
        } else if (o->inherits(&JSHTMLCanvasElement::s_info)) {
            HTMLCanvasElement* element = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLCanvasElement*>(o)->impl());
            context->texSubImage2D(target, level, xoff, yoff, element, flipY, premultiplyAlpha, ec);
#if ENABLE(VIDEO)
        } else if (o->inherits(&JSHTMLVideoElement::s_info)) {
            HTMLVideoElement* element = static_cast<HTMLVideoElement*>(static_cast<JSHTMLVideoElement*>(o)->impl());
            context->texSubImage2D(target, level, xoff, yoff, element, flipY, premultiplyAlpha, ec);
#endif
        } else
            ec = TYPE_MISMATCH_ERR;
    } else {
        // This must be the WebGLArray form
        if (args.size() != 9)
            return throwError(exec, SyntaxError);

        unsigned width = args.at(4).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();
        
        unsigned height = args.at(5).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();
        
        unsigned format = args.at(6).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();
        
        unsigned type = args.at(7).toInt32(exec);
        if (exec->hadException())    
            return jsUndefined();
        
        JSValue value = args.at(8);
        if (!value.isObject())
            context->texSubImage2D(target, level, xoff, yoff, width, height, format, type, 0, ec);
        else {
            o = asObject(value);
        
            if (o->inherits(&JSWebGLArray::s_info)) {
                WebGLArray* obj = static_cast<WebGLArray*>(static_cast<JSWebGLArray*>(o)->impl());
                context->texSubImage2D(target, level, xoff, yoff, width, height, format, type, obj, ec);
            } else
                return throwError(exec, TypeError);
        }
    }
    
    setDOMException(exec, ec);
    return jsUndefined();
}

template<typename T, size_t inlineCapacity>
bool toVector(JSC::ExecState* exec, JSC::JSValue value, Vector<T, inlineCapacity>& vector)
{
    if (!value.isObject())
        return false;

    JSC::JSObject* object = asObject(value);
    int32_t length = object->get(exec, JSC::Identifier(exec, "length")).toInt32(exec);
    vector.resize(length);

    for (int32_t i = 0; i < length; ++i) {
        JSC::JSValue v = object->get(exec, i);
        if (exec->hadException())
            return false;
        vector[i] = static_cast<T>(v.toNumber(exec));
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
        break;
    default: break;
    }
    return false;
}

static JSC::JSValue dataFunctionf(DataFunctionToCall f, JSC::ExecState* exec, const JSC::ArgList& args, WebGLRenderingContext* context)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);
    
    WebGLUniformLocation* location = 0;
    long index = -1;
    
    if (functionForUniform(f))
        location = toWebGLUniformLocation(args.at(0));
    else
        index = args.at(0).toInt32(exec);

    if (exec->hadException())
        return jsUndefined();
        
    RefPtr<WebGLFloatArray> webGLArray = toWebGLFloatArray(args.at(1));
    if (exec->hadException())    
        return jsUndefined();
        
    ExceptionCode ec = 0;
    if (webGLArray) {
        switch (f) {
        case f_uniform1v:
            context->uniform1fv(location, webGLArray.get(), ec);
            break;
        case f_uniform2v:
            context->uniform2fv(location, webGLArray.get(), ec);
            break;
        case f_uniform3v:
            context->uniform3fv(location, webGLArray.get(), ec);
            break;
        case f_uniform4v:
            context->uniform4fv(location, webGLArray.get(), ec);
            break;
        case f_vertexAttrib1v:
            context->vertexAttrib1fv(index, webGLArray.get());
            break;
        case f_vertexAttrib2v:
            context->vertexAttrib2fv(index, webGLArray.get());
            break;
        case f_vertexAttrib3v:
            context->vertexAttrib3fv(index, webGLArray.get());
            break;
        case f_vertexAttrib4v:
            context->vertexAttrib4fv(index, webGLArray.get());
            break;
        }
        
        setDOMException(exec, ec);
        return jsUndefined();
    }

    Vector<float, 64> array;
    if (!toVector(exec, args.at(1), array))
        return throwError(exec, TypeError);

    switch (f) {
    case f_uniform1v:
        context->uniform1fv(location, array.data(), array.size(), ec);
        break;
    case f_uniform2v:
        context->uniform2fv(location, array.data(), array.size(), ec);
        break;
    case f_uniform3v:
        context->uniform3fv(location, array.data(), array.size(), ec);
        break;
    case f_uniform4v:
        context->uniform4fv(location, array.data(), array.size(), ec);
        break;
    case f_vertexAttrib1v:
        context->vertexAttrib1fv(index, array.data(), array.size());
        break;
    case f_vertexAttrib2v:
        context->vertexAttrib2fv(index, array.data(), array.size());
        break;
    case f_vertexAttrib3v:
        context->vertexAttrib3fv(index, array.data(), array.size());
        break;
    case f_vertexAttrib4v:
        context->vertexAttrib4fv(index, array.data(), array.size());
        break;
    }
    
    setDOMException(exec, ec);
    return jsUndefined();
}

static JSC::JSValue dataFunctioni(DataFunctionToCall f, JSC::ExecState* exec, const JSC::ArgList& args, WebGLRenderingContext* context)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    WebGLUniformLocation* location = toWebGLUniformLocation(args.at(0));
  
    if (exec->hadException())
        return jsUndefined();
        
    RefPtr<WebGLIntArray> webGLArray = toWebGLIntArray(args.at(1));
    if (exec->hadException())    
        return jsUndefined();
        
    ExceptionCode ec = 0;
    if (webGLArray) {
        switch (f) {
        case f_uniform1v:
            context->uniform1iv(location, webGLArray.get(), ec);
            break;
        case f_uniform2v:
            context->uniform2iv(location, webGLArray.get(), ec);
            break;
        case f_uniform3v:
            context->uniform3iv(location, webGLArray.get(), ec);
            break;
        case f_uniform4v:
            context->uniform4iv(location, webGLArray.get(), ec);
            break;
        default:
            break;
        }
        
        setDOMException(exec, ec);
        return jsUndefined();
    }


    Vector<int, 64> array;
    if (!toVector(exec, args.at(1), array))
        return throwError(exec, TypeError);

    switch (f) {
    case f_uniform1v:
        context->uniform1iv(location, array.data(), array.size(), ec);
        break;
    case f_uniform2v:
        context->uniform2iv(location, array.data(), array.size(), ec);
        break;
    case f_uniform3v:
        context->uniform3iv(location, array.data(), array.size(), ec);
        break;
    case f_uniform4v:
        context->uniform4iv(location, array.data(), array.size(), ec);
        break;
    default:
        break;
    }
    
    setDOMException(exec, ec);
    return jsUndefined();
}

static JSC::JSValue dataFunctionMatrix(DataFunctionMatrixToCall f, JSC::ExecState* exec, const JSC::ArgList& args, WebGLRenderingContext* context)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    WebGLUniformLocation* location = toWebGLUniformLocation(args.at(0));

    if (exec->hadException())    
        return jsUndefined();
        
    bool transpose = args.at(1).toBoolean(exec);
    if (exec->hadException())    
        return jsUndefined();
        
    RefPtr<WebGLFloatArray> webGLArray = toWebGLFloatArray(args.at(2));
    if (exec->hadException())    
        return jsUndefined();
        
    ExceptionCode ec = 0;
    if (webGLArray) {
        switch (f) {
        case f_uniformMatrix2fv:
            context->uniformMatrix2fv(location, transpose, webGLArray.get(), ec);
            break;
        case f_uniformMatrix3fv:
            context->uniformMatrix3fv(location, transpose, webGLArray.get(), ec);
            break;
        case f_uniformMatrix4fv:
            context->uniformMatrix4fv(location, transpose, webGLArray.get(), ec);
            break;
        }
        
        setDOMException(exec, ec);
        return jsUndefined();
    }

    Vector<float, 64> array;
    if (!toVector(exec, args.at(2), array))
        return throwError(exec, TypeError);

    switch (f) {
    case f_uniformMatrix2fv:
        context->uniformMatrix2fv(location, transpose, array.data(), array.size(), ec);
        break;
    case f_uniformMatrix3fv:
        context->uniformMatrix3fv(location, transpose, array.data(), array.size(), ec);
        break;
    case f_uniformMatrix4fv:
        context->uniformMatrix4fv(location, transpose, array.data(), array.size(), ec);
        break;
    }

    setDOMException(exec, ec);
    return jsUndefined();
}

JSC::JSValue JSWebGLRenderingContext::uniform1fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform1v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform1iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform1v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform2fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform2v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform2iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform2v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform3fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform3v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform3iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform3v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform4fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform4v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniform4iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform4v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniformMatrix2fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionMatrix(f_uniformMatrix2fv, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniformMatrix3fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionMatrix(f_uniformMatrix3fv, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::uniformMatrix4fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionMatrix(f_uniformMatrix4fv, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::vertexAttrib1fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib1v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::vertexAttrib2fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib2v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::vertexAttrib3fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib3v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

JSC::JSValue JSWebGLRenderingContext::vertexAttrib4fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib4v, exec, args, static_cast<WebGLRenderingContext*>(impl()));
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
