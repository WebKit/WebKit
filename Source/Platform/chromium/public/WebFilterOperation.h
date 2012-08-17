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

#include "SkScalar.h"
#include "WebCommon.h"
#include "WebColor.h"
#include "WebPoint.h"
#include "WebRect.h"

namespace WebKit {

class WebFilterOperation {
public:
    enum FilterType {
        FilterTypeGrayscale,
        FilterTypeSepia,
        FilterTypeSaturate,
        FilterTypeHueRotate,
        FilterTypeInvert,
        FilterTypeBrightness,
        FilterTypeContrast,
        FilterTypeOpacity,
        FilterTypeBlur,
        FilterTypeDropShadow,
        FilterTypeColorMatrix,
        FilterTypeZoom,
    };

    FilterType type() const { return m_type; }

    float amount() const
    {
        return m_amount;
    }
    WebPoint dropShadowOffset() const
    {
        WEBKIT_ASSERT(m_type == FilterTypeDropShadow);
        return WebPoint(m_dropShadowOffset);
    }
    WebColor dropShadowColor() const
    {
        WEBKIT_ASSERT(m_type == FilterTypeDropShadow);
        return m_dropShadowColor;
    }
    const SkScalar* matrix() const
    {
        return m_matrix;
    }

    WebRect zoomRect() const
    {
        WEBKIT_ASSERT(m_type == FilterTypeZoom);
        return WebRect(m_zoomRect);
    }

#define WEBKIT_HAS_NEW_WEBFILTEROPERATION_API 1
    static WebFilterOperation createGrayscaleFilter(float amount) { return WebFilterOperation(FilterTypeGrayscale, amount); }
    static WebFilterOperation createSepiaFilter(float amount) { return WebFilterOperation(FilterTypeSepia, amount); }
    static WebFilterOperation createSaturateFilter(float amount) { return WebFilterOperation(FilterTypeSaturate, amount); }
    static WebFilterOperation createHueRotateFilter(float amount) { return WebFilterOperation(FilterTypeHueRotate, amount); }
    static WebFilterOperation createInvertFilter(float amount) { return WebFilterOperation(FilterTypeInvert, amount); }
    static WebFilterOperation createBrightnessFilter(float amount) { return WebFilterOperation(FilterTypeBrightness, amount); }
    static WebFilterOperation createContrastFilter(float amount) { return WebFilterOperation(FilterTypeContrast, amount); }
    static WebFilterOperation createOpacityFilter(float amount) { return WebFilterOperation(FilterTypeOpacity, amount); }
    static WebFilterOperation createBlurFilter(float amount) { return WebFilterOperation(FilterTypeBlur, amount); }
    static WebFilterOperation createDropShadowFilter(WebPoint offset, float stdDeviation, WebColor color) { return WebFilterOperation(FilterTypeDropShadow, offset, stdDeviation, color); }
    static WebFilterOperation createColorMatrixFilter(SkScalar matrix[20]) { return WebFilterOperation(FilterTypeColorMatrix, matrix); }
    static WebFilterOperation createZoomFilter(WebRect rect, int inset) { return WebFilterOperation(FilterTypeZoom, rect, inset); }

    bool equals(const WebFilterOperation& other) const;

private:
    FilterType m_type;

    float m_amount;
    WebPoint m_dropShadowOffset;
    WebColor m_dropShadowColor;
    SkScalar m_matrix[20];
    WebRect m_zoomRect;

    WebFilterOperation(FilterType type, float amount)
    {
        WEBKIT_ASSERT(type != FilterTypeDropShadow && type != FilterTypeColorMatrix);
        m_type = type;
        m_amount = amount;
        m_dropShadowColor = 0;
    }

    WebFilterOperation(FilterType type, WebPoint offset, float stdDeviation, WebColor color)
    {
        WEBKIT_ASSERT(type == FilterTypeDropShadow);
        m_type = type;
        m_amount = stdDeviation;
        m_dropShadowOffset = offset;
        m_dropShadowColor = color;
    }

    WebFilterOperation(FilterType, SkScalar matrix[20]);

    WebFilterOperation(FilterType type, WebRect rect, float inset)
    {
        WEBKIT_ASSERT(type == FilterTypeZoom);
        m_type = type;
        m_amount = inset;
        m_zoomRect = rect;
    }
};

inline bool operator==(const WebFilterOperation& a, const WebFilterOperation& b)
{
    return a.equals(b);
}

inline bool operator!=(const WebFilterOperation& a, const WebFilterOperation& b)
{
    return !(a == b);
}

}

#endif
