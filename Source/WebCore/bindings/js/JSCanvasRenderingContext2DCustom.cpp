/*
 * Copyright (C) 2006, 2007, 2009 Apple Inc. All rights reserved.
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
#include "HTMLCanvasElement.h"
#include "HTMLImageElement.h"
#include "ImageData.h"
#include "JSCanvasGradient.h"
#include "JSCanvasPattern.h"
#include "JSHTMLCanvasElement.h"
#include "JSHTMLImageElement.h"
#include "JSImageData.h"

using namespace JSC;

namespace WebCore {

bool JSCanvasRenderingContext2DOwner::isReachableFromOpaqueRoots(JSC::Handle<JSC::Unknown> handle, void*, SlotVisitor& visitor)
{
    JSCanvasRenderingContext2D* jsCanvasRenderingContext = jsCast<JSCanvasRenderingContext2D*>(handle.slot()->asCell());
    void* root = WebCore::root(jsCanvasRenderingContext->wrapped().canvas());
    return visitor.containsOpaqueRoot(root);
}

void JSCanvasRenderingContext2D::visitAdditionalChildren(SlotVisitor& visitor)
{
    visitor.addOpaqueRoot(root(wrapped().canvas()));
}

static JSValue toJS(ExecState* exec, JSDOMGlobalObject* globalObject, const CanvasStyle& style)
{
    if (style.canvasGradient())
        return toJS(exec, globalObject, style.canvasGradient());
    if (style.canvasPattern())
        return toJS(exec, globalObject, style.canvasPattern());
    return jsStringWithCache(exec, style.color());
}

static CanvasStyle toHTMLCanvasStyle(ExecState*, JSValue value)
{
    if (!value.isObject())
        return CanvasStyle();
    JSObject* object = asObject(value);
    if (object->inherits(JSCanvasGradient::info()))
        return CanvasStyle(&jsCast<JSCanvasGradient*>(object)->wrapped());
    if (object->inherits(JSCanvasPattern::info()))
        return CanvasStyle(&jsCast<JSCanvasPattern*>(object)->wrapped());
    return CanvasStyle();
}

JSValue JSCanvasRenderingContext2D::strokeStyle(ExecState& state) const
{
    return toJS(&state, globalObject(), wrapped().strokeStyle());
}

void JSCanvasRenderingContext2D::setStrokeStyle(ExecState& state, JSValue value)
{
    CanvasRenderingContext2D& context = wrapped();
    if (value.isString()) {
        context.setStrokeColor(asString(value)->value(&state));
        return;
    }
    context.setStrokeStyle(toHTMLCanvasStyle(&state, value));
}

JSValue JSCanvasRenderingContext2D::fillStyle(ExecState& state) const
{
    return toJS(&state, globalObject(), wrapped().fillStyle());
}

void JSCanvasRenderingContext2D::setFillStyle(ExecState& state, JSValue value)
{
    CanvasRenderingContext2D& context = wrapped();
    if (value.isString()) {
        context.setFillColor(asString(value)->value(&state));
        return;
    }
    context.setFillStyle(toHTMLCanvasStyle(&state, value));
}

} // namespace WebCore
