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
 
#include <config.h>

#if ENABLE(SVG)
#include "V8SVGLength.h"

#include "ExceptionCode.h"
#include "SVGLengthContext.h"
#include "SVGPropertyTearOff.h"
#include "V8Binding.h"
#include "V8BindingMacros.h"

namespace WebCore {

v8::Handle<v8::Value> V8SVGLength::valueAccessorGetter(v8::Local<v8::String> name, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.SVGLength.value._get");
    SVGPropertyTearOff<SVGLength>* wrapper = V8SVGLength::toNative(info.Holder());
    SVGLength& imp = wrapper->propertyReference();
    ExceptionCode ec = 0;
    SVGLengthContext lengthContext(wrapper->contextElement());
    float value = imp.value(lengthContext, ec);
    if (UNLIKELY(ec)) {
        V8Proxy::setDOMException(ec, info.GetIsolate());
        return v8::Handle<v8::Value>();
    }
    return v8::Number::New(value);
}

void V8SVGLength::valueAccessorSetter(v8::Local<v8::String> name, v8::Local<v8::Value> value, const v8::AccessorInfo& info)
{
    INC_STATS("DOM.SVGLength.value._set");
    SVGPropertyTearOff<SVGLength>* wrapper = V8SVGLength::toNative(info.Holder());
    if (wrapper->role() == AnimValRole) {
        V8Proxy::setDOMException(NO_MODIFICATION_ALLOWED_ERR, info.GetIsolate());
        return;
    }

    if (!isUndefinedOrNull(value) && !value->IsNumber() && !value->IsBoolean()) {
        V8Proxy::throwTypeError();
        return;
    }

    SVGLength& imp = wrapper->propertyReference();
    ExceptionCode ec = 0;
    SVGLengthContext lengthContext(wrapper->contextElement());
    imp.setValue(static_cast<float>(value->NumberValue()), lengthContext, ec);
    if (UNLIKELY(ec))
        V8Proxy::setDOMException(ec, info.GetIsolate());
    else
        wrapper->commitChange();
}

v8::Handle<v8::Value> V8SVGLength::convertToSpecifiedUnitsCallback(const v8::Arguments& args)
{
    INC_STATS("DOM.SVGLength.convertToSpecifiedUnits");
    SVGPropertyTearOff<SVGLength>* wrapper = V8SVGLength::toNative(args.Holder());
    if (wrapper->role() == AnimValRole) {
        V8Proxy::setDOMException(NO_MODIFICATION_ALLOWED_ERR, args.GetIsolate());
        return v8::Handle<v8::Value>();
    }

    if (args.Length() < 1)
        return V8Proxy::throwNotEnoughArgumentsError();

    SVGLength& imp = wrapper->propertyReference();
    ExceptionCode ec = 0;
    EXCEPTION_BLOCK(int, unitType, toUInt32(args[0]));
    SVGLengthContext lengthContext(wrapper->contextElement());
    imp.convertToSpecifiedUnits(unitType, lengthContext, ec);
    if (UNLIKELY(ec))
        V8Proxy::setDOMException(ec, args.GetIsolate());
    else
        wrapper->commitChange();
    return v8::Handle<v8::Value>();
}

} // namespace WebCore

#endif
