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

#include "JSCanvasRenderingContext3D.h"

#include "CanvasRenderingContext3D.h"
#include "ExceptionCode.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "JSCanvasFloatArray.h"
#include "JSCanvasIntArray.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLImageElement.h"
#include "JSWebKitCSSMatrix.h"
#include <runtime/Error.h>
#include <wtf/FastMalloc.h>
#include <wtf/OwnFastMallocPtr.h>

using namespace JSC;

namespace WebCore {

JSValue JSCanvasRenderingContext3D::bufferData(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    unsigned target = args.at(0).toInt32(exec);
    unsigned usage = args.at(2).toInt32(exec);

    // If argument 1 is a number, we are initializing this buffer to that size
    if (!args.at(1).isObject()) {
        unsigned int count = args.at(1).toInt32(exec);
        static_cast<CanvasRenderingContext3D*>(impl())->bufferData(target, count, usage);
        return jsUndefined();
    }

    CanvasArray* array = toCanvasArray(args.at(1));
    
    static_cast<CanvasRenderingContext3D*>(impl())->bufferData(target, array, usage);
    return jsUndefined();
}

JSValue JSCanvasRenderingContext3D::bufferSubData(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    unsigned target = args.at(0).toInt32(exec);
    unsigned offset = args.at(1).toInt32(exec);
    
    CanvasArray* array = toCanvasArray(args.at(2));
    
    static_cast<CanvasRenderingContext3D*>(impl())->bufferSubData(target, offset, array);
    return jsUndefined();
}

// void texImage2DHTML(in unsigned long target, in unsigned long level, in HTMLImageElement image);
JSValue JSCanvasRenderingContext3D::texImage2D(ExecState* exec, const ArgList& args)
{ 
    if (args.size() < 3)
        return throwError(exec, SyntaxError);

    ExceptionCode ec = 0;
    CanvasRenderingContext3D* context = static_cast<CanvasRenderingContext3D*>(impl());    
    unsigned target = args.at(0).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
        
    unsigned level = args.at(1).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
    
    if (args.size() > 5) {
        // This must be the bare array case.
        if (args.size() != 9)
            return throwError(exec, SyntaxError);
            
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

        CanvasArray* array = toCanvasArray(args.at(8));
        if (exec->hadException())    
            return jsUndefined();
            
        if (!array)
            return throwError(exec, TypeError);
        
        // FIXME: Need to check to make sure CanvasArray is a CanvasByteArray or CanvasShortArray,
        // depending on the passed type parameter.
        
        context->texImage2D(target, level, internalformat, width, height, border, format, type, array, ec);
        return jsUndefined();
    }
    
    // The image parameter can be a <img> or <canvas> element.
    JSValue value = args.at(2);
    if (!value.isObject())
        return throwError(exec, TypeError);
    JSObject* o = asObject(value);
    
    bool flipY = (args.size() > 3) ? args.at(3).toBoolean(exec) : false;
    bool premultiplyAlpha = (args.size() > 4) ? args.at(3).toBoolean(exec) : false;
    
    if (o->inherits(&JSHTMLImageElement::s_info)) {
        HTMLImageElement* imgElt = static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texImage2D(target, level, imgElt, flipY, premultiplyAlpha, ec);
    } else if (o->inherits(&JSHTMLCanvasElement::s_info)) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texImage2D(target, level, canvas, flipY, premultiplyAlpha, ec);
    } else {
        setDOMException(exec, TYPE_MISMATCH_ERR);
    }
    
    return jsUndefined();    
}

