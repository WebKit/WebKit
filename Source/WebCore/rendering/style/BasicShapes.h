/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
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

#ifndef BasicShapes_h
#define BasicShapes_h

#include "Length.h"
#include "LengthSize.h"
#include "RenderStyleConstants.h"
#include "WindRule.h"
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace WebCore {

class FloatRect;
class Path;
class RenderBox;

class BasicShape : public RefCounted<BasicShape> {
public:
    virtual ~BasicShape() { }

    enum Type {
        BasicShapeRectangleType,
        DeprecatedBasicShapeCircleType,
        DeprecatedBasicShapeEllipseType,
        BasicShapePolygonType,
        BasicShapeInsetRectangleType,
        BasicShapeCircleType,
        BasicShapeEllipseType,
        BasicShapeInsetType
    };

    bool canBlend(const BasicShape*) const;

    virtual void path(Path&, const FloatRect&) = 0;
    virtual WindRule windRule() const { return RULE_NONZERO; }
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double, const RenderBox&) const = 0;

    virtual Type type() const = 0;

    LayoutBox layoutBox() const { return m_layoutBox; }
    void setLayoutBox(LayoutBox layoutBox) { m_layoutBox = layoutBox; }

protected:
    BasicShape()
        : m_layoutBox(BoxMissing)
    {
    }

    FloatSize referenceBoxSize(const RenderBox&) const;

private:
    LayoutBox m_layoutBox;
};

class BasicShapeRectangle : public BasicShape {
public:
    static PassRefPtr<BasicShapeRectangle> create() { return adoptRef(new BasicShapeRectangle); }

    const Length& x() const { return m_x; }
    const Length& y() const { return m_y; }
    const Length& width() const { return m_width; }
    const Length& height() const { return m_height; }
    const Length& cornerRadiusX() const { return m_cornerRadiusX; }
    const Length& cornerRadiusY() const { return m_cornerRadiusY; }

    void setX(Length x) { m_x = std::move(x); }
    void setY(Length y) { m_y = std::move(y); }
    void setWidth(Length width) { m_width = std::move(width); }
    void setHeight(Length height) { m_height = std::move(height); }
    void setCornerRadiusX(Length radiusX)
    {
        ASSERT(!radiusX.isUndefined());
        m_cornerRadiusX = std::move(radiusX);
    }
    void setCornerRadiusY(Length radiusY)
    {
        ASSERT(!radiusY.isUndefined());
        m_cornerRadiusY = std::move(radiusY);
    }

    virtual void path(Path&, const FloatRect&) override;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double, const RenderBox&) const override;

    virtual Type type() const override { return BasicShapeRectangleType; }
private:
    BasicShapeRectangle() { }

    Length m_y;
    Length m_x;
    Length m_width;
    Length m_height;
    Length m_cornerRadiusX;
    Length m_cornerRadiusY;
};

class BasicShapeCenterCoordinate {
public:
    enum Keyword {
        None,
        Top,
        Right,
        Bottom,
        Left
    };
    BasicShapeCenterCoordinate() : m_keyword(None), m_length(Undefined) { }
    explicit BasicShapeCenterCoordinate(Length length) : m_keyword(None), m_length(length) { }
    BasicShapeCenterCoordinate(Keyword keyword, Length length) : m_keyword(keyword), m_length(length) { }
    BasicShapeCenterCoordinate(const BasicShapeCenterCoordinate& other) : m_keyword(other.keyword()), m_length(other.length()) { }

    Keyword keyword() const { return m_keyword; }
    const Length& length() const { return m_length; }
    Length lengthForBlending(const FloatSize&) const;

    BasicShapeCenterCoordinate blend(const BasicShapeCenterCoordinate& other, double progress, const FloatSize& boxSize) const
    {
        return BasicShapeCenterCoordinate(lengthForBlending(boxSize).blend(other.lengthForBlending(boxSize), progress));
    }

private:
    Keyword m_keyword;
    Length m_length;
};

class BasicShapeRadius {
public:
    enum Type {
        Value,
        ClosestSide,
        FarthestSide
    };
    BasicShapeRadius() : m_value(Undefined), m_type(ClosestSide) { }
    explicit BasicShapeRadius(Length v) : m_value(v), m_type(Value) { }
    explicit BasicShapeRadius(Type t) : m_value(Undefined), m_type(t) { }
    BasicShapeRadius(const BasicShapeRadius& other) : m_value(other.value()), m_type(other.type()) { }

    const Length& value() const { return m_value; }
    Type type() const { return m_type; }

