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

#if WEBKIT_IMPLEMENTATION
namespace WebCore {
class FilterOperation;
}
#endif

namespace WebKit {

struct WebFilterOperation {
#if WEBKIT_IMPLEMENTATION
    virtual PassRefPtr<WebCore::FilterOperation> toFilterOperation() const = 0;
#endif

protected:
    WebFilterOperation() { }
};

struct WebBasicColorMatrixFilterOperation : public WebFilterOperation {
    enum FilterType {
        FilterTypeGrayscale = 1,
        FilterTypeSepia = 2,
        FilterTypeSaturate = 3,
        FilterTypeHueRotate = 4
    };

    WebBasicColorMatrixFilterOperation(FilterType type, float amount)
        : type(type)
        , amount(amount)
    { }

#if WEBKIT_IMPLEMENTATION
    virtual PassRefPtr<WebCore::FilterOperation> toFilterOperation() const OVERRIDE;
#endif

    FilterType type;
    float amount;
};

struct WebBasicComponentTransferFilterOperation : public WebFilterOperation {
    enum FilterType {
        FilterTypeInvert = 5,
        FilterTypeBrightness = 7,
        FilterTypeContrast = 8
        // Opacity is missing because this is more expensive than just setting opacity on the layer.
    };

    WebBasicComponentTransferFilterOperation(FilterType type, float amount)
        : type(type)
        , amount(amount)
    { }

#if WEBKIT_IMPLEMENTATION
    virtual PassRefPtr<WebCore::FilterOperation> toFilterOperation() const OVERRIDE;
#endif

    FilterType type;
    float amount;
};

struct WebBlurFilterOperation : public WebFilterOperation {
    explicit WebBlurFilterOperation(int pixelRadius) : pixelRadius(pixelRadius) { }

#if WEBKIT_IMPLEMENTATION
    virtual PassRefPtr<WebCore::FilterOperation> toFilterOperation() const OVERRIDE;
#endif

    int pixelRadius;
};

class WebDropShadowFilterOperation : public WebFilterOperation {
    WebDropShadowFilterOperation(int x, int y, int stdDeviation, WebColor color)
        : x(x)
        , y(y)
        , stdDeviation(stdDeviation)
        , color(color)
    { }

#if WEBKIT_IMPLEMENTATION
    virtual PassRefPtr<WebCore::FilterOperation> toFilterOperation() const OVERRIDE;
#endif

    int x;
    int y;
    int stdDeviation;
    WebColor color;
};

}

#endif
