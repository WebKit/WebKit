/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "JSCanvasRenderingContext2D.h"

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasStyle.h"
#include "ExceptionCode.h"
#include "FloatRect.h"
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageData.h"
#include "JSCanvasGradient.h"
#include "JSCanvasPattern.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLImageElement.h"
#include "JSImageData.h"
#include <runtime/Error.h>

using namespace JSC;

namespace WebCore {

static JSValue toJS(ExecState* exec, CanvasStyle* style)
{
    if (style->canvasGradient())
        return toJS(exec, style->canvasGradient());
    if (style->canvasPattern())
        return toJS(exec, style->canvasPattern());
    return jsString(exec, style->color());
}

static PassRefPtr<CanvasStyle> toHTMLCanvasStyle(ExecState*, JSValue value)
{
    if (value.isString())
        return CanvasStyle::create(asString(value)->value());
    if (!value.isObject())
        return 0;
    JSObject* object = asObject(value);
    if (object->inherits(&JSCanvasGradient::s_info))
        return CanvasStyle::create(static_cast<JSCanvasGradient*>(object)->impl());
    if (object->inherits(&JSCanvasPattern::s_info))
        return CanvasStyle::create(static_cast<JSCanvasPattern*>(object)->impl());
    return 0;
}

JSValue JSCanvasRenderingContext2D::strokeStyle(ExecState* exec) const
{
    return toJS(exec, impl()->strokeStyle());        
}

void JSCanvasRenderingContext2D::setStrokeStyle(ExecState* exec, JSValue value)
{
    impl()->setStrokeStyle(toHTMLCanvasStyle(exec, value));
}

JSValue JSCanvasRenderingContext2D::fillStyle(ExecState* exec) const
{
    return toJS(exec, impl()->fillStyle());
}

void JSCanvasRenderingContext2D::setFillStyle(ExecState* exec, JSValue value)
{
    impl()->setFillStyle(toHTMLCanvasStyle(exec, value));
}

JSValue JSCanvasRenderingContext2D::setFillColor(ExecState* exec, const ArgList& args)
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
            if (args.at(0).isString())
                context->setFillColor(asString(args.at(0))->value());
            else
                context->setFillColor(args.at(0).toFloat(exec));
            break;
        case 2:
            if (args.at(0).isString())
                context->setFillColor(asString(args.at(0))->value(), args.at(1).toFloat(exec));
            else
                context->setFillColor(args.at(0).toFloat(exec), args.at(1).toFloat(exec));
            break;
        case 4:
            context->setFillColor(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                                  args.at(2).toFloat(exec), args.at(3).toFloat(exec));
            break;
        case 5:
            context->setFillColor(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                                  args.at(2).toFloat(exec), args.at(3).toFloat(exec), args.at(4).toFloat(exec));
            break;
        default:
            return throwError(exec, SyntaxError);
    }
    return jsUndefined();
}    

JSValue JSCanvasRenderingContext2D::setStrokeColor(ExecState* exec, const ArgList& args)
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
            if (args.at(0).isString())
                context->setStrokeColor(asString(args.at(0))->value());
            else
                context->setStrokeColor(args.at(0).toFloat(exec));
            break;
        case 2:
            if (args.at(0).isString())
                context->setStrokeColor(asString(args.at(0))->value(), args.at(1).toFloat(exec));
            else
                context->setStrokeColor(args.at(0).toFloat(exec), args.at(1).toFloat(exec));
            break;
        case 4:
            context->setStrokeColor(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                                    args.at(2).toFloat(exec), args.at(3).toFloat(exec));
            break;
        case 5:
            context->setStrokeColor(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                                    args.at(2).toFloat(exec), args.at(3).toFloat(exec), args.at(4).toFloat(exec));
            break;
        default:
            return throwError(exec, SyntaxError);
    }
    
    return jsUndefined();
}

JSValue JSCanvasRenderingContext2D::strokeRect(ExecState* exec, const ArgList& args)
{ 
    CanvasRenderingContext2D* context = impl();
    
    if (args.size() <= 4)
        context->strokeRect(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                            args.at(2).toFloat(exec), args.at(3).toFloat(exec));
    else
        context->strokeRect(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                            args.at(2).toFloat(exec), args.at(3).toFloat(exec), args.at(4).toFloat(exec));

    return jsUndefined();    
}

