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

#include "SVGLength.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8SVGPODTypeWrapper.h"
#include "V8Proxy.h"

namespace WebCore {

ACCESSOR_GETTER(SVGLengthValue)
{
    INC_STATS("DOM.SVGLength.value");
    V8SVGPODTypeWrapper<SVGLength>* wrapper = V8Proxy::ToNativeObject<V8SVGPODTypeWrapper<SVGLength> >(V8ClassIndex::SVGLENGTH, info.Holder());
    SVGLength imp = *wrapper;
    return v8::Number::New(imp.value(V8Proxy::GetSVGContext(wrapper)));
}

CALLBACK_FUNC_DECL(SVGLengthConvertToSpecifiedUnits)
{
    INC_STATS("DOM.SVGLength.convertToSpecifiedUnits");
    V8SVGPODTypeWrapper<SVGLength>* wrapper = V8Proxy::ToNativeObject<V8SVGPODTypeWrapper<SVGLength> >(V8ClassIndex::SVGLENGTH, args.Holder());
    SVGLength imp = *wrapper;
    SVGElement* context = V8Proxy::GetSVGContext(wrapper);
    imp.convertToSpecifiedUnits(toInt32(args[0]), context);
    wrapper->commitChange(imp, context);
    return v8::Undefined();
}

} // namespace WebCore

#endif
