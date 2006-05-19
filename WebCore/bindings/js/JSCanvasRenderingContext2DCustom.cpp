/*
 * Copyright (C) 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */
#include "config.h"

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasStyle.h"
#include "ExceptionCode.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "JSCanvasGradient.h"
#include "JSCanvasPattern.h"
#include "JSCanvasRenderingContext2D.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLImageElement.h"
#include "kjs_html.h"

using namespace KJS;

namespace WebCore {

static JSValue* toJS(ExecState* exec, CanvasStyle* style)
{
    if (style->gradient())
        return toJS(exec, style->gradient());
    if (style->pattern())
        return toJS(exec, style->pattern());
    return jsString(style->color());
}

static PassRefPtr<CanvasStyle> toHTMLCanvasStyle(ExecState* exec, JSValue* value)
{
    if (value->isString())
        return new CanvasStyle(value->toString(exec));
    if (!value->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(value);
    if (object->inherits(&JSCanvasGradient::info))
        return new CanvasStyle(static_cast<JSCanvasGradient*>(object)->impl());
    if (object->inherits(&JSCanvasPattern::info))
        return new CanvasStyle(static_cast<JSCanvasPattern*>(object)->impl());
    return 0;
}

JSValue* JSCanvasRenderingContext2D::strokeStyle(ExecState* exec) const
{
    return toJS(exec, impl()->strokeStyle());        
}

void JSCanvasRenderingContext2D::setStrokeStyle(ExecState* exec, JSValue* value)
{
    impl()->setStrokeStyle(toHTMLCanvasStyle(exec, value));
}

JSValue* JSCanvasRenderingContext2D::fillStyle(ExecState* exec) const
{
    return toJS(exec, impl()->fillStyle());
}

void JSCanvasRenderingContext2D::setFillStyle(ExecState* exec, JSValue* value)
{
    impl()->setFillStyle(toHTMLCanvasStyle(exec, value));
}

JSValue* JSCanvasRenderingContext2D::setFillColor(ExecState* exec, const List& args)
{
    CanvasRenderingContext2D* context = impl();

    // string arg = named color
    // number arg = gray color
    // string arg, number arg = named color, alpha
    // number arg, number arg = gray color, alpha
    // 4 args = r, g, b, a
    // 5 args = c, m, y, k, a
    switch (args.size()) {
        case 1:
            if (args[0]->isString())
                context->setFillColor(args[0]->toString(exec));
            else
                context->setFillColor(args[0]->toNumber(exec));
            break;
        case 2:
            if (args[0]->isString())
                context->setFillColor(args[0]->toString(exec), args[1]->toNumber(exec));
            else
                context->setFillColor(args[0]->toNumber(exec), args[1]->toNumber(exec));
            break;
        case 4:
            context->setFillColor(args[0]->toNumber(exec), args[1]->toNumber(exec),
                                  args[2]->toNumber(exec), args[3]->toNumber(exec));
            break;
        case 5:
            context->setFillColor(args[0]->toNumber(exec), args[1]->toNumber(exec),
                                  args[2]->toNumber(exec), args[3]->toNumber(exec), args[4]->toNumber(exec));
            break;
        default:
            return throwError(exec, SyntaxError);
    }
    return jsUndefined();
}    

JSValue* JSCanvasRenderingContext2D::setStrokeColor(ExecState* exec, const List& args)
{ 
    CanvasRenderingContext2D* context = impl();

    // string arg = named color
    // number arg = gray color
    // string arg, number arg = named color, alpha
    // number arg, number arg = gray color, alpha
    // 4 args = r, g, b, a
    // 5 args = c, m, y, k, a
    switch (args.size()) {
        case 1:
            if (args[0]->isString())
                context->setStrokeColor(args[0]->toString(exec));
            else
                context->setStrokeColor(args[0]->toNumber(exec));
            break;
        case 2:
            if (args[0]->isString())
                context->setStrokeColor(args[0]->toString(exec), args[1]->toNumber(exec));
            else
                context->setStrokeColor(args[0]->toNumber(exec), args[1]->toNumber(exec));
            break;
        case 4:
            context->setStrokeColor(args[0]->toNumber(exec), args[1]->toNumber(exec),
                                    args[2]->toNumber(exec), args[3]->toNumber(exec));
            break;
        case 5:
            context->setStrokeColor(args[0]->toNumber(exec), args[1]->toNumber(exec),
                                    args[2]->toNumber(exec), args[3]->toNumber(exec), args[4]->toNumber(exec));
            break;
        default:
            return throwError(exec, SyntaxError);
    }
    
    return jsUndefined();
}

JSValue* JSCanvasRenderingContext2D::strokeRect(ExecState* exec, const List& args)
{ 
    CanvasRenderingContext2D* context = impl();    
    ExceptionCode ec;
    
    if (args.size() <= 4)
        context->strokeRect(args[0]->toNumber(exec), args[1]->toNumber(exec),
                            args[2]->toNumber(exec), args[3]->toNumber(exec), ec);
    else
        context->strokeRect(args[0]->toNumber(exec), args[1]->toNumber(exec),
                            args[2]->toNumber(exec), args[3]->toNumber(exec), args[4]->toNumber(exec), ec);
    setDOMException(exec, ec);
    
    return jsUndefined();    
}

JSValue* JSCanvasRenderingContext2D::drawImage(ExecState* exec, const List& args)
{ 
    CanvasRenderingContext2D* context = impl();

    // DrawImage has three variants:
    //     drawImage(img, dx, dy)
    //     drawImage(img, dx, dy, dw, dh)
    //     drawImage(img, sx, sy, sw, sh, dx, dy, dw, dh)
    // Composite operation is specified with globalCompositeOperation.
    // The img parameter can be a <img> or <canvas> element.
    JSObject* o = static_cast<JSObject*>(args[0]);
    if (!o->isObject())
        return throwError(exec, TypeError);
    ExceptionCode ec;
    if (o->inherits(&JSHTMLImageElement::info)) {
        HTMLImageElement* imgElt = static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(args[0])->impl());
        switch (args.size()) {
            case 3:
                context->drawImage(imgElt, args[1]->toNumber(exec), args[2]->toNumber(exec));
                break;
            case 5:
                context->drawImage(imgElt, args[1]->toNumber(exec), args[2]->toNumber(exec),
                                   args[3]->toNumber(exec), args[4]->toNumber(exec), ec);
                setDOMException(exec, ec);
                break;
            case 9:
                context->drawImage(imgElt, args[1]->toNumber(exec), args[2]->toNumber(exec),
                                   args[3]->toNumber(exec), args[4]->toNumber(exec),
                                   args[5]->toNumber(exec), args[6]->toNumber(exec),
                                   args[7]->toNumber(exec), args[8]->toNumber(exec), ec);
                setDOMException(exec, ec);
                break;
            default:
                return throwError(exec, SyntaxError);
        }
    } else if (o->inherits(&JSHTMLCanvasElement::info)) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(args[0])->impl());
        switch (args.size()) {
            case 3:
                context->drawImage(canvas, args[1]->toNumber(exec), args[2]->toNumber(exec));
                break;
            case 5:
                context->drawImage(canvas, args[1]->toNumber(exec), args[2]->toNumber(exec),
                                   args[3]->toNumber(exec), args[4]->toNumber(exec), ec);
                setDOMException(exec, ec);
                break;
            case 9:
                context->drawImage(canvas, args[1]->toNumber(exec), args[2]->toNumber(exec),
                                   args[3]->toNumber(exec), args[4]->toNumber(exec),
                                   args[5]->toNumber(exec), args[6]->toNumber(exec),
                                   args[7]->toNumber(exec), args[8]->toNumber(exec), ec);
                setDOMException(exec, ec);
                break;
            default:
                return throwError(exec, SyntaxError);
        }
    } else {
        setDOMException(exec, TYPE_MISMATCH_ERR);
        return 0;
    }
    
    return jsUndefined();    
}

