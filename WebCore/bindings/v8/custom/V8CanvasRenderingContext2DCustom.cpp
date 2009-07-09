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
#include "CanvasRenderingContext2D.h"

#include "CanvasGradient.h"
#include "CanvasPattern.h"
#include "CanvasStyle.h"
#include "ExceptionCode.h"
#include "FloatRect.h"

#include "V8Binding.h"
#include "V8CanvasGradient.h"
#include "V8CanvasPattern.h"
#include "V8CustomBinding.h"
#include "V8HTMLCanvasElement.h"
#include "V8HTMLImageElement.h"
#include "V8Proxy.h"

namespace WebCore {

static v8::Handle<v8::Value> toV8(CanvasStyle* style)
{
    if (style->canvasGradient())
        return V8DOMWrapper::convertToV8Object(V8ClassIndex::CANVASGRADIENT, style->canvasGradient());

    if (style->canvasPattern())
        return V8DOMWrapper::convertToV8Object(V8ClassIndex::CANVASPATTERN, style->canvasPattern());

    return v8String(style->color());
}

static PassRefPtr<CanvasStyle> toCanvasStyle(v8::Handle<v8::Value> value)
{
    if (value->IsString())
        return CanvasStyle::create(toWebCoreString(value));

    if (V8CanvasGradient::HasInstance(value))
        return CanvasStyle::create(V8DOMWrapper::convertDOMWrapperToNative<CanvasGradient>(value));

    if (V8CanvasPattern::HasInstance(value))
        return CanvasStyle::create(V8DOMWrapper::convertDOMWrapperToNative<CanvasPattern>(value));

    return 0;
}

ACCESSOR_GETTER(CanvasRenderingContext2DStrokeStyle)
{
    CanvasRenderingContext2D* impl = V8DOMWrapper::convertDOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
    return toV8(impl->strokeStyle());
}

ACCESSOR_SETTER(CanvasRenderingContext2DStrokeStyle)
{
    CanvasRenderingContext2D* impl = V8DOMWrapper::convertDOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
    impl->setStrokeStyle(toCanvasStyle(value));
}

ACCESSOR_GETTER(CanvasRenderingContext2DFillStyle)
{
    CanvasRenderingContext2D* impl = V8DOMWrapper::convertDOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
    return toV8(impl->fillStyle());
}

ACCESSOR_SETTER(CanvasRenderingContext2DFillStyle)
{
    CanvasRenderingContext2D* impl = V8DOMWrapper::convertDOMWrapperToNative<CanvasRenderingContext2D>(info.Holder());
    impl->setFillStyle(toCanvasStyle(value));
}

// TODO: SetStrokeColor and SetFillColor are similar except function names,
// consolidate them into one.
CALLBACK_FUNC_DECL(CanvasRenderingContext2DSetStrokeColor)
{
    INC_STATS("DOM.CanvasRenderingContext2D.setStrokeColor()");
    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());
    switch (args.Length()) {
    case 1:
        if (args[0]->IsString())
            context->setStrokeColor(toWebCoreString(args[0]));
        else
            context->setStrokeColor(toFloat(args[0]));
        break;
    case 2:
        if (args[0]->IsString())
            context->setStrokeColor(toWebCoreString(args[0]), toFloat(args[1]));
        else
            context->setStrokeColor(toFloat(args[0]), toFloat(args[1]));
        break;
    case 4:
        context->setStrokeColor(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]));
        break;
    case 5:
        context->setStrokeColor(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]));
        break;
    default:
        V8Proxy::throwError(V8Proxy::SyntaxError, "setStrokeColor: Invalid number of arguments");
        break;
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DSetFillColor)
{
    INC_STATS("DOM.CanvasRenderingContext2D.setFillColor()");
    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());
    switch (args.Length()) {
    case 1:
        if (args[0]->IsString())
            context->setFillColor(toWebCoreString(args[0]));
        else 
            context->setFillColor(toFloat(args[0]));
        break;
    case 2:
        if (args[0]->IsString())
            context->setFillColor(toWebCoreString(args[0]), toFloat(args[1]));
        else
            context->setFillColor(toFloat(args[0]), toFloat(args[1]));
        break;
    case 4:
        context->setFillColor(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]));
        break;
    case 5:
        context->setFillColor(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]));
        break;
    default:
        V8Proxy::throwError(V8Proxy::SyntaxError, "setFillColor: Invalid number of arguments");
        break;
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DStrokeRect)
{
    INC_STATS("DOM.CanvasRenderingContext2D.strokeRect()");
    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());
    if (args.Length() == 5)
        context->strokeRect(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]));
    else if (args.Length() == 4)
        context->strokeRect(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]));
    else {
        V8Proxy::setDOMException(INDEX_SIZE_ERR);
        return notHandledByInterceptor();
    }
    return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DSetShadow)
{
    INC_STATS("DOM.CanvasRenderingContext2D.setShadow()");
    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

    switch (args.Length()) {
    case 3:
        context->setShadow(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]));
        break;
    case 4:
        if (args[3]->IsString())
            context->setShadow(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toWebCoreString(args[3]));
        else
            context->setShadow(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]));
        break;
    case 5:
        if (args[3]->IsString())
            context->setShadow(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toWebCoreString(args[3]), toFloat(args[4]));
        else
            context->setShadow(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]));
        break;
    case 7:
        context->setShadow(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]), toFloat(args[5]), toFloat(args[6]));
        break;
    case 8:
        context->setShadow(toFloat(args[0]), toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]), toFloat(args[5]), toFloat(args[6]), toFloat(args[7]));
        break;
    default:
        V8Proxy::throwError(V8Proxy::SyntaxError, "setShadow: Invalid number of arguments");
        break;
    }

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DDrawImage)
{
    INC_STATS("DOM.CanvasRenderingContext2D.drawImage()");
    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

    v8::Handle<v8::Value> arg = args[0];

    if (V8HTMLImageElement::HasInstance(arg)) {
        ExceptionCode ec = 0;
        HTMLImageElement* image_element = V8DOMWrapper::convertDOMWrapperToNode<HTMLImageElement>(arg);
        switch (args.Length()) {
        case 3:
            context->drawImage(image_element, toFloat(args[1]), toFloat(args[2]));
            break;
        case 5:
            context->drawImage(image_element, toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]), ec);
            if (ec != 0) {
                V8Proxy::setDOMException(ec);
                return notHandledByInterceptor();
            }
            break;
        case 9:
            context->drawImage(image_element, 
                FloatRect(toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4])), 
                FloatRect(toFloat(args[5]), toFloat(args[6]), toFloat(args[7]), toFloat(args[8])),
                ec);
            if (ec != 0) {
                V8Proxy::setDOMException(ec);
                return notHandledByInterceptor();
            }
            break;
        default:
            V8Proxy::throwError(V8Proxy::SyntaxError, "drawImage: Invalid number of arguments");
            return v8::Undefined();
        }
        return v8::Undefined();
    }

    // HTMLCanvasElement
    if (V8HTMLCanvasElement::HasInstance(arg)) {
        ExceptionCode ec = 0;
        HTMLCanvasElement* canvas_element = V8DOMWrapper::convertDOMWrapperToNode<HTMLCanvasElement>(arg);
        switch (args.Length()) {
        case 3:
            context->drawImage(canvas_element, toFloat(args[1]), toFloat(args[2]));
            break;
        case 5:
            context->drawImage(canvas_element, toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]), ec);
            if (ec != 0) {
                V8Proxy::setDOMException(ec);
                return notHandledByInterceptor();
            }
            break;
        case 9:
            context->drawImage(canvas_element,
                FloatRect(toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4])),
                FloatRect(toFloat(args[5]), toFloat(args[6]), toFloat(args[7]), toFloat(args[8])),
                ec);
            if (ec != 0) {
                V8Proxy::setDOMException(ec);
                return notHandledByInterceptor();
            }
            break;
        default:
            V8Proxy::throwError(V8Proxy::SyntaxError, "drawImage: Invalid number of arguments");
            return v8::Undefined();
        }
        return v8::Undefined();
    }

    V8Proxy::setDOMException(TYPE_MISMATCH_ERR);
    return notHandledByInterceptor();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DDrawImageFromRect)
{
    INC_STATS("DOM.CanvasRenderingContext2D.drawImageFromRect()");
    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

    v8::Handle<v8::Value> arg = args[0];

    if (V8HTMLImageElement::HasInstance(arg)) {
        HTMLImageElement* image_element = V8DOMWrapper::convertDOMWrapperToNode<HTMLImageElement>(arg);
        context->drawImageFromRect(image_element,  toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]), toFloat(args[5]), toFloat(args[6]), toFloat(args[7]), toFloat(args[8]), toWebCoreString(args[9]));
    } else
        V8Proxy::throwError(V8Proxy::TypeError, "drawImageFromRect: Invalid type of arguments");

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DCreatePattern)
{
    INC_STATS("DOM.CanvasRenderingContext2D.createPattern()");
    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

    v8::Handle<v8::Value> arg = args[0];

    if (V8HTMLImageElement::HasInstance(arg)) {
        HTMLImageElement* image_element = V8DOMWrapper::convertDOMWrapperToNode<HTMLImageElement>(arg);
        ExceptionCode ec = 0;
        RefPtr<CanvasPattern> pattern = context->createPattern(image_element, toWebCoreStringWithNullCheck(args[1]), ec);
        if (ec != 0) {
            V8Proxy::setDOMException(ec);
            return notHandledByInterceptor();
        }
        return V8DOMWrapper::convertToV8Object(V8ClassIndex::CANVASPATTERN, pattern.get());
    }

    if (V8HTMLCanvasElement::HasInstance(arg)) {
        HTMLCanvasElement* canvas_element = V8DOMWrapper::convertDOMWrapperToNode<HTMLCanvasElement>(arg);
        ExceptionCode ec = 0;
        RefPtr<CanvasPattern> pattern = context->createPattern(canvas_element, toWebCoreStringWithNullCheck(args[1]), ec);
        if (ec != 0) {
            V8Proxy::setDOMException(ec);
            return notHandledByInterceptor();
        }
        return V8DOMWrapper::convertToV8Object(V8ClassIndex::CANVASPATTERN, pattern.get());
    }

    V8Proxy::setDOMException(TYPE_MISMATCH_ERR);
    return notHandledByInterceptor();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DFillText)
{
    INC_STATS("DOM.CanvasRenderingContext2D.fillText()");

    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

    // Two forms:
    // * fillText(text, x, y)
    // * fillText(text, x, y, maxWidth)
    if (args.Length() < 3 || args.Length() > 4) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    String text = toWebCoreString(args[0]);
    float x = toFloat(args[1]);
    float y = toFloat(args[2]);

    if (args.Length() == 4) {
        float maxWidth = toFloat(args[3]);
        context->fillText(text, x, y, maxWidth);
    } else
        context->fillText(text, x, y);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DStrokeText)
{
    INC_STATS("DOM.CanvasRenderingContext2D.strokeText()");
    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

    // Two forms:
    // * strokeText(text, x, y)
    // * strokeText(text, x, y, maxWidth)
    if (args.Length() < 3 || args.Length() > 4) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    String text = toWebCoreString(args[0]);
    float x = toFloat(args[1]);
    float y = toFloat(args[2]);

    if (args.Length() == 4) {
        float maxWidth = toFloat(args[3]);
        context->strokeText(text, x, y, maxWidth);
    } else
        context->strokeText(text, x, y);

    return v8::Undefined();
}

