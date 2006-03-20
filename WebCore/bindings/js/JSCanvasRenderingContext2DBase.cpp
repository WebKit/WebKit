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
#include "JSCanvasRenderingContext2DBase.h"

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasRenderingContext2D.h"
#include "CanvasStyle.h"
#include "JSCanvasGradient.h"
#include "JSCanvasPattern.h"
#include "html_imageimpl.h"
#include "kjs_html.h"

#include "JSCanvasRenderingContext2DBaseTable.cpp"

using namespace KJS;

namespace WebCore {

/*
@begin JSCanvasRenderingContext2DBaseProtoTable 7
  setStrokeColor           WebCore::JSCanvasRenderingContext2DBase::SetStrokeColor              DontDelete|Function 1
  setFillColor             WebCore::JSCanvasRenderingContext2DBase::SetFillColor                DontDelete|Function 1
  strokeRect               WebCore::JSCanvasRenderingContext2DBase::StrokeRect                  DontDelete|Function 4
  drawImage                WebCore::JSCanvasRenderingContext2DBase::DrawImage                   DontDelete|Function 3
  drawImageFromRect        WebCore::JSCanvasRenderingContext2DBase::DrawImageFromRect           DontDelete|Function 10
  setShadow                WebCore::JSCanvasRenderingContext2DBase::SetShadow                   DontDelete|Function 3
  createPattern            WebCore::JSCanvasRenderingContext2DBase::CreatePattern               DontDelete|Function 2
@end
@begin JSCanvasRenderingContext2DBaseTable 2
  strokeStyle              WebCore::JSCanvasRenderingContext2DBase::StrokeStyle                 DontDelete
  fillStyle                WebCore::JSCanvasRenderingContext2DBase::FillStyle                   DontDelete
@end
*/

KJS_IMPLEMENT_PROTOFUNC(JSCanvasRenderingContext2DBaseProtoFunc)
KJS_IMPLEMENT_PROTOTYPE("CanvasRenderingContext2DBase", JSCanvasRenderingContext2DBaseProto, JSCanvasRenderingContext2DBaseProtoFunc)

JSValue* JSCanvasRenderingContext2DBaseProtoFunc::callAsFunction(ExecState* exec, JSObject* thisObj, const List& args)
{
    if (!thisObj->inherits(&JSCanvasRenderingContext2DBase::info))
        return throwError(exec, TypeError);

    CanvasRenderingContext2D* context = static_cast<JSCanvasRenderingContext2DBase*>(thisObj)->impl();
    
    switch (id) {
        case JSCanvasRenderingContext2DBase::SetStrokeColor:
            // string arg = named color
            // number arg = gray color
            // string arg, number arg = named color, alpha
            // number arg, number arg = gray color, alpha
            // 4 args = r, g, b, a
            // 5 args = c, m, y, k, a
            switch (args.size()) {
                case 1:
                    if (args[0]->isString())
                        context->setStrokeColor(args[0]->toString(exec).domString());
                    else
                        context->setStrokeColor(args[0]->toNumber(exec));
                    break;
                case 2:
                    if (args[0]->isString())
                        context->setStrokeColor(args[0]->toString(exec).domString(), args[1]->toNumber(exec));
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
            break;
        case JSCanvasRenderingContext2DBase::SetFillColor:
            // string arg = named color
            // number arg = gray color
            // string arg, number arg = named color, alpha
            // number arg, number arg = gray color, alpha
            // 4 args = r, g, b, a
            // 5 args = c, m, y, k, a
            switch (args.size()) {
                case 1:
                    if (args[0]->isString())
                        context->setFillColor(args[0]->toString(exec).domString());
                    else
                        context->setFillColor(args[0]->toNumber(exec));
                    break;
                case 2:
                    if (args[0]->isString())
                        context->setFillColor(args[0]->toString(exec).domString(), args[1]->toNumber(exec));
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
            break;
        case JSCanvasRenderingContext2DBase::StrokeRect:
            if (args.size() <= 4)
                context->strokeRect(args[0]->toNumber(exec), args[1]->toNumber(exec),
                    args[2]->toNumber(exec), args[3]->toNumber(exec));
            else
                context->strokeRect(args[0]->toNumber(exec), args[1]->toNumber(exec),
                    args[2]->toNumber(exec), args[3]->toNumber(exec), args[4]->toNumber(exec));
            break;
        case JSCanvasRenderingContext2DBase::SetShadow:
            switch (args.size()) {
                case 3:
                    context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                        args[2]->toNumber(exec));
                    break;
                case 4:
                    if (args[3]->isString())
                        context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                            args[2]->toNumber(exec), args[3]->toString(exec).domString());
                    else
                        context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                            args[2]->toNumber(exec), args[3]->toNumber(exec));
                    break;
                case 5:
                    if (args[3]->isString())
                        context->setShadow(args[0]->toNumber(exec), args[1]->toNumber(exec),
                            args[2]->toNumber(exec), args[3]->toString(exec).domString(),
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
            break;
        case JSCanvasRenderingContext2DBase::DrawImage: {
            // DrawImage has three variants:
            // drawImage(img, dx, dy)
            // drawImage(img, dx, dy, dw, dh)
            // drawImage(img, sx, sy, sw, sh, dx, dy, dw, dh)
            // composite operation is specified with globalCompositeOperation
            // img parameter can be a JavaScript Image, <img>, or a <canvas>
            JSObject* o = static_cast<JSObject*>(args[0]);
            if (!o->isObject())
                return throwError(exec, TypeError);
            if (!(o->inherits(&JSHTMLElement::img_info) || o->inherits(&JSHTMLElement::canvas_info)))
                return throwError(exec, TypeError);
            HTMLImageElement* imgElt = static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(args[0])->impl());
            switch (args.size()) {
                case 3:
                    context->drawImage(imgElt, args[1]->toNumber(exec), args[2]->toNumber(exec));
                    break;
                case 5:
                    context->drawImage(imgElt, args[1]->toNumber(exec), args[2]->toNumber(exec),
                        args[3]->toNumber(exec), args[4]->toNumber(exec));
                    break;
                case 9:
                    context->drawImage(imgElt, args[1]->toNumber(exec), args[2]->toNumber(exec),
                        args[3]->toNumber(exec), args[4]->toNumber(exec),
                        args[5]->toNumber(exec), args[6]->toNumber(exec),
                        args[7]->toNumber(exec), args[8]->toNumber(exec));
                    break;
                default:
                    return throwError(exec, SyntaxError);
            }
            break;
        }
        case JSCanvasRenderingContext2DBase::DrawImageFromRect: {
            JSObject* o = static_cast<JSObject*>(args[0]);
            if (!o->isObject())
                return throwError(exec, TypeError);
            if (!o->inherits(&JSHTMLElement::img_info))
                return throwError(exec, TypeError);
            context->drawImageFromRect(static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(args[0])->impl()),
                args[1]->toNumber(exec), args[2]->toNumber(exec),
                args[3]->toNumber(exec), args[4]->toNumber(exec),
                args[5]->toNumber(exec), args[6]->toNumber(exec),
                args[7]->toNumber(exec), args[8]->toNumber(exec),
                args[9]->toString(exec).domString());
            break;
        }
        case JSCanvasRenderingContext2DBase::CreatePattern:
            JSObject* o = static_cast<JSObject*>(args[0]);
            if (!o->isObject())
                return throwError(exec, TypeError);
            if (!o->inherits(&JSHTMLElement::img_info))
                return throwError(exec, TypeError);
            return toJS(exec,
                context->createPattern(static_cast<HTMLImageElement*>(static_cast<JSHTMLElement*>(args[0])->impl()),
                args[1]->toString(exec).domString()).get());
    }