// void texSubImage2DHTML(in unsigned long target, in unsigned long level, in unsigned long xoff, in unsigned long yoff, in unsigned long width, in unsigned long height, in HTMLImageElement image);
JSValue JSCanvasRenderingContext3D::texSubImage2D(ExecState* exec, const ArgList& args)
{ 
    if (args.size() < 7 || args.size() > 9)
        return throwError(exec, SyntaxError);

    CanvasRenderingContext3D* context = static_cast<CanvasRenderingContext3D*>(impl());    
    unsigned target = args.at(0).toInt32(exec);
    unsigned level = args.at(1).toInt32(exec);
    unsigned xoff = args.at(2).toInt32(exec);
    unsigned yoff = args.at(3).toInt32(exec);
    unsigned width = args.at(4).toInt32(exec);
    unsigned height = args.at(5).toInt32(exec);
    
    // The image parameter can be a <img> or <canvas> element.
    JSValue value = args.at(6);
    if (!value.isObject())
        return throwError(exec, TypeError);
    JSObject* o = asObject(value);
    
    bool flipY = (args.size() > 3) ? args.at(3).toBoolean(exec) : false;
    bool premultiplyAlpha = (args.size() > 4) ? args.at(3).toBoolean(exec) : false;
    
    ExceptionCode ec = 0;
    if (o->inherits(&JSHTMLImageElement::s_info)) {
        HTMLImageElement* imgElt = static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texSubImage2D(target, level, xoff, yoff, width, height, imgElt, flipY, premultiplyAlpha, ec);
    } else if (o->inherits(&JSHTMLCanvasElement::s_info)) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texSubImage2D(target, level, xoff, yoff, width, height, canvas, flipY, premultiplyAlpha, ec);
    } else {
        setDOMException(exec, TYPE_MISMATCH_ERR);
    }
    
    return jsUndefined();    
}

template<typename T>
void toArray(JSC::ExecState* exec, JSC::JSValue value, T*& array, int& size)
{
    array = 0;
    
    if (!value.isObject())
        return;
        
    JSC::JSObject* object = asObject(value);
    int length = object->get(exec, JSC::Identifier(exec, "length")).toInt32(exec);
    void* tempValues;
    if (!tryFastMalloc(length * sizeof(T)).getValue(tempValues))
        return;
    
    T* values = static_cast<T*>(tempValues);
    for (int i = 0; i < length; ++i) {
        JSC::JSValue v = object->get(exec, i);
        if (exec->hadException())
            return;
        values[i] = static_cast<T>(v.toNumber(exec));
    }

    array = values;
    size = length;
}

enum DataFunctionToCall {
    f_uniform1v, f_uniform2v, f_uniform3v, f_uniform4v,
    f_vertexAttrib1v, f_vertexAttrib2v, f_vertexAttrib3v, f_vertexAttrib4v
};

enum DataFunctionMatrixToCall {
    f_uniformMatrix2fv, f_uniformMatrix3fv, f_uniformMatrix4fv
};

static JSC::JSValue dataFunctionf(DataFunctionToCall f, JSC::ExecState* exec, const JSC::ArgList& args, CanvasRenderingContext3D* context)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    long location = args.at(0).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
        
    RefPtr<CanvasFloatArray> canvasArray = toCanvasFloatArray(args.at(1));
    if (exec->hadException())    
        return jsUndefined();
        
    if (canvasArray) {
        switch(f) {
            case f_uniform1v: context->uniform1fv(location, canvasArray.get()); break;
            case f_uniform2v: context->uniform2fv(location, canvasArray.get()); break;
            case f_uniform3v: context->uniform3fv(location, canvasArray.get()); break;
            case f_uniform4v: context->uniform4fv(location, canvasArray.get()); break;
            case f_vertexAttrib1v: context->vertexAttrib1fv(location, canvasArray.get()); break;
            case f_vertexAttrib2v: context->vertexAttrib2fv(location, canvasArray.get()); break;
            case f_vertexAttrib3v: context->vertexAttrib3fv(location, canvasArray.get()); break;
            case f_vertexAttrib4v: context->vertexAttrib4fv(location, canvasArray.get()); break;
        }
        return jsUndefined();
    }
    
    float* array;
    int size;
    toArray<float>(exec, args.at(1), array, size);
    
    if (!array)
        return throwError(exec, TypeError);

    switch(f) {
        case f_uniform1v: context->uniform1fv(location, array, size); break;
        case f_uniform2v: context->uniform2fv(location, array, size); break;
        case f_uniform3v: context->uniform3fv(location, array, size); break;
        case f_uniform4v: context->uniform4fv(location, array, size); break;
        case f_vertexAttrib1v: context->vertexAttrib1fv(location, array, size); break;
        case f_vertexAttrib2v: context->vertexAttrib2fv(location, array, size); break;
        case f_vertexAttrib3v: context->vertexAttrib3fv(location, array, size); break;
        case f_vertexAttrib4v: context->vertexAttrib4fv(location, array, size); break;
    }
    return jsUndefined();
}

