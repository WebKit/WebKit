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

#ifndef CSSBasicShapes_h
#define CSSBasicShapes_h

#include "CSSPrimitiveValue.h"
#include "WindRule.h"
#include <wtf/RefPtr.h>
#include <wtf/TypeCasts.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

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
    virtual ~CSSBasicShape() { }

protected:
    CSSBasicShape() { }
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

    void setTop(PassRefPtr<CSSPrimitiveValue> top) { m_top = top; }
    void setRight(PassRefPtr<CSSPrimitiveValue> right) { m_right = right; }
    void setBottom(PassRefPtr<CSSPrimitiveValue> bottom) { m_bottom = bottom; }
    void setLeft(PassRefPtr<CSSPrimitiveValue> left) { m_left = left; }

    void updateShapeSize4Values(CSSPrimitiveValue* top, CSSPrimitiveValue* right, CSSPrimitiveValue* bottom, CSSPrimitiveValue* left)
    {
        setTop(top);
        setRight(right);
        setBottom(bottom);
        setLeft(left);
    }

    void updateShapeSize1Value(CSSPrimitiveValue* value1)
    {
        updateShapeSize4Values(value1, value1, value1, value1);
    }

    void updateShapeSize2Values(CSSPrimitiveValue* value1,  CSSPrimitiveValue* value2)
    {
        updateShapeSize4Values(value1, value2, value1, value2);
    }

    void updateShapeSize3Values(CSSPrimitiveValue* value1, CSSPrimitiveValue* value2,  CSSPrimitiveValue* value3)
    {
        updateShapeSize4Values(value1, value2, value3, value2);
    }

    void setTopLeftRadius(PassRefPtr<CSSPrimitiveValue> radius) { m_topLeftRadius = radius; }
    void setTopRightRadius(PassRefPtr<CSSPrimitiveValue> radius) { m_topRightRadius = radius; }
    void setBottomRightRadius(PassRefPtr<CSSPrimitiveValue> radius) { m_bottomRightRadius = radius; }
    void setBottomLeftRadius(PassRefPtr<CSSPrimitiveValue> radius) { m_bottomLeftRadius = radius; }

private:
    CSSBasicShapeInset() { }

    virtual Type type() const override { return CSSBasicShapeInsetType; }
    virtual String cssText() const override;
    virtual bool equals(const CSSBasicShape&) const override;

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

    void setCenterX(PassRefPtr<CSSPrimitiveValue> centerX) { m_centerX = centerX; }
    void setCenterY(PassRefPtr<CSSPrimitiveValue> centerY) { m_centerY = centerY; }
    void setRadius(PassRefPtr<CSSPrimitiveValue> radius) { m_radius = radius; }

private:
    CSSBasicShapeCircle() { }

    virtual Type type() const override { return CSSBasicShapeCircleType; }
    virtual String cssText() const override;
    virtual bool equals(const CSSBasicShape&) const override;

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

    void setCenterX(PassRefPtr<CSSPrimitiveValue> centerX) { m_centerX = centerX; }
    void setCenterY(PassRefPtr<CSSPrimitiveValue> centerY) { m_centerY = centerY; }
    void setRadiusX(PassRefPtr<CSSPrimitiveValue> radiusX) { m_radiusX = radiusX; }
    void setRadiusY(PassRefPtr<CSSPrimitiveValue> radiusY) { m_radiusY = radiusY; }

private:
    CSSBasicShapeEllipse() { }

    virtual Type type() const override { return CSSBasicShapeEllipseType; }
    virtual String cssText() const override;
    virtual bool equals(const CSSBasicShape&) const override;

    RefPtr<CSSPrimitiveValue> m_centerX;
    RefPtr<CSSPrimitiveValue> m_centerY;
    RefPtr<CSSPrimitiveValue> m_radiusX;
    RefPtr<CSSPrimitiveValue> m_radiusY;
};

class CSSBasicShapePolygon final : public CSSBasicShape {
public:
    static Ref<CSSBasicShapePolygon> create() { return adoptRef(*new CSSBasicShapePolygon); }

    void appendPoint(PassRefPtr<CSSPrimitiveValue> x, PassRefPtr<CSSPrimitiveValue> y)
    {
        m_values.append(x);
        m_values.append(y);
    }

    RefPtr<CSSPrimitiveValue> getXAt(unsigned i) const { return m_values.at(i * 2); }
    RefPtr<CSSPrimitiveValue> getYAt(unsigned i) const { return m_values.at(i * 2 + 1); }
    const Vector<RefPtr<CSSPrimitiveValue>>& values() const { return m_values; }

    void setWindRule(WindRule rule) { m_windRule = rule; }
    WindRule windRule() const { return m_windRule; }

private:
    CSSBasicShapePolygon()
        : m_windRule(RULE_NONZERO)
    {
    }

    virtual Type type() const override { return CSSBasicShapePolygonType; }
    virtual String cssText() const override;
    virtual bool equals(const CSSBasicShape&) const override;

    Vector<RefPtr<CSSPrimitiveValue>> m_values;
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

    virtual Type type() const override { return CSSBasicShapePathType; }
    virtual String cssText() const override;
    virtual bool equals(const CSSBasicShape&) const override;

    std::unique_ptr<SVGPathByteStream> m_byteStream;
    WindRule m_windRule { RULE_NONZERO };
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

#endif // CSSBasicShapes_h