    bool canBlend(const BasicShapeRadius& other) const
    {
        // FIXME determine how to interpolate between keywords. See bug 125108.
        return m_type == Value && other.type() == Value;
    }

    BasicShapeRadius blend(const BasicShapeRadius& other, double progress) const
    {
        if (m_type != Value || other.type() != Value)
            return BasicShapeRadius(other);

        return BasicShapeRadius(m_value.blend(other.value(), progress));
    }

private:
    Length m_value;
    Type m_type;

};

class BasicShapeCircle : public BasicShape {
public:
    static PassRefPtr<BasicShapeCircle> create() { return adoptRef(new BasicShapeCircle); }

    const BasicShapeCenterCoordinate& centerX() const { return m_centerX; }
    const BasicShapeCenterCoordinate& centerY() const { return m_centerY; }
    const BasicShapeRadius& radius() const { return m_radius; }
    float floatValueForRadiusInBox(float boxWidth, float boxHeight) const;

    void setCenterX(BasicShapeCenterCoordinate centerX) { m_centerX = std::move(centerX); }
    void setCenterY(BasicShapeCenterCoordinate centerY) { m_centerY = std::move(centerY); }
    void setRadius(BasicShapeRadius radius) { m_radius = std::move(radius); }

    virtual void path(Path&, const FloatRect&) override;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double, const RenderBox&) const override;

    virtual Type type() const override { return BasicShapeCircleType; }
private:
    BasicShapeCircle() { }

    BasicShapeCenterCoordinate m_centerX;
    BasicShapeCenterCoordinate m_centerY;
    BasicShapeRadius m_radius;
};

class DeprecatedBasicShapeCircle : public BasicShape {
public:
    static PassRefPtr<DeprecatedBasicShapeCircle> create() { return adoptRef(new DeprecatedBasicShapeCircle); }

    const Length& centerX() const { return m_centerX; }
    const Length& centerY() const { return m_centerY; }
    const Length& radius() const { return m_radius; }

    void setCenterX(Length centerX) { m_centerX = std::move(centerX); }
    void setCenterY(Length centerY) { m_centerY = std::move(centerY); }
    void setRadius(Length radius) { m_radius = std::move(radius); }

    virtual void path(Path&, const FloatRect&) override;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double, const RenderBox&) const override;

    virtual Type type() const override { return DeprecatedBasicShapeCircleType; }
private:
    DeprecatedBasicShapeCircle() { }

    Length m_centerX;
    Length m_centerY;
    Length m_radius;
};

class BasicShapeEllipse : public BasicShape {
public:
    static PassRefPtr<BasicShapeEllipse> create() { return adoptRef(new BasicShapeEllipse); }

    const BasicShapeCenterCoordinate& centerX() const { return m_centerX; }
    const BasicShapeCenterCoordinate& centerY() const { return m_centerY; }
    const BasicShapeRadius& radiusX() const { return m_radiusX; }
    const BasicShapeRadius& radiusY() const { return m_radiusY; }
    float floatValueForRadiusInBox(const BasicShapeRadius&, float center, float boxWidthOrHeight) const;

    void setCenterX(BasicShapeCenterCoordinate centerX) { m_centerX = std::move(centerX); }
    void setCenterY(BasicShapeCenterCoordinate centerY) { m_centerY = std::move(centerY); }
    void setRadiusX(BasicShapeRadius radiusX) { m_radiusX = std::move(radiusX); }
    void setRadiusY(BasicShapeRadius radiusY) { m_radiusY = std::move(radiusY); }

    virtual void path(Path&, const FloatRect&) override;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double, const RenderBox&) const override;

    virtual Type type() const override { return BasicShapeEllipseType; }
private:
    BasicShapeEllipse() { }

    BasicShapeCenterCoordinate m_centerX;
    BasicShapeCenterCoordinate m_centerY;
    BasicShapeRadius m_radiusX;
    BasicShapeRadius m_radiusY;
};

class DeprecatedBasicShapeEllipse : public BasicShape {
public:
    static PassRefPtr<DeprecatedBasicShapeEllipse> create() { return adoptRef(new DeprecatedBasicShapeEllipse); }

    const Length& centerX() const { return m_centerX; }
    const Length& centerY() const { return m_centerY; }
    const Length& radiusX() const { return m_radiusX; }
    const Length& radiusY() const { return m_radiusY; }

    void setCenterX(Length centerX) { m_centerX = std::move(centerX); }
    void setCenterY(Length centerY) { m_centerY = std::move(centerY); }
    void setRadiusX(Length radiusX) { m_radiusX = std::move(radiusX); }
    void setRadiusY(Length radiusY) { m_radiusY = std::move(radiusY); }