JSValue JSCanvasRenderingContext2D::drawImage(ExecState* exec, const ArgList& args)
{ 
    CanvasRenderingContext2D* context = impl();

    // DrawImage has three variants:
    //     drawImage(img, dx, dy)
    //     drawImage(img, dx, dy, dw, dh)
    //     drawImage(img, sx, sy, sw, sh, dx, dy, dw, dh)
    // Composite operation is specified with globalCompositeOperation.
    // The img parameter can be a <img> or <canvas> element.
    JSValue value = args.at(0);
    if (!value.isObject())
        return throwError(exec, TypeError);
    JSObject* o = asObject(value);
    
    ExceptionCode ec = 0;
    if (o->inherits(&JSHTMLImageElement::s_info)) {
        HTMLImageElement* imgElt = static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(o)->impl());
        switch (args.size()) {
            case 3:
                context->drawImage(imgElt, args.at(1).toFloat(exec), args.at(2).toFloat(exec));
                break;
            case 5:
                context->drawImage(imgElt, args.at(1).toFloat(exec), args.at(2).toFloat(exec),
                                   args.at(3).toFloat(exec), args.at(4).toFloat(exec), ec);
                setDOMException(exec, ec);
                break;
            case 9:
                context->drawImage(imgElt, FloatRect(args.at(1).toFloat(exec), args.at(2).toFloat(exec),
                                   args.at(3).toFloat(exec), args.at(4).toFloat(exec)),
                                   FloatRect(args.at(5).toFloat(exec), args.at(6).toFloat(exec),
                                   args.at(7).toFloat(exec), args.at(8).toFloat(exec)), ec);
                setDOMException(exec, ec);
                break;
            default:
                return throwError(exec, SyntaxError);
        }
    } else if (o->inherits(&JSHTMLCanvasElement::s_info)) {
        HTMLCanvasElement* canvas = static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(o)->impl());
        switch (args.size()) {
            case 3:
                context->drawImage(canvas, args.at(1).toFloat(exec), args.at(2).toFloat(exec));
                break;
            case 5:
                context->drawImage(canvas, args.at(1).toFloat(exec), args.at(2).toFloat(exec),
                                   args.at(3).toFloat(exec), args.at(4).toFloat(exec), ec);
                setDOMException(exec, ec);
                break;
            case 9:
                context->drawImage(canvas, FloatRect(args.at(1).toFloat(exec), args.at(2).toFloat(exec),
                                   args.at(3).toFloat(exec), args.at(4).toFloat(exec)),
                                   FloatRect(args.at(5).toFloat(exec), args.at(6).toFloat(exec),
                                   args.at(7).toFloat(exec), args.at(8).toFloat(exec)), ec);
                setDOMException(exec, ec);
                break;
            default:
                return throwError(exec, SyntaxError);
        }
    } else {
        setDOMException(exec, TYPE_MISMATCH_ERR);
    }
    
    return jsUndefined();    
}

JSValue JSCanvasRenderingContext2D::drawImageFromRect(ExecState* exec, const ArgList& args)
{ 
    CanvasRenderingContext2D* context = impl();
    
    JSValue value = args.at(0);
    if (!value.isObject())
        return throwError(exec, TypeError);
    JSObject* o = asObject(value);
    
    if (!o->inherits(&JSHTMLImageElement::s_info))
        return throwError(exec, TypeError);
    context->drawImageFromRect(static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(o)->impl()),
                               args.at(1).toFloat(exec), args.at(2).toFloat(exec),
                               args.at(3).toFloat(exec), args.at(4).toFloat(exec),
                               args.at(5).toFloat(exec), args.at(6).toFloat(exec),
                               args.at(7).toFloat(exec), args.at(8).toFloat(exec),
                               args.at(9).toString(exec));    
    return jsUndefined();    
}