    return jsUndefined();
}

const ClassInfo JSCanvasRenderingContext2DBase::info = { "CanvasRenderingContext2DBase", 0, &JSCanvasRenderingContext2DBaseTable, 0 };

bool JSCanvasRenderingContext2DBase::getOwnPropertySlot(ExecState* exec, const Identifier& propertyName, PropertySlot& slot)
{
    return getStaticValueSlot<JSCanvasRenderingContext2DBase, DOMObject>
        (exec, &JSCanvasRenderingContext2DBaseTable, this, propertyName, slot);
}

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
        return new CanvasStyle(value->toString(exec).domString());
    if (!value->isObject())
        return 0;
    JSObject* object = static_cast<JSObject*>(value);
    if (object->inherits(&JSCanvasGradient::info))
        return new CanvasStyle(static_cast<JSCanvasGradient*>(object)->impl());
    if (object->inherits(&JSCanvasPattern::info))
        return new CanvasStyle(static_cast<JSCanvasPattern*>(object)->impl());
    return 0;
}

JSValue* JSCanvasRenderingContext2DBase::getValueProperty(ExecState* exec, int token) const
{
    switch (token) {
        case StrokeStyle:
            return toJS(exec, m_impl->strokeStyle());        
        case FillStyle:
            return toJS(exec, m_impl->fillStyle());        
    }
    return 0;
}

void JSCanvasRenderingContext2DBase::put(ExecState* exec, const Identifier& propertyName, JSValue* value, int attr)
{
    lookupPut<JSCanvasRenderingContext2DBase, DOMObject>(exec, propertyName, value, attr, &JSCanvasRenderingContext2DBaseTable, this);
}

void JSCanvasRenderingContext2DBase::putValueProperty(ExecState* exec, int token, JSValue* value, int /*attr*/)
{
    switch (token) {
        case StrokeStyle:
            impl()->setStrokeStyle(toHTMLCanvasStyle(exec, value));
            break;
        case FillStyle:
            impl()->setFillStyle(toHTMLCanvasStyle(exec, value));
            break;
    }
}

JSCanvasRenderingContext2DBase::JSCanvasRenderingContext2DBase(ExecState*, PassRefPtr<WebCore::CanvasRenderingContext2D> impl)
    : m_impl(impl)
{
}

JSCanvasRenderingContext2DBase::~JSCanvasRenderingContext2DBase()
{
    ScriptInterpreter::forgetDOMObject(m_impl.get());
}

}