static JSC::JSValue dataFunctioni(DataFunctionToCall f, JSC::ExecState* exec, const JSC::ArgList& args, CanvasRenderingContext3D* context)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    long location = args.at(0).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
        
    RefPtr<CanvasIntArray> canvasArray = toCanvasIntArray(args.at(1));
    if (exec->hadException())    
        return jsUndefined();
        
    if (canvasArray) {
        switch(f) {
            case f_uniform1v: context->uniform1iv(location, canvasArray.get()); break;
            case f_uniform2v: context->uniform2iv(location, canvasArray.get()); break;
            case f_uniform3v: context->uniform3iv(location, canvasArray.get()); break;
            case f_uniform4v: context->uniform4iv(location, canvasArray.get()); break;
            default: break;
        }
        return jsUndefined();
    }
    
    int* array;
    int size;
    toArray<int>(exec, args.at(1), array, size);
    
    if (!array)
        return throwError(exec, TypeError);

    switch(f) {
        case f_uniform1v: context->uniform1iv(location, array, size); break;
        case f_uniform2v: context->uniform2iv(location, array, size); break;
        case f_uniform3v: context->uniform3iv(location, array, size); break;
        case f_uniform4v: context->uniform4iv(location, array, size); break;
        default: break;
    }
    return jsUndefined();
}

static JSC::JSValue dataFunctionMatrix(DataFunctionMatrixToCall f, JSC::ExecState* exec, const JSC::ArgList& args, CanvasRenderingContext3D* context)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    long location = args.at(0).toInt32(exec);
    if (exec->hadException())    
        return jsUndefined();
        
    bool transpose = args.at(1).toBoolean(exec);
    if (exec->hadException())    
        return jsUndefined();
        
    RefPtr<CanvasFloatArray> canvasArray = toCanvasFloatArray(args.at(2));
    if (exec->hadException())    
        return jsUndefined();
        
    if (canvasArray) {
        switch(f) {
            case f_uniformMatrix2fv: context->uniformMatrix2fv(location, transpose, canvasArray.get()); break;
            case f_uniformMatrix3fv: context->uniformMatrix3fv(location, transpose, canvasArray.get()); break;
            case f_uniformMatrix4fv: context->uniformMatrix4fv(location, transpose, canvasArray.get()); break;
        }
        return jsUndefined();
    }
    
    float* array;
    int size;
    toArray<float>(exec, args.at(2), array, size);
    
    if (!array)
        return throwError(exec, TypeError);

    switch(f) {
        case f_uniformMatrix2fv: context->uniformMatrix2fv(location, transpose, array, size); break;
        case f_uniformMatrix3fv: context->uniformMatrix3fv(location, transpose, array, size); break;
        case f_uniformMatrix4fv: context->uniformMatrix4fv(location, transpose, array, size); break;
    }
    return jsUndefined();
}

JSC::JSValue JSCanvasRenderingContext3D::uniform1fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform1v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniform1iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform1v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniform2fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform2v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniform2iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform2v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniform3fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform3v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniform3iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform3v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniform4fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_uniform4v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniform4iv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctioni(f_uniform4v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniformMatrix2fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionMatrix(f_uniformMatrix2fv, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniformMatrix3fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionMatrix(f_uniformMatrix3fv, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::uniformMatrix4fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionMatrix(f_uniformMatrix4fv, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::vertexAttrib1fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib1v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::vertexAttrib2fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib2v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::vertexAttrib3fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib3v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

JSC::JSValue JSCanvasRenderingContext3D::vertexAttrib4fv(JSC::ExecState* exec, const JSC::ArgList& args)
{
    return dataFunctionf(f_vertexAttrib4v, exec, args, static_cast<CanvasRenderingContext3D*>(impl()));
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