JSValue JSCanvasRenderingContext2D::setShadow(ExecState* exec, const ArgList& args)
{ 
    CanvasRenderingContext2D* context = impl();

    switch (args.size()) {
        case 3:
            context->setShadow(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                               args.at(2).toFloat(exec));
            break;
        case 4:
            if (args.at(3).isString())
                context->setShadow(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                                   args.at(2).toFloat(exec), asString(args.at(3))->value());
            else
                context->setShadow(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                                   args.at(2).toFloat(exec), args.at(3).toFloat(exec));
            break;
        case 5:
            if (args.at(3).isString())
                context->setShadow(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                                   args.at(2).toFloat(exec), asString(args.at(3))->value(),
                                   args.at(4).toFloat(exec));
            else
                context->setShadow(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                                   args.at(2).toFloat(exec), args.at(3).toFloat(exec),
                                   args.at(4).toFloat(exec));
            break;
        case 7:
            context->setShadow(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                               args.at(2).toFloat(exec), args.at(3).toFloat(exec),
                               args.at(4).toFloat(exec), args.at(5).toFloat(exec),
                               args.at(6).toFloat(exec));
            break;
        case 8:
            context->setShadow(args.at(0).toFloat(exec), args.at(1).toFloat(exec),
                               args.at(2).toFloat(exec), args.at(3).toFloat(exec),
                               args.at(4).toFloat(exec), args.at(5).toFloat(exec),
                               args.at(6).toFloat(exec), args.at(7).toFloat(exec));
            break;
        default:
            return throwError(exec, SyntaxError);
    }
    
    return jsUndefined();    
}

JSValue JSCanvasRenderingContext2D::createPattern(ExecState* exec, const ArgList& args)
{ 
    CanvasRenderingContext2D* context = impl();

    JSValue value = args.at(0);
    if (!value.isObject())
        return throwError(exec, TypeError);
    JSObject* o = asObject(value);

    if (o->inherits(&JSHTMLImageElement::s_info)) {
        ExceptionCode ec;
        JSValue pattern = toJS(exec,
            context->createPattern(static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(o)->impl()),
                                   valueToStringWithNullCheck(exec, args.at(1)), ec).get());
        setDOMException(exec, ec);
        return pattern;
    }
    if (o->inherits(&JSHTMLCanvasElement::s_info)) {
        ExceptionCode ec;
        JSValue pattern = toJS(exec,
            context->createPattern(static_cast<HTMLCanvasElement*>(static_cast<JSHTMLElement*>(o)->impl()),
                valueToStringWithNullCheck(exec, args.at(1)), ec).get());
        setDOMException(exec, ec);
        return pattern;
    }
    setDOMException(exec, TYPE_MISMATCH_ERR);
    return jsUndefined();
}

JSValue JSCanvasRenderingContext2D::putImageData(ExecState* exec, const ArgList& args)
{
    // putImageData has two variants
    // putImageData(ImageData, x, y)
    // putImageData(ImageData, x, y, dirtyX, dirtyY, dirtyWidth, dirtyHeight)
    CanvasRenderingContext2D* context = impl();

    ExceptionCode ec = 0;
    if (args.size() >= 7)
        context->putImageData(toImageData(args.at(0)), args.at(1).toFloat(exec), args.at(2).toFloat(exec), 
                              args.at(3).toFloat(exec), args.at(4).toFloat(exec), args.at(5).toFloat(exec), args.at(6).toFloat(exec), ec);
    else
        context->putImageData(toImageData(args.at(0)), args.at(1).toFloat(exec), args.at(2).toFloat(exec), ec);

    setDOMException(exec, ec);
    return jsUndefined();
}

JSValue JSCanvasRenderingContext2D::fillText(ExecState* exec, const ArgList& args)
{ 
    CanvasRenderingContext2D* context = impl();

    // string arg = text to draw
    // number arg = x
    // number arg = y
    // optional number arg = maxWidth
    if (args.size() < 3 || args.size() > 4)
        return throwError(exec, SyntaxError);
    
    if (args.size() == 4)
        context->fillText(args.at(0).toString(exec), args.at(1).toFloat(exec), args.at(2).toFloat(exec), args.at(3).toFloat(exec));
    else
        context->fillText(args.at(0).toString(exec), args.at(1).toFloat(exec), args.at(2).toFloat(exec));
    return jsUndefined();
}

JSValue JSCanvasRenderingContext2D::strokeText(ExecState* exec, const ArgList& args)
{ 
    CanvasRenderingContext2D* context = impl();

    // string arg = text to draw
    // number arg = x
    // number arg = y
    // optional number arg = maxWidth
    if (args.size() < 3 || args.size() > 4)
        return throwError(exec, SyntaxError);
    
    if (args.size() == 4)
        context->strokeText(args.at(0).toString(exec), args.at(1).toFloat(exec), args.at(2).toFloat(exec), args.at(3).toFloat(exec));
    else
        context->strokeText(args.at(0).toString(exec), args.at(1).toFloat(exec), args.at(2).toFloat(exec));
    return jsUndefined();
}

} // namespace WebCore