    virtual void path(Path&, const FloatRect&) override;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double, const RenderBox&) const override;

    virtual Type type() const override { return DeprecatedBasicShapeEllipseType; }
private:
    DeprecatedBasicShapeEllipse() { }

    Length m_centerX;
    Length m_centerY;
    Length m_radiusX;
    Length m_radiusY;
};

class BasicShapePolygon : public BasicShape {
public:
    static PassRefPtr<BasicShapePolygon> create() { return adoptRef(new BasicShapePolygon); }

    const Vector<Length>& values() const { return m_values; }
    const Length& getXAt(unsigned i) const { return m_values[2 * i]; }
    const Length& getYAt(unsigned i) const { return m_values[2 * i + 1]; }

    void setWindRule(WindRule windRule) { m_windRule = windRule; }
    void appendPoint(Length x, Length y) { m_values.append(std::move(x)); m_values.append(std::move(y)); }

    virtual void path(Path&, const FloatRect&) override;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double, const RenderBox&) const override;

    virtual WindRule windRule() const override { return m_windRule; }

    virtual Type type() const override { return BasicShapePolygonType; }
private:
    BasicShapePolygon()
        : m_windRule(RULE_NONZERO)
    { }

    WindRule m_windRule;
    Vector<Length> m_values;
};

class BasicShapeInsetRectangle : public BasicShape {
public:
    static PassRefPtr<BasicShapeInsetRectangle> create() { return adoptRef(new BasicShapeInsetRectangle); }

    const Length& top() const { return m_top; }
    const Length& right() const { return m_right; }
    const Length& bottom() const { return m_bottom; }
    const Length& left() const { return m_left; }
    const Length& cornerRadiusX() const { return m_cornerRadiusX; }
    const Length& cornerRadiusY() const { return m_cornerRadiusY; }

    void setTop(Length top) { m_top = std::move(top); }
    void setRight(Length right) { m_right = std::move(right); }
    void setBottom(Length bottom) { m_bottom = std::move(bottom); }
    void setLeft(Length left) { m_left = std::move(left); }
    void setCornerRadiusX(Length radiusX)
    {
        ASSERT(!radiusX.isUndefined());
        m_cornerRadiusX = std::move(radiusX);
    }
    void setCornerRadiusY(Length radiusY)
    {
        ASSERT(!radiusY.isUndefined());
        m_cornerRadiusY = std::move(radiusY);
    }

    virtual void path(Path&, const FloatRect&) override;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double, const RenderBox&) const override;

    virtual Type type() const override { return BasicShapeInsetRectangleType; }
private:
    BasicShapeInsetRectangle() { }

    Length m_right;
    Length m_top;
    Length m_bottom;
    Length m_left;
    Length m_cornerRadiusX;
    Length m_cornerRadiusY;
};

class BasicShapeInset : public BasicShape {
public:
    static PassRefPtr<BasicShapeInset> create() { return adoptRef(new BasicShapeInset); }

    const Length& top() const { return m_top; }
    const Length& right() const { return m_right; }
    const Length& bottom() const { return m_bottom; }
    const Length& left() const { return m_left; }

    const LengthSize& topLeftRadius() const { return m_topLeftRadius; }
    const LengthSize& topRightRadius() const { return m_topRightRadius; }
    const LengthSize& bottomRightRadius() const { return m_bottomRightRadius; }
    const LengthSize& bottomLeftRadius() const { return m_bottomLeftRadius; }

    void setTop(Length top) { m_top = std::move(top); }
    void setRight(Length right) { m_right = std::move(right); }
    void setBottom(Length bottom) { m_bottom = std::move(bottom); }
    void setLeft(Length left) { m_left = std::move(left); }

    void setTopLeftRadius(LengthSize radius) { m_topLeftRadius = std::move(radius); }
    void setTopRightRadius(LengthSize radius) { m_topRightRadius = std::move(radius); }
    void setBottomRightRadius(LengthSize radius) { m_bottomRightRadius = std::move(radius); }
    void setBottomLeftRadius(LengthSize radius) { m_bottomLeftRadius = std::move(radius); }

    virtual void path(Path&, const FloatRect&) override;
    virtual PassRefPtr<BasicShape> blend(const BasicShape*, double, const RenderBox&) const override;

    virtual Type type() const override { return BasicShapeInsetType; }
private:
    BasicShapeInset() { }

    Length m_right;
    Length m_top;
    Length m_bottom;
    Length m_left;

    LengthSize m_topLeftRadius;
    LengthSize m_topRightRadius;
    LengthSize m_bottomRightRadius;
    LengthSize m_bottomLeftRadius;
};

}
#endif