CALLBACK_FUNC_DECL(CanvasRenderingContext2DPutImageData)
{
    INC_STATS("DOM.CanvasRenderingContext2D.putImageData()");

    // Two froms:
    // * putImageData(ImageData, x, y)
    // * putImageData(ImageData, x, y, dirtyX, dirtyY, dirtyWidth, dirtyHeight)
    if (args.Length() != 3 && args.Length() != 7) {
        V8Proxy::setDOMException(SYNTAX_ERR);
        return notHandledByInterceptor();
    }

    CanvasRenderingContext2D* context = V8DOMWrapper::convertToNativeObject<CanvasRenderingContext2D>(V8ClassIndex::CANVASRENDERINGCONTEXT2D, args.Holder());

    ImageData* imageData = 0;

    // Need to check that the argument is of the correct type, since
    // convertToNativeObject() expects it to be correct. If the argument was incorrect
    // we leave it null, and putImageData() will throw the correct exception
    // (TYPE_MISMATCH_ERR).
    if (V8DOMWrapper::isWrapperOfType(args[0], V8ClassIndex::IMAGEDATA))
        imageData = V8DOMWrapper::convertToNativeObject<ImageData>(V8ClassIndex::IMAGEDATA, args[0]);

    ExceptionCode ec = 0;

    if (args.Length() == 7)
        context->putImageData(imageData, toFloat(args[1]), toFloat(args[2]), toFloat(args[3]), toFloat(args[4]), toFloat(args[5]), toFloat(args[6]), ec);
    else
        context->putImageData(imageData, toFloat(args[1]), toFloat(args[2]), ec);

    if (ec != 0) {
        V8Proxy::setDOMException(ec);
        return notHandledByInterceptor();
    }

    return v8::Undefined();
}

} // namespace WebCore
