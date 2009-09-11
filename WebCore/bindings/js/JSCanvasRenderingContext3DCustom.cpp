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
#include "JSCanvasArray.h"
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

} // namespace WebCore

#endif // ENABLE(3D_CANVAS)
