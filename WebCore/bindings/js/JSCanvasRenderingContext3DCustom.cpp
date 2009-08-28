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
#include "JSHTMLCanvasElement.h"
#include "JSHTMLImageElement.h"
#include "JSWebKitCSSMatrix.h"
#include <runtime/Error.h>
#include <wtf/FastMalloc.h>
#include <wtf/OwnFastMallocPtr.h>

using namespace JSC;

namespace WebCore {

static inline PassRefPtr<CanvasNumberArray> toCanvasNumberArray(JSC::ExecState* exec, const JSValue& value)
{
    if (!value.isObject())
        return 0;
        
    JSObject* array = asObject(value);
    int length = array->get(exec, Identifier(exec, "length")).toInt32(exec);
    RefPtr<CanvasNumberArray> numberArray = CanvasNumberArray::create(length);
    for (int i = 0; i < length; ++i) {
        JSValue v = array->get(exec, i);
        if (exec->hadException())
            return 0;
        float value = v.toFloat(exec);
        if (exec->hadException())
            return 0;
        numberArray->data()[i] = value;
    }
    
    return numberArray;
}

JSValue JSCanvasRenderingContext3D::bufferData(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    unsigned target = args.at(0).toInt32(exec);
    unsigned usage = args.at(2).toInt32(exec);
    
    RefPtr<CanvasNumberArray> numberArray = toCanvasNumberArray(exec, args.at(1));
    
    static_cast<CanvasRenderingContext3D*>(impl())->bufferData(target, numberArray.get(), usage);
    return jsUndefined();
}

JSValue JSCanvasRenderingContext3D::bufferSubData(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    unsigned target = args.at(0).toInt32(exec);
    unsigned offset = args.at(1).toInt32(exec);
    
    RefPtr<CanvasNumberArray> numberArray = toCanvasNumberArray(exec, args.at(2));
    
    static_cast<CanvasRenderingContext3D*>(impl())->bufferSubData(target, offset, numberArray.get());
    return jsUndefined();
}

JSValue JSCanvasRenderingContext3D::vertexAttrib(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 2)
        return throwError(exec, SyntaxError);

    unsigned indx = args.at(0).toInt32(exec);
    
    RefPtr<CanvasNumberArray> numberArray = toCanvasNumberArray(exec, args.at(1));
    
    static_cast<CanvasRenderingContext3D*>(impl())->vertexAttrib(indx, numberArray.get());
    return jsUndefined();
}

// void glVertexAttribPointer (in unsigned long indx, in long size, in unsigned long type, in boolean normalized, in unsigned long stride, in CanvasNumberArray array);
JSValue JSCanvasRenderingContext3D::vertexAttribPointer(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 6)
        return throwError(exec, SyntaxError);

    unsigned indx = args.at(0).toInt32(exec);
    unsigned size = args.at(1).toInt32(exec);
    unsigned type = args.at(2).toInt32(exec);
    unsigned normalized = args.at(3).toInt32(exec);
    bool stride = args.at(4).toBoolean(exec);
    
    RefPtr<CanvasNumberArray> numberArray = toCanvasNumberArray(exec, args.at(5));
    
    static_cast<CanvasRenderingContext3D*>(impl())->vertexAttribPointer(indx, size, type, normalized, stride, numberArray.get());
    return jsUndefined();
}

JSValue JSCanvasRenderingContext3D::uniformMatrix(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 4)
        return throwError(exec, SyntaxError);

    unsigned location = args.at(0).toInt32(exec);
    unsigned count = args.at(1).toInt32(exec);
    bool transpose = args.at(2).toBoolean(exec);
    
    if (!args.at(3).isObject())
        return jsUndefined();
    
    CanvasRenderingContext3D* context = static_cast<CanvasRenderingContext3D*>(impl());    
    WebKitCSSMatrix* matrix = toWebKitCSSMatrix(args.at(3));
    if (matrix)
        context->uniformMatrix(location, transpose, matrix);
    else {
        JSObject* array = asObject(args.at(3));
        int length = array->get(exec, Identifier(exec, "length")).toInt32(exec);
        if (length) {
            // If the first element is a WebKitCSSMatrix, handle as though all are. Otherwise handle as a number array
            WebKitCSSMatrix* matrix = toWebKitCSSMatrix(array->get(exec, 0));
            if (matrix) {
                Vector<WebKitCSSMatrix*> matrixArray(length);
                for (int i = 0; i < length; ++i) {
                    WebKitCSSMatrix* matrix = toWebKitCSSMatrix(array->get(exec, i));
                    matrixArray[i] = matrix;
                }

                // FIXME: The array may have null entries. We currently don't check for this in uniformMatrix
                context->uniformMatrix(location, transpose, matrixArray);
            } else {
                RefPtr<CanvasNumberArray> numberArray = CanvasNumberArray::create(length);
                for (int i = 0; i < length; ++i) {
                    float value = array->get(exec, i).toFloat(exec);
                    numberArray->data()[i] = value;
                }
                context->uniformMatrix(location, count, transpose, numberArray.get());
            }
        }
    }

    return jsUndefined();
}

