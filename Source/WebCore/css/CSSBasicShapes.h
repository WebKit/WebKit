/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "CSSValueList.h"
#include "SVGPathByteStream.h"
#include <wtf/UniqueRef.h>

namespace WebCore {

enum class WindRule : bool;

class CSSInsetShapeValue : public CSSValue {
public:
    static Ref<CSSInsetShapeValue> create(Ref<CSSValue> top, Ref<CSSValue> right, Ref<CSSValue> bottom, Ref<CSSValue> left,
        RefPtr<CSSValue> topLeftRadius, RefPtr<CSSValue> topRightRadius, RefPtr<CSSValue> bottomRightRadius, RefPtr<CSSValue> bottomLeftRadius);

    const CSSValue& top() const { return m_top; }
    const CSSValue& right() const { return m_right; }
    const CSSValue& bottom() const { return m_bottom; }
    const CSSValue& left() const { return m_left; }

    const CSSValue* topLeftRadius() const { return m_topLeftRadius.get(); }
    const CSSValue* topRightRadius() const { return m_topRightRadius.get(); }
    const CSSValue* bottomRightRadius() const { return m_bottomRightRadius.get(); }
    const CSSValue* bottomLeftRadius() const { return m_bottomLeftRadius.get(); }

    String customCSSText() const;
    bool equals(const CSSInsetShapeValue&) const;

private:
    CSSInsetShapeValue(Ref<CSSValue> top, Ref<CSSValue> right, Ref<CSSValue> bottom, Ref<CSSValue> left, RefPtr<CSSValue> topLeftRadius, RefPtr<CSSValue> topRightRadius, RefPtr<CSSValue> bottomRightRadius, RefPtr<CSSValue> bottomLeftRadius);

    Ref<CSSValue> m_top;
    Ref<CSSValue> m_right;
    Ref<CSSValue> m_bottom;
    Ref<CSSValue> m_left;

    RefPtr<CSSValue> m_topLeftRadius;
    RefPtr<CSSValue> m_topRightRadius;
    RefPtr<CSSValue> m_bottomRightRadius;
    RefPtr<CSSValue> m_bottomLeftRadius;
};

class CSSCircleValue : public CSSValue {
public:
    static Ref<CSSCircleValue> create(RefPtr<CSSValue> radius, RefPtr<CSSValue> centerX, RefPtr<CSSValue> centerY);

    const CSSValue* radius() const { return m_radius.get(); }
    const CSSValue* centerX() const { return m_centerX.get(); }
    const CSSValue* centerY() const { return m_centerY.get(); }

    String customCSSText() const;
    bool equals(const CSSCircleValue&) const;

private:
    CSSCircleValue(RefPtr<CSSValue> radius, RefPtr<CSSValue> centerX, RefPtr<CSSValue> centerY);

    RefPtr<CSSValue> m_radius;
    RefPtr<CSSValue> m_centerX;
    RefPtr<CSSValue> m_centerY;
};

class CSSEllipseValue : public CSSValue {
public:
    static Ref<CSSEllipseValue> create(RefPtr<CSSValue> radiusX, RefPtr<CSSValue> radiusY, RefPtr<CSSValue> centerX, RefPtr<CSSValue> centerY);

    const CSSValue* radiusX() const { return m_radiusX.get(); }
    const CSSValue* radiusY() const { return m_radiusY.get(); }
    const CSSValue* centerX() const { return m_centerX.get(); }
    const CSSValue* centerY() const { return m_centerY.get(); }

    String customCSSText() const;
    bool equals(const CSSEllipseValue&) const;

private:
    CSSEllipseValue(RefPtr<CSSValue> radiusX, RefPtr<CSSValue> radiusY, RefPtr<CSSValue> centerX, RefPtr<CSSValue> centerY);

    RefPtr<CSSValue> m_radiusX;
    RefPtr<CSSValue> m_radiusY;
    RefPtr<CSSValue> m_centerX;
    RefPtr<CSSValue> m_centerY;
};

class CSSPolygonValue : public CSSValueContainingVector {
public:
    static Ref<CSSPolygonValue> create(CSSValueListBuilder values, WindRule);

    WindRule windRule() const { return m_windRule; }

    String customCSSText() const;
    bool equals(const CSSPolygonValue&) const;

private:
    explicit CSSPolygonValue(CSSValueListBuilder, WindRule);

    WindRule m_windRule { };
};

class CSSPathValue : public CSSValue {
public:
    static Ref<CSSPathValue> create(SVGPathByteStream, WindRule);

    const SVGPathByteStream& pathData() const { return m_pathData; }
    WindRule windRule() const { return m_windRule; }

    String customCSSText() const;
    bool equals(const CSSPathValue&) const;

private:
    CSSPathValue(SVGPathByteStream, WindRule);

    SVGPathByteStream m_pathData;
    WindRule m_windRule { };
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSCircleValue, isCircle())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSEllipseValue, isEllipse())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSInsetShapeValue, isInsetShape())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPolygonValue, isPolygon())
SPECIALIZE_TYPE_TRAITS_CSS_VALUE(CSSPathValue, isPath())
