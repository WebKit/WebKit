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

#if ENABLE(3D_CANVAS)

#include "WebGLRenderingContext.h"

#include "ExceptionCode.h"

#include "NotImplemented.h"

#include <wtf/FastMalloc.h>

#include "V8Binding.h"
#include "V8WebGLArray.h"
#include "V8WebGLByteArray.h"
#include "V8WebGLFloatArray.h"
#include "V8WebGLIntArray.h"
#include "V8WebGLShortArray.h"
#include "V8WebGLUnsignedByteArray.h"
#include "V8WebGLUnsignedIntArray.h"
#include "V8WebGLUnsignedShortArray.h"
#include "V8HTMLImageElement.h"
#include "V8Proxy.h"

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

CALLBACK_FUNC_DECL(WebGLRenderingContextBufferData)
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

    WebGLRenderingContext* context =
        V8DOMWrapper::convertDOMWrapperToNative<WebGLRenderingContext>(args.Holder());
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
        context->bufferData(target, size, usage);
    } else if (V8WebGLArray::HasInstance(args[1])) {
        WebGLArray* array = V8DOMWrapper::convertToNativeObject<WebGLArray>(V8ClassIndex::CANVASARRAY, args[1]->ToObject());
        context->bufferData(target, array, usage);
    } else {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(WebGLRenderingContextBufferSubData)
{
    INC_STATS("DOM.WebGLRenderingContext.bufferSubData()");

    // Forms:
    // * bufferSubData(GLenum target, GLintptr offset, WebGLArray data);
    if (args.Length() != 3) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context =
        V8DOMWrapper::convertDOMWrapperToNative<WebGLRenderingContext>(args.Holder());
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
    WebGLArray* array = V8DOMWrapper::convertToNativeObject<WebGLArray>(V8ClassIndex::CANVASARRAY, args[2]->ToObject());
    context->bufferSubData(target, offset, array);
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(WebGLRenderingContextTexImage2D)
{
    INC_STATS("DOM.WebGLRenderingContext.texImage2D()");

    // Currently supported forms:
    // * void texImage2D(in GLenum target, in GLint level,
    //                   in GLint internalformat,
    //                   in GLsizei width, in GLsizei height, in GLint border,
    //                   in GLenum format, in GLenum type, in WebGLArray pixels);
    // * void texImage2D(in GLenum target, in GLint level, in HTMLImageElement image,
    //                   [Optional] in GLboolean flipY, [Optional] in GLboolean premultiplyAlpha);
    if (args.Length() != 3 &&
        args.Length() != 4 &&
        args.Length() != 5 &&
        args.Length() != 9) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context =
        V8DOMWrapper::convertDOMWrapperToNative<WebGLRenderingContext>(args.Holder());
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
        v8::Handle<v8::Value> arg = args[2];
        if (V8HTMLImageElement::HasInstance(arg)) {
            HTMLImageElement* image_element = V8DOMWrapper::convertDOMWrapperToNode<HTMLImageElement>(v8::Handle<v8::Object>::Cast(arg));
            bool flipY = false;
            bool premultiplyAlpha = false;
            if (args.Length() >= 4)
                flipY = args[3]->BooleanValue();
            if (args.Length() >= 5)
                premultiplyAlpha = args[4]->BooleanValue();
            context->texImage2D(target, level, image_element, flipY, premultiplyAlpha, ec);
        } else {
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
        if (V8WebGLArray::HasInstance(arg)) {
            WebGLArray* array = V8DOMWrapper::convertToNativeObject<WebGLArray>(V8ClassIndex::CANVASARRAY, arg->ToObject());
            // FIXME: must do validation similar to JOGL's to ensure that
            // the incoming array is of the appropriate length and type
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

CALLBACK_FUNC_DECL(WebGLRenderingContextTexSubImage2D)
{
    INC_STATS("DOM.WebGLRenderingContext.texSubImage2D()");

    // FIXME: implement
    notImplemented();

    return v8::Undefined();
}

enum FunctionToCall {
    kUniform1v, kUniform2v, kUniform3v, kUniform4v,
    kVertexAttrib1v, kVertexAttrib2v, kVertexAttrib3v, kVertexAttrib4v
};

static v8::Handle<v8::Value> vertexAttribAndUniformHelperf(const v8::Arguments& args,
                                                           FunctionToCall functionToCall) {
    // Forms:
    // * glUniform1fv(GLint location, Array data);
    // * glUniform1fv(GLint location, WebGLFloatArray data);
    // * glUniform2fv(GLint location, Array data);
    // * glUniform2fv(GLint location, WebGLFloatArray data);
    // * glUniform3fv(GLint location, Array data);
    // * glUniform3fv(GLint location, WebGLFloatArray data);
    // * glUniform4fv(GLint location, Array data);
    // * glUniform4fv(GLint location, WebGLFloatArray data);
    // * glVertexAttrib1fv(GLint location, Array data);
    // * glVertexAttrib1fv(GLint location, WebGLFloatArray data);
    // * glVertexAttrib2fv(GLint location, Array data);
    // * glVertexAttrib2fv(GLint location, WebGLFloatArray data);
    // * glVertexAttrib3fv(GLint location, Array data);
    // * glVertexAttrib3fv(GLint location, WebGLFloatArray data);
    // * glVertexAttrib4fv(GLint location, Array data);
    // * glVertexAttrib4fv(GLint location, WebGLFloatArray data);

    if (args.Length() != 3) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context =
        V8DOMWrapper::convertDOMWrapperToNative<WebGLRenderingContext>(args.Holder());
    bool ok;
    int location = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    if (V8WebGLFloatArray::HasInstance(args[1])) {
        WebGLFloatArray* array = 
            V8DOMWrapper::convertToNativeObject<WebGLFloatArray>(V8ClassIndex::CANVASFLOATARRAY, args[1]->ToObject());
        ASSERT(array != NULL);
        switch (functionToCall) {
            case kUniform1v: context->uniform1fv(location, array); break;
            case kUniform2v: context->uniform2fv(location, array); break;
            case kUniform3v: context->uniform3fv(location, array); break;
            case kUniform4v: context->uniform4fv(location, array); break;
            case kVertexAttrib1v: context->vertexAttrib1fv(location, array); break;
            case kVertexAttrib2v: context->vertexAttrib2fv(location, array); break;
            case kVertexAttrib3v: context->vertexAttrib3fv(location, array); break;
            case kVertexAttrib4v: context->vertexAttrib4fv(location, array); break;
            default: ASSERT_NOT_REACHED(); break;
        }
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
    switch (functionToCall) {
        case kUniform1v: context->uniform1fv(location, data, len); break;
        case kUniform2v: context->uniform2fv(location, data, len); break;
        case kUniform3v: context->uniform3fv(location, data, len); break;
        case kUniform4v: context->uniform4fv(location, data, len); break;
        case kVertexAttrib1v: context->vertexAttrib1fv(location, data, len); break;
        case kVertexAttrib2v: context->vertexAttrib2fv(location, data, len); break;
        case kVertexAttrib3v: context->vertexAttrib3fv(location, data, len); break;
        case kVertexAttrib4v: context->vertexAttrib4fv(location, data, len); break;
        default: ASSERT_NOT_REACHED(); break;
    }
    fastFree(data);
    return v8::Undefined();
}

static v8::Handle<v8::Value> uniformHelperi(const v8::Arguments& args,
                                            FunctionToCall functionToCall) {
    // Forms:
    // * glUniform1iv(GLint location, Array data);
    // * glUniform1iv(GLint location, WebGLIntArray data);
    // * glUniform2iv(GLint location, Array data);
    // * glUniform2iv(GLint location, WebGLIntArray data);
    // * glUniform3iv(GLint location, Array data);
    // * glUniform3iv(GLint location, WebGLIntArray data);
    // * glUniform4iv(GLint location, Array data);
    // * glUniform4iv(GLint location, WebGLIntArray data);

    if (args.Length() != 3) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    WebGLRenderingContext* context =
        V8DOMWrapper::convertDOMWrapperToNative<WebGLRenderingContext>(args.Holder());
    bool ok;
    int location = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    if (V8WebGLIntArray::HasInstance(args[1])) {
        WebGLIntArray* array = 
            V8DOMWrapper::convertToNativeObject<WebGLIntArray>(V8ClassIndex::CANVASINTARRAY, args[1]->ToObject());
        ASSERT(array != NULL);
        switch (functionToCall) {
            case kUniform1v: context->uniform1iv(location, array); break;
            case kUniform2v: context->uniform2iv(location, array); break;
            case kUniform3v: context->uniform3iv(location, array); break;
            case kUniform4v: context->uniform4iv(location, array); break;
            default: ASSERT_NOT_REACHED(); break;
        }
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
    switch (functionToCall) {
        case kUniform1v: context->uniform1iv(location, data, len); break;
        case kUniform2v: context->uniform2iv(location, data, len); break;
        case kUniform3v: context->uniform3iv(location, data, len); break;
        case kUniform4v: context->uniform4iv(location, data, len); break;
        default: ASSERT_NOT_REACHED(); break;
    }
    fastFree(data);
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniform1fv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform1fv()");
    return vertexAttribAndUniformHelperf(args, kUniform1v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniform1iv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform1iv()");
    return uniformHelperi(args, kUniform1v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniform2fv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform2fv()");
    return vertexAttribAndUniformHelperf(args, kUniform2v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniform2iv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform2iv()");
    return uniformHelperi(args, kUniform2v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniform3fv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform3fv()");
    return vertexAttribAndUniformHelperf(args, kUniform3v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniform3iv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform3iv()");
    return uniformHelperi(args, kUniform3v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniform4fv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniform4fv()");
    return vertexAttribAndUniformHelperf(args, kUniform4v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniform4iv)
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

    WebGLRenderingContext* context =
        V8DOMWrapper::convertDOMWrapperToNative<WebGLRenderingContext>(args.Holder());
    bool ok;
    int location = toInt32(args[0], ok);
    if (!ok) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }
    bool transpose = args[1]->BooleanValue();
    if (V8WebGLFloatArray::HasInstance(args[2])) {
        WebGLFloatArray* array = 
            V8DOMWrapper::convertToNativeObject<WebGLFloatArray>(V8ClassIndex::CANVASFLOATARRAY, args[2]->ToObject());
        ASSERT(array != NULL);
        switch (matrixSize) {
            case 2: context->uniformMatrix2fv(location, transpose, array); break;
            case 3: context->uniformMatrix3fv(location, transpose, array); break;
            case 4: context->uniformMatrix4fv(location, transpose, array); break;
            default: ASSERT_NOT_REACHED(); break;
        }
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
    switch (matrixSize) {
        case 2: context->uniformMatrix2fv(location, transpose, data, len); break;
        case 3: context->uniformMatrix3fv(location, transpose, data, len); break;
        case 4: context->uniformMatrix4fv(location, transpose, data, len); break;
        default: ASSERT_NOT_REACHED(); break;
    }
    fastFree(data);
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniformMatrix2fv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniformMatrix2fv()");
    return uniformMatrixHelper(args, 2);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniformMatrix3fv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniformMatrix3fv()");
    return uniformMatrixHelper(args, 3);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextUniformMatrix4fv)
{
    INC_STATS("DOM.WebGLRenderingContext.uniformMatrix4fv()");
    return uniformMatrixHelper(args, 4);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextVertexAttrib1fv)
{
    INC_STATS("DOM.WebGLRenderingContext.vertexAttrib1fv()");
    return vertexAttribAndUniformHelperf(args, kVertexAttrib1v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextVertexAttrib2fv)
{
    INC_STATS("DOM.WebGLRenderingContext.vertexAttrib2fv()");
    return vertexAttribAndUniformHelperf(args, kVertexAttrib2v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextVertexAttrib3fv)
{
    INC_STATS("DOM.WebGLRenderingContext.vertexAttrib3fv()");
    return vertexAttribAndUniformHelperf(args, kVertexAttrib3v);
}

CALLBACK_FUNC_DECL(WebGLRenderingContextVertexAttrib4fv)
{
    INC_STATS("DOM.WebGLRenderingContext.vertexAttrib4fv()");
    return vertexAttribAndUniformHelperf(args, kVertexAttrib4v);
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
