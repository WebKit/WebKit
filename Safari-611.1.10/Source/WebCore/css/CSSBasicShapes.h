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

#include "WindRule.h"
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class CSSPrimitiveValue;
class SVGPathByteStream;

class CSSBasicShape : public RefCounted<CSSBasicShape> {
public:
    enum Type {
        CSSBasicShapePolygonType,
        CSSBasicShapeCircleType,
        CSSBasicShapeEllipseType,
        CSSBasicShapeInsetType,
        CSSBasicShapePathType
    };

    virtual Type type() const = 0;
    virtual String cssText() const = 0;
    virtual bool equals(const CSSBasicShape&) const = 0;

public:
    virtual ~CSSBasicShape() = default;

protected:
    CSSBasicShape() = default;
    RefPtr<CSSPrimitiveValue> m_referenceBox;
};

class CSSBasicShapeInset final : public CSSBasicShape {
public:
    static Ref<CSSBasicShapeInset> create() { return adoptRef(*new CSSBasicShapeInset); }

    CSSPrimitiveValue* top() const { return m_top.get(); }
    CSSPrimitiveValue* right() const { return m_right.get(); }
    CSSPrimitiveValue* bottom() const { return m_bottom.get(); }
    CSSPrimitiveValue* left() const { return m_left.get(); }

    CSSPrimitiveValue* topLeftRadius() const { return m_topLeftRadius.get(); }
    CSSPrimitiveValue* topRightRadius() const { return m_topRightRadius.get(); }
    CSSPrimitiveValue* bottomRightRadius() const { return m_bottomRightRadius.get(); }
    CSSPrimitiveValue* bottomLeftRadius() const { return m_bottomLeftRadius.get(); }

    void setTop(Ref<CSSPrimitiveValue>&& top) { m_top = WTFMove(top); }
    void setRight(Ref<CSSPrimitiveValue>&& right) { m_right = WTFMove(right); }
    void setBottom(Ref<CSSPrimitiveValue>&& bottom) { m_bottom = WTFMove(bottom); }
    void setLeft(Ref<CSSPrimitiveValue>&& left) { m_left = WTFMove(left); }

    void updateShapeSize4Values(Ref<CSSPrimitiveValue>&& top, Ref<CSSPrimitiveValue>&& right, Ref<CSSPrimitiveValue>&& bottom, Ref<CSSPrimitiveValue>&& left)
    {
        setTop(WTFMove(top));
        setRight(WTFMove(right));
        setBottom(WTFMove(bottom));
        setLeft(WTFMove(left));
    }

    void updateShapeSize1Value(Ref<CSSPrimitiveValue>&& value1)
    {
        updateShapeSize4Values(value1.copyRef(), value1.copyRef(), value1.copyRef(), value1.copyRef());
    }

    void updateShapeSize2Values(Ref<CSSPrimitiveValue>&& value1, Ref<CSSPrimitiveValue>&& value2)
    {
        updateShapeSize4Values(value1.copyRef(), value2.copyRef(), value1.copyRef(), value2.copyRef());
    }

    void updateShapeSize3Values(Ref<CSSPrimitiveValue>&& value1, Ref<CSSPrimitiveValue>&& value2,  Ref<CSSPrimitiveValue>&& value3)
    {
        updateShapeSize4Values(WTFMove(value1), value2.copyRef(), WTFMove(value3), value2.copyRef());
    }

    void setTopLeftRadius(RefPtr<CSSPrimitiveValue>&& radius) { m_topLeftRadius = WTFMove(radius); }
    void setTopRightRadius(RefPtr<CSSPrimitiveValue>&& radius) { m_topRightRadius = WTFMove(radius); }
    void setBottomRightRadius(RefPtr<CSSPrimitiveValue>&& radius) { m_bottomRightRadius = WTFMove(radius); }
    void setBottomLeftRadius(RefPtr<CSSPrimitiveValue>&& radius) { m_bottomLeftRadius = WTFMove(radius); }

private:
    CSSBasicShapeInset() = default;

    Type type() const final { return CSSBasicShapeInsetType; }
    String cssText() const final;
    bool equals(const CSSBasicShape&) const final;

    RefPtr<CSSPrimitiveValue> m_top;
    RefPtr<CSSPrimitiveValue> m_right;
    RefPtr<CSSPrimitiveValue> m_bottom;
    RefPtr<CSSPrimitiveValue> m_left;

    RefPtr<CSSPrimitiveValue> m_topLeftRadius;
    RefPtr<CSSPrimitiveValue> m_topRightRadius;
    RefPtr<CSSPrimitiveValue> m_bottomRightRadius;
    RefPtr<CSSPrimitiveValue> m_bottomLeftRadius;
};

class CSSBasicShapeCircle final : public CSSBasicShape {
public:
    static Ref<CSSBasicShapeCircle> create() { return adoptRef(*new CSSBasicShapeCircle); }

    CSSPrimitiveValue* centerX() const { return m_centerX.get(); }
    CSSPrimitiveValue* centerY() const { return m_centerY.get(); }
    CSSPrimitiveValue* radius() const { return m_radius.get(); }

