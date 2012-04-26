/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebFilterOperation_h
#define WebFilterOperation_h

#include "WebCommon.h"

#include "WebColor.h"

#if WEBKIT_IMPLEMENTATION
#include <wtf/PassRefPtr.h>
#endif

namespace WebKit {

struct WebFilterOperation {
    enum FilterType {
        FilterTypeBasicColorMatrix,
        FilterTypeBasicComponentTransfer,
        FilterTypeBlur,
        FilterTypeDropShadow
    };

    FilterType type;

protected:
    WebFilterOperation(FilterType type)
        : type(type)
    { }
};

struct WebBasicColorMatrixFilterOperation : public WebFilterOperation {
    enum BasicColorMatrixFilterType {
        BasicColorMatrixFilterTypeGrayscale = 1,
        BasicColorMatrixFilterTypeSepia = 2,
        BasicColorMatrixFilterTypeSaturate = 3,
        BasicColorMatrixFilterTypeHueRotate = 4
    };

    WebBasicColorMatrixFilterOperation(BasicColorMatrixFilterType type, float amount)
        : WebFilterOperation(FilterTypeBasicColorMatrix)
        , subtype(type)
        , amount(amount)
    { }

    BasicColorMatrixFilterType subtype;
    float amount;
};

struct WebBasicComponentTransferFilterOperation : public WebFilterOperation {
    enum BasicComponentTransferFilterType {
        BasicComponentTransferFilterTypeInvert = 5,
        BasicComponentTransferFilterTypeBrightness = 7,
        BasicComponentTransferFilterTypeContrast = 8
        // Opacity is missing because this is more expensive than just setting opacity on the layer.
    };

    WebBasicComponentTransferFilterOperation(BasicComponentTransferFilterType type, float amount)
        : WebFilterOperation(FilterTypeBasicComponentTransfer)
        , subtype(type)
        , amount(amount)
    { }

    BasicComponentTransferFilterType subtype;
    float amount;
};

struct WebBlurFilterOperation : public WebFilterOperation {
    explicit WebBlurFilterOperation(int pixelRadius)
        : WebFilterOperation(FilterTypeBlur)
        , pixelRadius(pixelRadius)
    { }

    int pixelRadius;
};

struct WebDropShadowFilterOperation : public WebFilterOperation {
    WebDropShadowFilterOperation(int x, int y, int stdDeviation, WebColor color) 
        : WebFilterOperation(FilterTypeDropShadow)
        , x(x)
        , y(y)
        , stdDeviation(stdDeviation)
        , color(color)
    { }

    int x;
    int y;
    int stdDeviation;
    WebColor color;
};

}

#endif
