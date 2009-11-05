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

#include "TransformationMatrix.h"

#include "SVGException.h"

#include "V8Binding.h"
#include "V8CustomBinding.h"
#include "V8SVGMatrix.h"
#include "V8SVGPODTypeWrapper.h"
#include "V8Proxy.h"

namespace WebCore {

CALLBACK_FUNC_DECL(SVGMatrixMultiply)
{
    INC_STATS("DOM.SVGMatrix.multiply()");
    if (args.Length() < 1)
        return throwError("Not enough arguments");

    if (!V8SVGMatrix::HasInstance(args[0]))
        return throwError("secondMatrix argument was not a SVGMatrix");

    TransformationMatrix m1 = *V8DOMWrapper::convertToNativeObject<V8SVGPODTypeWrapper<TransformationMatrix> >(V8ClassIndex::SVGMATRIX, args.Holder());
    TransformationMatrix m2 = *V8DOMWrapper::convertToNativeObject<V8SVGPODTypeWrapper<TransformationMatrix> >(V8ClassIndex::SVGMATRIX, v8::Handle<v8::Object>::Cast(args[0]));

    return V8DOMWrapper::convertToV8Object(V8ClassIndex::SVGMATRIX, V8SVGStaticPODTypeWrapper<TransformationMatrix>::create(m1.multLeft(m2)));
}

CALLBACK_FUNC_DECL(SVGMatrixInverse)
{
    INC_STATS("DOM.SVGMatrix.inverse()");
    TransformationMatrix matrix = *V8DOMWrapper::convertToNativeObject<V8SVGPODTypeWrapper<TransformationMatrix> >(V8ClassIndex::SVGMATRIX, args.Holder());
    ExceptionCode ec = 0;
    TransformationMatrix result = matrix.inverse();

    if (!matrix.isInvertible())
        ec = SVGException::SVG_MATRIX_NOT_INVERTABLE;

    if (ec != 0) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }

    return V8DOMWrapper::convertToV8Object(V8ClassIndex::SVGMATRIX, V8SVGStaticPODTypeWrapper<TransformationMatrix>::create(result));
}

CALLBACK_FUNC_DECL(SVGMatrixRotateFromVector)
{
    INC_STATS("DOM.SVGMatrix.rotateFromVector()");
    TransformationMatrix matrix = *V8DOMWrapper::convertToNativeObject<V8SVGPODTypeWrapper<TransformationMatrix> >(V8ClassIndex::SVGMATRIX, args.Holder());
    ExceptionCode ec = 0;
    float x = toFloat(args[0]);
    float y = toFloat(args[1]);
    TransformationMatrix result = matrix;
    result.rotateFromVector(x, y);
    if (x == 0.0 || y == 0.0)
        ec = SVGException::SVG_INVALID_VALUE_ERR;

    if (ec != 0) {
        V8Proxy::setDOMException(ec);
        return v8::Handle<v8::Value>();
    }

    return V8DOMWrapper::convertToV8Object(V8ClassIndex::SVGMATRIX, V8SVGStaticPODTypeWrapper<TransformationMatrix>::create(result));
}

} // namespace WebCore

#endif