JSValue JSCanvasRenderingContext3D::uniformf(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError);

    unsigned location = args.at(0).toInt32(exec);
    
    CanvasRenderingContext3D* context = static_cast<CanvasRenderingContext3D*>(impl());    
    switch (args.size()) {
        case 2:
            if (args.at(1).isObject()) {
                RefPtr<CanvasNumberArray> numberArray = toCanvasNumberArray(exec, args.at(1));
                context->uniform(location, numberArray.get());
            }
            else
                context->uniform(location, static_cast<float>(args.at(1).toNumber(exec)));
            break;
        case 3: 
            context->uniform(location, static_cast<float>(args.at(1).toNumber(exec)),
                                        static_cast<float>(args.at(2).toNumber(exec))); 
            break;
        case 4: 
            context->uniform(location, static_cast<float>(args.at(1).toNumber(exec)),
                                        static_cast<float>(args.at(2).toNumber(exec)),
                                        static_cast<float>(args.at(3).toNumber(exec))); 
            break;
        case 5: 
            context->uniform(location, static_cast<float>(args.at(1).toNumber(exec)),
                                        static_cast<float>(args.at(2).toNumber(exec)),
                                        static_cast<float>(args.at(3).toNumber(exec)),
                                        static_cast<float>(args.at(4).toNumber(exec))); 
            break;
        default:
            return throwError(exec, SyntaxError);
    }
    
    return jsUndefined();
}

JSValue JSCanvasRenderingContext3D::uniformi(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() < 1)
        return throwError(exec, SyntaxError);

    unsigned location = args.at(0).toInt32(exec);
    
    CanvasRenderingContext3D* context = static_cast<CanvasRenderingContext3D*>(impl());    
    switch(args.size()) {
        case 2:
            context->uniform(location, args.at(1).toInt32(exec));
            break;
        case 3: 
            context->uniform(location, args.at(1).toInt32(exec),
                                        args.at(2).toInt32(exec)); 
            break;
        case 4: 
            context->uniform(location, args.at(1).toInt32(exec),
                                        args.at(2).toInt32(exec),
                                        args.at(3).toInt32(exec)); 
            break;
        case 5: 
            context->uniform(location, args.at(1).toInt32(exec),
                                        args.at(2).toInt32(exec),
                                        args.at(3).toInt32(exec),
                                        args.at(4).toInt32(exec)); 
            break;
        default:
            if (args.at(1).isObject()) {
                RefPtr<CanvasNumberArray> numberArray = toCanvasNumberArray(exec, args.at(1));
                context->uniform(location, numberArray.get());
            }
            break;
    }
    
    return jsUndefined();
}

JSValue JSCanvasRenderingContext3D::texParameter(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    unsigned target = args.at(0).toInt32(exec);
    unsigned pname = args.at(1).toInt32(exec);
    
    CanvasRenderingContext3D* context = static_cast<CanvasRenderingContext3D*>(impl());    
    if (args.at(2).isObject()) {
        RefPtr<CanvasNumberArray> numberArray = toCanvasNumberArray(exec, args.at(2));
        context->texParameter(target, pname, numberArray.get());
    } else {
        double value = args.at(2).toNumber(exec);
        context->texParameter(target, pname, value);
    }
    
    return jsUndefined();
}