JSValue* JSCanvasRenderingContext2D::drawImageFromRect(ExecState* exec, const List& args)
{ 
    CanvasRenderingContext2D* context = impl();

    JSObject* o = static_cast<JSObject*>(args[0]);
    if (!o->isObject())
        return throwError(exec, TypeError);
    if (!o->inherits(&JSHTMLImageElement::info))
        return throwError(exec, TypeError);
    context->drawImageFromRect(static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(args[0])->impl()),
                               args[1]->toNumber(exec), args[2]->toNumber(exec),
                               args[3]->toNumber(exec), args[4]->toNumber(exec),
                               args[5]->toNumber(exec), args[6]->toNumber(exec),
                               args[7]->toNumber(exec), args[8]->toNumber(exec),
                               args[9]->toString(exec));    
    return jsUndefined();    
}

JSValue* JSCanvasRenderingContext2D::setShadow(ExecState* exec, const List& args)
{ 
    CanvasRenderingContext2D* context = impl();

    switch (args.size()) {
        case 3:
            context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                               args[2]->toNumber(exec));
            break;
        case 4:
            if (args[3]->isString())
                context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                                   args[2]->toNumber(exec), args[3]->toString(exec));
            else
                context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                                   args[2]->toNumber(exec), args[3]->toNumber(exec));
            break;
        case 5:
            if (args[3]->isString())
                context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                                   args[2]->toNumber(exec), args[3]->toString(exec),
                                   args[4]->toNumber(exec));
            else
                context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                                   args[2]->toNumber(exec), args[3]->toNumber(exec),
                                   args[4]->toNumber(exec));
            break;
        case 7:
            context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                               args[2]->toNumber(exec), args[3]->toNumber(exec),
                               args[4]->toNumber(exec), args[5]->toNumber(exec),
                               args[6]->toNumber(exec));
            break;
        case 8:
            context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                               args[2]->toNumber(exec), args[3]->toNumber(exec),
                               args[4]->toNumber(exec), args[5]->toNumber(exec),
                               args[6]->toNumber(exec), args[7]->toNumber(exec));
            break;
        default:
            return throwError(exec, SyntaxError);
    }
    
    return jsUndefined();    
}

JSValue* JSCanvasRenderingContext2D::createPattern(ExecState* exec, const List& args)
{ 
    CanvasRenderingContext2D* context = impl();

    JSObject* o = static_cast<JSObject*>(args[0]);
    if (!o->isObject())
        return throwError(exec, TypeError);
    if (o->inherits(&JSHTMLImageElement::info)) {
        ExceptionCode ec;
        JSValue* pattern = toJS(exec,
            context->createPattern(static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(args[0])->impl()),
                args[1]->toString(exec), ec).get());
        setDOMException(exec, ec);
        return pattern;
    }
    if (o->inherits(&JSHTMLCanvasElement::info)) {
        ExceptionCode ec;
        JSValue* pattern = toJS(exec,
            context->createPattern(static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(args[0])->impl()),
                args[1]->toString(exec), ec).get());
        setDOMException(exec, ec);
        return pattern;
    }
    setDOMException(exec, TYPE_MISMATCH_ERR);
    return 0;
}

}