    void setCenterX(Ref<CSSPrimitiveValue>&& centerX) { m_centerX = WTFMove(centerX); }
    void setCenterY(Ref<CSSPrimitiveValue>&& centerY) { m_centerY = WTFMove(centerY); }
    void setRadius(Ref<CSSPrimitiveValue>&& radius) { m_radius = WTFMove(radius); }

private:
    CSSBasicShapeCircle() = default;

    Type type() const final { return CSSBasicShapeCircleType; }
    String cssText() const final;
    bool equals(const CSSBasicShape&) const final;

    RefPtr<CSSPrimitiveValue> m_centerX;
    RefPtr<CSSPrimitiveValue> m_centerY;
    RefPtr<CSSPrimitiveValue> m_radius;
};

class CSSBasicShapeEllipse final : public CSSBasicShape {
public:
    static Ref<CSSBasicShapeEllipse> create() { return adoptRef(*new CSSBasicShapeEllipse); }

    CSSPrimitiveValue* centerX() const { return m_centerX.get(); }
    CSSPrimitiveValue* centerY() const { return m_centerY.get(); }
    CSSPrimitiveValue* radiusX() const { return m_radiusX.get(); }
    CSSPrimitiveValue* radiusY() const { return m_radiusY.get(); }

    void setCenterX(Ref<CSSPrimitiveValue>&& centerX) { m_centerX = WTFMove(centerX); }
    void setCenterY(Ref<CSSPrimitiveValue>&& centerY) { m_centerY = WTFMove(centerY); }
    void setRadiusX(Ref<CSSPrimitiveValue>&& radiusX) { m_radiusX = WTFMove(radiusX); }
    void setRadiusY(Ref<CSSPrimitiveValue>&& radiusY) { m_radiusY = WTFMove(radiusY); }

private:
    CSSBasicShapeEllipse() = default;

    Type type() const final { return CSSBasicShapeEllipseType; }
    String cssText() const final;
    bool equals(const CSSBasicShape&) const final;

    RefPtr<CSSPrimitiveValue> m_centerX;
    RefPtr<CSSPrimitiveValue> m_centerY;
    RefPtr<CSSPrimitiveValue> m_radiusX;
    RefPtr<CSSPrimitiveValue> m_radiusY;
};

class CSSBasicShapePolygon final : public CSSBasicShape {
public:
    static Ref<CSSBasicShapePolygon> create() { return adoptRef(*new CSSBasicShapePolygon); }

    void appendPoint(Ref<CSSPrimitiveValue>&& x, Ref<CSSPrimitiveValue>&& y)
    {
        m_values.append(WTFMove(x));
        m_values.append(WTFMove(y));
    }

    const Vector<Ref<CSSPrimitiveValue>>& values() const { return m_values; }

    void setWindRule(WindRule rule) { m_windRule = rule; }
    WindRule windRule() const { return m_windRule; }

private:
    CSSBasicShapePolygon()
        : m_windRule(WindRule::NonZero)
    {
    }

    Type type() const final { return CSSBasicShapePolygonType; }
    String cssText() const final;
    bool equals(const CSSBasicShape&) const final;

    Vector<Ref<CSSPrimitiveValue>> m_values;
    WindRule m_windRule;
};

class CSSBasicShapePath final : public CSSBasicShape {
public:
    static Ref<CSSBasicShapePath> create(std::unique_ptr<SVGPathByteStream>&& pathData)
    {
        return adoptRef(*new CSSBasicShapePath(WTFMove(pathData)));
    }

    const SVGPathByteStream& pathData() const
    {
        return *m_byteStream;
    }

    void setWindRule(WindRule rule) { m_windRule = rule; }
    WindRule windRule() const { return m_windRule; }

private:
    CSSBasicShapePath(std::unique_ptr<SVGPathByteStream>&&);

    Type type() const final { return CSSBasicShapePathType; }
    String cssText() const final;
    bool equals(const CSSBasicShape&) const final;

    std::unique_ptr<SVGPathByteStream> m_byteStream;
    WindRule m_windRule { WindRule::NonZero };
};

} // namespace WebCore

#define SPECIALIZE_TYPE_TRAITS_CSS_BASIC_SHAPES(ToValueTypeName) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ToValueTypeName) \
    static bool isType(const WebCore::CSSBasicShape& basicShape) { return basicShape.type() == WebCore::CSSBasicShape::ToValueTypeName##Type; } \
SPECIALIZE_TYPE_TRAITS_END()

SPECIALIZE_TYPE_TRAITS_CSS_BASIC_SHAPES(CSSBasicShapeInset)
SPECIALIZE_TYPE_TRAITS_CSS_BASIC_SHAPES(CSSBasicShapeCircle)
SPECIALIZE_TYPE_TRAITS_CSS_BASIC_SHAPES(CSSBasicShapeEllipse)
SPECIALIZE_TYPE_TRAITS_CSS_BASIC_SHAPES(CSSBasicShapePolygon)
SPECIALIZE_TYPE_TRAITS_CSS_BASIC_SHAPES(CSSBasicShapePath)