JSValue JSCanvasRenderingContext3D::drawElements(JSC::ExecState* exec, JSC::ArgList const& args)
{
    if (args.size() < 3)
        return throwError(exec, SyntaxError);

    unsigned mode = args.at(0).toInt32(exec);
    unsigned type = args.at(1).toInt32(exec);
    
    // If the third param is not an object, it is a number, which is the count.
    // In this case if there is a 4th param, it is the offset. If there is no
    // 4th param, the offset is 0
    CanvasRenderingContext3D* context = static_cast<CanvasRenderingContext3D*>(impl());    
    if (!args.at(2).isObject()) {
        if (args.size() > 4)
            return throwError(exec, SyntaxError);
            
        unsigned int count = args.at(2).toInt32(exec);
        unsigned int offset = (args.size() == 4) ? args.at(3).toInt32(exec) : 0;
        context->drawElements(mode, count, type, (void*) offset);
        return jsUndefined();
    }
    
    // 3rd param is an object. Treat it as an array
    if (args.size() != 3)
        return throwError(exec, SyntaxError);
        
    if (type != GL_UNSIGNED_BYTE && type != GL_UNSIGNED_SHORT)
         return throwError(exec, TypeError);
         
    JSObject* array = asObject(args.at(2));
    unsigned int count = array->get(exec, Identifier(exec, "length")).toInt32(exec);
    size_t size = count * ((type == GL_UNSIGNED_BYTE) ? sizeof(unsigned char) : sizeof(unsigned short));
    
    void* tempIndices;
    if (!tryFastMalloc(size).getValue(tempIndices))
        return throwError(exec, GeneralError);

    OwnFastMallocPtr<void> passedIndices(tempIndices);

    if (type == GL_UNSIGNED_BYTE) {
        unsigned char* indices = static_cast<unsigned char*>(passedIndices.get());
        for (unsigned int i = 0; i < count; ++i) {
            unsigned short value = static_cast<unsigned char>(array->get(exec, i).toUInt32(exec));
            indices[i] = value;
        }
    } else {
        unsigned short* indices = static_cast<unsigned short*>(passedIndices.get());
        for (unsigned int i = 0; i < count; ++i) {
            unsigned short value = static_cast<unsigned short>(array->get(exec, i).toUInt32(exec));
            indices[i] = value;
        }
    }
        
    context->drawElements(mode, count, type, passedIndices.get());
    return jsUndefined();
}

// void texImage2DHTML(in unsigned long target, in unsigned long level, in HTMLImageElement image);
JSValue JSCanvasRenderingContext3D::texImage2D(ExecState* exec, const ArgList& args)
{ 
    if (args.size() != 3)
        return throwError(exec, SyntaxError);

    CanvasRenderingContext3D* context = static_cast<CanvasRenderingContext3D*>(impl());    
    unsigned target = args.at(0).toInt32(exec);
    unsigned level = args.at(1).toInt32(exec);
    
    // The image parameter can be a <img> or <canvas> element.
    JSValue value = args.at(2);
    if (!value.isObject())
        return throwError(exec, TypeError);
    JSObject* o = asObject(value);
    
    ExceptionCode ec = 0;
    if (o->inherits(&JSHTMLImageElement::s_info)) {
        HTMLImageElement* imgElt = static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texImage2D(target, level, imgElt, ec);
    } else if (o->inherits(&JSHTMLCanvasElement::s_info)) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texImage2D(target, level, canvas, ec);
    } else {
        setDOMException(exec, TYPE_MISMATCH_ERR);
    }
    
    return jsUndefined();    
}

// void texSubImage2DHTML(in unsigned long target, in unsigned long level, in unsigned long xoff, in unsigned long yoff, in unsigned long width, in unsigned long height, in HTMLImageElement image);
JSValue JSCanvasRenderingContext3D::texSubImage2D(ExecState* exec, const ArgList& args)
{ 
    if (args.size() != 7)
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
    
    ExceptionCode ec = 0;
    if (o->inherits(&JSHTMLImageElement::s_info)) {
        HTMLImageElement* imgElt = static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texSubImage2D(target, level, xoff, yoff, width, height, imgElt, ec);
    } else if (o->inherits(&JSHTMLCanvasElement::s_info)) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(o)->impl());
        context->texSubImage2D(target, level, xoff, yoff, width, height, canvas, ec);
    } else {
        setDOMException(exec, TYPE_MISMATCH_ERR);
    }
    
    return jsUndefined();    
}

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
